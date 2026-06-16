#include "WhipPch.h"
#include <Whip/Scripting/ScriptEngine.h>

#include <Whip/Scripting/ScriptGlue.h>

#include <Whip/Helper/Buffer.h>
#include <Whip/Utils/FileSystem.h>
#include <Whip/Core/Application.h>
#include <Whip/Project/Project.h>

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/object.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/threads.h>

#include <FileWatch.h>
#include <nps_formatter.h>

#include <algorithm>
#include <cctype>
#include <vector>

#ifndef WHP_MONO_FIELD_ATTRIBUTE_FIELD_ACCESS_MASK
#define WHP_MONO_FIELD_ATTRIBUTE_FIELD_ACCESS_MASK 0x0007u
#endif

#ifndef WHP_MONO_FIELD_ATTRIBUTE_PUBLIC
#define WHP_MONO_FIELD_ATTRIBUTE_PUBLIC 0x0006u
#endif

_WHIP_START

static constexpr size_t MaxTypeSize = 16; // Whip.Vector4
static constexpr size_t InitialBufferSize = 1024; // 1kb

namespace
{
	MonoAssembly* LoadMonoAssembly(const std::filesystem::path& assemblyPath, bool loadPdb = false)
	{
		ScopedBuffer fileData = FileSystem::ReadFileBinary(assemblyPath);

		MonoImageOpenStatus status;
		MonoImage* image = mono_image_open_from_data_full(fileData.As<char>(), static_cast<uint32_t>(fileData.Size()), 1, &status, 0);

		if (status != MONO_IMAGE_OK)
		{
			const char* errorMessage = mono_image_strerror(status);
			WHP_CORE_ERROR("[Script Engine] PDB loading failed: ({0})", errorMessage);
			return nullptr;
		}

		if (loadPdb)
		{
			std::filesystem::path pdbPath = assemblyPath;
			pdbPath.replace_extension(".pdb");

			if (std::filesystem::exists(pdbPath))
			{
				ScopedBuffer pdbFileData = FileSystem::ReadFileBinary(pdbPath);
				mono_debug_open_image_from_memory(image, pdbFileData.As<const mono_byte>(), static_cast<int>(pdbFileData.Size()));
				WHP_CORE_INFO("[Script Engine] Loaded PDB {0}", pdbPath.string());
			}
		}

		std::string pathString = assemblyPath.string();
		MonoAssembly* assembly = mono_assembly_load_from_full(image, pathString.c_str(), &status, 0);
		mono_image_close(image);

		return assembly;
	}

	std::filesystem::path CopyAssemblyToRuntimeShadow(const std::filesystem::path& assemblyPath)
	{
		std::error_code error;
		if (!std::filesystem::exists(assemblyPath, error))
			return assemblyPath;

		std::filesystem::path root = std::filesystem::temp_directory_path(error);
		if (error || root.empty())
			return assemblyPath;

		const std::uintmax_t size = std::filesystem::file_size(assemblyPath, error);
		if (error)
			return assemblyPath;

		const auto writeTime = std::filesystem::last_write_time(assemblyPath, error);
		if (error)
			return assemblyPath;

		std::filesystem::path runtimeDirectory = root / "Whip" / "ScriptRuntime" / assemblyPath.stem();
		std::filesystem::create_directories(runtimeDirectory, error);
		if (error)
			return assemblyPath;

		const std::string runtimeName = nps::formatter::format("{}_{}_{}", assemblyPath.stem().string(), size, writeTime.time_since_epoch().count());
		std::filesystem::path runtimeAssembly = runtimeDirectory / (runtimeName + ".dll");
		if (!std::filesystem::exists(runtimeAssembly, error))
		{
			error.clear();
			std::filesystem::copy_file(assemblyPath, runtimeAssembly, std::filesystem::copy_options::overwrite_existing, error);
			if (error)
			{
				WHP_CORE_WARN("[Script Engine] Could not create runtime shadow copy for app assembly: {0}", error.message());
				return assemblyPath;
			}
		}

		std::filesystem::path pdbPath = assemblyPath;
		pdbPath.replace_extension(".pdb");
		if (std::filesystem::exists(pdbPath, error))
		{
			error.clear();
			std::filesystem::copy_file(pdbPath, runtimeDirectory / (runtimeName + ".pdb"), std::filesystem::copy_options::overwrite_existing, error);
		}

		return runtimeAssembly;
	}

	MonoType* GetMonoTypeElementType(MonoType* type);

	std::pair<ScriptFieldType, bool> MonoTypeToScriptFieldType(MonoType* monoType)
	{
		static std::unordered_map<std::string, ScriptFieldType> s_ScriptFieldTypes =
		{
			{ "System.String", ScriptFieldType::String },
			{ "System.Single", ScriptFieldType::Float },
			{ "System.Double", ScriptFieldType::Double },
			{ "System.Boolean", ScriptFieldType::Bool },
			{ "System.Char", ScriptFieldType::Char },
			{ "System.Int16", ScriptFieldType::Short },
			{ "System.Int32", ScriptFieldType::Int },
			{ "System.Int64", ScriptFieldType::Long },
			{ "System.SByte", ScriptFieldType::SByte },
			{ "System.Byte", ScriptFieldType::Byte },
			{ "System.UInt16", ScriptFieldType::UShort },
			{ "System.UInt32", ScriptFieldType::UInt },
			{ "System.UInt64", ScriptFieldType::ULong },

			{ "Whip.KeyCode", ScriptFieldType::KeyCode },
			{ "Whip.MouseCode", ScriptFieldType::MouseCode },

			{ "Whip.Vector2", ScriptFieldType::Vector2 },
			{ "Whip.Vector3", ScriptFieldType::Vector3 },
			{ "Whip.Vector4", ScriptFieldType::Vector4 },

			{ "Whip.Entity", ScriptFieldType::Entity },
			{ "Whip.SceneReference", ScriptFieldType::Scene },
			{ "Whip.Logger", ScriptFieldType::Logger }
		};

		std::string typeName = mono_type_get_name(monoType);

		bool isArray = false;
		if (typeName.size() > 2 && typeName.back() == ']')
		{
			isArray = true;
			std::string_view view(typeName);
			view.remove_suffix(2);
			typeName = view;
		}

		auto it = s_ScriptFieldTypes.find(typeName);
		if (it == s_ScriptFieldTypes.end())
		{
			MonoType* elementType = isArray ? GetMonoTypeElementType(monoType) : monoType;
			MonoClass* monoClass = elementType ? mono_type_get_class(elementType) : nullptr;
			while (monoClass)
			{
				const char* className = mono_class_get_name(monoClass);
				const char* classNamespace = mono_class_get_namespace(monoClass);
				if (className && classNamespace &&
					std::string_view(className) == "Entity" &&
					std::string_view(classNamespace) == "Whip")
				{
					return { ScriptFieldType::Entity, isArray };
				}

				monoClass = mono_class_get_parent(monoClass);
			}

			WHP_CORE_ERROR("[Script Engine] Unknown type: {0}", typeName);
			return { ScriptFieldType::None, false };
		}
		return { it->second, isArray };
	}

