#include "scene_hierarchy_panel.h"

#include <Whip/Core/Input.h>
#include <Whip/Scene/components.h>
#include <Whip/UI/UI_helpers.h>
#include <Whip/UI/UI_scoped_style.h>
#include <Whip/Scripting/script_engine.h>
#include <Whip/Utils/platform_utils.h>
#include <Whip/Project/project.h>
#include <Whip/Asset/asset_manager.h>
#include <Whip/Asset/asset_metadata.h>
#include <Whip/Asset/texture_importer.h>
#include <Whip/Audio/audio_source.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <cstring>

#define BEGIN_COMPONENT_TABLE_ROW(...) do { ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text(__VA_ARGS__); ImGui::TableNextColumn(); ImGui::PushItemWidth(-1); } while(false)
#define END_COMPONENT_TABLE_ROW() do { ImGui::PopItemWidth(); } while(false)

_WHIP_START

static audio_component::audio_data* find_ac_AD(std::vector<audio_component::audio_data>&handle_list, const std::string & tag)
{
	for (audio_component::audio_data& handle : handle_list)
		if (handle.tag == tag)
			return &handle;
	return nullptr;
}

template<typename T, typename UIFunction>
static void draw_component(const std::string& name, entity entity_in, UIFunction uiFunction)
{
	constexpr ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
	if (entity_in.has_component<T>())
	{
		auto& component = entity_in.get_component<T>();
		ImVec2 content_region_available = ImGui::GetContentRegionAvail();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 3, 3 });
		float line_height = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImGui::Separator();
		bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), tree_node_flags, name.c_str());
		ImGui::PopStyleVar();
		ImGui::SameLine(content_region_available.x - line_height * 0.5f);
		if (ImGui::Button("+", ImVec2{ line_height, line_height }))
			ImGui::OpenPopup("Component Settings");

		bool remove_component = false;
		if (ImGui::BeginPopup("Component Settings"))
		{
			if constexpr (!std::is_same_v<T, transform_component>)
				if (ImGui::MenuItem("Remove component"))
					remove_component = true;

			ImGui::EndPopup();
		}

		if (open)
		{
			uiFunction(component);
			ImGui::TreePop();
		}

		if (remove_component)
			entity_in.remove_component<T>();
	}
}

scene_hierarchy_panel::scene_hierarchy_panel(const ref<scene> context)
{
	set_context(context);
}

void scene_hierarchy_panel::set_context(const ref<scene>& context)
{
	m_context = context;
	m_selection_context = {};
}

void scene_hierarchy_panel::on_imgui_render()
{
	ImGui::Begin("Scene Hierarchy");

	if (m_context)
	{
		auto group = m_context->m_registry.group<>(entt::get<ID_component>);

		for (auto entityID : group)
		{
			entity ent{ entityID , m_context.get() };
			draw_entity_node(ent);
		}

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
			m_selection_context = {};

		if (ImGui::BeginPopupContextWindow(0, 1 | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Entity"))
				m_context->create_entity("New Entity");

			ImGui::EndPopup();
		}
	}

	ImGui::End();

	ImGui::Begin("Properties");
	if (m_selection_context)
		draw_components(m_selection_context);

	ImGui::End();
}

void scene_hierarchy_panel::set_selected_entity(entity entity_in)
{
	m_selection_context = entity_in;
}

void scene_hierarchy_panel::draw_entity_node(entity entity_in)
{
	if (entity_in.has_component<tag_component>())
	{
		auto& tag = entity_in.get_component<tag_component>().tag;

		ImGuiTreeNodeFlags flags = ((m_selection_context == entity_in) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity_in, flags, tag.c_str());
		if (opened)
			ImGui::TreePop();

		if (ImGui::IsItemClicked())
			m_selection_context = entity_in;

		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Delete Entity"))
				entityDeleted = true;

			ImGui::EndPopup();
		}


		if (entityDeleted)
		{
			m_context->destroy_entity(entity_in);
			if (m_selection_context == entity_in)
				m_selection_context = {};
		}
	}
}

