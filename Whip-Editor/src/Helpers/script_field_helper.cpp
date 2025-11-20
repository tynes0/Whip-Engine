#include "script_field_helper.h"

#include <Whip/UI/UI_helpers.h>
#include <Whip/Core/KeyCodes.h>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include "OpenAL-Soft/src/common/alspan.h"

_WHIP_START
#define BEGIN_COMPONENT_TABLE_ROW(cond, ...) do { if (cond) { ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text(__VA_ARGS__); ImGui::TableNextColumn(); ImGui::PushItemWidth(-1); } } while(false)
#define END_COMPONENT_TABLE_ROW(cond) do { if (cond) { ImGui::PopItemWidth(); } } while(false)

namespace UI
{
	// ===================================================================================================
	// ============================================== Float ==============================================
	// ===================================================================================================

	template <>
	void draw_field<script_field_type::Float, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		if (!field.is_array)
		{
			float data = sc_instance->get_field_value<float>(field.name);
			if (ImGui::DragFloat(("##" + field.name).c_str(), &data))
				sc_instance->set_field_value(field.name, data);
		}
		else
		{
			size_t size = 0;
			float* raw_array = sc_instance->get_field_array<float>(field.name, &size);
			if (ImGui::BeginTable("ArrayTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			{
				for (size_t i = 0; i < size; ++i)
				{
					std::string index_str = nps::formatter::format("[{0}]", i);
					
					BEGIN_COMPONENT_TABLE_ROW(true, index_str.c_str());
					
					if (ImGui::DragFloat(("##" + field.name + index_str).c_str(), raw_array + i))
						sc_instance->set_field_array_index(field.name, i, raw_array[i]);
					
					END_COMPONENT_TABLE_ROW(true);
				}
				ImGui::EndTable();
			}
		}

		END_COMPONENT_TABLE_ROW(in_table);
	}
	
	template <>
	void draw_field<script_field_type::Float, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		script_field_instance& sc_field = entity_fields.at(field.name);
	
		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		if (!field.is_array)
		{
			float data = sc_field.get_value<float>();
			if (ImGui::DragFloat(("##" + field.name).c_str(), &data))
				sc_field.set_value(data);
		}
		else
		{
			float* raw_array = sc_field.get_value_array<float>();
			if (ImGui::BeginTable("ArrayTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			{
				for (size_t i = 0; i < sc_field.get_array_size<float>(); ++i)
				{
					std::string index_str = nps::formatter::format("[{0}]", i);
					BEGIN_COMPONENT_TABLE_ROW(true, index_str.c_str());
					if (ImGui::DragFloat(("##" + field.name + index_str).c_str(), raw_array + i))
					{
						sc_field.set_value_array(raw_array, sc_field.get_array_size<float>());
					}
					END_COMPONENT_TABLE_ROW(true);
				}
				ImGui::EndTable();
			}
		}

		END_COMPONENT_TABLE_ROW(in_table);
	}
	
	template <>
	void draw_field<script_field_type::Float, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
		script_field_instance& sc_field = base_entity_fields.at(field.name);
	
		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		if (!field.is_array)
		{
			float data = sc_field.get_value<float>();
			if (ImGui::DragFloat(("##" + field.name).c_str(), &data))
			{
				script_field_instance& field_instance = entity_fields[field.name];
				field_instance.field = field;
				field_instance.set_value(data);
			}
		}
		else
		{
			float* raw_array = sc_field.get_value_array<float>();
			if (ImGui::BeginTable("ArrayTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			{
				for (size_t i = 0; i < sc_field.get_array_size<float>(); ++i)
				{
					std::string index_str = nps::formatter::format("[{0}]", i);
					BEGIN_COMPONENT_TABLE_ROW(true, index_str.c_str());
					if (ImGui::DragFloat(("##" + field.name + index_str).c_str(), raw_array + i))
					{
						script_field_instance& field_instance = entity_fields[field.name];
						field_instance.field = field;
						field_instance.set_value_array<float>(raw_array, sc_field.get_array_size<float>());
					}
					END_COMPONENT_TABLE_ROW(true);
				}
				ImGui::EndTable();
			}
		}

		END_COMPONENT_TABLE_ROW(in_table);
	}

	// ===================================================================================================
	// =============================================== Int ===============================================
	// ===================================================================================================