	int AlignOfType(ScriptFieldType type)
	{
		switch (type)
		{
		case ScriptFieldType::None:    return 0;

		case ScriptFieldType::String:  // referans tip -> pointer
		case ScriptFieldType::Entity:  // referans tip
		case ScriptFieldType::Logger:  return alignof(void*);   // referans tip
		case ScriptFieldType::Scene:   return alignof(AssetHandle);

		case ScriptFieldType::Float:   return alignof(float);
		case ScriptFieldType::Double:  return alignof(double);
		case ScriptFieldType::Bool:    return alignof(bool);
		case ScriptFieldType::Char:    return alignof(char);
		case ScriptFieldType::SByte:   return alignof(int8_t);
		case ScriptFieldType::Short:   return alignof(int16_t);
		case ScriptFieldType::Int:     return alignof(int32_t);
		case ScriptFieldType::Long:    return alignof(int64_t);
		case ScriptFieldType::Byte:    return alignof(uint8_t);
		case ScriptFieldType::UShort:  return alignof(uint16_t);
		case ScriptFieldType::UInt:    return alignof(uint32_t);
		case ScriptFieldType::ULong:   return alignof(uint64_t);

		case ScriptFieldType::KeyCode:   return alignof(KeyCode);
		case ScriptFieldType::MouseCode: return alignof(MouseCode);

		case ScriptFieldType::Vector2: return alignof(glm::vec2);
		case ScriptFieldType::Vector3: return alignof(glm::vec3);
		case ScriptFieldType::Vector4: return alignof(glm::vec4);
		}

		return 0;
	}

	MonoType* GetMonoTypeElementType(MonoType* type)
	{
		if (!type)
			return nullptr;

		switch (mono_type_get_type(type))
		{
		case MONO_TYPE_SZARRAY: // float[]
		{
			MonoClass* arrayClass = mono_type_get_class(type);
			if (!arrayClass)
				return nullptr;

			MonoClass* elemClass = mono_class_get_element_class(arrayClass);
			return elemClass ? mono_class_get_type(elemClass) : nullptr;
		}

		case MONO_TYPE_ARRAY: // float[,]
		{
			MonoArrayType* arrType = mono_type_get_array_type(type);
			if (!arrType || !arrType->eklass)
				return nullptr;

			return mono_class_get_type(arrType->eklass);
		}

		default:
			return type;
		}
	}

}

struct ScriptEngineData
{
	MonoDomain* m_RootDomain = nullptr;
	MonoDomain* m_AppDomain = nullptr;

	MonoAssembly* m_CoreAssembly = nullptr;
	MonoImage* m_CoreAssemblyImage = nullptr;

	MonoAssembly* m_AppAssembly = nullptr;
	MonoImage* m_AppAssemblyImage = nullptr;

	std::filesystem::path m_CoreAssemblyFilepath;
	std::filesystem::path m_AppAssemblyFilepath;
	std::filesystem::path m_AppAssemblyRuntimeFilepath;

	ScriptClass m_EntityClass;

	std::unordered_map<std::string, Ref<ScriptClass>> m_EntityClasses;
	std::unordered_map<UUID, Ref<ScriptInstance>> m_EntityInstances;
	std::unordered_map<UUID, ScriptFieldMap> m_EntityScriptFields;
	std::unordered_map<std::string, ScriptFieldMap> m_BaseEntityScriptFields;

	Scope<filewatch::FileWatch<std::string>> m_AppAssemblyWatcher;
	bool m_AssemblyReloadingPending = false;
	bool m_ShouldReloadAssembly = false;
	bool m_IsShuttingDown = false;

#if defined(WHP_DEBUG) && 0
	bool m_EnableDebugging = true;
#else
	bool m_EnableDebugging = false;
#endif // WHP_DEBUG

	// Runtime
	Scene* m_SceneContext = nullptr;
	AssetHandle m_RuntimeActiveSceneHandle = 0;
	RuntimeSceneTransitionRequest m_RuntimeSceneTransition;
};


namespace
{
	ScriptEngineData* s_ScriptEngineData = nullptr;

	std::string MonoStringToString(MonoString* monoString)
	{
		if (!monoString)
			return {};

		char* cString = mono_string_to_utf8(monoString);
		std::string result = cString ? cString : "";
		mono_free(cString);
		return result;
	}

	UUID EntityIdFromManagedObject(MonoObject* entityObject)
	{
		if (!entityObject)
			return {0};

		MonoClass* entityClass = mono_class_from_name(s_ScriptEngineData->m_CoreAssemblyImage, "Whip", "Entity");
		MonoClassField* idField = mono_class_get_field_from_name(entityClass, "ID");
		if (!idField)
			return {0};

		uint64_t id = 0;
		mono_field_get_value(entityObject, idField, &id);
		return {id};
	}

	MonoObject* CreateManagedEntityReference(UUID entityId)
	{
		if (entityId == 0)
			return nullptr;

		MonoObject* entityObject = s_ScriptEngineData->m_EntityClass.Instantiate();
		MonoMethod* constructor = s_ScriptEngineData->m_EntityClass.GetMethod(".ctor", 1);
		void* param = &entityId;
		s_ScriptEngineData->m_EntityClass.InvokeMethod(entityObject, constructor, &param);
		return entityObject;
	}

	AssetHandle AssetHandleFromManagedObject(MonoObject* assetHandleObject)
	{
		if (!assetHandleObject)
			return 0;

		MonoClass* assetHandleClass = mono_class_from_name(s_ScriptEngineData->m_CoreAssemblyImage, "Whip", "AssetHandle");
		if (!assetHandleClass)
			return 0;

		MonoClassField* idField = mono_class_get_field_from_name(assetHandleClass, "ID");
		if (!idField)
			return 0;

		uint64_t id = 0;
		mono_field_get_value(assetHandleObject, idField, &id);
		return {id};
	}

	MonoObject* CreateManagedSceneReference(AssetHandle handle)
	{
		MonoClass* sceneReferenceClass = mono_class_from_name(s_ScriptEngineData->m_CoreAssemblyImage, "Whip", "SceneReference");
		if (!sceneReferenceClass)
			return nullptr;

		MonoObject* sceneReference = mono_object_new(s_ScriptEngineData->m_AppDomain, sceneReferenceClass);
		if (!sceneReference)
			return nullptr;

		MonoMethod* constructor = mono_class_get_method_from_name(sceneReferenceClass, ".ctor", 1);
		if (!constructor)
		{
			mono_runtime_object_init(sceneReference);
			return sceneReference;
		}

		uint64_t rawHandle = handle;
		void* param = &rawHandle;
		MonoObject* exception = nullptr;
		mono_runtime_invoke(constructor, sceneReference, &param, &exception);
		if (exception)
		{
			WHP_CORE_ERROR("[Script Engine] Mono Exception while creating SceneReference.");
			return nullptr;
		}

		return sceneReference;
	}