void scene_hierarchy_panel::draw_components(entity entity_in)
{
	if (entity_in.has_component<tag_component>())
	{
		auto& tag = entity_in.get_component<tag_component>().tag;

		char buffer[256];
		memset(buffer, 0, sizeof(buffer));
		strncpy_s(buffer, sizeof(buffer), tag.c_str(), sizeof(buffer));
		if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
		{
			m_context->m_unique_name_manager.remove_name(tag);
			tag = m_context->m_unique_name_manager.add_name(buffer);
		}
	}

	ImGui::SameLine();
	ImGui::PushItemWidth(-1);

	if (ImGui::Button("Add Component"))
		ImGui::OpenPopup("Add Component");

	if (ImGui::BeginPopup("Add Component"))
	{
		display_add_component_entry<camera_component>("Camera");
		display_add_component_entry<script_component>("Script");
		display_add_component_entry<sprite_renderer_component>("Sprite Renderer");
		display_add_component_entry<circle_renderer_component>("Circle Renderer");
		display_add_component_entry<text_component>("Text");
		display_add_component_entry<rigidbody2D_component>("Rigidbody 2D");
		display_add_component_entry<box_collider2D_component>("Box Collider 2D");
		display_add_component_entry<circle_collider2D_component>("Circle Collider 2D");
		display_add_component_entry<audio_component>("Audio");
		ImGui::EndPopup();
	}

	ImGui::PopItemWidth();

	
	//ImGui::Spacing();
	draw_component<transform_component>("Transform", entity_in, [](auto& component)
		{
			float spacing = ImGui::GetStyle().IndentSpacing;
			UI::draw_vec3_control("Translation", component.translation, 0, 100, spacing);
			glm::vec3 rotation = glm::degrees(component.rotation);
			UI::draw_vec3_control("Rotation", rotation, 0, 100, spacing);
			component.rotation = glm::radians(rotation);
			UI::draw_vec3_control("Scale", component.scale, 1.0f, 100, spacing);
		});
	ImGui::Spacing();
	draw_component<camera_component>("Camera", entity_in, [](auto& component)
		{
			auto& camera = component.camera;

			ImGui::Checkbox("Primary", &component.primary);

			const char* projection_type_strings[] = { "Perspective", "Orthographic" };
			const char* current_projection_type_string = projection_type_strings[(int)camera.get_projection_type()];
			if (ImGui::BeginCombo("Projection", current_projection_type_string))
			{
				for (int i = 0; i < 2; i++)
				{
					bool is_selected = current_projection_type_string == projection_type_strings[i];
					if (ImGui::Selectable(projection_type_strings[i], is_selected))
					{
						current_projection_type_string = projection_type_strings[i];
						camera.set_projection_type((scene_camera::projection_type)i);
					}

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			if (camera.get_projection_type() == scene_camera::projection_type::perspective)
			{
				float perspective_vertical_fov = glm::degrees(camera.get_perspective_vertical_FOV());
				if (ImGui::DragFloat("Vertical FOV", &perspective_vertical_fov))
					camera.set_perspective_vertical_FOV(glm::radians(perspective_vertical_fov));

				float perspective_near = camera.get_perspective_near_clip();
				if (ImGui::DragFloat("Near", &perspective_near))
					camera.set_perspective_near_clip(perspective_near);

				float perspective_far = camera.get_perspective_far_clip();
				if (ImGui::DragFloat("Far", &perspective_far))
					camera.set_perspective_far_clip(perspective_far);
			}

			if (camera.get_projection_type() == scene_camera::projection_type::orthographic)
			{
				float ortho_size = camera.get_orthographic_size();
				if (ImGui::DragFloat("Size", &ortho_size))
					camera.set_orthographic_size(ortho_size);

				float ortho_near = camera.get_orthographic_near_clip();
				if (ImGui::DragFloat("Near", &ortho_near))
					camera.set_orthographic_near_clip(ortho_near);

				float ortho_far = camera.get_orthographic_far_clip();
				if (ImGui::DragFloat("Far", &ortho_far))
					camera.set_orthographic_far_clip(ortho_far);

				ImGui::Checkbox("Fixed Aspect Ratio", &component.fixed_aspect_ratio);
			}
		});
	ImGui::Spacing();
	draw_component<script_component>("Script", entity_in, [entity_in, scene_in = m_context](auto& component) mutable
		{
			bool script_class_exists = script_engine::entity_class_exists(component.class_name);
			auto entity_classes = script_engine::get_entity_classes();
			UI::scoped_style_color scope_color(ImGuiCol_Text, ImVec4(0.8f, 0.3f, 0.3f, 1.0f), !script_class_exists);
			if (ImGui::BeginTable("ScriptTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			{
				BEGIN_COMPONENT_TABLE_ROW("Class");
				if (ImGui::BeginCombo("Class", component.class_name.c_str()))
				{
					for (const auto& [first, second] : entity_classes)
					{
						bool is_selected = component.class_name == first;

						if (ImGui::Selectable(first.c_str(), is_selected))
						{
							component.class_name = first.c_str();
						}

						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}

					ImGui::EndCombo();
				}
				END_COMPONENT_TABLE_ROW();
				ImGui::Separator();
				bool scene_running = scene_in->is_running();
				if (scene_running)
				{
					ref<script_instance> sc_instance = script_engine::get_entity_script_instance(entity_in.get_UUID());
					if (sc_instance)
					{
						const auto& fields = sc_instance->get_script_class()->get_fields();
						for (const auto& [name, field] : fields)
						{
							switch (field.type)
							{
							case whip::script_field_type::Float:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								float data = sc_instance->get_field_value<float>(name);
								if (ImGui::DragFloat(("##" + name).c_str(), &data))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::Int:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								int data = sc_instance->get_field_value<int>(name);
								if (ImGui::InputInt(("##" + name).c_str(), &data))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::Bool:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								bool data = sc_instance->get_field_value<bool>(name);
								if (ImGui::Checkbox(("##" + name).c_str(), &data))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::Long:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								int64_t data = sc_instance->get_field_value<int64_t>(name);
								if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_S64, &data))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::Vector3:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								glm::vec3 data = sc_instance->get_field_value<glm::vec3>(name);
								if (UI::draw_field_vec3_control(name.c_str(), data, 0, ImGui::GetColumnWidth())) 
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::Vector2:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								glm::vec2 data = sc_instance->get_field_value<glm::vec2>(name);
								if (UI::draw_field_vec2_control(name.c_str(), data, 0, ImGui::GetColumnWidth()))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::Vector4:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								glm::vec4 data = sc_instance->get_field_value<glm::vec4>(name);
								if (ImGui::ColorEdit4(("##" + name).c_str(), glm::value_ptr(data)))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::UInt:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								uint32_t data = sc_instance->get_field_value<uint32_t>(name);
								if(ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_U32, &data))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::ULong:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								uint64_t data = sc_instance->get_field_value<uint64_t>(name);
								if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_U64, &data))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::Double:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								double data = sc_instance->get_field_value<double>(name);
								if (ImGui::InputDouble(("##" + name).c_str(), &data))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::Byte:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								uint8_t data = sc_instance->get_field_value<uint8_t>(name);
								if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_U8, &data))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::SByte:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								int8_t data = sc_instance->get_field_value<int8_t>(name);
								if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_S8, &data))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::Char:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								char data = sc_instance->get_field_value<char>(name);
								if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_S8, &data))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::Short:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								short data = sc_instance->get_field_value<short>(name);
								if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_S16, &data))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::UShort:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());
								uint16_t data = sc_instance->get_field_value<uint16_t>(name);
								if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_U16, &data))
									sc_instance->set_field_value(name, data);
								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::KeyCode:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());

								key_code data = sc_instance->get_field_value<key_code>(name);

								if (ImGui::BeginCombo(name.c_str(), key::to_string(data)))
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
											data = current_key;
											sc_instance->set_field_value<key_code>(name, data);
										}

										if (is_selected)
											ImGui::SetItemDefaultFocus();
									}

									ImGui::EndCombo();
								}

								END_COMPONENT_TABLE_ROW();
								break;
							}

							case whip::script_field_type::MouseCode:
							{
								BEGIN_COMPONENT_TABLE_ROW(name.c_str());

								mouse_code data = sc_instance->get_field_value<mouse_code>(name);

								if (ImGui::BeginCombo("MouseCode", mouse::to_string(data)))
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
											sc_instance->set_field_value<mouse_code>(name, current_button);
										}

										if (is_selected)
											ImGui::SetItemDefaultFocus();
									}

									ImGui::EndCombo();
								}

								END_COMPONENT_TABLE_ROW();
								break;
							}
							case whip::script_field_type::Entity:
							case whip::script_field_type::None:
							case whip::script_field_type::Logger:
							default:
								// do nothing
								break;
							}
						}
					}
				}
				else
				{
					if (script_class_exists)
					{
						ref<script_class> entity_class = script_engine::get_entity_class(component.class_name);
						const auto& fields = entity_class->get_fields();

						auto& entity_fields = script_engine::get_script_field_map(entity_in);
						for (const auto& [name, field] : fields)
						{
							// Field has been set in editor
							if (entity_fields.find(name) != entity_fields.end())
							{
								script_field_instance& sc_field = entity_fields.at(name);

								switch (field.type)
								{
								case whip::script_field_type::Float:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									float data = sc_field.get_value<float>();
									if (ImGui::DragFloat(("##" + name).c_str(), &data))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Int:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									int data = sc_field.get_value<int>();
									if (ImGui::InputInt(("##" + name).c_str(), &data))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Bool:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									bool data = sc_field.get_value<bool>();
									if (ImGui::Checkbox(("##" + name).c_str(), &data))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Long:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									int64_t data = sc_field.get_value<int64_t>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_S64, &data))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Vector3:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									glm::vec3 data = sc_field.get_value<glm::vec3>();

									if (UI::draw_field_vec3_control(name.c_str(), data, 0, ImGui::GetColumnWidth()))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Vector2:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									glm::vec2 data = sc_field.get_value<glm::vec2>();

									if (UI::draw_field_vec2_control(name.c_str(), data, 0, ImGui::GetColumnWidth()))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Vector4:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									glm::vec4 data = sc_field.get_value<glm::vec4>();

									if (ImGui::ColorEdit4(("##" + name).c_str(), glm::value_ptr(data)))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::UInt:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									uint32_t data = sc_field.get_value<uint32_t>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_U32, &data))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::ULong:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									uint64_t data = sc_field.get_value<uint64_t>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_U64, &data))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Double:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									double data = sc_field.get_value<double>();
									if (ImGui::InputDouble(("##" + name).c_str(), &data))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Byte:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									uint8_t data = sc_field.get_value<uint8_t>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_U8, &data))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::SByte:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									int8_t data = sc_field.get_value<int8_t>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_S8, &data))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Char:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									char data = sc_field.get_value<char>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_S8, &data))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Short:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									short data = sc_field.get_value<short>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_S16, &data))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::UShort:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									uint16_t data = sc_field.get_value<uint16_t>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_U16, &data))
										sc_field.set_value(data);
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::KeyCode:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());

									key_code data = sc_field.get_value<key_code>();

									if (ImGui::BeginCombo(name.c_str(), key::to_string(data)))
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

									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::MouseCode:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());

									key_code data = sc_field.get_value<key_code>();

									if (ImGui::BeginCombo("MouseCode", mouse::to_string(data)))
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

									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Entity:
								case whip::script_field_type::None:
								case whip::script_field_type::Logger:
								default:
									// do nothing
									break;
								}
							}
							else
							{
								auto& base_entity_fields = script_engine::get_base_script_field_map(component.class_name);
								script_field_instance& sc_field = base_entity_fields.at(name);

								switch (field.type)
								{
								case whip::script_field_type::Float:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									float data = sc_field.get_value<float>();
									if (ImGui::DragFloat(("##" + name).c_str(), &data))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Int:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									int data = sc_field.get_value<int>();
									if (ImGui::InputInt(("##" + name).c_str(), &data))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Bool:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									bool data = sc_field.get_value<bool>();
									if (ImGui::Checkbox(("##" + name).c_str(), &data))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Long:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									int64_t data = sc_field.get_value<int64_t>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_S64, &data))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Vector3:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									glm::vec3 data = sc_field.get_value<glm::vec3>();

									if (UI::draw_field_vec3_control(name.c_str(), data, 0, ImGui::GetColumnWidth()))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Vector2:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									glm::vec2 data = sc_field.get_value<glm::vec2>();

									if (UI::draw_field_vec2_control(name.c_str(), data, 0, ImGui::GetColumnWidth()))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Vector4:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									glm::vec4 data = sc_field.get_value<glm::vec4>();

									if (ImGui::ColorEdit4(("##" + name).c_str(), glm::value_ptr(data)))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::UInt:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									uint32_t data = sc_field.get_value<uint32_t>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_U32, &data))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::ULong:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									uint64_t data = sc_field.get_value<uint64_t>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_U64, &data))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Double:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									double data = sc_field.get_value<double>();
									if (ImGui::InputDouble(("##" + name).c_str(), &data))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Byte:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									uint8_t data = sc_field.get_value<uint8_t>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_U8, &data))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::SByte:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									int8_t data = sc_field.get_value<int8_t>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_S8, &data))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Char:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									char data = sc_field.get_value<char>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_S8, &data))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Short:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									short data = sc_field.get_value<short>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_S16, &data))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::UShort:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());
									uint16_t data = sc_field.get_value<uint16_t>();
									if (ImGui::InputScalar(("##" + name).c_str(), ImGuiDataType_U16, &data))
									{
										script_field_instance& field_instance = entity_fields[name];
										field_instance.field = field;
										field_instance.set_value(data);
									}
									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::KeyCode:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());

									key_code data = sc_field.get_value<key_code>();

									if (ImGui::BeginCombo(name.c_str(), key::to_string(data)))
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
												script_field_instance& field_instance = entity_fields[name];
												field_instance.field = field;
												field_instance.set_value(current_key);
											}

											if (is_selected)
												ImGui::SetItemDefaultFocus();
										}

										ImGui::EndCombo();
									}

									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::MouseCode:
								{
									BEGIN_COMPONENT_TABLE_ROW(name.c_str());

									key_code data = sc_field.get_value<key_code>();

									if (ImGui::BeginCombo("MouseCode", mouse::to_string(data)))
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
												script_field_instance& field_instance = entity_fields[name];
												field_instance.field = field;
												field_instance.set_value(current_button);
											}

											if (is_selected)
												ImGui::SetItemDefaultFocus();
										}

										ImGui::EndCombo();
									}

									END_COMPONENT_TABLE_ROW();
									break;
								}
								case whip::script_field_type::Entity:
								case whip::script_field_type::None:
								case whip::script_field_type::Logger:
								default:
									// do nothing
									break;
								}
							}
						}
					}
				}
			ImGui::EndTable();
			}
		});
	ImGui::Spacing();
	draw_component<sprite_renderer_component>("Sprite Renderer", entity_in, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.color));
			std::string label = "None";
			bool is_texture_valid = false;
			if (component.texture != 0)
			{
				if (asset_manager::is_asset_handle_valid(component.texture) && asset_manager::get_asset_type(component.texture) == asset_type::texture2D)
				{
					const asset_metadata& metadata = project::get_active()->get_editor_asset_manager()->get_metadata(component.texture);
					label = metadata.filepath.filename().string();
					is_texture_valid = true;
				}
				else
				{
					label = "Invalid";
				}
			}

			ImVec2 button_label_size = ImGui::CalcTextSize(label.c_str());
			button_label_size.x += 20.0f;
			float button_label_width = glm::max<float>(100.0f, button_label_size.x);
			
			static const auto drag_drop_callback = [&component](asset_handle handle) 
				{
					component.texture = handle;
				};

			UI::drag_drop_target(asset_type::texture2D, drag_drop_callback, label.c_str(), true, button_label_width, 0.0f);

			if (is_texture_valid)
			{
				ImGui::SameLine();
				ImVec2 x_label_size = ImGui::CalcTextSize("X");
				float button_size = x_label_size.y + ImGui::GetStyle().FramePadding.y * 2.0f;
				if (ImGui::Button("X", ImVec2(button_size, button_size)))
				{
					component.texture = 0;
				}
			}

			ImGui::SameLine();
			ImGui::Text("Texture");

			ImGui::DragFloat("Tiling Factor", &component.tiling_factor, 0.1f, 0.0f, 100.0f);
		});
	ImGui::Spacing();
	draw_component<circle_renderer_component>("Circle Renderer", entity_in, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.color));
			ImGui::DragFloat("Thickness", &component.thickness, 0.025f, 0.0f, 1.0f);
			ImGui::DragFloat("Fade", &component.fade, 0.00025f, 0.0f, 1.0f);
		});
	ImGui::Spacing();
	draw_component<text_component>("Text Renderer", entity_in, [](auto& component)
		{
			ImGui::InputTextMultiline("Text String", &component.text_string);
			ImGui::ColorEdit4("Color", glm::value_ptr(component.color));
			ImGui::DragFloat("Kerning", &component.kerning, 0.025f);
			ImGui::DragFloat("Line Spacing", &component.line_spacing, 0.025f);

			std::string label = "None";
			bool is_font_valid = false;
			if (component.font != 0)
			{
				if (asset_manager::is_asset_handle_valid(component.font) && asset_manager::get_asset_type(component.font) == asset_type::font)
				{
					const asset_metadata& metadata = project::get_active()->get_editor_asset_manager()->get_metadata(component.font);
					label = metadata.filepath.filename().string();
					is_font_valid = true;
				}
				else
				{
					label = "Invalid";
				}
			}

			ImVec2 button_label_size = ImGui::CalcTextSize(label.c_str());
			button_label_size.x += 20.0f;
			float button_label_width = glm::max<float>(100.0f, button_label_size.x);

			static const auto drag_drop_callback = [&component](asset_handle handle)
				{
					component.font = handle;
				};

			UI::drag_drop_target(asset_type::font, drag_drop_callback, label.c_str(), true, button_label_width, 0.0f);

			if (is_font_valid)
			{
				ImGui::SameLine();
				ImVec2 x_label_size = ImGui::CalcTextSize("X");
				float button_size = x_label_size.y + ImGui::GetStyle().FramePadding.y * 2.0f;
				if (ImGui::Button("X", ImVec2(button_size, button_size)))
				{
					component.font = 0;
				}
			}

			ImGui::SameLine();
			ImGui::Text("Font");
		});
	ImGui::Spacing();
	draw_component<rigidbody2D_component>("Rigidbody 2D", entity_in, [](auto& component)
		{
			const char* body_type_strings[] = { "Static", "Dynamic", "Kinematic" };
			const char* current_body_type_string = body_type_strings[(int)component.type];
			if (ImGui::BeginCombo("Body Type", current_body_type_string))
			{
				for (int i = 0; i < 3; i++)
				{
					bool is_selected = current_body_type_string == body_type_strings[i];
					if (ImGui::Selectable(body_type_strings[i], is_selected))
					{
						current_body_type_string = body_type_strings[i];
						component.type = (rigidbody2D_component::body_type)i;
					}

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			ImGui::DragFloat("Gravity Scale", &component.gravity_scale, 0.01f, 0.0f);
			ImGui::Checkbox("Fixed Rotation", &component.fixed_rotation);
		});
	ImGui::Spacing();
	draw_component<box_collider2D_component>("Box Collider 2D", entity_in, [](auto& component)
		{
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strncpy_s(buffer, sizeof(buffer), component.tag.c_str(), sizeof(buffer));
			if (ImGui::InputText("Tag", buffer, sizeof(buffer)))
				component.tag = buffer;
			ImGui::Checkbox("Is Sensor", &component.sensor);
			ImGui::DragFloat2("Offset", glm::value_ptr(component.offset));
			ImGui::DragFloat2("Size", glm::value_ptr(component.size));
			ImGui::DragFloat("Density", &component.density, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Friction", &component.friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold", &component.restitution_threshold, 0.01f, 0.0f);
		});
	ImGui::Spacing();
	draw_component<circle_collider2D_component>("Circle Collider 2D", entity_in, [](auto& component)
		{
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strncpy_s(buffer, sizeof(buffer), component.tag.c_str(), sizeof(buffer));
			if (ImGui::InputText("Tag", buffer, sizeof(buffer)))
				component.tag = buffer;
			ImGui::Checkbox("Is Sensor", &component.sensor);
			ImGui::DragFloat2("Offset", glm::value_ptr(component.offset));
			ImGui::DragFloat("Radius", &component.radius);
			ImGui::DragFloat("Density", &component.density, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Friction", &component.friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold", &component.restitution_threshold, 0.01f, 0.0f);
		});
	ImGui::Spacing();
	draw_component<audio_component>("Audio", entity_in, [](auto& component)
		{
			if (ImGui::Button("Add Audio", ImVec2(component.selected_audio_index != npos<size_t> ? ImGui::GetColumnWidth() / 2 : ImGui::GetColumnWidth(), 0)))
			{
				audio_component::audio_data new_handle{};
				new_handle.tag = component.un_manager.add_name(audio_component::audio_data::default_tag);
				new_handle.ID = UUID32{};
				component.audio_datas.push_back(new_handle);
				component.selected_audio_index = component.audio_datas.size() - 1;
			}
			if (component.selected_audio_index != npos<size_t>)
			{
				ImGui::SameLine();
				if (ImGui::Button("Delete Audio", ImVec2(ImGui::GetColumnWidth(), 0)))
				{
					component.un_manager.remove_name(component.audio_datas[component.selected_audio_index].tag);
					component.audio_datas.erase(component.audio_datas.begin() + component.selected_audio_index);
					component.selected_audio_index = npos<size_t>;
				}
			}

			ImGui::Spacing();
			if (ImGui::BeginTable("AudioTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			{
				if (component.selected_audio_index == npos<size_t>)
				{
					BEGIN_COMPONENT_TABLE_ROW("Select Audio");
					if (ImGui::BeginCombo("##Audio Handle", ""))
					{
						size_t index = 0;
						for (auto& _audio_data : component.audio_datas)
						{
							if (ImGui::Selectable(_audio_data.tag.c_str(), false))
								component.selected_audio_index = index;
							index++;
						}

						ImGui::EndCombo();
					}
					END_COMPONENT_TABLE_ROW();
				}
				else
				{
					BEGIN_COMPONENT_TABLE_ROW("Audio");
					if (ImGui::BeginCombo("##Audio Handle", component.audio_datas[component.selected_audio_index].tag.c_str()))
					{
						size_t index = 0;
						for (auto& audio_handle : component.audio_datas)
						{
							bool is_selected = component.audio_datas[component.selected_audio_index].tag == audio_handle.tag;

							if (ImGui::Selectable(audio_handle.tag.c_str(), is_selected))
								component.selected_audio_index = index;
						
							if (is_selected)
								ImGui::SetItemDefaultFocus();
							index++;
						}

						ImGui::EndCombo();
					}
					END_COMPONENT_TABLE_ROW();
					ImGui::Separator();
					{
						BEGIN_COMPONENT_TABLE_ROW("ID");
						ImGui::Text("%u", component.audio_datas[component.selected_audio_index].ID);
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Tag");
						char buffer[256];
						memset(buffer, 0, sizeof(buffer));
						strncpy_s(buffer, sizeof(buffer), component.audio_datas[component.selected_audio_index].tag.c_str(), sizeof(buffer));
						if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
						{
							component.un_manager.remove_name(component.audio_datas[component.selected_audio_index].tag);
							component.audio_datas[component.selected_audio_index].tag = component.un_manager.add_name(buffer);
						}
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Audio File");
						std::string label = "None";
						bool is_audio_valid = false;
						if (component.audio_datas[component.selected_audio_index].audio != 0)
						{
							if (asset_manager::is_asset_handle_valid(component.audio_datas[component.selected_audio_index].audio) && asset_manager::get_asset_type(component.audio_datas[component.selected_audio_index].audio) == asset_type::audio)
							{
								const asset_metadata& metadata = project::get_active()->get_editor_asset_manager()->get_metadata(component.audio_datas[component.selected_audio_index].audio);
								label = metadata.filepath.filename().string();
								is_audio_valid = true;
							}
							else
							{
								label = "Invalid";
							}
						}

						float button_size = 0.0f;
						float padding = 0.0f;
						if (is_audio_valid)
						{
							ImVec2 x_label_size = ImGui::CalcTextSize("X");
							button_size = x_label_size.y + ImGui::GetStyle().FramePadding.y * 2.0f;
							padding = button_size / 2.5f;
						}

						float width = ImGui::GetColumnWidth() - (button_size + padding);

						static const auto drag_drop_callback = [&component](asset_handle handle)
							{
								component.audio_datas[component.selected_audio_index].audio = handle;
								auto audio_asset = asset_manager::get_asset<audio_source>(handle);
								component.audio_datas[component.selected_audio_index].full_clip_length = audio_asset->get_length();
								component.audio_datas[component.selected_audio_index].clip_start = 0.0f;
								component.audio_datas[component.selected_audio_index].clip_end = component.audio_datas[component.selected_audio_index].full_clip_length;
							};

						UI::drag_drop_target(asset_type::audio, drag_drop_callback, label.c_str(), true, width, 0.0f);

						if (is_audio_valid)
						{
							ImGui::SameLine();
							if (ImGui::Button("X", ImVec2(button_size, button_size)))
							{
								component.audio_datas[component.selected_audio_index].audio = 0;
								component.audio_datas[component.selected_audio_index].clip_start = component.audio_datas[component.selected_audio_index].clip_end = component.audio_datas[component.selected_audio_index].full_clip_length = 0;
							}
						}
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Spitial");
						ImGui::Checkbox("##Spitial", &component.audio_datas[component.selected_audio_index].spitial);
						END_COMPONENT_TABLE_ROW();
					}
					if(component.audio_datas[component.selected_audio_index].spitial)
					{
						BEGIN_COMPONENT_TABLE_ROW("Translation");
						UI::draw_field_vec3_control("##Translation", component.audio_datas[component.selected_audio_index].translation, 0, ImGui::GetColumnWidth());
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Loop");
						ImGui::Checkbox("##Loop", &component.audio_datas[component.selected_audio_index].loop);
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Gain");
						ImGui::DragFloat("##Gain", &component.audio_datas[component.selected_audio_index].gain, 0.01f, 0.0f, FLT_MAX);
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Pitch");
						ImGui::DragFloat("##Pitch", &component.audio_datas[component.selected_audio_index].pitch, 0.01f, 0.5f, 2.0f);
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Full Clip Length");
						ImGui::Text("%.2fs", component.audio_datas[component.selected_audio_index].full_clip_length);
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Clip Part (%.2fs - %.2fs)", component.audio_datas[component.selected_audio_index].clip_start, component.audio_datas[component.selected_audio_index].clip_end);
						UI::draw_dual_handle_slider(0.0f, component.audio_datas[component.selected_audio_index].full_clip_length, &component.audio_datas[component.selected_audio_index].clip_start, &component.audio_datas[component.selected_audio_index].clip_end, 0.0f, 0.0f, false);
						END_COMPONENT_TABLE_ROW();
					}
				}
				ImGui::EndTable();
			}
		});
}

template<class T>
inline void scene_hierarchy_panel::display_add_component_entry(const std::string& entry_name)
{
	if (!m_selection_context.has_component<T>())
	{
		if (ImGui::MenuItem(entry_name.c_str()))
		{
			m_selection_context.add_component<T>();
			ImGui::CloseCurrentPopup();
		}
	}
}

_WHIP_END

#undef BEGIN_COMPONENT_TABLE_ROW