	template <>
	void draw_field<script_field_type::Int, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		int data = sc_instance->get_field_value<int32_t>(field.name);
		if (ImGui::InputInt(("##" + field.name).c_str(), &data))
			sc_instance->set_field_value(field.name, data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::Int, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		script_field_instance& sc_field = entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		int data = sc_field.get_value<int>();
		if (ImGui::InputInt(("##" + field.name).c_str(), &data))
			sc_field.set_value(data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::Int, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
		script_field_instance& sc_field = base_entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		int data = sc_field.get_value<int>();
		if (ImGui::InputInt(("##" + field.name).c_str(), &data))
		{
			script_field_instance& field_instance = entity_fields[field.name];
			field_instance.field = field;
			field_instance.set_value(data);
		}

		END_COMPONENT_TABLE_ROW(in_table);
	}

	// ===================================================================================================
	// =============================================== Bool ==============================================
	// ===================================================================================================

	template <>
	void draw_field<script_field_type::Bool, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		bool data = sc_instance->get_field_value<bool>(field.name);
		if (ImGui::Checkbox(("##" + field.name).c_str(), &data))
			sc_instance->set_field_value(field.name, data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::Bool, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		script_field_instance& sc_field = entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		bool data = sc_field.get_value<bool>();
		if (ImGui::Checkbox(("##" + field.name).c_str(), &data))
			sc_field.set_value(data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::Bool, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
		script_field_instance& sc_field = base_entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		bool data = sc_field.get_value<bool>();
		if (ImGui::Checkbox(("##" + field.name).c_str(), &data))
		{
			script_field_instance& field_instance = entity_fields[field.name];
			field_instance.field = field;
			field_instance.set_value(data);
		}

		END_COMPONENT_TABLE_ROW(in_table);
	}

	// ===================================================================================================
	// =============================================== Long ==============================================
	// ===================================================================================================

	template <>
	void draw_field<script_field_type::Long, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		int64_t data = sc_instance->get_field_value<int64_t>(field.name);
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_S64, &data))
			sc_instance->set_field_value(field.name, data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::Long, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		script_field_instance& sc_field = entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		int64_t data = sc_field.get_value<int64_t>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_S64, &data))
			sc_field.set_value(data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::Long, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
		script_field_instance& sc_field = base_entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		int64_t data = sc_field.get_value<int64_t>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_S64, &data))
		{
			script_field_instance& field_instance = entity_fields[field.name];
			field_instance.field = field;
			field_instance.set_value(data);
		}

		END_COMPONENT_TABLE_ROW(in_table);
	}

	// ===================================================================================================
	// ============================================= Vector2 =============================================
	// ===================================================================================================

	template <>
	void draw_field<script_field_type::Vector2, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		glm::vec2 data = sc_instance->get_field_value<glm::vec2>(field.name);
		if (UI::draw_field_vec2_control(field.name.c_str(), data, 0, ImGui::GetColumnWidth()))
			sc_instance->set_field_value(field.name, data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::Vector2, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		script_field_instance& sc_field = entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		glm::vec2 data = sc_field.get_value<glm::vec2>();
		if (UI::draw_field_vec2_control(field.name.c_str(), data, 0, ImGui::GetColumnWidth()))
			sc_field.set_value(data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::Vector2, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
		script_field_instance& sc_field = base_entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		glm::vec2 data = sc_field.get_value<glm::vec2>();
		if (UI::draw_field_vec2_control(field.name.c_str(), data, 0, ImGui::GetColumnWidth()))
		{
			script_field_instance& field_instance = entity_fields[field.name];
			field_instance.field = field;
			field_instance.set_value(data);
		}

		END_COMPONENT_TABLE_ROW(in_table);
	}

	// ===================================================================================================
	// ============================================= Vector3 =============================================
	// ===================================================================================================

	template <>
	void draw_field<script_field_type::Vector3, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		glm::vec3 data = sc_instance->get_field_value<glm::vec3>(field.name);
		if (UI::draw_field_vec3_control(field.name.c_str(), data, 0, ImGui::GetColumnWidth()))
			sc_instance->set_field_value(field.name, data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::Vector3, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		script_field_instance& sc_field = entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		glm::vec3 data = sc_field.get_value<glm::vec3>();
		if (UI::draw_field_vec3_control(field.name.c_str(), data, 0, ImGui::GetColumnWidth()))
			sc_field.set_value(data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::Vector3, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
		script_field_instance& sc_field = base_entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		glm::vec3 data = sc_field.get_value<glm::vec3>();
		if (UI::draw_field_vec3_control(field.name.c_str(), data, 0, ImGui::GetColumnWidth()))
		{
			script_field_instance& field_instance = entity_fields[field.name];
			field_instance.field = field;
			field_instance.set_value(data);
		}

		END_COMPONENT_TABLE_ROW(in_table);
	}

	// ===================================================================================================
	// ============================================= Vector4 =============================================
	// ===================================================================================================

	template <>
	void draw_field<script_field_type::Vector4, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		glm::vec4 data = sc_instance->get_field_value<glm::vec4>(field.name);
		if (ImGui::ColorEdit4(("##" + field.name).c_str(), glm::value_ptr(data)))
			sc_instance->set_field_value(field.name, data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::Vector4, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		script_field_instance& sc_field = entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		glm::vec4 data = sc_field.get_value<glm::vec4>();
		if (ImGui::ColorEdit4(("##" + field.name).c_str(), glm::value_ptr(data)))
			sc_field.set_value(data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::Vector4, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
		script_field_instance& sc_field = base_entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		glm::vec4 data = sc_field.get_value<glm::vec4>();
		if (ImGui::ColorEdit4(("##" + field.name).c_str(), glm::value_ptr(data)))
		{
			script_field_instance& field_instance = entity_fields[field.name];
			field_instance.field = field;
			field_instance.set_value(data);
		}

		END_COMPONENT_TABLE_ROW(in_table);
	}

	// ===================================================================================================
	// =============================================== Uint ==============================================
	// ===================================================================================================

	template <>
	void draw_field<script_field_type::UInt, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		uint32_t data = sc_instance->get_field_value<uint32_t>(field.name);
		if(ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_U32, &data))
			sc_instance->set_field_value(field.name, data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::UInt, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		script_field_instance& sc_field = entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		uint32_t data = sc_field.get_value<uint32_t>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_U32, &data))
			sc_field.set_value(data);

		END_COMPONENT_TABLE_ROW(in_table);
	}

	template <>
	void draw_field<script_field_type::UInt, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		auto& entity_fields = script_engine::get_script_field_map(ent);
		auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
		script_field_instance& sc_field = base_entity_fields.at(field.name);

		BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		uint32_t data = sc_field.get_value<uint32_t>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_U32, &data))
		{
			script_field_instance& field_instance = entity_fields[field.name];
			field_instance.field = field;
			field_instance.set_value(data);
		}

		END_COMPONENT_TABLE_ROW(in_table);
	}

	// ===================================================================================================
	// =============================================== Ulong =============================================
	// ===================================================================================================

	template <>
    void draw_field<script_field_type::ULong, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
    	uint64_t data = sc_instance->get_field_value<uint64_t>(field.name);
    	if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_U64, &data))
    		sc_instance->set_field_value(field.name, data);
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }
     
    template <>
    void draw_field<script_field_type::ULong, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	script_field_instance& sc_field = entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
    	uint64_t data = sc_field.get_value<uint64_t>();
    	if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_U64, &data))
    		sc_field.set_value(data);	
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::ULong, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
    	script_field_instance& sc_field = base_entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
    	uint64_t data = sc_field.get_value<uint64_t>();
    	if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_U64, &data))
    	{
    		script_field_instance& field_instance = entity_fields[field.name];
    		field_instance.field = field;
    		field_instance.set_value(data);
    	}
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }

	// ===================================================================================================
	// ============================================== Double =============================================
	// ===================================================================================================

	template <>
    void draw_field<script_field_type::Double, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
     
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
     
		double data = sc_instance->get_field_value<double>(field.name);
		if (ImGui::InputDouble(("##" + field.name).c_str(), &data))
			sc_instance->set_field_value(field.name, data);
     
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::Double, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	script_field_instance& sc_field = entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		double data = sc_field.get_value<double>();
		if (ImGui::InputDouble(("##" + field.name).c_str(), &data))
			sc_field.set_value(data);
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::Double, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
    	script_field_instance& sc_field = base_entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		double data = sc_field.get_value<double>();
		if (ImGui::InputDouble(("##" + field.name).c_str(), &data))
		{
			script_field_instance& field_instance = entity_fields[field.name];
			field_instance.field = field;
			field_instance.set_value(data);
		}
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }

	// ===================================================================================================
	// =============================================== Byte ==============================================
	// ===================================================================================================

	template <>
    void draw_field<script_field_type::Byte, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		uint8_t data = sc_instance->get_field_value<uint8_t>(field.name);
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_U8, &data))
			sc_instance->set_field_value(field.name, data);
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::Byte, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	script_field_instance& sc_field = entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		uint8_t data = sc_field.get_value<uint8_t>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_U8, &data))
			sc_field.set_value(data);
	
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::Byte, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
    	script_field_instance& sc_field = base_entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		uint8_t data = sc_field.get_value<uint8_t>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_U8, &data))
		{
			script_field_instance& field_instance = entity_fields[field.name];
			field_instance.field = field;
			field_instance.set_value(data);
		}
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }

	// ===================================================================================================
	// =============================================== SByte =============================================
	// ===================================================================================================

	template <>
    void draw_field<script_field_type::SByte, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		int8_t data = sc_instance->get_field_value<int8_t>(field.name);
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_S8, &data))
			sc_instance->set_field_value(field.name, data);
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::SByte, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	script_field_instance& sc_field = entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		int8_t data = sc_field.get_value<int8_t>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_S8, &data))
			sc_field.set_value(data);
	
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::SByte, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
    	script_field_instance& sc_field = base_entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		int8_t data = sc_field.get_value<int8_t>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_S8, &data))
		{
			script_field_instance& field_instance = entity_fields[field.name];
			field_instance.field = field;
			field_instance.set_value(data);
		}
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }
	// ===================================================================================================
	// =============================================== Char ==============================================
	// ===================================================================================================

	template <>
    void draw_field<script_field_type::Char, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		char data = sc_instance->get_field_value<char>(field.name);
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_S8, &data))
			sc_instance->set_field_value(field.name, data);
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::Char, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	script_field_instance& sc_field = entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		char data = sc_field.get_value<char>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_S8, &data))
			sc_field.set_value(data);
	
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::Char, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
    	script_field_instance& sc_field = base_entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		char data = sc_field.get_value<char>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_S8, &data))
		{
			script_field_instance& field_instance = entity_fields[field.name];
			field_instance.field = field;
			field_instance.set_value(data);
		}
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }
	// ===================================================================================================
	// =============================================== Short =============================================
	// ===================================================================================================

	template <>
    void draw_field<script_field_type::Short, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		short data = sc_instance->get_field_value<short>(field.name);
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_S16, &data))
			sc_instance->set_field_value(field.name, data);
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::Short, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	script_field_instance& sc_field = entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		short data = sc_field.get_value<short>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_S16, &data))
			sc_field.set_value(data);
	
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::Short, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
    	script_field_instance& sc_field = base_entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		short data = sc_field.get_value<short>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_S16, &data))
		{
			script_field_instance& field_instance = entity_fields[field.name];
			field_instance.field = field;
			field_instance.set_value(data);
		}
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }

	// ===================================================================================================
	// ============================================== UShort =============================================
	// ===================================================================================================

	template <>
    void draw_field<script_field_type::UShort, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		uint16_t data = sc_instance->get_field_value<uint16_t>(field.name);
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_U16, &data))
			sc_instance->set_field_value(field.name, data);
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::UShort, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	script_field_instance& sc_field = entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		uint16_t data = sc_field.get_value<uint16_t>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_U16, &data))
			sc_field.set_value(data);
	
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::UShort, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
    	script_field_instance& sc_field = base_entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		uint16_t data = sc_field.get_value<uint16_t>();
		if (ImGui::InputScalar(("##" + field.name).c_str(), ImGuiDataType_U16, &data))
		{
			script_field_instance& field_instance = entity_fields[field.name];
			field_instance.field = field;
			field_instance.set_value(data);
		}
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }

	// ===================================================================================================
	// ============================================== KeyCode ============================================
	// ===================================================================================================

	template <>
    void draw_field<script_field_type::KeyCode, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		key_code data = sc_instance->get_field_value<key_code>(field.name);

		if (ImGui::BeginCombo(field.name.c_str(), key::to_string(data)))
		{
			for (key_code i = static_cast<key_code>(key::space); i <= static_cast<key_code>(key::menu); ++i)
			{
				const key_code current_key = i;
				const bool is_selected = (data == current_key);
				std::string_view sv = key::to_string(current_key);
				if (sv == "unknown")
					continue;

				if (ImGui::Selectable(sv.data(), is_selected))
				{
					data = current_key;
					sc_instance->set_field_value<key_code>(field.name, data);
				}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::KeyCode, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	script_field_instance& sc_field = entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		key_code data = sc_field.get_value<key_code>();

		if (ImGui::BeginCombo(field.name.c_str(), key::to_string(data)))
		{
			for (key_code i = static_cast<key_code>(key::space); i <= static_cast<key_code>(key::menu); ++i)
			{
				key_code current_key = i;
				bool is_selected = (data == current_key);
				std::string_view sv = key::to_string(current_key);
				if (sv == "unknown")
					continue;

				if (ImGui::Selectable(sv.data(), is_selected))
				{
					sc_field.set_value<key_code>(current_key);
				}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
	
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::KeyCode, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
    	script_field_instance& sc_field = base_entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		key_code data = sc_field.get_value<key_code>();

		if (ImGui::BeginCombo(field.name.c_str(), key::to_string(data)))
		{
			for (key_code i = static_cast<key_code>(key::space); i <= static_cast<key_code>(key::menu); ++i)
			{
				key_code current_key = i;
				bool is_selected = (data == current_key);
				std::string_view sv = key::to_string(current_key);
				if (sv == "unknown")
					continue;

				if (ImGui::Selectable(sv.data(), is_selected))
				{
					script_field_instance& field_instance = entity_fields[field.name];
					field_instance.field = field;
					field_instance.set_value(current_key);
				}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
    	END_COMPONENT_TABLE_ROW(in_table);
    }

	// ===================================================================================================
	// ============================================= MouseCode ===========================================
	// ===================================================================================================

	template <>
    void draw_field<script_field_type::MouseCode, UI::script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		mouse_code data = sc_instance->get_field_value<mouse_code>(field.name);

		if (ImGui::BeginCombo(field.name.c_str(), mouse::to_string(data)))
		{
			for (mouse_code i = static_cast<mouse_code>(mouse::button0); i <= static_cast<mouse_code>(mouse::button_last); ++i)
			{
				mouse_code current_button = i;
				bool is_selected = (data == current_button);
				std::string_view sv = mouse::to_string(current_button);
				if (sv == "unknown")
					continue;

				if (ImGui::Selectable(sv.data(), is_selected))
				{
					sc_instance->set_field_value<mouse_code>(field.name, current_button);
				}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
    
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::MouseCode, UI::script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	script_field_instance& sc_field = entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());
    
		mouse_code data = sc_field.get_value<mouse_code>();

		if (ImGui::BeginCombo(field.name.c_str(), mouse::to_string(data)))
		{
			for (mouse_code i = static_cast<mouse_code>(mouse::button0); i <= static_cast<mouse_code>(mouse::button_last); ++i)
			{
				mouse_code current_button = i;
				bool is_selected = (data == current_button);
				std::string_view sv = mouse::to_string(current_button);
				if (sv == "unknown")
					continue;

				if (ImGui::Selectable(sv.data(), is_selected))
				{
					sc_field.set_value<key_code>(current_button);
				}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
	
    	END_COMPONENT_TABLE_ROW(in_table);
    }
    
    template <>
    void draw_field<script_field_type::MouseCode, UI::script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
    {
    	auto& entity_fields = script_engine::get_script_field_map(ent);
    	auto& base_entity_fields = script_engine::get_base_script_field_map(class_name);
    	script_field_instance& sc_field = base_entity_fields.at(field.name);
    
    	BEGIN_COMPONENT_TABLE_ROW(in_table, field.name.c_str());

		mouse_code data = sc_field.get_value<mouse_code>();

		if (ImGui::BeginCombo(field.name.c_str(), mouse::to_string(data)))
		{
			for (mouse_code i = static_cast<mouse_code>(mouse::button0); i <= static_cast<mouse_code>(mouse::button_last); ++i)
			{
				mouse_code current_button = i;
				bool is_selected = (data == current_button);
				std::string_view sv = mouse::to_string(current_button);
				if (sv == "unknown")
					continue;

				if (ImGui::Selectable(sv.data(), is_selected))
				{
					script_field_instance& field_instance = entity_fields[field.name];
					field_instance.field = field;
					field_instance.set_value(current_button);
				}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
    	END_COMPONENT_TABLE_ROW(in_table);
    }
}

_WHIP_END