	std::string NormalizeSceneLookupKey(std::string_view value)
	{
		std::string result(value);
		std::ranges::replace(result, '\\', '/');
		std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		constexpr const char* AssetsPrefix = "assets/";
		constexpr size_t AssetsPrefixLength = 7;
		if (result.starts_with(AssetsPrefix))
			result.erase(0, AssetsPrefixLength);

		return result;
	}

	bool SceneLookupKeyMatches(const std::filesystem::path& path, std::string_view query)
	{
		std::filesystem::path pathWithoutExtension = path;
		pathWithoutExtension.replace_extension();

		return NormalizeSceneLookupKey(path.generic_string()) == query ||
			NormalizeSceneLookupKey(pathWithoutExtension.generic_string()) == query ||
			NormalizeSceneLookupKey(path.filename().generic_string()) == query ||
			NormalizeSceneLookupKey(path.stem().generic_string()) == query;
	}
}

ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore)
	: m_ClassNamespace(classNamespace), m_ClassName(className)
{
	m_MonoClass = mono_class_from_name(isCore ? s_ScriptEngineData->m_CoreAssemblyImage : s_ScriptEngineData->m_AppAssemblyImage, classNamespace.c_str(), className.c_str());
}

MonoObject* ScriptClass::Instantiate() const
{
	if (!m_MonoClass)
	{
		WHP_CORE_WARN("[Script Engine] Cannot instantiate missing script class: {0}", GetFullName());
		return nullptr;
	}
	return ScriptEngine::InstantiateClass(m_MonoClass);
}

MonoMethod* ScriptClass::GetMethod(const std::string& name, int parameterCount) const
{
	if (!m_MonoClass)
		return nullptr;
	return mono_class_get_method_from_name(m_MonoClass, name.c_str(), parameterCount);
}

std::string ScriptClass::GetFullName() const
{
	return m_ClassNamespace.empty() ? m_ClassName : m_ClassNamespace + "." + m_ClassName;
}

const std::map<std::string, ScriptField>& ScriptClass::GetFields() const
{
	return m_Fields;
}

MonoObject* ScriptClass::InvokeMethod(MonoObject* instance, MonoMethod* method, void** params, std::string_view context)
{
	if (!method)
	{
		if (!context.empty())
			WHP_CORE_WARN("[Script Engine] Cannot invoke missing managed method while {0}", context);
		return nullptr;
	}

	MonoObject* exception = nullptr;
	auto* ptr = mono_runtime_invoke(method, instance, params, &exception);
	if (exception)
	{
		MonoString* exceptionMessage = mono_object_to_string(exception, nullptr);
		const std::string message = MonoStringToString(exceptionMessage);
		if (!context.empty())
			WHP_CORE_ERROR("[Script Engine] Mono Exception while {0}: {1}", context, message);
		else
			WHP_CORE_ERROR("[Script Engine] Mono Exception: {0}", message);
	}
	return ptr;
}

ScriptInstance::ScriptInstance(const Ref<ScriptClass>& scriptClass, Entity entityIn) : m_ScriptClass(scriptClass)
{
	m_EntityId = entityIn.GetUUID();
	m_EntityName = entityIn.GetName();
	m_Instance = scriptClass->Instantiate();
	m_Methods[EntityMethodType::Constructor] = s_ScriptEngineData->m_EntityClass.GetMethod(".ctor", 1);
	m_Methods[EntityMethodType::OnCreate] = scriptClass->GetMethod("OnCreate", 0);
	m_Methods[EntityMethodType::OnUpdate] = scriptClass->GetMethod("OnUpdate", 1);
	m_Methods[EntityMethodType::OnDestroy] = scriptClass->GetMethod("OnDestroy", 0);
	m_Methods[EntityMethodType::OnColliderEnter] = scriptClass->GetMethod("OnColliderEnter", 1);
	m_Methods[EntityMethodType::OnColliderExit] = scriptClass->GetMethod("OnColliderExit", 1);
	if (!m_Instance)
		return;

	// Call Entity Constructor
	{
		void* param = &m_EntityId;
		m_ScriptClass->InvokeMethod(m_Instance, m_Methods[EntityMethodType::Constructor], &param, MakeMethodContext("Entity.ctor"));
	}
}

void ScriptInstance::InvokeOnCreate()
{
	if (m_Instance && m_Methods[EntityMethodType::OnCreate])
		m_ScriptClass->InvokeMethod(m_Instance, m_Methods[EntityMethodType::OnCreate], nullptr, MakeMethodContext("OnCreate"));
}

void ScriptInstance::InvokeOnUpdate(float ts)
{
	if (m_Instance && m_Methods[EntityMethodType::OnUpdate])
	{
		void* param = &ts;
		m_ScriptClass->InvokeMethod(m_Instance, m_Methods[EntityMethodType::OnUpdate], &param, MakeMethodContext("OnUpdate"));
	}
}

void ScriptInstance::InvokeOnDestroy()
{
	if (m_Instance && m_Methods[EntityMethodType::OnDestroy])
		m_ScriptClass->InvokeMethod(m_Instance, m_Methods[EntityMethodType::OnDestroy], nullptr, MakeMethodContext("OnDestroy"));
}

void ScriptInstance::InvokeOnColliderEnter(std::string_view tag)
{
	if (m_Instance && m_Methods[EntityMethodType::OnColliderEnter])
	{
		const std::string tagString(tag);
		MonoString* monoString = mono_string_new(s_ScriptEngineData->m_AppDomain, tagString.c_str());
		void* param = monoString;
		m_ScriptClass->InvokeMethod(m_Instance, m_Methods[EntityMethodType::OnColliderEnter], &param, MakeMethodContext("OnColliderEnter"));
	}
}

void ScriptInstance::InvokeOnColliderExit(std::string_view tag)
{
	if (m_Instance && m_Methods[EntityMethodType::OnColliderExit])
	{
		const std::string tagString(tag);
		MonoString* monoString = mono_string_new(s_ScriptEngineData->m_AppDomain, tagString.c_str());
		void* param = monoString;
		m_ScriptClass->InvokeMethod(m_Instance, m_Methods[EntityMethodType::OnColliderExit], &param, MakeMethodContext("OnColliderExit"));
	}
}

void ScriptInstance::InvokeMethod(EntityMethodType methodType, const Payload& payload)
{
	if (!m_Instance || !m_Methods[methodType])
		return;

	void* param = nullptr;

	if (methodType == EntityMethodType::OnUpdate)
	{
		param = const_cast<float*>(&payload.Get<float>());
	}
	else if (methodType == EntityMethodType::OnColliderEnter || methodType == EntityMethodType::OnColliderExit)
	{
		const std::string tagString(payload.Get<std::string_view>());
		MonoString* monoString = mono_string_new(s_ScriptEngineData->m_AppDomain, tagString.c_str());
		param = monoString;
	}

	m_ScriptClass->InvokeMethod(m_Instance, m_Methods[methodType], param ? &param : nullptr, MakeMethodContext(frenum::to_string_view(methodType)));
}

