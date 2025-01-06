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
#include <mono/metadata/tabledef.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/threads.h>

#include <FileWatch.h>
#include <nps_formatter.h>

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

	static script_field_type mono_type_to_script_field_type(MonoType* monoType)
	{
		std::string typeName = mono_type_get_name(monoType);

		auto it = s_script_field_type.find(typeName);
		if (it == s_script_field_type.end())
		{
			WHP_CORE_ERROR("[Script Engine] Unknown type: {0}", typeName);
			return script_field_type::None;
		}
		return it->second;
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
	static void on_app_assembly_file_system_event_1(const std::string& path, const filewatch::Event change_type)
	{
		if ((!s_script_engine_data->assembly_reloading_pending && change_type == filewatch::Event::modified) || s_script_engine_data->should_reload_assembly)
		{
			s_script_engine_data->assembly_reloading_pending = true;

			application::get().submit_to_main_thread([]()
				{
					s_script_engine_data->app_assembly_watcher.reset();
					assembly_manager::reload_assembly();
					s_script_engine_data->should_reload_assembly = false;
				});
		}
	}
	static void on_app_assembly_file_system_event_2(const std::string& path, const filewatch::Event change_type)
	{
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
	return script_engine::instantiate_class(m_mono_class);
}

MonoMethod* script_class::get_method(const std::string& name, int parameter_count)
{
	return mono_class_get_method_from_name(m_mono_class, name.c_str(), parameter_count);
}

MonoObject* script_class::invoke_method(MonoObject* instance, MonoMethod* method, void** params)
{
	MonoObject* exception = nullptr;
	auto* ptr = mono_runtime_invoke(method, instance, params, &exception);
	if (exception)
	{
		MonoString* exception_message = mono_object_to_string(exception, nullptr);
		const char* message = mono_string_to_utf8(exception_message);
		WHP_CORE_ERROR("[Script Engine] Mono Exception: {0}", message);
		mono_free((void*)message);
	}
	return ptr;
}

script_instance::script_instance(ref<script_class> script_class_in, entity entity_in) : m_script_class(script_class_in)
{
	m_instance = script_class_in->instantiate();
	m_constructor = s_script_engine_data->entity_class.get_method(".ctor", 1);
	m_on_create_method = script_class_in->get_method("OnCreate", 0);
	m_on_update_method = script_class_in->get_method("OnUpdate", 1);
	m_on_collider_enter_method = script_class_in->get_method("OnColliderEnter", 1);
	m_on_collider_exit_method = script_class_in->get_method("OnColliderExit", 1);

	// Call Entity constructor
	{
		UUID entityID = entity_in.get_UUID();
		void* param = &entityID;
		m_script_class->invoke_method(m_instance, m_constructor, &param);
	}
}

void script_instance::invoke_on_create()
{
	if (m_on_create_method)
		m_script_class->invoke_method(m_instance, m_on_create_method);
}

void script_instance::invoke_on_update(float ts)
{
	if (m_on_update_method)
	{
		void* param = &ts;
		m_script_class->invoke_method(m_instance, m_on_update_method, &param);
	}
}

void script_instance::invoke_on_collider_enter(std::string_view tag)
{
	if (m_on_collider_enter_method)
	{
		MonoString* mono_string = mono_string_new(s_script_engine_data->app_domain, tag.data());
		void* param = mono_string;
		m_script_class->invoke_method(m_instance, m_on_collider_enter_method, &param);
	}
}

void script_instance::invoke_on_collider_exit(std::string_view tag)
{
	if (m_on_collider_exit_method)
	{
		MonoString* mono_string = mono_string_new(s_script_engine_data->app_domain, tag.data());
		void* param = mono_string;
		m_script_class->invoke_method(m_instance, m_on_collider_exit_method, &param);
	}
}

bool script_instance::get_field_value_internal(const std::string& name, void* buffer)
{
	const auto& fields = m_script_class->get_fields();
	auto it = fields.find(name);
	if (it == fields.end())
		return false;

	const script_field& field = it->second;
	mono_field_get_value(m_instance, field.class_field, buffer);
	return true;
}

bool script_instance::set_field_value_internal(const std::string& name, const void* value)
{
	const auto& fields = m_script_class->get_fields();
	auto it = fields.find(name);
	if (it == fields.end())
		return false;

	const script_field& field = it->second;
	mono_field_set_value(m_instance, field.class_field, (void*)value);
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
	if (filepath.extension().string() == ".dll")
	{
		s_script_engine_data->app_assembly_filepath = filepath;
		s_script_engine_data->app_assembly = utils::load_mono_assembly(filepath, s_script_engine_data->enable_debugging);
		if (s_script_engine_data->app_assembly == nullptr)
			return false;
		s_script_engine_data->app_assembly_image = mono_assembly_get_image(s_script_engine_data->app_assembly);

		s_script_engine_data->app_assembly_watcher = make_scope<filewatch::FileWatch<std::string>>(filepath.string(), utils::on_app_assembly_file_system_event_1);
		s_script_engine_data->assembly_reloading_pending = false;
		return true;
	}
	return false;
}

void assembly_manager::reload_assembly(bool reset_app_assembly_filepath)
{
	mono_domain_set(mono_get_root_domain(), false);

	mono_domain_unload(s_script_engine_data->app_domain);

	assembly_manager::load_assembly(s_script_engine_data->core_assembly_filepath);
	if (reset_app_assembly_filepath)
		s_script_engine_data->app_assembly_filepath = project::get_active_asset_directory() / project::get_active()->get_config().script_module_path;

	bool status = load_app_assembly(s_script_engine_data->app_assembly_filepath);
	if (!status)
	{
		WHP_CORE_ERROR("[ScriptEngine] Could not load app assembly.");
		s_script_engine_data->entity_classes.clear();
	}
	else
		assembly_manager::load_assembly_classes();

	script_glue::register_components();

	s_script_engine_data->entity_class = script_class("Whip", "Entity", true);
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
		for (auto& [field_name, field] : entity_class->get_fields())
			mono_field_get_value(entity_class->instantiate(), field.class_field, entity_fields[class_name][field_name].m_buffer.as<void>());
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
			const char* fieldName = mono_field_get_name(field);
			uint32_t flags = mono_field_get_flags(field);
			if (flags & FIELD_ATTRIBUTE_PUBLIC)
			{
				MonoType* type = mono_field_get_type(field);
				script_field_type fieldType = utils::mono_type_to_script_field_type(type);
				scriptClass->m_fields[fieldName] = { fieldType, fieldName, field };
			}
		}
	}
	load_base_script_fields();
}

