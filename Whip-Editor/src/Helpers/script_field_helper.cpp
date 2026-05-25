#include "script_field_helper.h"

#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/MouseButtonCodes.h>
#include <Whip/UI/UI_helpers.h>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

_WHIP_START

namespace
{
	class table_row_scope
	{
	public:
		table_row_scope(bool active, const char* label)
			: m_active(active)
		{
			if (!m_active)
				return;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(label);
			ImGui::TableNextColumn();
			ImGui::PushItemWidth(-1.0f);
		}

		~table_row_scope()
		{
			if (m_active)
				ImGui::PopItemWidth();
		}

		table_row_scope(const table_row_scope&) = delete;
		table_row_scope& operator=(const table_row_scope&) = delete;

	private:
		bool m_active = false;
	};

	std::string control_label(const script_field& field, bool in_table)
	{
		if (!in_table)
			return field.name;

		return "##" + field.name;
	}

	std::string array_control_label(const script_field& field, std::string_view row_label)
	{
		std::string label = "##";
		label += field.name;
		label += row_label;
		return label;
	}

	std::string array_table_id(const script_field& field)
	{
		return "ArrayTable##" + field.name;
	}

	std::string array_row_label(size_t index)
	{
		return "[" + std::to_string(index) + "]";
	}

	script_field_instance& editor_field(entity ent, const script_field& field)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		return entity_fields.at(field.name);
	}

	script_field_instance& base_field(const std::string& class_name, const script_field& field)
	{
		auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
		return base_entity_fields.at(field.name);
	}

	script_field_instance& override_field(entity ent, const script_field& field)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		script_field_instance& field_instance = entity_fields[field.name];
		field_instance.field = field;
		return field_instance;
	}

	bool draw_float_control(const char* id, float& value)
	{
		return ImGui::DragFloat(id, &value);
	}

	bool draw_int_control(const char* id, int& value)
	{
		return ImGui::InputInt(id, &value);
	}

	bool draw_bool_control(const char* id, bool& value)
	{
		return ImGui::Checkbox(id, &value);
	}

	template <ImGuiDataType DataType, typename T>
	bool draw_scalar_control(const char* id, T& value)
	{
		return ImGui::InputScalar(id, DataType, &value);
	}

	bool draw_long_control(const char* id, int64_t& value)
	{
		return draw_scalar_control<ImGuiDataType_S64>(id, value);
	}

	bool draw_uint_control(const char* id, uint32_t& value)
	{
		return draw_scalar_control<ImGuiDataType_U32>(id, value);
	}

	bool draw_ulong_control(const char* id, uint64_t& value)
	{
		return draw_scalar_control<ImGuiDataType_U64>(id, value);
	}

	bool draw_byte_control(const char* id, uint8_t& value)
	{
		return draw_scalar_control<ImGuiDataType_U8>(id, value);
	}

	bool draw_sbyte_control(const char* id, int8_t& value)
	{
		return draw_scalar_control<ImGuiDataType_S8>(id, value);
	}

	bool draw_char_control(const char* id, char& value)
	{
		return draw_scalar_control<ImGuiDataType_S8>(id, value);
	}

	bool draw_short_control(const char* id, short& value)
	{
		return draw_scalar_control<ImGuiDataType_S16>(id, value);
	}

	bool draw_ushort_control(const char* id, uint16_t& value)
	{
		return draw_scalar_control<ImGuiDataType_U16>(id, value);
	}

	bool draw_double_control(const char* id, double& value)
	{
		return ImGui::InputDouble(id, &value);
	}

	bool draw_script_vec2_control(const char* id, glm::vec2& value)
	{
		return UI::draw_field_vec2_control(id, value, 0.0f, ImGui::GetColumnWidth());
	}

	bool draw_script_vec3_control(const char* id, glm::vec3& value)
	{
		return UI::draw_field_vec3_control(id, value, 0.0f, ImGui::GetColumnWidth());
	}

	bool draw_script_vec4_control(const char* id, glm::vec4& value)
	{
		return ImGui::ColorEdit4(id, glm::value_ptr(value));
	}

	template <typename Code, typename ToStringFn>
	bool draw_code_combo(const char* id, Code& value, Code first, Code last, ToStringFn to_string)
	{
		bool changed = false;

		if (ImGui::BeginCombo(id, to_string(value)))
		{
			for (Code candidate = first; candidate <= last; ++candidate)
			{
				const bool is_selected = value == candidate;
				std::string_view label = to_string(candidate);
				if (label == "unknown")
					continue;

				if (ImGui::Selectable(label.data(), is_selected))
				{
					value = candidate;
					changed = true;
				}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		return changed;
	}

	bool draw_key_combo(const char* id, key_code& value)
	{
		return draw_code_combo(id, value, static_cast<key_code>(key::space), static_cast<key_code>(key::menu), key::to_string);
	}

	bool draw_mouse_combo(const char* id, mouse_code& value)
	{
		return draw_code_combo(id, value, static_cast<mouse_code>(mouse::button0), static_cast<mouse_code>(mouse::button_last), mouse::to_string);
	}

	template <UI::script_field_draw DrawMode, typename T, typename DrawFn>
	void draw_value_contents(const script_field& field, entity ent, const std::string& class_name, const std::string& id, DrawFn draw)
	{
		if constexpr (DrawMode == UI::script_field_draw::while_scene_running)
		{
			(void)class_name;
			ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
			if (!sc_instance)
				return;

			T data = sc_instance->get_field_value<T>(field.name);
			if (draw(id.c_str(), data))
				sc_instance->set_field_value<T>(field.name, data);
		}
		else if constexpr (DrawMode == UI::script_field_draw::set_in_the_editor)
		{
			(void)class_name;
			script_field_instance& sc_field = editor_field(ent, field);
			T data = sc_field.get_value<T>();
			if (draw(id.c_str(), data))
				sc_field.set_value<T>(data);
		}
		else
		{
			script_field_instance& sc_field = base_field(class_name, field);
			T data = sc_field.get_value<T>();
			if (draw(id.c_str(), data))
				override_field(ent, field).set_value<T>(data);
		}
	}

	template <UI::script_field_draw DrawMode, typename T, typename DrawFn>
	void draw_value(const script_field& field, entity ent, const std::string& class_name, bool in_table, DrawFn draw)
	{
		table_row_scope row(in_table, field.name.c_str());
		draw_value_contents<DrawMode, T>(field, ent, class_name, control_label(field, in_table), draw);
	}

	template <typename OnChange>
	void draw_float_array_table(const script_field& field, float* values, size_t size, OnChange on_change)
	{
		if (!values && size > 0)
			return;

		const std::string table_id = array_table_id(field);
		if (!ImGui::BeginTable(table_id.c_str(), 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			return;

		for (size_t i = 0; i < size; ++i)
		{
			const std::string row_label = array_row_label(i);
			table_row_scope row(true, row_label.c_str());

			const std::string id = array_control_label(field, row_label);
			if (draw_float_control(id.c_str(), values[i]))
				on_change(i);
		}

		ImGui::EndTable();
	}

	template <UI::script_field_draw DrawMode>
	void draw_float_field(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		if (!field.is_array)
		{
			draw_value<DrawMode, float>(field, ent, class_name, in_table, draw_float_control);
			return;
		}

		table_row_scope row(in_table, field.name.c_str());

		if constexpr (DrawMode == UI::script_field_draw::while_scene_running)
		{
			(void)class_name;
			ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
			if (!sc_instance)
				return;

			size_t size = 0;
			float* raw_array = sc_instance->get_field_array<float>(field.name, &size);
			draw_float_array_table(field, raw_array, size, [&](size_t index)
				{
					sc_instance->set_field_array_index(field.name, index, raw_array[index]);
				});
		}
		else if constexpr (DrawMode == UI::script_field_draw::set_in_the_editor)
		{
			(void)class_name;
			script_field_instance& sc_field = editor_field(ent, field);
			const size_t size = sc_field.get_array_size<float>();
			float* raw_array = sc_field.get_value_array<float>();

			std::vector<float> values;
			if (raw_array && size > 0)
				values.assign(raw_array, raw_array + size);

			draw_float_array_table(field, values.data(), values.size(), [&](size_t)
				{
					sc_field.set_value_array<float>(values.data(), values.size());
				});
		}
		else
		{
			script_field_instance& sc_field = base_field(class_name, field);
			const size_t size = sc_field.get_array_size<float>();
			float* raw_array = sc_field.get_value_array<float>();

			std::vector<float> values;
			if (raw_array && size > 0)
				values.assign(raw_array, raw_array + size);

			draw_float_array_table(field, values.data(), values.size(), [&](size_t)
				{
					override_field(ent, field).set_value_array<float>(values.data(), values.size());
				});
		}
	}
}

namespace UI
{
	template <>
	void draw_field<script_field_type::Float, script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		draw_float_field<script_field_draw::while_scene_running>(field, ent, class_name, in_table);
	}

	template <>
	void draw_field<script_field_type::Float, script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		draw_float_field<script_field_draw::set_in_the_editor>(field, ent, class_name, in_table);
	}

	template <>
	void draw_field<script_field_type::Float, script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		draw_float_field<script_field_draw::with_base_value>(field, ent, class_name, in_table);
	}

#define WHIP_DEFINE_SCRIPT_FIELD(SCRIPT_TYPE, VALUE_TYPE, DRAW_FUNC) \
	template <> \
	void draw_field<script_field_type::SCRIPT_TYPE, script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table) \
	{ \
		draw_value<script_field_draw::while_scene_running, VALUE_TYPE>(field, ent, class_name, in_table, DRAW_FUNC); \
	} \
	template <> \
	void draw_field<script_field_type::SCRIPT_TYPE, script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table) \
	{ \
		draw_value<script_field_draw::set_in_the_editor, VALUE_TYPE>(field, ent, class_name, in_table, DRAW_FUNC); \
	} \
	template <> \
	void draw_field<script_field_type::SCRIPT_TYPE, script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table) \
	{ \
		draw_value<script_field_draw::with_base_value, VALUE_TYPE>(field, ent, class_name, in_table, DRAW_FUNC); \
	}

	WHIP_DEFINE_SCRIPT_FIELD(Int, int, draw_int_control)
	WHIP_DEFINE_SCRIPT_FIELD(Bool, bool, draw_bool_control)
	WHIP_DEFINE_SCRIPT_FIELD(Long, int64_t, draw_long_control)
	WHIP_DEFINE_SCRIPT_FIELD(Vector2, glm::vec2, draw_script_vec2_control)
	WHIP_DEFINE_SCRIPT_FIELD(Vector3, glm::vec3, draw_script_vec3_control)
	WHIP_DEFINE_SCRIPT_FIELD(Vector4, glm::vec4, draw_script_vec4_control)
	WHIP_DEFINE_SCRIPT_FIELD(UInt, uint32_t, draw_uint_control)
	WHIP_DEFINE_SCRIPT_FIELD(ULong, uint64_t, draw_ulong_control)
	WHIP_DEFINE_SCRIPT_FIELD(Double, double, draw_double_control)
	WHIP_DEFINE_SCRIPT_FIELD(Byte, uint8_t, draw_byte_control)
	WHIP_DEFINE_SCRIPT_FIELD(SByte, int8_t, draw_sbyte_control)
	WHIP_DEFINE_SCRIPT_FIELD(Char, char, draw_char_control)
	WHIP_DEFINE_SCRIPT_FIELD(Short, short, draw_short_control)
	WHIP_DEFINE_SCRIPT_FIELD(UShort, uint16_t, draw_ushort_control)
	WHIP_DEFINE_SCRIPT_FIELD(KeyCode, key_code, draw_key_combo)
	WHIP_DEFINE_SCRIPT_FIELD(MouseCode, mouse_code, draw_mouse_combo)

