#include "whippch.h"
#include "script_engine.h"

#include "script_glue.h"

#include <Whip/Core/buffer.h>
#include <Whip/Core/filesystem.h>
#include <Whip/Core/Application.h>
#include <Whip/Project/project.h>

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/object.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/threads.h>

#include <FileWatch.h>
#include <nps_formatter.h>


#ifndef WHP_MONO_FIELD_ATTRIBUTE_FIELD_ACCESS_MASK
#define WHP_MONO_FIELD_ATTRIBUTE_FIELD_ACCESS_MASK 0x0007u
#endif

#ifndef WHP_MONO_FIELD_ATTRIBUTE_PUBLIC
#define WHP_MONO_FIELD_ATTRIBUTE_PUBLIC 0x0006u
#endif


_WHIP_START

static std::unordered_map<std::string, script_field_type> s_script_field_type =
{
	{ "System.String", script_field_type::String },
	{ "System.Single", script_field_type::Float },
	{ "System.Double", script_field_type::Double },
	{ "System.Boolean", script_field_type::Bool },
	{ "System.Char", script_field_type::Char },
	{ "System.Int16", script_field_type::Short },
	{ "System.Int32", script_field_type::Int },
	{ "System.Int64", script_field_type::Long },
	{ "System.SByte", script_field_type::SByte },
	{ "System.Byte", script_field_type::Byte },
	{ "System.UInt16", script_field_type::UShort },
	{ "System.UInt32", script_field_type::UInt },
	{ "System.UInt64", script_field_type::ULong },

	{ "Whip.KeyCode", script_field_type::KeyCode },
	{ "Whip.MouseCode", script_field_type::MouseCode },

	{ "Whip.Vector2", script_field_type::Vector2 },
	{ "Whip.Vector3", script_field_type::Vector3 },
	{ "Whip.Vector4", script_field_type::Vector4 },

	{ "Whip.Entity", script_field_type::Entity },
	{ "Whip.Logger", script_field_type::Logger }
};

static constexpr size_t max_type_size = 16; // Whip.Vector4
static constexpr size_t initial_buffer_size = 1024; // 1kb

namespace utils
{
	static MonoAssembly* load_mono_assembly(const std::filesystem::path& assembly_path, bool loadPDB = false)
	{
		scoped_buffer file_data = filesystem::read_file_binary(assembly_path);

		MonoImageOpenStatus status;
		MonoImage* image = mono_image_open_from_data_full(file_data.as<char>(), (uint32_t)file_data.size(), 1, &status, 0);

		if (status != MONO_IMAGE_OK)
		{
			const char* errorMessage = mono_image_strerror(status);
			return nullptr;
		}

		if (loadPDB)
		{
			std::filesystem::path pdb_path = assembly_path;
			pdb_path.replace_extension(".pdb");

			if (std::filesystem::exists(pdb_path))
			{
				scoped_buffer pdb_file_data = filesystem::read_file_binary(pdb_path);
				mono_debug_open_image_from_memory(image, pdb_file_data.as<const mono_byte>(), (int)pdb_file_data.size());
				WHP_CORE_INFO("[Script Engine] Loaded PDB {0}", pdb_path.string());
			}
		}

		std::string path_string = assembly_path.string();
		MonoAssembly* assembly = mono_assembly_load_from_full(image, path_string.c_str(), &status, 0);
		mono_image_close(image);

		return assembly;
	}

	// development only -> todo: remove maybe
	static void print_assembly_types(MonoAssembly* assembly)
	{
		MonoImage* image = mono_assembly_get_image(assembly);
		const MonoTableInfo* type_definitions_table = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
		int32_t num_types = mono_table_info_get_rows(type_definitions_table);

		for (int32_t i = 0; i < num_types; i++)
		{
			uint32_t cols[MONO_TYPEDEF_SIZE];
			mono_metadata_decode_row(type_definitions_table, i, cols, MONO_TYPEDEF_SIZE);

			const char* name_space = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
			const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

			WHP_CORE_TRACE("{0}.{0}", name_space, name);
		}
	}

	static std::pair<script_field_type, bool> mono_type_to_script_field_type(MonoType* monoType)
	{
		std::string type_name = mono_type_get_name(monoType);

		bool is_array = false;
		if (type_name.size() > 2 && type_name.back() == ']')
		{
			is_array = true;
			std::string_view view(type_name);
			view.remove_suffix(2);
			type_name = view;
		}

		auto it = s_script_field_type.find(type_name);
		if (it == s_script_field_type.end())
		{
			WHP_CORE_ERROR("[Script Engine] Unknown type: {0}", type_name);
			return { script_field_type::None, false };
		}
		return { it->second, is_array };
	}

