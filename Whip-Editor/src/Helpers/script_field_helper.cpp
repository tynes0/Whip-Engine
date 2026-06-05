#include "script_field_helper.h"

#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/MouseButtonCodes.h>
#include <Whip/Project/project.h>
#include <Whip/UI/UI_helpers.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

_WHIP_START

namespace
{
	constexpr const char* scene_entity_payload_type = "WHIP_SCENE_ENTITY";

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

	size_t sanitize_array_size(int size)
	{
		if (size <= 0)
			return 0;

		return static_cast<size_t>(size);
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

	std::string entity_reference_label(entity context, UUID entity_id)
	{
		if (entity_id == 0)
			return "None";

		scene* scene_context = context.get_scene();
		if (!scene_context)
			return "Missing Entity";

		entity referenced_entity = scene_context->find_entity_by_UUID(entity_id);
		if (!referenced_entity)
			return "Missing Entity";

		return referenced_entity.get_name();
	}

	bool draw_string_control(const char* id, std::string& value)
	{
		return ImGui::InputText(id, &value);
	}

	bool draw_entity_control(const char* id, UUID& value, entity context)
	{
		bool changed = false;
		const std::string label = entity_reference_label(context, value);

		ImGui::PushID(id);

		const float clear_button_width = ImGui::GetFrameHeight();
		const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
		const float width = ImGui::GetContentRegionAvail().x;
		const float picker_width = value == 0 ? width : std::max(0.0f, width - clear_button_width - spacing);

		ImGui::Button(label.c_str(), ImVec2(picker_width, 0.0f));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(scene_entity_payload_type))
			{
				WHP_CORE_ASSERT(payload->DataSize == sizeof(UUID), "Invalid entity drag payload size!");
				value = *static_cast<const UUID*>(payload->Data);
				changed = true;
			}
			ImGui::EndDragDropTarget();
		}

		if (value != 0)
		{
			ImGui::SameLine();
			if (ImGui::Button("X", ImVec2(clear_button_width, 0.0f)))
			{
				value = UUID(0);
				changed = true;
			}
		}

