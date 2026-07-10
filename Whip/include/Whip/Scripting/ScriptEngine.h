#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>
#include <Whip/Helper/Buffer.h>
#include <Whip/Scene/Entity.h>
#include <Whip/Scene/Scene.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <map>
#include <unordered_map>

#include <frenum.h>

#include "Whip/Core/Payload.h"

// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C"
{
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoMethod MonoMethod;
	typedef struct _MonoAssembly MonoAssembly;
	typedef struct _MonoImage MonoImage;
	typedef struct _MonoClassField MonoClassField;
	typedef struct _MonoString MonoString;
	typedef struct _MonoDomain MonoDomain;
}

// NOLINTEND(bugprone-reserved-identifier)

_WHIP_START

enum class ScriptFieldType : uint8_t
{
	None,
	String,
	Float, Double,
	Bool,
	Char, SByte, Short, Int, Long, Byte,
	UShort, UInt, ULong,
	KeyCode, MouseCode,
	Vector2, Vector3, Vector4,
	Entity,
	Scene,
	Logger
};

MakeFrenumInNamespace(whip, ScriptFieldType, None, String, Float, Double, Bool, Char, SByte, Short, Int, Long, Byte, UShort, UInt, ULong, KeyCode, MouseCode, Vector2, Vector3, Vector4, Entity, Scene, Logger)

enum class RuntimeSceneTransitionType : uint8_t
{
	None = 0,
	Load,
	Unload,
	Reload
};

struct RuntimeSceneTransitionRequest
{
	RuntimeSceneTransitionType m_Type = RuntimeSceneTransitionType::None;
	AssetHandle m_SceneHandle = 0;
};

FrenumClassInNamespace(whip, EntityMethodType, uint8_t,
	Constructor,
	OnCreate,
	OnUpdate,
	OnDestroy,
	OnColliderEnter,
	OnColliderExit,
	OnAnimationEvent,
	OnUIClick,
	OnUIToggle,
	OnUISlider,
	OnUIInputChanged,
	OnUIInputSubmit
)

struct ScriptField
{
	ScriptFieldType m_Type = ScriptFieldType::None;
	int m_TypeSize = 0;
	std::string m_Name;

	MonoClassField* m_ClassField = nullptr;
	bool m_IsArray = false;
};

struct ScriptFieldInstance // NOLINT(cppcoreguidelines-special-member-functions)
{
	ScriptField m_Field;

	ScriptFieldInstance() = default;

	~ScriptFieldInstance()
	{
		m_Buffer.Release();
	}

	template<typename T>
	T GetValue()
	{
		return m_Buffer.Load<T>();
	}

	template<typename T>
	bool CanGetValue() const
	{
		return m_Buffer.CanCastTo<T>();
	}

	template<typename T>
	void SetValue(T value)
	{
		m_Buffer.Store<T>(value);
	}

	std::string GetStringValue() const
	{
		if (!m_Buffer.m_Data || m_Buffer.m_Size == 0)
			return {};

		const char* data = m_Buffer.As<const char>();
		size_t length = static_cast<size_t>(m_Buffer.m_Size);
		if (data[length - 1] == '\0')
			--length;

		return std::string{ data, length };
	}

	void SetStringValue(std::string_view value)
	{
		m_Buffer.Allocate(value.size() + 1);
		std::memcpy(m_Buffer.m_Data, value.data(), value.size());
		m_Buffer.m_Data[value.size()] = '\0';
	}

	UUID GetEntityValue()
	{
		if (!m_Buffer.CanCastTo<UUID>())
			return {0};

		return m_Buffer.Load<UUID>();
	}

	void SetEntityValue(UUID value)
	{
		SetValue(value);
	}

	template <typename T>
	T* GetValueArray()
	{
		return m_Buffer.As<T>();
	}

	template <typename T>
	size_t GetArraySize() const
	{
		return m_Buffer.m_Size / sizeof(T);
	}

	// Todo: needs an update or make a new set array index method
	template <typename T>
	void SetValueArray(const T* array, size_t size)
	{
		const size_t bufferSize = size * sizeof(T);
		if (bufferSize == 0)
		{
			m_Buffer.Release();
			return;
		}

		WHP_CORE_ASSERT(array, "Cannot set script field array from null data!");
		if (m_Buffer.m_Data == reinterpret_cast<const uint8_t*>(array) && m_Buffer.m_Size == bufferSize)
			return;

		m_Buffer.Allocate(bufferSize);
		std::memcpy(m_Buffer.m_Data, array, bufferSize);
	}
private:
	RawBuffer m_Buffer;

	friend class ScriptEngine;
	friend class AssemblyManager;
	friend class ScriptInstance;
};

using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldInstance>;

class ScriptClass
{
public:
	ScriptClass() = default;
	ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore = false);

	MonoObject* Instantiate() const;
	MonoMethod* GetMethod(const std::string& name, int parameterCount) const;
	std::string GetFullName() const;
	const std::map<std::string, ScriptField>& GetFields() const;

	static MonoObject* InvokeMethod(MonoObject* instance, MonoMethod* method, void** params = nullptr, std::string_view context = {});
private:
	std::string m_ClassNamespace;
	std::string m_ClassName;
	std::map<std::string, ScriptField> m_Fields;

	MonoClass* m_MonoClass = nullptr;

	friend class ScriptEngine;
	friend class AssemblyManager;
};

class ScriptInstance
{
public:
	ScriptInstance(const Ref<ScriptClass>& scriptClass, Entity entityIn);

