#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <Whip/Core/buffer.h>
#include <Whip/Scene/entity.h>
#include <Whip/Scene/scene.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <map>
#include <unordered_map>

#include "../vendor/frenum/frenum.h"

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

_WHIP_START

enum class script_field_type
{
	None = 0,
	String,
	Float, Double,
	Bool, Char, SByte, Short, Int, Long,
	Byte, UShort, UInt, ULong,
	KeyCode, MouseCode,
	Vector2, Vector3, Vector4,
	Entity,
	Logger
};

MakeFrenumInNamespace(whip, script_field_type, None, String, Float, Double, Bool, Char, SByte, Short, Int, Long, Byte, UShort, UInt, ULong, KeyCode, MouseCode, Vector2, Vector3, Vector4, Entity, Logger)

struct script_field
{
	script_field_type type = script_field_type::None;
	int type_size = 0;
	std::string name;

	MonoClassField* class_field = nullptr;
	bool is_array;
};

struct script_field_instance
{
	script_field field;

	script_field_instance()
	{
	}

	~script_field_instance()
	{
		m_buffer.release();
	}

	template<typename T>
	T get_value()
	{
		return m_buffer.load<T>();
	}

	template<typename T>
	void set_value(T value)
	{
		m_buffer.store<T>(value);
	}

	std::string get_string_value() const
	{
		if (!m_buffer.data || m_buffer.size == 0)
			return {};

		const char* data = m_buffer.as<const char>();
		size_t length = static_cast<size_t>(m_buffer.size);
		if (data[length - 1] == '\0')
			--length;

		return std::string(data, length);
	}

	void set_string_value(std::string_view value)
	{
		m_buffer.allocate(value.size() + 1);
		std::memcpy(m_buffer.data, value.data(), value.size());
		m_buffer.data[value.size()] = '\0';
	}

	UUID get_entity_value()
	{
		if (!m_buffer.can_cast_to<UUID>())
			return UUID(0);

		return m_buffer.load<UUID>();
	}

	void set_entity_value(UUID value)
	{
		set_value(value);
	}

	template <typename T>
	T* get_value_array()
	{
		return m_buffer.as<T>();
	}

	template <typename T>
	size_t get_array_size()
	{
		return m_buffer.size / sizeof(T);
	}
	
	// Todo: needs an update or make a new set array index method
	template <typename T>
	void set_value_array(const T* array, size_t size)
	{
		const size_t buffer_size = size * sizeof(T);
		if (buffer_size == 0)
		{
			m_buffer.release();
			return;
		}

		WHP_CORE_ASSERT(array, "Cannot set script field array from null data!");
		if (m_buffer.data == reinterpret_cast<const uint8_t*>(array) && m_buffer.size == buffer_size)
			return;

		m_buffer.allocate(buffer_size);
		std::memcpy(m_buffer.data, array, buffer_size);
	}
private:
	raw_buffer m_buffer;

	friend class script_engine;
	friend class assembly_manager;
	friend class script_instance;
};

using script_field_map = std::unordered_map<std::string, script_field_instance>;

class script_class
{
public:
	script_class() = default;
	script_class(const std::string& class_namespace, const std::string& class_name, bool is_core = false);

	MonoObject* instantiate();
	MonoMethod* get_method(const std::string& name, int parameter_count);
	static MonoObject* invoke_method(MonoObject* instance, MonoMethod* method, void** params = nullptr, std::string_view context = {});
	std::string get_full_name() const { return m_class_namespace.empty() ? m_class_name : m_class_namespace + "." + m_class_name; }
	const std::map<std::string, script_field>& get_fields() const { return m_fields; }
private:
	std::string m_class_namespace;
	std::string m_class_name;
	std::map<std::string, script_field> m_fields;

	MonoClass* m_mono_class = nullptr;

	friend class script_engine;
	friend class assembly_manager;
};

class script_instance
{
public:
	script_instance(ref<script_class> script_class_in, entity entity_in);