Ref<ScriptClass> ScriptInstance::GetScriptClass()
{
	return m_ScriptClass;
}

std::string ScriptInstance::GetFieldString(const std::string& name) const
{
	if (!GetFieldValueInternal(name))
		return {};

	const char* value = s_FieldValueBuffer.As<const char>();
	return value ? std::string(value) : std::string();
}

void ScriptInstance::SetFieldString(const std::string& name, std::string_view value) const
{
	std::string valueCopy(value);
	if (!SetFieldValueInternal(name, valueCopy.c_str()))
		WHP_CORE_ERROR("[Script Engine] Failed to set string field value.");
}

UUID ScriptInstance::GetFieldEntity(const std::string& name) const
{
	if (!GetFieldValueInternal(name))
		return {0};

	return s_FieldValueBuffer.Load<UUID>();
}

void ScriptInstance::SetFieldEntity(const std::string& name, UUID value) const
{
	if (!SetFieldValueInternal(name, &value))
		WHP_CORE_ERROR("[Script Engine] Failed to set entity field value.");
}

MonoObject* ScriptInstance::GetManagedObject()
{
	return m_Instance;
}

const MonoObject* ScriptInstance::GetManagedObject() const
{
	return m_Instance;
}

bool ScriptInstance::GetFieldValueInternal(const std::string& name) const
{
	const auto& fields = m_ScriptClass->GetFields();
	auto it = fields.find(name);
	if (it == fields.end())
		return false;

	const ScriptField& field = it->second;

	if (field.m_Type == ScriptFieldType::String)
	{
		MonoString* string = nullptr;
		mono_field_get_value(m_Instance, field.m_ClassField, static_cast<void*>(&string));
		std::string value = MonoStringToString(string);
		s_FieldValueBuffer.Allocate(value.size() + 1);
		std::memcpy(s_FieldValueBuffer.m_Data, value.data(), value.size());
		s_FieldValueBuffer.m_Data[value.size()] = '\0';
		return true;
	}

	if (field.m_Type == ScriptFieldType::Entity)
	{
		MonoObject* entityObject = nullptr;
		mono_field_get_value(m_Instance, field.m_ClassField, static_cast<void*>(&entityObject));
		UUID entityId = EntityIdFromManagedObject(entityObject);
		s_FieldValueBuffer.Store(entityId);
		return true;
	}

	if (field.m_Type == ScriptFieldType::Scene)
	{
		MonoObject* sceneReference = nullptr;
		mono_field_get_value(m_Instance, field.m_ClassField, static_cast<void*>(&sceneReference));
		AssetHandle sceneHandle = AssetHandleFromManagedObject(sceneReference);
		s_FieldValueBuffer.Store<uint64_t>(sceneHandle);
		return true;
	}

	if (size_t fieldSize = static_cast<size_t>(field.m_TypeSize); fieldSize > s_FieldValueBuffer.m_Size)
		s_FieldValueBuffer.Allocate(field.m_TypeSize);

	mono_field_get_value(m_Instance, field.m_ClassField, s_FieldValueBuffer.m_Data);
	return true;
}

bool ScriptInstance::SetFieldValueInternal(const std::string& name, const void* value) const
{
	const auto& fields = m_ScriptClass->GetFields();
	const auto it = fields.find(name);
	if (it == fields.end())
		return false;

	const ScriptField& field = it->second;

	if (field.m_Type == ScriptFieldType::String)
	{
		const char* stringValue = static_cast<const char*>(value);
		MonoString* string = mono_string_new(s_ScriptEngineData->m_AppDomain, stringValue ? stringValue : "");
		mono_field_set_value(m_Instance, field.m_ClassField, string);
		return true;
	}

	if (field.m_Type == ScriptFieldType::Entity)
	{
		const UUID entityId = value ? *static_cast<const UUID*>(value) : UUID(0);
		MonoObject* entityObject = CreateManagedEntityReference(entityId);
		mono_field_set_value(m_Instance, field.m_ClassField, entityObject);
		return true;
	}

	if (field.m_Type == ScriptFieldType::Scene)
	{
		const AssetHandle sceneHandle = value ? AssetHandle(*static_cast<const uint64_t*>(value)) : AssetHandle(0);
		MonoObject* sceneReference = CreateManagedSceneReference(sceneHandle);
		mono_field_set_value(m_Instance, field.m_ClassField, sceneReference);
		return true;
	}

	mono_field_set_value(m_Instance, field.m_ClassField, const_cast<void*>(value));
	return true;
}

bool ScriptInstance::GetFieldArrayValueInternal(const std::string& name, size_t* size) const
{
	const auto& fields = m_ScriptClass->GetFields();
	auto it = fields.find(name);
	if (it == fields.end())
		return false;

	const ScriptField& field = it->second;
	if (!field.m_IsArray)
		return false;

	if (field.m_Type == ScriptFieldType::Scene)
	{
		if (size)
			*size = 0;
		WHP_CORE_WARN("[Script Engine] SceneReference arrays are not supported yet: {0}", name);
		return false;
	}

	MonoArray* array;
	mono_field_get_value(m_Instance, field.m_ClassField, static_cast<void*>(&array));
	if (!array)
		return false;

	uintptr_t length = mono_array_length(array);
	if (size != nullptr)
		*size = length;

	size_t requiredBufferSize = length * field.m_TypeSize;
	if (requiredBufferSize > s_FieldValueBuffer.m_Size)
		s_FieldValueBuffer.Allocate(requiredBufferSize);

	char* arrayData = mono_array_addr_with_size(array, field.m_TypeSize, 0);
	std::memcpy(s_FieldValueBuffer.m_Data, arrayData, requiredBufferSize);
	return true;
}

bool ScriptInstance::SetFieldArrayValueInternal(const std::string& name, const void* values, size_t size) const
{
	const auto& fields = m_ScriptClass->GetFields();
	auto it = fields.find(name);
	if (it == fields.end())
		return false;

	const ScriptField& field = it->second;
	if (!field.m_IsArray)
		return false;

	if (field.m_Type == ScriptFieldType::String || field.m_Type == ScriptFieldType::Entity || field.m_Type == ScriptFieldType::Scene || field.m_Type == ScriptFieldType::Logger)
	{
		WHP_CORE_WARN("[Script Engine] Script field array type is not assignable at runtime yet: {0}", name);
		return false;
	}

	MonoType* arrayType = mono_field_get_type(field.m_ClassField);
	MonoType* elementType = GetMonoTypeElementType(arrayType);
	MonoClass* elementClass = mono_class_from_mono_type(elementType);
	if (!elementClass)
		return false;

	MonoArray* array = mono_array_new(s_ScriptEngineData->m_AppDomain, elementClass, static_cast<uintptr_t>(size));
	if (!array)
		return false;

	if (values && size > 0)
	{
		char* arrayData = mono_array_addr_with_size(array, field.m_TypeSize, 0);
		std::memcpy(arrayData, values, size * field.m_TypeSize);
	}

	mono_field_set_value(m_Instance, field.m_ClassField, array);
	return true;
}