		ImGui::PopID();
		return changed;
	}

	struct scene_picker_item
	{
		asset_handle handle = 0;
		std::filesystem::path path;
		std::string label;
	};

	std::vector<scene_picker_item> collect_scene_picker_items()
	{
		std::vector<scene_picker_item> items;
		ref<project> active_project = project::get_active();
		if (!active_project || !active_project->get_editor_asset_manager())
			return items;

		active_project->get_editor_asset_manager()->get_asset_registry().foreach(asset_type::scene, [&](const asset_registry::value_type& value)
			{
				scene_picker_item item;
				item.handle = value.first;
				item.path = value.second.filepath;
				item.label = value.second.filepath.stem().string();
				if (item.label.empty())
					item.label = value.second.filepath.filename().string();
				items.push_back(std::move(item));
			});

		std::sort(items.begin(), items.end(), [](const scene_picker_item& lhs, const scene_picker_item& rhs)
			{
				return lhs.path.generic_string() < rhs.path.generic_string();
			});

		return items;
	}

	std::string scene_reference_label(asset_handle handle)
	{
		if (handle == 0)
			return "None";

		ref<project> active_project = project::get_active();
		if (!active_project || !active_project->get_editor_asset_manager() ||
			!active_project->get_editor_asset_manager()->is_asset_handle_valid(handle) ||
			active_project->get_editor_asset_manager()->get_asset_type(handle) != asset_type::scene)
			return "Missing Scene";

		const std::filesystem::path& path = active_project->get_editor_asset_manager()->get_filepath(handle);
		std::string label = path.stem().string();
		if (label.empty())
			label = path.filename().string();
		return label;
	}

	std::string to_lower(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	}

	bool scene_matches_query(const scene_picker_item& scene, const std::string& query)
	{
		if (query.empty())
			return true;

		const std::string lowered_query = to_lower(query);
		return to_lower(scene.label).find(lowered_query) != std::string::npos ||
			to_lower(scene.path.generic_string()).find(lowered_query) != std::string::npos;
	}

	bool draw_scene_control(const char* id, uint64_t& value)
	{
		static std::string scene_search_query;
		bool changed = false;
		asset_handle current_handle(value);
		std::string label = scene_reference_label(current_handle);

		ImGui::PushID(id);

		const float clear_button_width = ImGui::GetFrameHeight();
		const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
		const float width = ImGui::GetContentRegionAvail().x;
		const float picker_width = current_handle == 0 ? width : std::max(0.0f, width - clear_button_width - spacing);

		if (ImGui::Button(label.c_str(), ImVec2(picker_width, 0.0f)))
			ImGui::OpenPopup("ScenePickerPopup");

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				asset_handle dropped_handle = *static_cast<asset_handle*>(payload->Data);
				ref<project> active_project = project::get_active();
				if (active_project && active_project->get_editor_asset_manager() &&
					active_project->get_editor_asset_manager()->is_asset_handle_valid(dropped_handle) &&
					active_project->get_editor_asset_manager()->get_asset_type(dropped_handle) == asset_type::scene)
				{
					value = dropped_handle;
					changed = true;
				}
				else
				{
					WHP_CORE_WARN("[Asset Manager] Wrong asset type!");
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (current_handle != 0)
		{
			ImGui::SameLine();
			if (ImGui::Button("X", ImVec2(clear_button_width, 0.0f)))
			{
				value = 0;
				changed = true;
			}
		}

		if (ImGui::BeginPopup("ScenePickerPopup"))
		{
			ImGui::SetNextItemWidth(260.0f);
			ImGui::InputTextWithHint("##SceneSearch", "Search scenes", &scene_search_query);
			ImGui::Separator();

			if (ImGui::Selectable("None", current_handle == 0))
			{
				value = 0;
				changed = true;
			}

			const std::vector<scene_picker_item> scenes = collect_scene_picker_items();
			if (!scenes.empty())
				ImGui::Separator();

			size_t visible_count = 0;
			for (const scene_picker_item& scene : scenes)
			{
				if (!scene_matches_query(scene, scene_search_query))
					continue;

				++visible_count;
				ImGui::PushID(static_cast<int>((uint64_t)scene.handle & 0xffffffffu));
				const bool selected = scene.handle == current_handle;
				if (ImGui::Selectable(scene.label.c_str(), selected))
				{
					value = scene.handle;
					changed = true;
				}

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", scene.path.generic_string().c_str());

				if (selected)
					ImGui::SetItemDefaultFocus();
				ImGui::PopID();
			}

			if (visible_count == 0 && !scene_search_query.empty())
				ImGui::TextDisabled("No matching scenes");

			ImGui::EndPopup();
		}

		ImGui::PopID();
		return changed;
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

	void draw_unsupported_array(const script_field& field, bool in_table)
	{
		table_row_scope row(in_table, field.name.c_str());
		ImGui::TextDisabled("Unsupported array");
	}

	template <UI::script_field_draw DrawMode>
	void draw_string_field(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		if (field.is_array)
		{
			draw_unsupported_array(field, in_table);
			return;
		}

		table_row_scope row(in_table, field.name.c_str());
		const std::string id = control_label(field, in_table);

		if constexpr (DrawMode == UI::script_field_draw::while_scene_running)
		{
			(void)class_name;
			ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
			if (!sc_instance)
				return;

			std::string data = sc_instance->get_field_string(field.name);
			if (draw_string_control(id.c_str(), data))
				sc_instance->set_field_string(field.name, data);
		}
		else if constexpr (DrawMode == UI::script_field_draw::set_in_the_editor)
		{
			(void)class_name;
			script_field_instance& sc_field = editor_field(ent, field);
			std::string data = sc_field.get_string_value();
			if (draw_string_control(id.c_str(), data))
				sc_field.set_string_value(data);
		}
		else
		{
			script_field_instance& sc_field = base_field(class_name, field);
			std::string data = sc_field.get_string_value();
			if (draw_string_control(id.c_str(), data))
				override_field(ent, field).set_string_value(data);
		}
	}

	template <UI::script_field_draw DrawMode>
	void draw_entity_field(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		if (field.is_array)
		{
			draw_unsupported_array(field, in_table);
			return;
		}

		table_row_scope row(in_table, field.name.c_str());
		const std::string id = control_label(field, in_table);

		if constexpr (DrawMode == UI::script_field_draw::while_scene_running)
		{
			(void)class_name;
			ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
			if (!sc_instance)
				return;

			UUID data = sc_instance->get_field_entity(field.name);
			if (draw_entity_control(id.c_str(), data, ent))
				sc_instance->set_field_entity(field.name, data);
		}
		else if constexpr (DrawMode == UI::script_field_draw::set_in_the_editor)
		{
			(void)class_name;
			script_field_instance& sc_field = editor_field(ent, field);
			UUID data = sc_field.get_entity_value();
			if (draw_entity_control(id.c_str(), data, ent))
				sc_field.set_entity_value(data);
		}
		else
		{
			script_field_instance& sc_field = base_field(class_name, field);
			UUID data = sc_field.get_entity_value();
			if (draw_entity_control(id.c_str(), data, ent))
				override_field(ent, field).set_entity_value(data);
		}
	}

	template <UI::script_field_draw DrawMode>
	void draw_scene_field(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		if (field.is_array)
		{
			draw_unsupported_array(field, in_table);
			return;
		}

		table_row_scope row(in_table, field.name.c_str());
		const std::string id = control_label(field, in_table);

		if constexpr (DrawMode == UI::script_field_draw::while_scene_running)
		{
			(void)class_name;
			ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
			if (!sc_instance)
				return;

			uint64_t data = sc_instance->get_field_value<uint64_t>(field.name);
			if (draw_scene_control(id.c_str(), data))
				sc_instance->set_field_value<uint64_t>(field.name, data);
		}
		else if constexpr (DrawMode == UI::script_field_draw::set_in_the_editor)
		{
			(void)class_name;
			script_field_instance& sc_field = editor_field(ent, field);
			uint64_t data = sc_field.get_value<uint64_t>();
			if (draw_scene_control(id.c_str(), data))
				sc_field.set_value<uint64_t>(data);
		}
		else
		{
			script_field_instance& sc_field = base_field(class_name, field);
			uint64_t data = sc_field.get_value<uint64_t>();
			if (draw_scene_control(id.c_str(), data))
				override_field(ent, field).set_value<uint64_t>(data);
		}
	}

	template <typename OnResize>
	bool draw_array_size_control(const script_field& field, size_t size, bool allow_resize, OnResize on_resize)
	{
		bool resized = false;
		int size_value = size > static_cast<size_t>(std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max() : static_cast<int>(size);

		ImGui::PushID(field.name.c_str());
		ImGui::SetNextItemWidth(96.0f);

		if (!allow_resize)
			ImGui::BeginDisabled();

		if (ImGui::InputInt("Size", &size_value))
		{
			const size_t requested_size = sanitize_array_size(size_value);
			if (requested_size != size)
			{
				on_resize(requested_size);
				resized = true;
			}
		}

		if (!allow_resize)
			ImGui::EndDisabled();

		if (allow_resize)
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("+"))
			{
				on_resize(size + 1);
				resized = true;
			}

			ImGui::SameLine();
			if (size == 0)
				ImGui::BeginDisabled();

			if (ImGui::SmallButton("Clear"))
			{
				on_resize(0);
				resized = true;
			}

			if (size == 0)
				ImGui::EndDisabled();
		}

		ImGui::PopID();
		return resized;
	}

	template <typename T, typename DrawFn, typename OnChange, typename OnRemove>
	bool draw_array_table(const script_field& field, T* values, size_t size, bool allow_remove, DrawFn draw, OnChange on_change, OnRemove on_remove)
	{
		if (size == 0)
		{
			ImGui::TextDisabled("Empty");
			return false;
		}

		if (!values && size > 0)
		{
			ImGui::TextDisabled("Unavailable");
			return false;
		}

		const std::string table_id = array_table_id(field);
		const int column_count = allow_remove ? 3 : 2;
		if (!ImGui::BeginTable(table_id.c_str(), column_count, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			return false;

		ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 48.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		if (allow_remove)
			ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, 32.0f);

		size_t remove_index = static_cast<size_t>(-1);

		for (size_t i = 0; i < size; ++i)
		{
			const std::string row_label = array_row_label(i);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(row_label.c_str());

			ImGui::TableNextColumn();
			ImGui::PushItemWidth(-1.0f);
			const std::string id = array_control_label(field, row_label);
			if (draw(id.c_str(), values[i]))
				on_change(i);
			ImGui::PopItemWidth();

			if (allow_remove)
			{
				ImGui::TableNextColumn();
				ImGui::PushID(static_cast<int>(i));
				if (ImGui::SmallButton("X"))
					remove_index = i;
				ImGui::PopID();
			}
		}

		ImGui::EndTable();

		if (remove_index != static_cast<size_t>(-1))
		{
			on_remove(remove_index);
			return true;
		}

		return false;
	}

	template <typename T>
	std::unique_ptr<T[]> copy_array_values(T* source, size_t size)
	{
		if (size == 0)
			return nullptr;

		auto values = std::make_unique<T[]>(size);
		if (source)
		{
			for (size_t i = 0; i < size; ++i)
				values[i] = source[i];
		}
		return values;
	}

	template <typename T>
	std::unique_ptr<T[]> resize_array_values(T* source, size_t old_size, size_t new_size)
	{
		if (new_size == 0)
			return nullptr;

		auto values = std::make_unique<T[]>(new_size);
		const size_t copy_size = old_size < new_size ? old_size : new_size;
		if (source)
		{
			for (size_t i = 0; i < copy_size; ++i)
				values[i] = source[i];
		}
		return values;
	}

	template <typename T>
	std::unique_ptr<T[]> remove_array_value(T* source, size_t size, size_t remove_index)
	{
		if (size <= 1)
			return nullptr;

		auto values = std::make_unique<T[]>(size - 1);
		size_t target_index = 0;
		for (size_t i = 0; i < size; ++i)
		{
			if (i == remove_index)
				continue;

			values[target_index++] = source ? source[i] : T{};
		}
		return values;
	}

	template <typename T, typename DrawFn, typename OnChange, typename OnResize, typename OnRemove>
	void draw_array_editor(const script_field& field, T* values, size_t size, bool allow_resize, DrawFn draw, OnChange on_change, OnResize on_resize, OnRemove on_remove)
	{
		if (draw_array_size_control(field, size, allow_resize, on_resize))
			return;

		draw_array_table(field, values, size, allow_resize, draw, on_change, on_remove);
	}

	template <UI::script_field_draw DrawMode, typename T, typename DrawFn>
	void draw_array_contents(const script_field& field, entity ent, const std::string& class_name, DrawFn draw)
	{
		if constexpr (DrawMode == UI::script_field_draw::while_scene_running)
		{
			(void)class_name;
			ref<script_instance> sc_instance = script_engine::get_entity_script_instance(ent.get_UUID());
			if (!sc_instance)
				return;

			size_t size = 0;
			T* raw_array = sc_instance->get_field_array<T>(field.name, &size);
			draw_array_editor(field, raw_array, size, false, draw, [&](size_t index)
				{
					sc_instance->set_field_array_index(field.name, index, raw_array[index]);
				}, [&](size_t)
				{
				}, [&](size_t)
				{
				});
		}
		else if constexpr (DrawMode == UI::script_field_draw::set_in_the_editor)
		{
			(void)class_name;
			script_field_instance& sc_field = editor_field(ent, field);
			const size_t size = sc_field.get_array_size<T>();
			T* raw_array = sc_field.get_value_array<T>();

			draw_array_editor(field, raw_array, size, true, draw, [&](size_t)
				{
					sc_field.set_value_array<T>(raw_array, size);
				}, [&](size_t new_size)
				{
					auto values = resize_array_values<T>(raw_array, size, new_size);
					sc_field.set_value_array<T>(values.get(), new_size);
				}, [&](size_t remove_index)
				{
					const size_t new_size = size > 0 ? size - 1 : 0;
					auto values = remove_array_value<T>(raw_array, size, remove_index);
					sc_field.set_value_array<T>(values.get(), new_size);
				});
		}
		else
		{
			script_field_instance& sc_field = base_field(class_name, field);
			const size_t size = sc_field.get_array_size<T>();
			T* raw_array = sc_field.get_value_array<T>();
			auto values = copy_array_values<T>(raw_array, size);

			draw_array_editor(field, values.get(), size, true, draw, [&](size_t)
				{
					override_field(ent, field).set_value_array<T>(values.get(), size);
				}, [&](size_t new_size)
				{
					auto resized_values = resize_array_values<T>(raw_array, size, new_size);
					override_field(ent, field).set_value_array<T>(resized_values.get(), new_size);
				}, [&](size_t remove_index)
				{
					const size_t new_size = size > 0 ? size - 1 : 0;
					auto resized_values = remove_array_value<T>(raw_array, size, remove_index);
					override_field(ent, field).set_value_array<T>(resized_values.get(), new_size);
				});
		}
	}

	template <UI::script_field_draw DrawMode, typename T, typename DrawFn>
	void draw_script_field(const script_field& field, entity ent, const std::string& class_name, bool in_table, DrawFn draw)
	{
		if (!field.is_array)
		{
			draw_value<DrawMode, T>(field, ent, class_name, in_table, draw);
			return;
		}

		table_row_scope row(in_table, field.name.c_str());
		draw_array_contents<DrawMode, T>(field, ent, class_name, draw);
	}
}

namespace UI
{
#define WHIP_DEFINE_SCRIPT_FIELD(SCRIPT_TYPE, VALUE_TYPE, DRAW_FUNC) \
	template <> \
	void draw_field<script_field_type::SCRIPT_TYPE, script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table) \
	{ \
		draw_script_field<script_field_draw::while_scene_running, VALUE_TYPE>(field, ent, class_name, in_table, DRAW_FUNC); \
	} \
	template <> \
	void draw_field<script_field_type::SCRIPT_TYPE, script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table) \
	{ \
		draw_script_field<script_field_draw::set_in_the_editor, VALUE_TYPE>(field, ent, class_name, in_table, DRAW_FUNC); \
	} \
	template <> \
	void draw_field<script_field_type::SCRIPT_TYPE, script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table) \
	{ \
		draw_script_field<script_field_draw::with_base_value, VALUE_TYPE>(field, ent, class_name, in_table, DRAW_FUNC); \
	}

	WHIP_DEFINE_SCRIPT_FIELD(Float, float, draw_float_control)
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

	template <>
	void draw_field<script_field_type::String, script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		draw_string_field<script_field_draw::while_scene_running>(field, ent, class_name, in_table);
	}

	template <>
	void draw_field<script_field_type::String, script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		draw_string_field<script_field_draw::set_in_the_editor>(field, ent, class_name, in_table);
	}

	template <>
	void draw_field<script_field_type::String, script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		draw_string_field<script_field_draw::with_base_value>(field, ent, class_name, in_table);
	}

	template <>
	void draw_field<script_field_type::Entity, script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		draw_entity_field<script_field_draw::while_scene_running>(field, ent, class_name, in_table);
	}

	template <>
	void draw_field<script_field_type::Entity, script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		draw_entity_field<script_field_draw::set_in_the_editor>(field, ent, class_name, in_table);
	}

	template <>
	void draw_field<script_field_type::Entity, script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		draw_entity_field<script_field_draw::with_base_value>(field, ent, class_name, in_table);
	}

	template <>
	void draw_field<script_field_type::Scene, script_field_draw::while_scene_running>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		draw_scene_field<script_field_draw::while_scene_running>(field, ent, class_name, in_table);
	}

	template <>
	void draw_field<script_field_type::Scene, script_field_draw::set_in_the_editor>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		draw_scene_field<script_field_draw::set_in_the_editor>(field, ent, class_name, in_table);
	}

	template <>
	void draw_field<script_field_type::Scene, script_field_draw::with_base_value>(const script_field& field, entity ent, const std::string& class_name, bool in_table)
	{
		draw_scene_field<script_field_draw::with_base_value>(field, ent, class_name, in_table);
	}

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
		case script_field_type::String: draw_field<script_field_type::String, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::Entity: draw_field<script_field_type::Entity, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::Scene: draw_field<script_field_type::Scene, DrawMode>(field, ent, class_name, in_table); break;
		case script_field_type::None:
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