	void invoke_on_create();
	void invoke_on_update(float ts);
	void invoke_on_collider_enter(std::string_view tag);
	void invoke_on_collider_exit(std::string_view tag);

	ref<script_class> get_script_class() { return m_script_class; }

	template<class T>
	T get_field_value(const std::string& name)
	{
		bool success = get_field_value_internal(name);
		if (!success)
			return T();

		return s_field_value_buffer.load<T>();
	}

	template <class T>
	T* get_field_array(const std::string& name, size_t* size)
	{
		bool success = get_field_array_value_internal(name, size);
		if (!success)
			return nullptr;

		return s_field_value_buffer.as<T>();
	}

	template<class T>
	void set_field_value(const std::string& name, T value)
	{
		//static_assert(sizeof(T) <= 16, "Type too large!");
		set_field_value_internal(name, &value);
	}

	std::string get_field_string(const std::string& name);
	void set_field_string(const std::string& name, std::string_view value);

	UUID get_field_entity(const std::string& name);
	void set_field_entity(const std::string& name, UUID value);

	template <class T>
	void set_field_array_index(const std::string& name, size_t index, T value)
	{
		set_field_array_index_value_internal(name, index, &value);
	}

	MonoObject* get_managed_object() { return m_instance; }
	const MonoObject* get_managed_object() const { return m_instance; }
private:
	bool get_field_value_internal(const std::string& name); // loads value to s_field_value_buffer
	bool set_field_value_internal(const std::string& name, const void* value);
	bool get_field_array_value_internal(const std::string& name, size_t* size); // loads value to s_field_value_buffer
	bool set_field_array_value_internal(const std::string& name, const void* values, size_t size);
	bool set_field_array_index_value_internal(const std::string& name, size_t index, const void* value);
	std::string make_method_context(std::string_view method_name) const;
private:
	ref<script_class> m_script_class;
	MonoObject* m_instance = nullptr;
	MonoMethod* m_constructor = nullptr;
	MonoMethod* m_on_create_method = nullptr;
	MonoMethod* m_on_update_method = nullptr;
	MonoMethod* m_on_collider_enter_method = nullptr;
	MonoMethod* m_on_collider_exit_method = nullptr;
	UUID m_entity_id = 0;
	std::string m_entity_name;

	inline static raw_buffer s_field_value_buffer;

	friend class script_engine;
};

class assembly_manager
{
public:
	static bool load_assembly(const std::filesystem::path& filepath);
	static bool load_app_assembly(const std::filesystem::path& filepath);
	static bool reload_assembly(bool reset_app_assembly_filepath = false);

	static MonoImage* get_core_assembly_image();

private:
	static void load_base_script_fields();
	static void load_assembly_classes();

	friend class script_engine;
	friend struct script_field_instance;
};

class script_engine
{
public:
	static void init();
	static void shutdown();

	static void set_filewatcher_state(bool run = true);

	static void on_runtime_start(scene* scene_in);
	static void on_runtime_stop();

	static void invoke_all_on_create_methods();

	static bool entity_class_exists(const std::string& full_class_name);
	static void on_create_entity(entity entity_in);
	static void on_update_entity(entity entity_in, timestep ts);
	static void on_collider_enter_entity(UUID entity_left, std::string_view tag);
	static void on_collider_exit_entity(UUID entity_left, std::string_view tag);

	static scene* get_scene_context();
	static ref<script_instance> get_entity_script_instance(UUID entityID);
	static ref<script_class> get_entity_class(const std::string& class_name);
	static std::unordered_map<std::string, ref<script_class>> get_entity_classes();
	static script_field_map& get_script_field_map(entity entity_in);
	static script_field_map& get_base_script_field_map(const std::string& class_name);

	static MonoObject* get_managed_instance(UUID uuid);
private:
	static void init_mono();
	static void shutdown_mono();

	static MonoObject* instantiate_class(MonoClass* mono_class);

	friend class script_class;
	friend class script_glue;
};

namespace utils
{
	MonoString* create_string(const char* string);
	MonoString* create_string(const wchar_t* wstring);
}

_WHIP_END