	static int align_of_type(script_field_type type)
	{
		switch (type)
		{
		case whip::script_field_type::None:    return 0;

		case whip::script_field_type::String:  return alignof(void*);   // referans tip -> pointer
		case whip::script_field_type::Entity:  return alignof(void*);   // referans tip
		case whip::script_field_type::Logger:  return alignof(void*);   // referans tip

		case whip::script_field_type::Float:   return alignof(float);
		case whip::script_field_type::Double:  return alignof(double);
		case whip::script_field_type::Bool:    return alignof(bool);
		case whip::script_field_type::Char:    return alignof(char);
		case whip::script_field_type::SByte:   return alignof(int8_t);
		case whip::script_field_type::Short:   return alignof(int16_t);
		case whip::script_field_type::Int:     return alignof(int32_t);
		case whip::script_field_type::Long:    return alignof(int64_t);
		case whip::script_field_type::Byte:    return alignof(uint8_t);
		case whip::script_field_type::UShort:  return alignof(uint16_t);
		case whip::script_field_type::UInt:    return alignof(uint32_t);
		case whip::script_field_type::ULong:   return alignof(uint64_t);

		case whip::script_field_type::KeyCode:   return alignof(key_code);
		case whip::script_field_type::MouseCode: return alignof(mouse_code);

		case whip::script_field_type::Vector2: return alignof(glm::vec2);
		case whip::script_field_type::Vector3: return alignof(glm::vec3);
		case whip::script_field_type::Vector4: return alignof(glm::vec4);

		default:
			return 0;
		}
	}

	static MonoType* get_mono_type_element_type(MonoType* type)
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

struct script_engine_data
{
	MonoDomain* root_domain = nullptr;
	MonoDomain* app_domain = nullptr;

	MonoAssembly* core_assembly = nullptr;
	MonoImage* core_assembly_image = nullptr;

	MonoAssembly* app_assembly = nullptr;
	MonoImage* app_assembly_image = nullptr;

	std::filesystem::path core_assembly_filepath;
	std::filesystem::path app_assembly_filepath;

	script_class entity_class;

	std::unordered_map<std::string, ref<script_class>> entity_classes;
	std::unordered_map<UUID, ref<script_instance>> entity_instances;
	std::unordered_map<UUID, script_field_map> entity_script_fields;
	std::unordered_map<std::string, script_field_map> base_entity_script_fields;

	scope<filewatch::FileWatch<std::string>> app_assembly_watcher;
	bool assembly_reloading_pending = false;
	bool should_reload_assembly = false;
	bool is_shutting_down = false;

#if defined(WHP_DEBUG) && 0
	bool enable_debugging = true;
#else
	bool enable_debugging = false;
#endif // WHP_DEBUG


	// Runtime
	scene* scene_context = nullptr;
};

static script_engine_data* s_script_engine_data = nullptr;

namespace utils
{
	static std::string mono_string_to_string(MonoString* string)
	{
		if (!string)
			return {};

		char* cstr = mono_string_to_utf8(string);
		std::string result = cstr ? cstr : "";
		mono_free(cstr);
		return result;
	}

	static UUID entity_id_from_managed_object(MonoObject* entity)
	{
		if (!entity)
			return UUID(0);

		MonoClass* entity_class = mono_class_from_name(s_script_engine_data->core_assembly_image, "Whip", "Entity");
		MonoClassField* id_field = mono_class_get_field_from_name(entity_class, "ID");
		if (!id_field)
			return UUID(0);

		uint64_t id = 0;
		mono_field_get_value(entity, id_field, &id);
		return UUID(id);
	}

	static MonoObject* create_managed_entity_reference(UUID entity_id)
	{
		if (entity_id == 0)
			return nullptr;

		MonoObject* entity = s_script_engine_data->entity_class.instantiate();
		MonoMethod* constructor = s_script_engine_data->entity_class.get_method(".ctor", 1);
		void* param = &entity_id;
		s_script_engine_data->entity_class.invoke_method(entity, constructor, &param);
		return entity;
	}