bool ScriptInstance::SetFieldArrayIndexValueInternal(const std::string& name, size_t index, const void* value) const
{
	const auto& fields = m_ScriptClass->GetFields();
	auto it = fields.find(name);
	if (it == fields.end())
		return false;

	const ScriptField& field = it->second;
	if (!field.m_IsArray)
		return false;

	MonoArray* array;
	mono_field_get_value(m_Instance, field.m_ClassField, static_cast<void*>(&array));
	if (!array)
		return false;

	char* addr = mono_array_addr_with_size(array, field.m_TypeSize, static_cast<uintptr_t>(index));
	std::memcpy(addr, value, field.m_TypeSize);
	return true;
}

std::string ScriptInstance::MakeMethodContext(std::string_view methodName) const
{
	return nps::formatter::format("{0}.{1} on entity '{2}' ({3})", m_ScriptClass->GetFullName(), methodName, m_EntityName, static_cast<uint64_t>(m_EntityId));
}

bool AssemblyManager::LoadAssembly(const std::filesystem::path& filepath)
{
	s_ScriptEngineData->m_AppDomain = mono_domain_create_appdomain(const_cast<char*>("WhipScriptRuntime"), nullptr);
	mono_domain_set(s_ScriptEngineData->m_AppDomain, true);

	s_ScriptEngineData->m_CoreAssemblyFilepath = filepath;
	s_ScriptEngineData->m_CoreAssembly = LoadMonoAssembly(filepath, s_ScriptEngineData->m_EnableDebugging);
	if (s_ScriptEngineData->m_CoreAssembly == nullptr)
		return false;
	s_ScriptEngineData->m_CoreAssemblyImage = mono_assembly_get_image(s_ScriptEngineData->m_CoreAssembly);
	return true;
}

bool AssemblyManager::LoadAppAssembly(const std::filesystem::path& filepath)
{
	if (filepath.extension().string() != ".dll")
	{
		WHP_CORE_WARN("[Script Engine] App assembly path is not a dll: {0}", filepath.string());
		return false;
	}

	s_ScriptEngineData->m_AppAssemblyFilepath = filepath;
	s_ScriptEngineData->m_AssemblyReloadingPending = false;

	if (!std::filesystem::exists(filepath))
	{
		WHP_CORE_WARN("[Script Engine] App assembly file does not exist yet: {0}", filepath.string());
		return false;
	}

	s_ScriptEngineData->m_AppAssemblyRuntimeFilepath = CopyAssemblyToRuntimeShadow(filepath);
	s_ScriptEngineData->m_AppAssembly = LoadMonoAssembly(s_ScriptEngineData->m_AppAssemblyRuntimeFilepath, s_ScriptEngineData->m_EnableDebugging);
	if (s_ScriptEngineData->m_AppAssembly == nullptr)
	{
		WHP_CORE_ERROR("[Script Engine] Failed to load app assembly bytes: {0}", s_ScriptEngineData->m_AppAssemblyRuntimeFilepath.string());
		return false;
	}

	s_ScriptEngineData->m_AppAssemblyImage = mono_assembly_get_image(s_ScriptEngineData->m_AppAssembly);
	WHP_CORE_INFO("[Script Engine] Loaded app assembly: {0}", filepath.string());
	if (s_ScriptEngineData->m_AppAssemblyRuntimeFilepath != filepath)
		WHP_CORE_INFO("[Script Engine] Runtime shadow copy: {0}", s_ScriptEngineData->m_AppAssemblyRuntimeFilepath.string());
	return true;
}

bool AssemblyManager::ReloadAssembly(bool resetAppAssemblyFilepath)
{
	if (!s_ScriptEngineData || s_ScriptEngineData->m_IsShuttingDown)
		return false;

	s_ScriptEngineData->m_AppAssemblyWatcher.reset();
	mono_domain_set(mono_get_root_domain(), false);

	if (s_ScriptEngineData->m_AppDomain)
	{
		mono_domain_unload(s_ScriptEngineData->m_AppDomain);
		s_ScriptEngineData->m_AppDomain = nullptr;
	}

	AssemblyManager::LoadAssembly(s_ScriptEngineData->m_CoreAssemblyFilepath);
	if (resetAppAssemblyFilepath)
		s_ScriptEngineData->m_AppAssemblyFilepath = Project::GetActiveAssetDirectory() / Project::GetActive()->GetConfig().m_ScriptModulePath;

	s_ScriptEngineData->m_AppAssembly = nullptr;
	s_ScriptEngineData->m_AppAssemblyImage = nullptr;
	s_ScriptEngineData->m_AppAssemblyRuntimeFilepath.clear();

	if (s_ScriptEngineData->m_AppAssemblyFilepath.empty() || s_ScriptEngineData->m_AppAssemblyFilepath == Project::GetActiveAssetDirectory())
	{
		WHP_CORE_INFO("[Script Engine] Project has no app assembly configured.");
		s_ScriptEngineData->m_EntityClasses.clear();
		ScriptGlue::RegisterComponents();
		s_ScriptEngineData->m_EntityClass = ScriptClass("Whip", "Entity", true);
		s_ScriptEngineData->m_AssemblyReloadingPending = false;
		return true;
	}

	WHP_CORE_INFO("[Script Engine] Reloading app assembly: {0}", s_ScriptEngineData->m_AppAssemblyFilepath.string());
	bool status = LoadAppAssembly(s_ScriptEngineData->m_AppAssemblyFilepath);
	if (!status)
	{
		WHP_CORE_WARN("[Script Engine] Reload finished without an app assembly.");
		s_ScriptEngineData->m_EntityClasses.clear();
	}
	else
	{
		AssemblyManager::LoadAssemblyClasses();
		WHP_CORE_INFO("[Script Engine] Reloaded {0} script class(es).", s_ScriptEngineData->m_EntityClasses.size());
	}

	ScriptGlue::RegisterComponents();

	s_ScriptEngineData->m_EntityClass = ScriptClass("Whip", "Entity", true);
	s_ScriptEngineData->m_AssemblyReloadingPending = false;
	return status;
}

MonoImage* AssemblyManager::GetCoreAssemblyImage()
{
	return s_ScriptEngineData->m_CoreAssemblyImage;
}