void script_engine::init()
{
	s_script_engine_data = new script_engine_data();

	init_mono();
	script_glue::register_functions();

	bool status = assembly_manager::load_assembly("Resources/Scripts/Whip-ScriptCore.dll");
	if (!status)
	{
		WHP_CORE_ERROR("[ScriptEngine] Could not load Whip-ScriptCore assembly.");
		return;
	}
	auto script_module_path = project::get_active_asset_directory() / project::get_active()->get_config().script_module_path;
	status = assembly_manager::load_app_assembly(script_module_path);
	if (!status)
	{
		WHP_CORE_ERROR("[ScriptEngine] Could not load app assembly.");
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
		shutdown_mono();
		delete s_script_engine_data;
		s_script_engine_data = nullptr;
	}
}

void script_engine::set_filewatcher_state(bool run)
{
	s_script_engine_data->app_assembly_watcher;
	if (run)
	{
		s_script_engine_data->app_assembly_watcher = make_scope<filewatch::FileWatch<std::string>>(s_script_engine_data->app_assembly_filepath.string(), utils::on_app_assembly_file_system_event_1);
		utils::on_app_assembly_file_system_event_1(std::string(), static_cast<filewatch::Event>(-1)); // If we need to reload, let it be reloaded.
	}
	else
		s_script_engine_data->app_assembly_watcher = make_scope<filewatch::FileWatch<std::string>>(s_script_engine_data->app_assembly_filepath.string(), utils::on_app_assembly_file_system_event_2);
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
				instance->set_field_value_internal(name, field_instance.m_buffer.as<void>());
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
	if (s_script_engine_data->entity_classes.find(class_name) == s_script_engine_data->entity_classes.end())
		return nullptr;
	return s_script_engine_data->entity_classes.at(class_name);
}