	void InvokeOnCreate();
	void InvokeOnUpdate(float ts);
	void InvokeOnDestroy();
	void InvokeOnColliderEnter(std::string_view tag);
	void InvokeOnColliderExit(std::string_view tag);
	void InvokeOnAnimationEvent(std::string_view eventName);
	void InvokeOnUIClick();
	void InvokeOnUIToggle(bool value);
	void InvokeOnUISlider(float value);
	void InvokeOnUIInputChanged(std::string_view value);
	void InvokeOnUIInputSubmit(std::string_view value);

	void InvokeMethod(EntityMethodType methodType, const Payload& payload);

	Ref<ScriptClass> GetScriptClass();

	template<class T>
	T GetFieldValue(const std::string& name)
	{
		bool success = GetFieldValueInternal(name);
		if (!success)
			return T();

		return s_FieldValueBuffer.Load<T>();
	}

	template <class T>
	T* GetFieldArray(const std::string& name, size_t* size)
	{
		bool success = GetFieldArrayValueInternal(name, size);
		if (!success)
			return nullptr;

		return s_FieldValueBuffer.As<T>();
	}

	template<class T>
	void SetFieldValue(const std::string& name, T value)
	{
		//static_assert(sizeof(T) <= 16, "Type too large!");
		SetFieldValueInternal(name, &value);
	}

	std::string GetFieldString(const std::string& name) const;
	void SetFieldString(const std::string& name, std::string_view value) const;

	UUID GetFieldEntity(const std::string& name) const;
	void SetFieldEntity(const std::string& name, UUID value) const;

	template <class T>
	void SetFieldArrayIndex(const std::string& name, size_t index, T value)
	{
		SetFieldArrayIndexValueInternal(name, index, &value);
	}

	MonoObject* GetManagedObject();
	const MonoObject* GetManagedObject() const;

private:
	bool GetFieldValueInternal(const std::string& name) const; // loads value to s_FieldValueBuffer
	bool SetFieldValueInternal(const std::string& name, const void* value) const;
	bool GetFieldArrayValueInternal(const std::string& name, size_t* size) const; // loads value to s_FieldValueBuffer
	bool SetFieldArrayValueInternal(const std::string& name, const void* values, size_t size) const;
	bool SetFieldArrayIndexValueInternal(const std::string& name, size_t index, const void* value) const;
	std::string MakeMethodContext(std::string_view methodName) const;
private:
	Ref<ScriptClass> m_ScriptClass;
	MonoObject* m_Instance = nullptr;

	std::unordered_map<EntityMethodType, MonoMethod*> m_Methods;

	UUID m_EntityId = 0;
	std::string m_EntityName;

	inline static RawBuffer s_FieldValueBuffer;

	friend class ScriptEngine;
};

class AssemblyManager
{
public:
	static bool LoadAssembly(const std::filesystem::path& filepath);
	static bool LoadAppAssembly(const std::filesystem::path& filepath);
	static bool ReloadAssembly(bool resetAppAssemblyFilepath = false);

	static MonoImage* GetCoreAssemblyImage();

private:
	static void LoadBaseScriptFields();
	static void LoadAssemblyClasses();

	friend class ScriptEngine;
	friend struct ScriptFieldInstance;
};

class ScriptEngine
{
public:
	static void Init();
	static void Shutdown();

	static void SetFilewatcherState(bool run = true);

	static void OnRuntimeStart(Scene* sceneIn);
	static void OnRuntimeStop();

	static void InvokeAllOnCreateMethods();
	static void InvokeAllOnDestroyMethods();

	static bool EntityClassExists(const std::string& fullClassName);
	static void InvokeEntityMethod(EntityMethodType methodType, const Entity& entity, const Payload& payload = Payload::Null());

	static Scene* GetSceneContext();
	static void SetRuntimeActiveSceneHandle(AssetHandle handle);
	static AssetHandle GetRuntimeActiveSceneHandle();
	static AssetHandle FindRuntimeSceneHandle(std::string_view sceneName);
	static bool RequestRuntimeSceneLoad(AssetHandle handle);
	static bool RequestRuntimeSceneLoad(std::string_view sceneName);
	static bool RequestRuntimeStartSceneLoad();
	static bool RequestRuntimeSceneReload();
	static bool RequestRuntimeSceneUnload();
	static RuntimeSceneTransitionRequest ConsumeRuntimeSceneTransitionRequest();
	static void ClearRuntimeSceneTransitionRequest();
	static Ref<ScriptInstance> GetEntityScriptInstance(UUID entityId);
	static Ref<ScriptClass> GetEntityClass(const std::string& className);
	static const std::unordered_map<std::string, Ref<ScriptClass>>& GetEntityClasses();
	static ScriptFieldMap& GetScriptFieldMap(Entity entityIn);
	static ScriptFieldMap& GetBaseScriptFieldMap(const std::string& className);
	static void CopyScriptFieldMap(Entity sourceEntity, Entity destinationEntity);
	static bool IsDebuggerEnabled();
	static std::string GetDebuggerHost();
	static int GetDebuggerPort();

	static MonoObject* GetManagedInstance(UUID uuid);
private:
	static void InitMono();
	static void ShutdownMono();

	static MonoObject* InstantiateClass(MonoClass* monoClass);

	friend class ScriptClass;
	friend class ScriptGlue;
};

namespace Utils
{
	MonoString* CreateString(const char* text);
	MonoString* CreateString(const wchar_t* wideString);
}

_WHIP_END