void AssemblyManager::LoadBaseScriptFields()
{
	for (auto& [className, entityClass] : s_ScriptEngineData->m_EntityClasses)
	{
		auto& entityFields = s_ScriptEngineData->m_BaseEntityScriptFields;
		MonoObject* instance = entityClass->Instantiate();
		for (auto& [fieldName, field] : entityClass->GetFields())
		{
			ScriptFieldInstance& fieldInstance = entityFields[className][fieldName];
			fieldInstance.m_Field = field;

			if (field.m_IsArray)
			{
				if (field.m_Type == ScriptFieldType::Scene)
				{
					WHP_CORE_WARN("[Script Engine] SceneReference arrays are not supported yet: {0}.{1}", className, fieldName);
					continue;
				}

				MonoArray* array;
				mono_field_get_value(instance, field.m_ClassField, static_cast<void*>(&array));
				if (!array)
				{
					WHP_CORE_WARN("{0}.{1} is null", className, fieldName);
					continue;
				}
				uintptr_t length = mono_array_length(array);
				RawBuffer& buf = fieldInstance.m_Buffer;
				buf.Allocate(length * field.m_TypeSize);
				char* arrayData = mono_array_addr_with_size(array, field.m_TypeSize, 0);
				std::memcpy(buf.m_Data, arrayData, buf.m_Size);
			}
			else
			{
				if (field.m_Type == ScriptFieldType::String)
				{
					MonoString* monoString = nullptr;
					mono_field_get_value(instance, field.m_ClassField, static_cast<void*>(&monoString));
					fieldInstance.SetStringValue(MonoStringToString(monoString));
				}
				else if (field.m_Type == ScriptFieldType::Entity)
				{
					MonoObject* entityObject = nullptr;
					mono_field_get_value(instance, field.m_ClassField, static_cast<void*>(&entityObject));
					fieldInstance.SetEntityValue(EntityIdFromManagedObject(entityObject));
				}
				else if (field.m_Type == ScriptFieldType::Scene)
				{
					MonoObject* sceneReference = nullptr;
					mono_field_get_value(instance, field.m_ClassField, static_cast<void*>(&sceneReference));
					fieldInstance.SetValue<uint64_t>(AssetHandleFromManagedObject(sceneReference));
				}
				else
				{
					RawBuffer& buf = fieldInstance.m_Buffer;
					buf.Allocate(MaxTypeSize);
					mono_field_get_value(instance, field.m_ClassField, buf.As<void>());
				}
			}
		}
	}
}

void AssemblyManager::LoadAssemblyClasses()
{
	s_ScriptEngineData->m_EntityClasses.clear();
	s_ScriptEngineData->m_BaseEntityScriptFields.clear();

	const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(s_ScriptEngineData->m_AppAssemblyImage, MONO_TABLE_TYPEDEF);
	int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);
	MonoClass* entityClass = mono_class_from_name(s_ScriptEngineData->m_CoreAssemblyImage, "Whip", "Entity");

	for (int32_t i = 0; i < numTypes; i++)
	{
		uint32_t cols[MONO_TYPEDEF_SIZE];
		mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

		const char* nameSpace = mono_metadata_string_heap(s_ScriptEngineData->m_AppAssemblyImage, cols[MONO_TYPEDEF_NAMESPACE]);
		const char* className = mono_metadata_string_heap(s_ScriptEngineData->m_AppAssemblyImage, cols[MONO_TYPEDEF_NAME]);
		std::string fullName;
		if (std::strlen(nameSpace) != 0)
			fullName = nps::formatter::format("{0}.{1}", nameSpace, className);
		else
			fullName = className;

		MonoClass* monoClass = mono_class_from_name(s_ScriptEngineData->m_AppAssemblyImage, nameSpace, className);

		if (monoClass == nullptr)
			continue; // probably class in class

		if (monoClass == entityClass)
			continue;

		bool isEntity = mono_class_is_subclass_of(monoClass, entityClass, false);
		if (!isEntity)
			continue;

		Ref<ScriptClass> scriptClass = MakeRef<ScriptClass>(nameSpace, className);
		s_ScriptEngineData->m_EntityClasses[fullName] = scriptClass;

		int fieldCount = mono_class_num_fields(monoClass);
		WHP_CORE_WARN("[Script Engine] {0} has {1} field(s)", className, fieldCount);
		void* fieldIterator = nullptr;
		while (MonoClassField* field = mono_class_get_fields(monoClass, &fieldIterator))
		{
			const char* fieldName = mono_field_get_name(field);
			uint32_t flags = mono_field_get_flags(field);
			if ((flags & WHP_MONO_FIELD_ATTRIBUTE_FIELD_ACCESS_MASK) == WHP_MONO_FIELD_ATTRIBUTE_PUBLIC)
			{
				MonoType* type = mono_field_get_type(field);
				auto [fieldType, isArray] = MonoTypeToScriptFieldType(type);
				int alignment = AlignOfType(fieldType);
				int typeSize;
				if (fieldType == ScriptFieldType::Scene)
				{
					typeSize = sizeof(uint64_t);
					alignment = alignof(uint64_t);
				}
				else if (isArray)
				{
					MonoType* elementType = GetMonoTypeElementType(type);
					typeSize = mono_type_size(elementType, &alignment);
				}
				else
				{
					typeSize = mono_type_size(type, &alignment);
				}
				scriptClass->m_Fields[fieldName] = {
					.m_Type = fieldType,
					.m_TypeSize = typeSize,
					.m_Name = fieldName,
					.m_ClassField = field,
					.m_IsArray = isArray
				};
			}
		}
	}
	LoadBaseScriptFields();
}

void ScriptEngine::Init()
{
	if (s_ScriptEngineData)
	{
		if (s_ScriptEngineData->m_IsShuttingDown)
			return;

		s_ScriptEngineData->m_EntityInstances.clear();
		s_ScriptEngineData->m_EntityScriptFields.clear();
		AssemblyManager::ReloadAssembly(true);
		return;
	}

	s_ScriptEngineData = new ScriptEngineData();

	ScriptInstance::s_FieldValueBuffer.Allocate(InitialBufferSize);

	InitMono();
	ScriptGlue::RegisterFunctions();

	bool status = AssemblyManager::LoadAssembly("Resources/Scripts/Whip-ScriptCore.dll");
	if (!status)
	{
		WHP_CORE_ERROR("[ScriptEngine] Could not load Whip-ScriptCore assembly.");
		return;
	}
	const std::filesystem::path& configuredScriptModulePath = Project::GetActive()->GetConfig().m_ScriptModulePath;
	if (configuredScriptModulePath.empty())
	{
		WHP_CORE_INFO("[Script Engine] Project has no app assembly configured.");
		ScriptGlue::RegisterComponents();
		s_ScriptEngineData->m_EntityClass = ScriptClass("Whip", "Entity", true);
		return;
	}

	auto scriptModulePath = Project::GetActiveAssetDirectory() / configuredScriptModulePath;
	status = AssemblyManager::LoadAppAssembly(scriptModulePath);
	if (!status)
	{
		WHP_CORE_WARN("[ScriptEngine] App assembly is not available yet: {0}", scriptModulePath.string());
		ScriptGlue::RegisterComponents();
		s_ScriptEngineData->m_EntityClass = ScriptClass("Whip", "Entity", true);
		return;
	}
	AssemblyManager::LoadAssemblyClasses();
	ScriptGlue::RegisterComponents();

	s_ScriptEngineData->m_EntityClass = ScriptClass("Whip", "Entity", true);
}