std::unordered_map<std::string, ref<script_class>> script_engine::get_entity_classes()
{
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
	mono_domain_set(mono_get_root_domain(), false);
	mono_domain_unload(s_script_engine_data->app_domain);
	s_script_engine_data->app_domain = nullptr;
	mono_jit_cleanup(s_script_engine_data->root_domain);
	s_script_engine_data->root_domain = nullptr;
}

MonoObject* script_engine::instantiate_class(MonoClass* mono_class)
{
	MonoObject* instance = mono_object_new(s_script_engine_data->app_domain, mono_class);
	mono_runtime_object_init(instance);
	return instance;
}

namespace utils
{
	const char* script_field_type_to_string(script_field_type type)
	{
		switch (type)
		{
		case script_field_type::String:   return "String";
		case script_field_type::Float:   return "Float";
		case script_field_type::Double:  return "Double";
		case script_field_type::Bool:    return "Bool";
		case script_field_type::Char:    return "Char";
		case script_field_type::SByte:    return "SByte";
		case script_field_type::Short:   return "Short";
		case script_field_type::Int:     return "Int";
		case script_field_type::Long:    return "Long";
		case script_field_type::Byte:   return "Byte";
		case script_field_type::UShort:  return "UShort";
		case script_field_type::UInt:    return "UInt";
		case script_field_type::ULong:   return "ULong";
		case script_field_type::KeyCode:   return "KeyCode";
		case script_field_type::MouseCode:   return "MouseCode";
		case script_field_type::Vector2: return "Vector2";
		case script_field_type::Vector3: return "Vector3";
		case script_field_type::Vector4: return "Vector4";
		case script_field_type::Entity:  return "Entity";
		case script_field_type::Logger:  return "Logger";
		}
		return "<Invalid>";
	}

	script_field_type script_field_type_from_string(std::string_view field_type)
	{

		{
			if (field_type == "None")    return script_field_type::None;
			if (field_type == "String")   return script_field_type::String;
			if (field_type == "Float")   return script_field_type::Float;
			if (field_type == "Double")  return script_field_type::Double;
			if (field_type == "Bool")    return script_field_type::Bool;
			if (field_type == "Char")    return script_field_type::Char;
			if (field_type == "Byte")    return script_field_type::Byte;
			if (field_type == "Short")   return script_field_type::Short;
			if (field_type == "Int")     return script_field_type::Int;
			if (field_type == "Long")    return script_field_type::Long;
			if (field_type == "SByte")   return script_field_type::SByte;
			if (field_type == "UShort")  return script_field_type::UShort;
			if (field_type == "UInt")    return script_field_type::UInt;
			if (field_type == "ULong")   return script_field_type::ULong;
			if (field_type == "KeyCode")   return script_field_type::KeyCode;
			if (field_type == "MouseCode")   return script_field_type::MouseCode;
			if (field_type == "Vector2") return script_field_type::Vector2;
			if (field_type == "Vector3") return script_field_type::Vector3;
			if (field_type == "Vector4") return script_field_type::Vector4;
			if (field_type == "Entity")  return script_field_type::Entity;
			if (field_type == "Logger")  return script_field_type::Logger;

			WHP_CORE_ASSERT(false, "[Script Engine] Unknown script_field_type");
			return script_field_type::None;
		}
	}

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