#undef WHIP_DEFINE_SCRIPT_FIELD

	template <script_field_draw DrawMode>
	void draw_field_by_type(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		switch (field.type)
		{
		case script_field_type::Float: draw_field<script_field_type::Float, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::Int: draw_field<script_field_type::Int, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::Bool: draw_field<script_field_type::Bool, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::Long: draw_field<script_field_type::Long, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::Vector3: draw_field<script_field_type::Vector3, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::Vector2: draw_field<script_field_type::Vector2, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::Vector4: draw_field<script_field_type::Vector4, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::UInt: draw_field<script_field_type::UInt, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::ULong: draw_field<script_field_type::ULong, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::Double: draw_field<script_field_type::Double, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::Byte: draw_field<script_field_type::Byte, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::SByte: draw_field<script_field_type::SByte, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::Char: draw_field<script_field_type::Char, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::Short: draw_field<script_field_type::Short, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::UShort: draw_field<script_field_type::UShort, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::KeyCode: draw_field<script_field_type::KeyCode, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::MouseCode: draw_field<script_field_type::MouseCode, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::None:
		case script_field_type::String:
		case script_field_type::Entity:
		case script_field_type::Logger:
		default:
			break;
		}
	}

	void draw_field_by_type(script_field_draw draw_mode, const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		switch (draw_mode)
		{
		case script_field_draw::while_scene_running:
			draw_field_by_type<script_field_draw::while_scene_running>(field, ent, class_name, in_table);
			break;
		case script_field_draw::set_in_the_editor:
			draw_field_by_type<script_field_draw::set_in_the_editor>(field, ent, class_name, in_table);
			break;
		case script_field_draw::with_base_value:
			draw_field_by_type<script_field_draw::with_base_value>(field, ent, class_name, in_table);
			break;
		}
	}
}

_WHIP_END