void ScriptEngine::Shutdown()
{
	if (s_ScriptEngineData)
	{
		s_ScriptEngineData->m_IsShuttingDown = true;
		s_ScriptEngineData->m_AppAssemblyWatcher.reset();
		s_ScriptEngineData->m_EntityInstances.clear();
		ShutdownMono();
		delete s_ScriptEngineData;
		s_ScriptEngineData = nullptr;
	}

	ScriptInstance::s_FieldValueBuffer.Release();
}

void ScriptEngine::SetFilewatcherState(bool run)
{
	WHP_UNUSED(run);
}

void ScriptEngine::OnRuntimeStart(Scene* sceneIn)
{
	s_ScriptEngineData->m_SceneContext = sceneIn;
	ScriptGlue::OnRuntimeStart();
}

void ScriptEngine::OnRuntimeStop()
{
	s_ScriptEngineData->m_SceneContext = nullptr;
	s_ScriptEngineData->m_RuntimeActiveSceneHandle = 0;
	s_ScriptEngineData->m_RuntimeSceneTransition = {};
	s_ScriptEngineData->m_EntityInstances.clear();
	ScriptGlue::OnRuntimeStop();
}

void ScriptEngine::InvokeAllOnCreateMethods()
{
	for (auto& instance : s_ScriptEngineData->m_EntityInstances)
		instance.second->InvokeOnCreate();
}

void ScriptEngine::InvokeAllOnDestroyMethods()
{
	if (!s_ScriptEngineData)
		return;

	std::vector<Ref<ScriptInstance>> instances;
	instances.reserve(s_ScriptEngineData->m_EntityInstances.size());
	for (auto& [entityId, instance] : s_ScriptEngineData->m_EntityInstances)
		instances.push_back(instance);

	for (const Ref<ScriptInstance>& instance : instances)
	{
		if (instance)
			instance->InvokeOnDestroy();
	}
}

bool ScriptEngine::EntityClassExists(const std::string& fullClassName)
{
	if (!s_ScriptEngineData)
		return false;
	return s_ScriptEngineData->m_EntityClasses.contains(fullClassName);
}

void ScriptEngine::InvokeEntityMethod(EntityMethodType methodType, const Entity& entity, const Payload& payload)
{
	const UUID uuid = entity.GetUUID();

	if (methodType == EntityMethodType::OnCreate)
	{
		const auto& sc = entity.GetComponent<ScriptComponent>();
		if (!EntityClassExists(sc.m_ClassName))
			return;

		Ref<ScriptInstance> instance = MakeRef<ScriptInstance>(s_ScriptEngineData->m_EntityClasses[sc.m_ClassName], entity);
		s_ScriptEngineData->m_EntityInstances[uuid] = instance;

		auto fieldMapIt = s_ScriptEngineData->m_EntityScriptFields.find(uuid);
		if (fieldMapIt != s_ScriptEngineData->m_EntityScriptFields.end())
		{
			for (const auto& [name, fieldInstance] : fieldMapIt->second)
			{
				if (fieldInstance.m_Field.m_IsArray && fieldInstance.m_Field.m_TypeSize > 0)
				{
					bool result = instance->SetFieldArrayValueInternal(name, fieldInstance.m_Buffer.As<void>(), fieldInstance.m_Buffer.m_Size / fieldInstance.m_Field.m_TypeSize);
					if (!result)
						WHP_CORE_ERROR("Failed to set field array value.");
				}
				else
				{
					bool result = instance->SetFieldValueInternal(name, fieldInstance.m_Buffer.As<void>());
					if (!result)
						WHP_CORE_ERROR("Failed to set field value.");
				}
			}
		}

		Application::Get().SubmitToNextTick([instance]() { instance->InvokeMethod(EntityMethodType::OnCreate, Payload::Null()); });
		return;
	}

	auto instanceIt = s_ScriptEngineData->m_EntityInstances.find(uuid);
	if (instanceIt == s_ScriptEngineData->m_EntityInstances.end())
	{
		WHP_CORE_ERROR("[Script Engine] Could not find Script Instance for entity {0}", static_cast<uint64_t>(uuid));
		return;
	}

	instanceIt->second->InvokeMethod(methodType, payload);

	if (methodType == EntityMethodType::OnDestroy)
		s_ScriptEngineData->m_EntityInstances.erase(instanceIt);
}

Scene* ScriptEngine::GetSceneContext()
{
	return s_ScriptEngineData->m_SceneContext;
}

void ScriptEngine::SetRuntimeActiveSceneHandle(AssetHandle handle)
{
	if (!s_ScriptEngineData)
		return;
	s_ScriptEngineData->m_RuntimeActiveSceneHandle = handle;
}

AssetHandle ScriptEngine::GetRuntimeActiveSceneHandle()
{
	if (!s_ScriptEngineData)
		return 0;
	return s_ScriptEngineData->m_RuntimeActiveSceneHandle;
}

AssetHandle ScriptEngine::FindRuntimeSceneHandle(std::string_view sceneName)
{
	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject || !activeProject->GetEditorAssetManager())
		return 0;

	const std::string query = NormalizeSceneLookupKey(sceneName);
	if (query.empty())
		return 0;

	AssetHandle match = 0;
	const AssetRegistry& registry = activeProject->GetEditorAssetManager()->GetAssetRegistry();
	registry.ForeachChecked(AssetType::Scene, [&](const AssetRegistry::ValueType& value) -> uint8_t
		{
			if (!SceneLookupKeyMatches(value.second.m_Filepath, query))
				return AssetRegistry::LoopContinue;

			match = value.first;
			return AssetRegistry::LoopStop;
		});

	return match;
}

bool ScriptEngine::RequestRuntimeSceneLoad(AssetHandle handle)
{
	if (!s_ScriptEngineData || handle == 0)
		return false;

	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject || !activeProject->GetRuntimeAssetManager() ||
		!activeProject->GetRuntimeAssetManager()->IsAssetHandleValid(handle) ||
		activeProject->GetRuntimeAssetManager()->GetAssetType(handle) != AssetType::Scene)
	{
		WHP_CORE_WARN("[Scene Manager] Cannot load scene. Invalid scene handle: {0}", static_cast<uint64_t>(handle));
		return false;
	}

	s_ScriptEngineData->m_RuntimeSceneTransition = {
		.m_Type = RuntimeSceneTransitionType::Load,
		.m_SceneHandle = handle
	};
	return true;
}