	static void on_app_assembly_file_system_event_1(const std::string& path, const filewatch::Event change_type)
	{
		if (!s_script_engine_data || s_script_engine_data->is_shutting_down)
			return;

		if ((!s_script_engine_data->assembly_reloading_pending && change_type == filewatch::Event::modified) || s_script_engine_data->should_reload_assembly)
		{
			s_script_engine_data->assembly_reloading_pending = true;

			application::get().submit_to_main_thread([]()
				{
					if (!s_script_engine_data || s_script_engine_data->is_shutting_down)
						return;

					s_script_engine_data->app_assembly_watcher.reset();
					assembly_manager::reload_assembly();
					s_script_engine_data->should_reload_assembly = false;
				});
		}
	}
	static void on_app_assembly_file_system_event_2(const std::string& path, const filewatch::Event change_type)
	{
		if (!s_script_engine_data || s_script_engine_data->is_shutting_down)
			return;

		if (!s_script_engine_data->assembly_reloading_pending && change_type == filewatch::Event::modified)
			s_script_engine_data->should_reload_assembly = true;
	}
}

script_class::script_class(const std::string& class_namespace, const std::string& class_name, bool is_core)
	: m_class_namespace(class_namespace), m_class_name(class_name)
{
	m_mono_class = mono_class_from_name(is_core ? s_script_engine_data->core_assembly_image : s_script_engine_data->app_assembly_image, class_namespace.c_str(), class_name.c_str());
}

MonoObject* script_class::instantiate()
{
	if (!m_mono_class)
	{
		WHP_CORE_WARN("[Script Engine] Cannot instantiate missing script class: {0}", get_full_name());
		return nullptr;
	}
	return script_engine::instantiate_class(m_mono_class);
}

MonoMethod* script_class::get_method(const std::string& name, int parameter_count)
{
	if (!m_mono_class)
		return nullptr;
	return mono_class_get_method_from_name(m_mono_class, name.c_str(), parameter_count);
}

MonoObject* script_class::invoke_method(MonoObject* instance, MonoMethod* method, void** params, std::string_view context)
{
	if (!method)
	{
		if (!context.empty())
			WHP_CORE_WARN("[Script Engine] Cannot invoke missing managed method while {0}", std::string(context));
		return nullptr;
	}

	MonoObject* exception = nullptr;
	auto* ptr = mono_runtime_invoke(method, instance, params, &exception);
	if (exception)
	{
		MonoString* exception_message = mono_object_to_string(exception, nullptr);
		const std::string message = utils::mono_string_to_string(exception_message);
		if (!context.empty())
			WHP_CORE_ERROR("[Script Engine] Mono Exception while {0}: {1}", std::string(context), message);
		else
			WHP_CORE_ERROR("[Script Engine] Mono Exception: {0}", message);
	}
	return ptr;
}

script_instance::script_instance(ref<script_class> script_class_in, entity entity_in) : m_script_class(script_class_in)
{
	m_entity_id = entity_in.get_UUID();
	m_entity_name = entity_in.get_name();
	m_instance = script_class_in->instantiate();
	m_constructor = s_script_engine_data->entity_class.get_method(".ctor", 1);
	m_on_create_method = script_class_in->get_method("OnCreate", 0);
	m_on_update_method = script_class_in->get_method("OnUpdate", 1);
	m_on_collider_enter_method = script_class_in->get_method("OnColliderEnter", 1);
	m_on_collider_exit_method = script_class_in->get_method("OnColliderExit", 1);
	if (!m_instance)
		return;

	// Call Entity constructor
	{
		void* param = &m_entity_id;
		m_script_class->invoke_method(m_instance, m_constructor, &param, make_method_context("Entity.ctor"));
	}
}

void script_instance::invoke_on_create()
{
	if (m_instance && m_on_create_method)
		m_script_class->invoke_method(m_instance, m_on_create_method, nullptr, make_method_context("OnCreate"));
}

void script_instance::invoke_on_update(float ts)
{
	if (m_instance && m_on_update_method)
	{
		void* param = &ts;
		m_script_class->invoke_method(m_instance, m_on_update_method, &param, make_method_context("OnUpdate"));
	}
}

void script_instance::invoke_on_collider_enter(std::string_view tag)
{
	if (m_instance && m_on_collider_enter_method)
	{
		const std::string tag_string(tag);
		MonoString* mono_string = mono_string_new(s_script_engine_data->app_domain, tag_string.c_str());
		void* param = mono_string;
		m_script_class->invoke_method(m_instance, m_on_collider_enter_method, &param, make_method_context("OnColliderEnter"));
	}
}

void script_instance::invoke_on_collider_exit(std::string_view tag)
{
	if (m_instance && m_on_collider_exit_method)
	{
		const std::string tag_string(tag);
		MonoString* mono_string = mono_string_new(s_script_engine_data->app_domain, tag_string.c_str());
		void* param = mono_string;
		m_script_class->invoke_method(m_instance, m_on_collider_exit_method, &param, make_method_context("OnColliderExit"));
	}
}

std::string script_instance::make_method_context(std::string_view method_name) const
{
	return nps::formatter::format("{0}.{1} on entity '{2}' ({3})", m_script_class->get_full_name(), std::string(method_name), m_entity_name, (uint64_t)m_entity_id);
}

bool script_instance::get_field_value_internal(const std::string& name)
{
	const auto& fields = m_script_class->get_fields();
	auto it = fields.find(name);
	if (it == fields.end())
		return false;

	const script_field& field = it->second;

	if (field.type == script_field_type::String)
	{
		MonoString* string = nullptr;
		mono_field_get_value(m_instance, field.class_field, &string);
		std::string value = utils::mono_string_to_string(string);
		s_field_value_buffer.allocate(value.size() + 1);
		std::memcpy(s_field_value_buffer.data, value.data(), value.size());
		s_field_value_buffer.data[value.size()] = '\0';
		return true;
	}

	if (field.type == script_field_type::Entity)
	{
		MonoObject* entity = nullptr;
		mono_field_get_value(m_instance, field.class_field, &entity);
		UUID entity_id = utils::entity_id_from_managed_object(entity);
		s_field_value_buffer.store(entity_id);
		return true;
	}

	if (static_cast<size_t>(field.type_size) > s_field_value_buffer.size)
		s_field_value_buffer.allocate(field.type_size);

	mono_field_get_value(m_instance, field.class_field, s_field_value_buffer.data);
	return true;



	return false;
}

bool script_instance::set_field_value_internal(const std::string& name, const void* value)
{
	const auto& fields = m_script_class->get_fields();
	const auto it = fields.find(name);
	if (it == fields.end())
		return false;

	const script_field& field = it->second;

	if (field.type == script_field_type::String)
	{
		const char* string_value = static_cast<const char*>(value);
		MonoString* string = mono_string_new(s_script_engine_data->app_domain, string_value ? string_value : "");
		mono_field_set_value(m_instance, field.class_field, string);
		return true;
	}

	if (field.type == script_field_type::Entity)
	{
		const UUID entity_id = value ? *static_cast<const UUID*>(value) : UUID(0);
		MonoObject* entity = utils::create_managed_entity_reference(entity_id);
		mono_field_set_value(m_instance, field.class_field, entity);
		return true;
	}

	mono_field_set_value(m_instance, field.class_field, const_cast<void*>(value));
	return true;
}

std::string script_instance::get_field_string(const std::string& name)
{
	if (!get_field_value_internal(name))
		return {};

	const char* value = s_field_value_buffer.as<const char>();
	return value ? std::string(value) : std::string();
}

void script_instance::set_field_string(const std::string& name, std::string_view value)
{
	std::string value_copy(value);
	set_field_value_internal(name, value_copy.c_str());
}

UUID script_instance::get_field_entity(const std::string& name)
{
	if (!get_field_value_internal(name))
		return UUID(0);

	return s_field_value_buffer.load<UUID>();
}

void script_instance::set_field_entity(const std::string& name, UUID value)
{
	set_field_value_internal(name, &value);
}

bool script_instance::get_field_array_value_internal(const std::string& name, size_t* size)
{
	const auto& fields = m_script_class->get_fields();
	auto it = fields.find(name);
	if (it == fields.end())
		return false;

	const script_field& field = it->second;
	if (!field.is_array)
		return false;

	MonoArray* array;
	mono_field_get_value(m_instance, field.class_field, &array);
	if (!array)
		return false;

	uintptr_t length = mono_array_length(array);
	if (size != nullptr)
		*size = length;

	size_t req_bufsiz = length * field.type_size;
	if (req_bufsiz > s_field_value_buffer.size)
		s_field_value_buffer.allocate(req_bufsiz);

	char* array_data = mono_array_addr_with_size(array, field.type_size, 0);
	std::memcpy(s_field_value_buffer.data, array_data, req_bufsiz);
	return true;
}

bool script_instance::set_field_array_value_internal(const std::string& name, const void* values, size_t size)
{
	const auto& fields = m_script_class->get_fields();
	auto it = fields.find(name);
	if (it == fields.end())
		return false;

	const script_field& field = it->second;
	if (!field.is_array)
		return false;

	if (field.type == script_field_type::String || field.type == script_field_type::Entity || field.type == script_field_type::Logger)
	{
		WHP_CORE_WARN("[Script Engine] Script field array type is not assignable at runtime yet: {0}", name);
		return false;
	}

	MonoType* array_type = mono_field_get_type(field.class_field);
	MonoType* element_type = utils::get_mono_type_element_type(array_type);
	MonoClass* element_class = mono_class_from_mono_type(element_type);
	if (!element_class)
		return false;

	MonoArray* array = mono_array_new(s_script_engine_data->app_domain, element_class, static_cast<uintptr_t>(size));
	if (!array)
		return false;

	if (values && size > 0)
	{
		char* array_data = mono_array_addr_with_size(array, field.type_size, 0);
		std::memcpy(array_data, values, size * field.type_size);
	}

	mono_field_set_value(m_instance, field.class_field, array);
	return true;
}

bool script_instance::set_field_array_index_value_internal(const std::string& name, size_t index, const void* value)
{
	const auto& fields = m_script_class->get_fields();
	auto it = fields.find(name);
	if (it == fields.end())
		return false;

	const script_field& field = it->second;
	if (!field.is_array)
		return false;

	MonoArray* array;
	mono_field_get_value(m_instance, field.class_field, &array);
	if (!array)
		return false;

	char* addr = mono_array_addr_with_size(array, field.type_size, static_cast<uintptr_t>(index));
	std::memcpy(addr, value, field.type_size);
	return true;
}

bool assembly_manager::load_assembly(const std::filesystem::path& filepath)
{
	s_script_engine_data->app_domain = mono_domain_create_appdomain(const_cast<char*>("WhipScriptRuntime"), nullptr);
	mono_domain_set(s_script_engine_data->app_domain, true);

	s_script_engine_data->core_assembly_filepath = filepath;
	s_script_engine_data->core_assembly = utils::load_mono_assembly(filepath, s_script_engine_data->enable_debugging);
	if (s_script_engine_data->core_assembly == nullptr)
		return false;
	s_script_engine_data->core_assembly_image = mono_assembly_get_image(s_script_engine_data->core_assembly);
	return true;
}

bool assembly_manager::load_app_assembly(const std::filesystem::path& filepath)
{
	if (filepath.extension().string() != ".dll")
	{
		WHP_CORE_WARN("[Script Engine] App assembly path is not a dll: {0}", filepath.string());
		return false;
	}

	s_script_engine_data->app_assembly_filepath = filepath;
	s_script_engine_data->assembly_reloading_pending = false;

	if (!std::filesystem::exists(filepath))
	{
		WHP_CORE_WARN("[Script Engine] App assembly file does not exist yet: {0}", filepath.string());
		return false;
	}

	s_script_engine_data->app_assembly = utils::load_mono_assembly(filepath, s_script_engine_data->enable_debugging);
	if (s_script_engine_data->app_assembly == nullptr)
	{
		WHP_CORE_ERROR("[Script Engine] Failed to load app assembly bytes: {0}", filepath.string());
		return false;
	}

	s_script_engine_data->app_assembly_image = mono_assembly_get_image(s_script_engine_data->app_assembly);
	WHP_CORE_INFO("[Script Engine] Loaded app assembly: {0}", filepath.string());
	return true;
}

bool assembly_manager::reload_assembly(bool reset_app_assembly_filepath)
{
	if (!s_script_engine_data || s_script_engine_data->is_shutting_down)
		return false;

	s_script_engine_data->app_assembly_watcher.reset();
	mono_domain_set(mono_get_root_domain(), false);

	if (s_script_engine_data->app_domain)
	{
		mono_domain_unload(s_script_engine_data->app_domain);
		s_script_engine_data->app_domain = nullptr;
	}

	assembly_manager::load_assembly(s_script_engine_data->core_assembly_filepath);
	if (reset_app_assembly_filepath)
		s_script_engine_data->app_assembly_filepath = project::get_active_asset_directory() / project::get_active()->get_config().script_module_path;

	s_script_engine_data->app_assembly = nullptr;
	s_script_engine_data->app_assembly_image = nullptr;

	if (s_script_engine_data->app_assembly_filepath.empty() || s_script_engine_data->app_assembly_filepath == project::get_active_asset_directory())
	{
		WHP_CORE_INFO("[Script Engine] Project has no app assembly configured.");
		s_script_engine_data->entity_classes.clear();
		script_glue::register_components();
		s_script_engine_data->entity_class = script_class("Whip", "Entity", true);
		s_script_engine_data->assembly_reloading_pending = false;
		return true;
	}

	WHP_CORE_INFO("[Script Engine] Reloading app assembly: {0}", s_script_engine_data->app_assembly_filepath.string());
	bool status = load_app_assembly(s_script_engine_data->app_assembly_filepath);
	if (!status)
	{
		WHP_CORE_WARN("[Script Engine] Reload finished without an app assembly.");
		s_script_engine_data->entity_classes.clear();
	}
	else
	{
		assembly_manager::load_assembly_classes();
		WHP_CORE_INFO("[Script Engine] Reloaded {0} script class(es).", s_script_engine_data->entity_classes.size());
	}

	script_glue::register_components();

	s_script_engine_data->entity_class = script_class("Whip", "Entity", true);
	s_script_engine_data->assembly_reloading_pending = false;
	return status;
}

MonoImage* assembly_manager::get_core_assembly_image()
{
	return s_script_engine_data->core_assembly_image;
}

void assembly_manager::load_base_script_fields()
{
	for (auto& [class_name, entity_class] : s_script_engine_data->entity_classes)
	{
		auto& entity_fields = s_script_engine_data->base_entity_script_fields;
		MonoObject* instance = entity_class->instantiate();
		for (auto& [field_name, field] : entity_class->get_fields())
		{
			script_field_instance& field_instance = entity_fields[class_name][field_name];
			field_instance.field = field;

			if (field.is_array)
			{
				MonoArray* array;
				mono_field_get_value(instance, field.class_field, &array);
				if (!array)
				{
					WHP_CORE_WARN("{0}.{1} is null", class_name, field_name);
					continue;
				}
				uintptr_t length = mono_array_length(array);
				raw_buffer& buf = field_instance.m_buffer;
				buf.allocate(length * field.type_size);
				char* array_data = mono_array_addr_with_size(array, field.type_size, 0);
				std::memcpy(buf.data, array_data, buf.size);
			}
			else
			{
				if (field.type == script_field_type::String)
				{
					MonoString* string = nullptr;
					mono_field_get_value(instance, field.class_field, &string);
					field_instance.set_string_value(utils::mono_string_to_string(string));
				}
				else if (field.type == script_field_type::Entity)
				{
					MonoObject* entity = nullptr;
					mono_field_get_value(instance, field.class_field, &entity);
					field_instance.set_entity_value(utils::entity_id_from_managed_object(entity));
				}
				else
				{
					raw_buffer& buf = field_instance.m_buffer;
					buf.allocate(max_type_size);
					mono_field_get_value(instance, field.class_field, buf.as<void>());
				}
			}
		}
	}
}

void assembly_manager::load_assembly_classes()
{
	s_script_engine_data->entity_classes.clear();
	s_script_engine_data->base_entity_script_fields.clear();

	const MonoTableInfo* type_definitions_table = mono_image_get_table_info(s_script_engine_data->app_assembly_image, MONO_TABLE_TYPEDEF);
	int32_t num_types = mono_table_info_get_rows(type_definitions_table);
	MonoClass* entity_class = mono_class_from_name(s_script_engine_data->core_assembly_image, "Whip", "Entity");

	for (int32_t i = 0; i < num_types; i++)
	{
		uint32_t cols[MONO_TYPEDEF_SIZE];
		mono_metadata_decode_row(type_definitions_table, i, cols, MONO_TYPEDEF_SIZE);

		const char* name_space = mono_metadata_string_heap(s_script_engine_data->app_assembly_image, cols[MONO_TYPEDEF_NAMESPACE]);
		const char* class_name = mono_metadata_string_heap(s_script_engine_data->app_assembly_image, cols[MONO_TYPEDEF_NAME]);
		std::string full_name;
		if (std::strlen(name_space) != 0)
			full_name = nps::formatter::format("{0}.{1}", name_space, class_name);
		else
			full_name = class_name;

		MonoClass* mono_class = mono_class_from_name(s_script_engine_data->app_assembly_image, name_space, class_name);

		if (mono_class == nullptr)
			continue; // probably class in class

		if (mono_class == entity_class)
			continue;

		bool is_entity = mono_class_is_subclass_of(mono_class, entity_class, false);
		if (!is_entity)
			continue;

		ref<script_class> scriptClass = make_ref<script_class>(name_space, class_name);
		s_script_engine_data->entity_classes[full_name] = scriptClass;

		int fieldCount = mono_class_num_fields(mono_class);
		WHP_CORE_WARN("[Script Engine] {0} has {1} field(s)", class_name, fieldCount);
		void* iterator = nullptr;
		while (MonoClassField* field = mono_class_get_fields(mono_class, &iterator))
		{
			const char* field_name = mono_field_get_name(field);
			uint32_t flags = mono_field_get_flags(field);
			if ((flags & WHP_MONO_FIELD_ATTRIBUTE_FIELD_ACCESS_MASK) == WHP_MONO_FIELD_ATTRIBUTE_PUBLIC)
			{
				MonoType* type = mono_field_get_type(field);
				auto [field_type, is_array] = utils::mono_type_to_script_field_type(type);
				int alignment = utils::align_of_type(field_type);
				int type_size = 0;
				if (is_array)
				{
					MonoType* element_type = utils::get_mono_type_element_type(type);
					type_size = mono_type_size(element_type, &alignment);
				}
				else
				{
					type_size = mono_type_size(type, &alignment);
				}
				scriptClass->m_fields[field_name] = { field_type, type_size, field_name, field, is_array};
			}
		}
	}
	load_base_script_fields();
}

void script_engine::init()
{
	if (s_script_engine_data)
	{
		if (s_script_engine_data->is_shutting_down)
			return;

		s_script_engine_data->entity_instances.clear();
		s_script_engine_data->entity_script_fields.clear();
		assembly_manager::reload_assembly(true);
		return;
	}

	s_script_engine_data = new script_engine_data();

	script_instance::s_field_value_buffer.allocate(initial_buffer_size);

	init_mono();
	script_glue::register_functions();

	bool status = assembly_manager::load_assembly("Resources/Scripts/Whip-ScriptCore.dll");
	if (!status)
	{
		WHP_CORE_ERROR("[ScriptEngine] Could not load Whip-ScriptCore assembly.");
		return;
	}
	const std::filesystem::path& configured_script_module_path = project::get_active()->get_config().script_module_path;
	if (configured_script_module_path.empty())
	{
		WHP_CORE_INFO("[Script Engine] Project has no app assembly configured.");
		script_glue::register_components();
		s_script_engine_data->entity_class = script_class("Whip", "Entity", true);
		return;
	}

	auto script_module_path = project::get_active_asset_directory() / configured_script_module_path;
	status = assembly_manager::load_app_assembly(script_module_path);
	if (!status)
	{
		WHP_CORE_WARN("[ScriptEngine] App assembly is not available yet: {0}", script_module_path.string());
		script_glue::register_components();
		s_script_engine_data->entity_class = script_class("Whip", "Entity", true);
		return;
	}
	assembly_manager::load_assembly_classes();
	script_glue::register_components();

	s_script_engine_data->entity_class = script_class("Whip", "Entity", true);
}

void script_engine::shutdown()
{
	if (s_script_engine_data)
	{
		s_script_engine_data->is_shutting_down = true;
		s_script_engine_data->app_assembly_watcher.reset();
		s_script_engine_data->entity_instances.clear();
		shutdown_mono();
		delete s_script_engine_data;
		s_script_engine_data = nullptr;
	}

	script_instance::s_field_value_buffer.release();
}

void script_engine::set_filewatcher_state(bool run)
{
	WHP_UNUSED(run);
}

void script_engine::on_runtime_start(scene* scene_in)
{
	s_script_engine_data->scene_context = scene_in;
	script_glue::on_runtime_start();
}

void script_engine::on_runtime_stop()
{
	s_script_engine_data->scene_context = nullptr;
	s_script_engine_data->entity_instances.clear();
	script_glue::on_runtime_stop();
}

void script_engine::invoke_all_on_create_methods()
{
	for (auto& instance : s_script_engine_data->entity_instances)
		instance.second->invoke_on_create();
}

bool script_engine::entity_class_exists(const std::string& full_class_name)
{
	if (!s_script_engine_data)
		return false;
	return s_script_engine_data->entity_classes.find(full_class_name) != s_script_engine_data->entity_classes.end();
}

void script_engine::on_create_entity(entity entity_in)
{
	const auto& sc = entity_in.get_component<script_component>();
	if (script_engine::entity_class_exists(sc.class_name))
	{
		UUID entityID = entity_in.get_UUID();
		ref<script_instance> instance = make_ref<script_instance>(s_script_engine_data->entity_classes[sc.class_name], entity_in);
		s_script_engine_data->entity_instances[entityID] = instance;

		if (s_script_engine_data->entity_script_fields.find(entityID) != s_script_engine_data->entity_script_fields.end())
		{
			const script_field_map& field_map = s_script_engine_data->entity_script_fields.at(entityID);
			for (const auto& [name, field_instance] : field_map)
			{
				if (field_instance.field.is_array && field_instance.field.type_size > 0)
					instance->set_field_array_value_internal(name, field_instance.m_buffer.as<void>(), field_instance.m_buffer.size / field_instance.field.type_size);
				else
					instance->set_field_value_internal(name, field_instance.m_buffer.as<void>());
			}
		}
	}
}

void script_engine::on_update_entity(entity entity_in, timestep ts)
{
	UUID entityUUID = entity_in.get_UUID();
	if (s_script_engine_data->entity_instances.find(entityUUID) != s_script_engine_data->entity_instances.end())
	{
		ref<script_instance> instance = s_script_engine_data->entity_instances[entityUUID];
		instance->invoke_on_update((float)ts);
	}
	else
		WHP_CORE_ERROR("[Script Engine] Could not find script_instance for entity {0}", (uint64_t)entityUUID);
}

void script_engine::on_collider_enter_entity(UUID entity_left, std::string_view tag)
{
	if (s_script_engine_data->entity_instances.find(entity_left) == s_script_engine_data->entity_instances.end())
		return;

	ref<script_instance> instance = s_script_engine_data->entity_instances[entity_left];
	instance->invoke_on_collider_enter(tag);
}

void script_engine::on_collider_exit_entity(UUID entity_left, std::string_view tag)
{
	if (s_script_engine_data->entity_instances.find(entity_left) == s_script_engine_data->entity_instances.end())
		return;

	ref<script_instance> instance = s_script_engine_data->entity_instances[entity_left];
	instance->invoke_on_collider_exit(tag);
}

scene* script_engine::get_scene_context()
{
	return s_script_engine_data->scene_context;
}

ref<script_instance> script_engine::get_entity_script_instance(UUID entityID)
{
	auto it = s_script_engine_data->entity_instances.find(entityID);
	if (it == s_script_engine_data->entity_instances.end())
		return nullptr;
	return it->second;
}

ref<script_class> script_engine::get_entity_class(const std::string& class_name)
{
	if (!s_script_engine_data)
		return nullptr;
	if (s_script_engine_data->entity_classes.find(class_name) == s_script_engine_data->entity_classes.end())
		return nullptr;
	return s_script_engine_data->entity_classes.at(class_name);
}

std::unordered_map<std::string, ref<script_class>> script_engine::get_entity_classes()
{
	if (!s_script_engine_data)
		return {};
	return s_script_engine_data->entity_classes;
}

script_field_map& script_engine::get_script_field_map(entity entity_in)
{
	WHP_CORE_ASSERT(entity_in, "[Script Engine] Entity does not exist!");
	UUID entityID = entity_in.get_UUID();
	return s_script_engine_data->entity_script_fields[entityID];
}

script_field_map& script_engine::get_base_script_field_map(const std::string& class_name)
{
	WHP_CORE_ASSERT(s_script_engine_data->base_entity_script_fields.find(class_name) != s_script_engine_data->base_entity_script_fields.end(), "[Script Engine] Class not found!");
	return s_script_engine_data->base_entity_script_fields[class_name];
}

MonoObject* script_engine::get_managed_instance(UUID uuid)
{
	WHP_CORE_ASSERT(s_script_engine_data->entity_instances.find(uuid) != s_script_engine_data->entity_instances.end(), "[Script Engine] Entity Instance not found!");
	return s_script_engine_data->entity_instances.at(uuid)->get_managed_object();
}

void script_engine::init_mono()
{
	mono_set_assemblies_path("mono/lib");

	if (s_script_engine_data->enable_debugging)
	{
		const char* argv[2] = {
			"--debugger-agent=transport=dt_socket,address=127.0.0.1:2550,server=y,suspend=n,loglevel=3,logfile=MonoDebugger.log",
			"--soft-breakpoints"
		};

		mono_jit_parse_options(2, (char**)argv);
		mono_debug_init(MONO_DEBUG_FORMAT_MONO);
	}

	MonoDomain* root_domain = mono_jit_init("WhipJITRuntime");
	WHP_CORE_ASSERT(root_domain);
	// Store the root domain pointer
	s_script_engine_data->root_domain = root_domain;

	if (s_script_engine_data->enable_debugging)
		mono_debug_domain_create(s_script_engine_data->root_domain);

	mono_thread_set_main(mono_thread_current());
}

void script_engine::shutdown_mono()
{
	if (MonoDomain* root_domain = mono_get_root_domain())
		mono_domain_set(root_domain, false);

	if (s_script_engine_data->app_domain)
	{
		mono_domain_unload(s_script_engine_data->app_domain);
		s_script_engine_data->app_domain = nullptr;
	}

	if (s_script_engine_data->root_domain)
	{
		mono_jit_cleanup(s_script_engine_data->root_domain);
		s_script_engine_data->root_domain = nullptr;
	}
}

MonoObject* script_engine::instantiate_class(MonoClass* mono_class)
{
	MonoObject* instance = mono_object_new(s_script_engine_data->app_domain, mono_class);
	mono_runtime_object_init(instance);
	return instance;
}

namespace utils
{
	MonoString* create_string(const char* string)
	{
		return mono_string_new(s_script_engine_data->app_domain, string);
	}

	MonoString* create_string(const wchar_t* wstring)
	{
		return mono_string_new_utf16(s_script_engine_data->app_domain, wstring, static_cast<uint32_t>(wcslen(wstring)));
	}
}

_WHIP_END