bool ScriptEngine::RequestRuntimeSceneLoad(std::string_view sceneName)
{
	const AssetHandle handle = FindRuntimeSceneHandle(sceneName);
	if (handle == 0)
	{
		WHP_CORE_WARN("[Scene Manager] Cannot load scene. Scene name was not found: {0}", std::string(sceneName));
		return false;
	}

	return RequestRuntimeSceneLoad(handle);
}

bool ScriptEngine::RequestRuntimeStartSceneLoad()
{
	Ref<Project> activeProject = Project::GetActive();
	if (!activeProject)
		return false;
	return RequestRuntimeSceneLoad(activeProject->GetConfig().m_StartScene);
}

bool ScriptEngine::RequestRuntimeSceneReload()
{
	if (!s_ScriptEngineData || s_ScriptEngineData->m_RuntimeActiveSceneHandle == 0)
		return false;

	s_ScriptEngineData->m_RuntimeSceneTransition = {
		.m_Type = RuntimeSceneTransitionType::Reload,
		.m_SceneHandle = s_ScriptEngineData->m_RuntimeActiveSceneHandle
	};
	return true;
}

bool ScriptEngine::RequestRuntimeSceneUnload()
{
	if (!s_ScriptEngineData)
		return false;

	s_ScriptEngineData->m_RuntimeSceneTransition = {
		.m_Type = RuntimeSceneTransitionType::Unload,
		.m_SceneHandle = 0
	};
	return true;
}

RuntimeSceneTransitionRequest ScriptEngine::ConsumeRuntimeSceneTransitionRequest()
{
	if (!s_ScriptEngineData)
		return {};

	RuntimeSceneTransitionRequest request = s_ScriptEngineData->m_RuntimeSceneTransition;
	s_ScriptEngineData->m_RuntimeSceneTransition = {};
	return request;
}

void ScriptEngine::ClearRuntimeSceneTransitionRequest()
{
	if (!s_ScriptEngineData)
		return;
	s_ScriptEngineData->m_RuntimeSceneTransition = {};
}

Ref<ScriptInstance> ScriptEngine::GetEntityScriptInstance(UUID entityId)
{
	auto it = s_ScriptEngineData->m_EntityInstances.find(entityId);
	if (it == s_ScriptEngineData->m_EntityInstances.end())
		return nullptr;
	return it->second;
}

Ref<ScriptClass> ScriptEngine::GetEntityClass(const std::string& className)
{
	if (!s_ScriptEngineData)
		return nullptr;
	if (!s_ScriptEngineData->m_EntityClasses.contains(className))
		return nullptr;
	return s_ScriptEngineData->m_EntityClasses.at(className);
}

std::unordered_map<std::string, Ref<ScriptClass>> ScriptEngine::GetEntityClasses()
{
	if (!s_ScriptEngineData)
		return {};
	return s_ScriptEngineData->m_EntityClasses;
}

ScriptFieldMap& ScriptEngine::GetScriptFieldMap(Entity entityIn)
{
	WHP_CORE_ASSERT(entityIn, "[Script Engine] Entity does not exist!");
	UUID entityId = entityIn.GetUUID();
	return s_ScriptEngineData->m_EntityScriptFields[entityId];
}

ScriptFieldMap& ScriptEngine::GetBaseScriptFieldMap(const std::string& className)
{
	WHP_CORE_ASSERT(s_ScriptEngineData->m_BaseEntityScriptFields.contains(className), "[Script Engine] Class not found!");
	return s_ScriptEngineData->m_BaseEntityScriptFields[className];
}

void ScriptEngine::CopyScriptFieldMap(Entity sourceEntity, Entity destinationEntity)
{
	if (!s_ScriptEngineData || !sourceEntity || !destinationEntity)
		return;

	UUID sourceId = sourceEntity.GetUUID();
	UUID destinationId = destinationEntity.GetUUID();
	if (sourceId == destinationId)
		return;

	auto sourceFieldsIt = s_ScriptEngineData->m_EntityScriptFields.find(sourceId);
	if (sourceFieldsIt == s_ScriptEngineData->m_EntityScriptFields.end())
		return;

	auto& destinationFields = s_ScriptEngineData->m_EntityScriptFields[destinationId];
	destinationFields.clear();

	for (const auto& [fieldName, sourceField] : sourceFieldsIt->second)
	{
		ScriptFieldInstance& destinationField = destinationFields[fieldName];
		destinationField.m_Field = sourceField.m_Field;
		if (sourceField.m_Buffer.m_Data && sourceField.m_Buffer.m_Size > 0)
			destinationField.m_Buffer = RawBuffer::Copy(sourceField.m_Buffer);
	}
}

MonoObject* ScriptEngine::GetManagedInstance(UUID uuid)
{
	WHP_CORE_ASSERT(s_ScriptEngineData->m_EntityInstances.contains(uuid), "[Script Engine] Entity Instance not found!");
	return s_ScriptEngineData->m_EntityInstances.at(uuid)->GetManagedObject();
}

void ScriptEngine::InitMono()
{
	mono_set_assemblies_path("mono/lib");

	if (s_ScriptEngineData->m_EnableDebugging)
	{
		const char* argv[2] = {
			"--debugger-agent=transport=dt_socket,address=127.0.0.1:2550,server=y,suspend=n,loglevel=3,logfile=MonoDebugger.log",
			"--soft-breakpoints"
		};

		mono_jit_parse_options(2, const_cast<char**>(argv));
		mono_debug_init(MONO_DEBUG_FORMAT_MONO);
	}

	MonoDomain* rootDomain = mono_jit_init("WhipJITRuntime");
	WHP_CORE_ASSERT(rootDomain);
	// Store the root domain pointer
	s_ScriptEngineData->m_RootDomain = rootDomain;

	if (s_ScriptEngineData->m_EnableDebugging)
		mono_debug_domain_create(s_ScriptEngineData->m_RootDomain);

	mono_thread_set_main(mono_thread_current());
}

void ScriptEngine::ShutdownMono()
{
	if (MonoDomain* rootDomain = mono_get_root_domain())
		mono_domain_set(rootDomain, false);

	if (s_ScriptEngineData->m_AppDomain)
	{
		mono_domain_unload(s_ScriptEngineData->m_AppDomain);
		s_ScriptEngineData->m_AppDomain = nullptr;
	}

	if (s_ScriptEngineData->m_RootDomain)
	{
		mono_jit_cleanup(s_ScriptEngineData->m_RootDomain);
		s_ScriptEngineData->m_RootDomain = nullptr;
	}
}

MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass)
{
	MonoObject* instance = mono_object_new(s_ScriptEngineData->m_AppDomain, monoClass);
	mono_runtime_object_init(instance);
	return instance;
}

namespace Utils
{
	MonoString* CreateString(const char* text)
	{
		return mono_string_new(s_ScriptEngineData->m_AppDomain, text);
	}

	MonoString* CreateString(const wchar_t* wideString)
	{
		return mono_string_new_utf16(s_ScriptEngineData->m_AppDomain, wideString, static_cast<int32_t>(wcslen(wideString)));
	}
}

_WHIP_END
