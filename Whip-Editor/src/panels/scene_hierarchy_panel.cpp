#include "scene_hierarchy_panel.h"

#include <Whip/Scene/components.h>
#include <Whip/UI/UI_helpers.h>
#include <Whip/UI/UI_scoped_style.h>
#include <Whip/Scripting/script_engine.h>
#include <Whip/Project/project.h>
#include <Whip/Asset/asset_manager.h>
#include <Whip/Asset/asset_metadata.h>
#include <Whip/Asset/texture_importer.h>
#include <Whip/Audio/audio_source.h>
#include <Whip/Core/Input.h>
#include <Whip/Core/KeyCodes.h>
#include <Whip/Utils/platform_utils.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <cctype>
#include <cstring>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <type_traits>
#include <utility>
#include <vector>

#include "../Helpers/script_field_helper.h"

#define BEGIN_COMPONENT_TABLE_ROW(...) do { ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text(__VA_ARGS__); ImGui::TableNextColumn(); ImGui::PushItemWidth(-1); } while(false)
#define END_COMPONENT_TABLE_ROW() do { ImGui::PopItemWidth(); } while(false)

_WHIP_START

namespace
{
	constexpr const char* scene_entity_payload_type = "WHIP_SCENE_ENTITY";

	void draw_property_section_title(const char* title)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", title);
		ImGui::Separator();
		ImGui::Spacing();
	}

	bool same_vec3(const glm::vec3& left, const glm::vec3& right)
	{
		return left.x == right.x && left.y == right.y && left.z == right.z;
	}

	bool same_vec2(const glm::vec2& left, const glm::vec2& right)
	{
		return left.x == right.x && left.y == right.y;
	}

	bool same_vec4(const glm::vec4& left, const glm::vec4& right)
	{
		return left.x == right.x && left.y == right.y && left.z == right.z && left.w == right.w;
	}

	void draw_mixed_hint(const char* label, bool mixed)
	{
		if (mixed)
			ImGui::TextDisabled("%s: Mixed", label);
	}

	std::string asset_label(asset_handle handle, asset_type expected_type)
	{
		if (handle == 0)
			return "None";

		if (!asset_manager::is_asset_handle_valid(handle) || asset_manager::get_asset_type(handle) != expected_type)
			return "Invalid";

		const asset_metadata& metadata = project::get_active()->get_editor_asset_manager()->get_metadata(handle);
		return metadata ? metadata.filepath.filename().string() : "Invalid";
	}

	std::filesystem::path find_file_with_extension(const std::filesystem::path& directory, const char* extension)
	{
		std::error_code error;
		if (!std::filesystem::exists(directory, error) || !std::filesystem::is_directory(directory, error))
			return {};

		for (const auto& entry : std::filesystem::directory_iterator(directory, error))
		{
			if (error)
				break;

			if (!entry.is_regular_file(error))
				continue;

			std::string entry_extension = entry.path().extension().string();
			std::transform(entry_extension.begin(), entry_extension.end(), entry_extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (entry_extension == extension)
				return entry.path();
		}

		return {};
	}

	std::filesystem::path active_scripts_directory()
	{
		if (!project::get_active())
			return {};

		const std::filesystem::path scripts_directory = project::get_active_asset_directory() / "Scripts";
		std::error_code error;
		return std::filesystem::exists(scripts_directory, error) && std::filesystem::is_directory(scripts_directory, error) ? scripts_directory : std::filesystem::path{};
	}

	std::filesystem::path active_script_workspace_file()
	{
		ref<project> active_project = project::get_active();
		if (!active_project)
			return {};

		const std::filesystem::path scripts_directory = active_scripts_directory();
		if (scripts_directory.empty())
			return {};

		const std::array<std::string, 2> preferred_stems =
		{
			active_project->get_config().script_module_path.stem().string(),
			active_project->get_project_path().stem().string()
		};

		std::error_code error;
		for (const std::string& stem : preferred_stems)
		{
			if (stem.empty())
				continue;

			std::filesystem::path preferred = scripts_directory / (stem + ".sln");
			if (std::filesystem::exists(preferred, error))
				return preferred;

			preferred = scripts_directory / (stem + ".csproj");
			if (std::filesystem::exists(preferred, error))
				return preferred;
		}

		std::filesystem::path workspace_file = find_file_with_extension(scripts_directory, ".sln");
		if (!workspace_file.empty())
			return workspace_file;

		return find_file_with_extension(scripts_directory, ".csproj");
	}

	std::filesystem::path first_existing_path(const std::vector<std::filesystem::path>& paths)
	{
		std::error_code error;
		for (const auto& path : paths)
			if (!path.empty() && std::filesystem::exists(path, error))
				return path;

		return {};
	}

	std::string lowercase(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value;
	}

	bool filename_matches(const std::filesystem::path& path, const std::vector<std::string>& filenames)
	{
		const std::string filename = lowercase(path.filename().string());
		return std::find(filenames.begin(), filenames.end(), filename) != filenames.end();
	}

	std::filesystem::path environment_path(const char* name)
	{
		const char* value = std::getenv(name);
		return value ? std::filesystem::path(value) : std::filesystem::path{};
	}

	std::filesystem::path find_first_named_file_under(const std::filesystem::path& root, const std::vector<std::string>& filenames, int max_depth)
	{
		std::error_code error;
		if (root.empty() || !std::filesystem::exists(root, error))
			return {};

		std::vector<std::pair<std::filesystem::path, int>> pending;
		pending.emplace_back(root, 0);

		while (!pending.empty())
		{
			const auto [directory, depth] = pending.back();
			pending.pop_back();

			for (const auto& entry : std::filesystem::directory_iterator(directory, error))
			{
				if (error)
					break;

				if (entry.is_regular_file(error) && filename_matches(entry.path(), filenames))
					return entry.path();

				if (depth < max_depth && entry.is_directory(error))
					pending.emplace_back(entry.path(), depth + 1);
			}

			error.clear();
		}

		return {};
	}

	std::filesystem::path visual_studio_executable()
	{
		static const std::filesystem::path executable = first_existing_path({
			"C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/devenv.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Professional/Common7/IDE/devenv.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/devenv.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Preview/Common7/IDE/devenv.exe"
			});
		return executable.empty() ? std::filesystem::path("devenv.exe") : executable;
	}

	std::filesystem::path rider_executable()
	{
		static const std::filesystem::path executable = []()
			{
				const std::filesystem::path program_files = environment_path("ProgramFiles");
				const std::filesystem::path program_files_x86 = environment_path("ProgramFiles(x86)");
				const std::filesystem::path local_app_data = environment_path("LOCALAPPDATA");

				std::filesystem::path candidate = first_existing_path({
					local_app_data / "JetBrains" / "Toolbox" / "scripts" / "rider.cmd",
					local_app_data / "JetBrains" / "Toolbox" / "scripts" / "rider.bat",
					local_app_data / "Programs" / "Rider" / "bin" / "rider64.exe",
					local_app_data / "Programs" / "Rider" / "bin" / "rider.exe"
					});
				if (!candidate.empty())
					return candidate;

				const std::vector<std::string> rider_files = { "rider64.exe", "rider.exe", "rider.cmd", "rider.bat" };
				candidate = find_first_named_file_under(local_app_data / "JetBrains" / "Toolbox" / "apps" / "Rider", rider_files, 6);
				if (!candidate.empty())
					return candidate;

				candidate = find_first_named_file_under(program_files / "JetBrains", rider_files, 4);
				if (!candidate.empty())
					return candidate;

				candidate = find_first_named_file_under(program_files_x86 / "JetBrains", rider_files, 4);
				if (!candidate.empty())
					return candidate;

				candidate = find_first_named_file_under(local_app_data / "Programs", rider_files, 4);
				return candidate;
			}();

		return executable.empty() ? std::filesystem::path("rider64.exe") : executable;
	}

	bool open_workspace_with_default_app(const std::filesystem::path& workspace_file)
	{
		if (utils::open_external_path(workspace_file))
			return true;

		WHP_CORE_WARN("[Script Workspace] Could not open '{0}'.", workspace_file.string());
		return false;
	}

	bool open_workspace_with_app(const char* app_name, const std::filesystem::path& executable, const std::filesystem::path& workspace_file)
	{
		if (utils::open_external_path_with(executable, workspace_file))
			return true;

		WHP_CORE_WARN("[Script Workspace] Could not open '{0}' with {1}.", workspace_file.string(), app_name);
		return false;
	}

	void draw_script_workspace_actions()
	{
		ImGui::PushID("ScriptWorkspaceActions");

		const std::filesystem::path workspace_file = active_script_workspace_file();
		const std::filesystem::path scripts_directory = active_scripts_directory();
		const bool has_workspace = !workspace_file.empty();
		const bool has_scripts_directory = !scripts_directory.empty();

		ImGui::BeginDisabled(!has_workspace);
		if (ImGui::SmallButton("Open"))
			open_workspace_with_default_app(workspace_file);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Open the C# solution with the default IDE");
		ImGui::SameLine();
		if (ImGui::SmallButton("VS"))
			open_workspace_with_app("Visual Studio", visual_studio_executable(), workspace_file);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Open in Visual Studio");
		ImGui::SameLine();
		if (ImGui::SmallButton("Rider"))
			open_workspace_with_app("Rider", rider_executable(), workspace_file);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Open in JetBrains Rider");
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::BeginDisabled(!has_scripts_directory);
		if (ImGui::SmallButton("Folder"))
			utils::open_external_path(scripts_directory);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Open Assets/Scripts");
		ImGui::EndDisabled();

		if (has_workspace)
			ImGui::TextDisabled("%s", workspace_file.filename().string().c_str());
		else
			ImGui::TextDisabled("No C# solution found in Assets/Scripts.");

		ImGui::PopID();
	}
}

static audio_component::audio_data* find_ac_AD(std::vector<audio_component::audio_data>&handle_list, const std::string & tag)
{
	for (audio_component::audio_data& handle : handle_list)
		if (handle.tag == tag)
			return &handle;
	return nullptr;
}

template<typename T, typename UIFunction>
static void draw_component(const std::string& name, entity entity_in, const std::function<void()>& before_change, UIFunction uiFunction)
{
	constexpr ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
	if (entity_in.has_component<T>())
	{
		auto& component = entity_in.get_component<T>();

		const ImGuiStyle& style = ImGui::GetStyle();
		ImGui::PushID(name.c_str());
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 10.0f, 7.0f });
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 8.0f, 7.0f });
		ImGui::PushStyleColor(ImGuiCol_Header, style.Colors[ImGuiCol_Header]);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style.Colors[ImGuiCol_HeaderHovered]);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.Colors[ImGuiCol_HeaderActive]);
		bool open = ImGui::TreeNodeEx("##Component", tree_node_flags, "%s", name.c_str());
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();

		bool remove_component = false;
		if (ImGui::BeginPopupContextItem("Component Settings"))
		{
			if constexpr (!std::is_same_v<T, transform_component>)
				if (ImGui::MenuItem("Remove component"))
					remove_component = true;

			ImGui::EndPopup();
		}

		if (open)
		{
			ImGui::Spacing();
			ImGui::Indent(8.0f);
			uiFunction(component);
			ImGui::Unindent(8.0f);
			ImGui::TreePop();
		}

		if (remove_component)
		{
			if (before_change)
				before_change();
			entity_in.remove_component<T>();
		}

		ImGui::Spacing();
		ImGui::PopID();
	}
}

scene_hierarchy_panel::scene_hierarchy_panel(const ref<scene> context)
{
	set_context(context);
}

void scene_hierarchy_panel::set_context(const ref<scene>& context)
{
	m_context = context;
	clear_selection();
}

void scene_hierarchy_panel::on_imgui_render()
{
	if (!m_open)
		return;

	bool open = m_open;
	ImGui::Begin("Scene Hierarchy", &open);
	if (open != m_open)
		set_open(open);

	if (m_context)
	{
		auto group = m_context->m_registry.group<>(entt::get<ID_component>);

		for (auto entityID : group)
		{
			entity ent{ entityID , m_context.get() };
			if (!ent.has_component<hierarchy_component>())
			{
				draw_entity_node(ent);
				continue;
			}

			auto& hierarchy = ent.get_component<hierarchy_component>();
			if (hierarchy.parent != 0 && !m_context->find_entity_by_UUID(hierarchy.parent))
				hierarchy.parent = 0;
			if (hierarchy.parent == 0)
				draw_entity_node(ent);
		}

		if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
			clear_selection();

		if (ImGui::BeginPopupContextWindow(0, 1 | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Entity"))
			{
				notify_scene_change();
				m_context->create_entity("New Entity");
			}
			if (ImGui::MenuItem("Create Group"))
			{
				notify_scene_change();
				entity group_entity = m_context->create_entity("Group");
				group_entity.get_component<hierarchy_component>().is_group = true;
			}

			ImGui::EndPopup();
		}
	}

	ImGui::End();

	ImGui::Begin("Properties");
	if (!m_context)
	{
		ImGui::TextDisabled("No scene loaded.");
	}
	else if (!m_selection_context)
	{
		ImGui::Dummy(ImVec2(0.0f, 8.0f));
		ImGui::TextDisabled("No entity selected.");
	}
	else
	{
		if (m_selection_contexts.size() > 1)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
			ImGui::Text("%zu entities selected", m_selection_contexts.size());
			ImGui::PopStyleColor();
			ImGui::TextDisabled("Showing primary selection.");
			ImGui::Separator();
		}

		ImGui::BeginChild("##PropertiesScroll", ImVec2(0.0f, 0.0f), false);
		std::vector<entity> selected_entities = get_selected_entities();
		if (selected_entities.size() > 1)
			draw_multi_edit_components(selected_entities);
		else
			draw_components(m_selection_context);
		track_property_edit_history();
		ImGui::EndChild();
	}

	ImGui::End();
}

void scene_hierarchy_panel::set_open(bool open)
{
	if (m_open == open)
		return;
	m_open = open;
	m_open_dirty = true;
}

bool scene_hierarchy_panel::consume_open_dirty()
{
	const bool dirty = m_open_dirty;
	m_open_dirty = false;
	return dirty;
}

std::vector<entity> scene_hierarchy_panel::get_selected_entities() const
{
	std::vector<entity> result;
	if (!m_context)
		return result;

	result.reserve(m_selection_contexts.size());
	for (UUID id : m_selection_contexts)
	{
		entity selected = m_context->find_entity_by_UUID(id);
		if (selected)
			result.push_back(selected);
	}
	return result;
}

void scene_hierarchy_panel::set_selected_entity(entity entity_in, bool append)
{
	if (!entity_in)
	{
		if (!append)
			clear_selection();
		return;
	}

	if (!append)
		m_selection_contexts.clear();

	UUID id = entity_in.get_UUID();
	auto it = std::find(m_selection_contexts.begin(), m_selection_contexts.end(), id);
	if (append && it != m_selection_contexts.end())
	{
		m_selection_contexts.erase(it);
		m_selection_context = m_selection_contexts.empty() ? entity{} : m_context->find_entity_by_UUID(m_selection_contexts.back());
		return;
	}

	if (it == m_selection_contexts.end())
		m_selection_contexts.push_back(id);
	m_selection_context = entity_in;
}

void scene_hierarchy_panel::set_selected_entity_ids(const std::vector<UUID>& ids)
{
	clear_selection();
	if (!m_context)
		return;

	for (UUID id : ids)
	{
		entity selected = m_context->find_entity_by_UUID(id);
		if (!selected)
			continue;

		m_selection_contexts.push_back(id);
		m_selection_context = selected;
	}
}

void scene_hierarchy_panel::select_all()
{
	clear_selection();
	if (!m_context)
		return;

	auto view = m_context->m_registry.view<ID_component>();
	for (auto entity_id : view)
	{
		entity selected{ entity_id, m_context.get() };
		m_selection_contexts.push_back(selected.get_UUID());
		m_selection_context = selected;
	}
}

void scene_hierarchy_panel::clear_selection()
{
	m_selection_context = {};
	m_selection_contexts.clear();
}

void scene_hierarchy_panel::draw_entity_node(entity entity_in)
{
	if (entity_in.has_component<tag_component>())
	{
		auto& tag = entity_in.get_component<tag_component>().tag;
		auto& hierarchy = entity_in.get_component<hierarchy_component>();

		ImGuiTreeNodeFlags flags = (is_selected(entity_in) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
		if (hierarchy.children.empty())
			flags |= ImGuiTreeNodeFlags_Leaf;

		const std::string label = hierarchy.is_group ? ("[Group] " + tag) : tag;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity_in, flags, label.c_str());

		if (ImGui::IsItemClicked())
			set_selected_entity(entity_in, input::is_key_down(key::left_control) || input::is_key_down(key::right_control));

		if (ImGui::BeginDragDropSource())
		{
			UUID entity_id = entity_in.get_UUID();
			ImGui::SetDragDropPayload(scene_entity_payload_type, &entity_id, sizeof(UUID));
			ImGui::TextUnformatted(tag.c_str());
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(scene_entity_payload_type))
			{
				UUID child_id = *(UUID*)payload->Data;
				entity child = m_context->find_entity_by_UUID(child_id);
				if (child && can_parent_entity(child, entity_in))
				{
					notify_scene_change();
					set_entity_parent(child, entity_in);
				}
			}
			ImGui::EndDragDropTarget();
		}

		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Create Child"))
			{
				notify_scene_change();
				entity child = m_context->create_entity("New Entity");
				set_entity_parent(child, entity_in);
			}
			if (ImGui::MenuItem("Create Child Group"))
			{
				notify_scene_change();
				entity child_group = m_context->create_entity("Group");
				child_group.get_component<hierarchy_component>().is_group = true;
				set_entity_parent(child_group, entity_in);
			}
			if (hierarchy.parent != 0 && ImGui::MenuItem("Move To Root"))
			{
				notify_scene_change();
				set_entity_parent(entity_in, {});
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete Entity"))
				entityDeleted = true;

			ImGui::EndPopup();
		}

		if (opened)
		{
			for (UUID child_id : hierarchy.children)
			{
				entity child = m_context->find_entity_by_UUID(child_id);
				if (child)
					draw_entity_node(child);
			}
			ImGui::TreePop();
		}

		if (entityDeleted)
		{
			notify_scene_change();
			destroy_entity_with_selection(entity_in);
		}
	}
}

void scene_hierarchy_panel::set_entity_parent(entity child, entity parent)
{
	if (!child || child == parent || !child.has_component<hierarchy_component>())
		return;

	auto& child_hierarchy = child.get_component<hierarchy_component>();
	if (child_hierarchy.parent != 0)
	{
		entity old_parent = m_context->find_entity_by_UUID(child_hierarchy.parent);
		if (old_parent && old_parent.has_component<hierarchy_component>())
		{
			auto& old_parent_hierarchy = old_parent.get_component<hierarchy_component>();
			old_parent_hierarchy.children.erase(
				std::remove(old_parent_hierarchy.children.begin(), old_parent_hierarchy.children.end(), child.get_UUID()),
				old_parent_hierarchy.children.end());
		}
	}

	child_hierarchy.parent = parent ? parent.get_UUID() : UUID(0);
	if (parent && parent.has_component<hierarchy_component>())
	{
		auto& parent_hierarchy = parent.get_component<hierarchy_component>();
		if (std::find(parent_hierarchy.children.begin(), parent_hierarchy.children.end(), child.get_UUID()) == parent_hierarchy.children.end())
			parent_hierarchy.children.push_back(child.get_UUID());
	}
}

bool scene_hierarchy_panel::can_parent_entity(entity child, entity parent) const
{
	if (!child || !parent || child == parent)
		return false;

	return !is_descendant_of(parent, child.get_UUID());
}

bool scene_hierarchy_panel::is_descendant_of(entity entity_in, UUID ancestor_id) const
{
	if (!entity_in || !entity_in.has_component<hierarchy_component>())
		return false;

	const auto& hierarchy = entity_in.get_component<hierarchy_component>();
	if (hierarchy.parent == 0)
		return false;
	if (hierarchy.parent == ancestor_id)
		return true;

	return is_descendant_of(m_context->find_entity_by_UUID(hierarchy.parent), ancestor_id);
}

void scene_hierarchy_panel::destroy_entity_with_selection(entity entity_in)
{
	if (!entity_in)
		return;

	if (m_selection_context == entity_in || is_descendant_of(m_selection_context, entity_in.get_UUID()))
		clear_selection();

	m_context->destroy_entity(entity_in);
}

bool scene_hierarchy_panel::is_selected(entity entity_in) const
{
	if (!entity_in)
		return false;

	UUID id = entity_in.get_UUID();
	return std::find(m_selection_contexts.begin(), m_selection_contexts.end(), id) != m_selection_contexts.end();
}

void scene_hierarchy_panel::notify_scene_change()
{
	if (m_scene_change_callback)
		m_scene_change_callback();
}

void scene_hierarchy_panel::begin_property_edit_history()
{
	if (m_property_edit_history_active)
		return;

	notify_scene_change();
	m_property_edit_history_active = true;
}

void scene_hierarchy_panel::track_property_edit_history()
{
	const bool editing_property = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::IsAnyItemActive() && ImGui::GetActiveID() != 0;
	if (editing_property && !m_property_edit_history_active)
	{
		notify_scene_change();
		m_property_edit_history_active = true;
	}
	if (!editing_property)
		m_property_edit_history_active = false;
}

void scene_hierarchy_panel::draw_multi_edit_components(const std::vector<entity>& selected_entities)
{
	draw_property_section_title("Multi Edit");
	ImGui::TextDisabled("%zu entities selected", selected_entities.size());

	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	if (ImGui::Button("Add Component To Selection", ImVec2(-1.0f, 0.0f)))
		ImGui::OpenPopup("Add Component");
	ImGui::PopStyleColor(3);

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

	draw_property_section_title("Shared Transform");
	entity primary = selected_entities.front();
	glm::vec3 translation = primary.get_component<transform_component>().translation;
	glm::vec3 rotation = glm::degrees(primary.get_component<transform_component>().rotation);
	glm::vec3 scale = primary.get_component<transform_component>().scale;
	bool translation_mixed = false;
	bool rotation_mixed = false;
	bool scale_mixed = false;
	for (entity selected : selected_entities)
	{
		const auto& transform = selected.get_component<transform_component>();
		translation_mixed |= !same_vec3(transform.translation, translation);
		rotation_mixed |= !same_vec3(glm::degrees(transform.rotation), rotation);
		scale_mixed |= !same_vec3(transform.scale, scale);
	}

	if (translation_mixed)
		ImGui::TextDisabled("Translation has mixed values.");
	glm::vec3 previous_translation = translation;
	UI::draw_vec3_control("Translation", translation, 0, 100, ImGui::GetStyle().IndentSpacing);
	if (!same_vec3(translation, previous_translation))
	{
		begin_property_edit_history();
		for (entity selected : selected_entities)
			selected.get_component<transform_component>().translation = translation;
	}

	if (rotation_mixed)
		ImGui::TextDisabled("Rotation has mixed values.");
	glm::vec3 previous_rotation = rotation;
	UI::draw_vec3_control("Rotation", rotation, 0, 100, ImGui::GetStyle().IndentSpacing);
	if (!same_vec3(rotation, previous_rotation))
	{
		begin_property_edit_history();
		for (entity selected : selected_entities)
			selected.get_component<transform_component>().rotation = glm::radians(rotation);
	}

	if (scale_mixed)
		ImGui::TextDisabled("Scale has mixed values.");
	glm::vec3 previous_scale = scale;
	UI::draw_vec3_control("Scale", scale, 1.0f, 100, ImGui::GetStyle().IndentSpacing);
	if (!same_vec3(scale, previous_scale))
	{
		begin_property_edit_history();
		for (entity selected : selected_entities)
			selected.get_component<transform_component>().scale = scale;
	}

	draw_multi_shared_components(selected_entities);

	draw_property_section_title("Component Coverage");
	draw_multi_component_summary<camera_component>("Camera", selected_entities.size());
	draw_multi_component_summary<script_component>("Script", selected_entities.size());
	draw_multi_component_summary<sprite_renderer_component>("Sprite Renderer", selected_entities.size());
	draw_multi_component_summary<circle_renderer_component>("Circle Renderer", selected_entities.size());
	draw_multi_component_summary<text_component>("Text Renderer", selected_entities.size());
	draw_multi_component_summary<rigidbody2D_component>("Rigidbody 2D", selected_entities.size());
	draw_multi_component_summary<box_collider2D_component>("Box Collider 2D", selected_entities.size());
	draw_multi_component_summary<circle_collider2D_component>("Circle Collider 2D", selected_entities.size());
	draw_multi_component_summary<audio_component>("Audio", selected_entities.size());
}

void scene_hierarchy_panel::draw_multi_shared_components(const std::vector<entity>& selected_entities)
{
	const size_t selected_count = selected_entities.size();
	bool drew_component = false;

	draw_property_section_title("Shared Components");
	if (count_selected_with_component<camera_component>() == selected_count)
	{
		draw_multi_camera_component(selected_entities);
		drew_component = true;
	}
	if (count_selected_with_component<script_component>() == selected_count)
	{
		draw_multi_script_component(selected_entities);
		drew_component = true;
	}
	if (count_selected_with_component<sprite_renderer_component>() == selected_count)
	{
		draw_multi_sprite_renderer_component(selected_entities);
		drew_component = true;
	}
	if (count_selected_with_component<circle_renderer_component>() == selected_count)
	{
		draw_multi_circle_renderer_component(selected_entities);
		drew_component = true;
	}
	if (count_selected_with_component<text_component>() == selected_count)
	{
		draw_multi_text_component(selected_entities);
		drew_component = true;
	}
	if (count_selected_with_component<rigidbody2D_component>() == selected_count)
	{
		draw_multi_rigidbody2D_component(selected_entities);
		drew_component = true;
	}
	if (count_selected_with_component<box_collider2D_component>() == selected_count)
	{
		draw_multi_box_collider2D_component(selected_entities);
		drew_component = true;
	}
	if (count_selected_with_component<circle_collider2D_component>() == selected_count)
	{
		draw_multi_circle_collider2D_component(selected_entities);
		drew_component = true;
	}

	if (!drew_component)
		ImGui::TextDisabled("No shared editable components in the current selection.");
}

void scene_hierarchy_panel::draw_multi_camera_component(const std::vector<entity>& selected_entities)
{
	ImGui::PushID("MultiCamera");
	if (ImGui::TreeNodeEx("Camera", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		entity primary_entity = selected_entities.front();
		camera_component& primary_component = primary_entity.get_component<camera_component>();
		bool primary = primary_component.primary;
		bool primary_mixed = false;
		bool fixed_aspect = primary_component.fixed_aspect_ratio;
		bool fixed_aspect_mixed = false;
		scene_camera::projection_type projection = primary_component.camera.get_projection_type();
		bool projection_mixed = false;

		for (entity selected : selected_entities)
		{
			const auto& component = selected.get_component<camera_component>();
			primary_mixed |= component.primary != primary;
			fixed_aspect_mixed |= component.fixed_aspect_ratio != fixed_aspect;
			projection_mixed |= component.camera.get_projection_type() != projection;
		}

		draw_mixed_hint("Primary", primary_mixed);
		if (ImGui::Checkbox("Primary", &primary))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<camera_component>().primary = primary;
		}

		const char* projection_type_strings[] = { "Perspective", "Orthographic" };
		const char* current_projection_type_string = projection_mixed ? "Mixed" : projection_type_strings[(int)projection];
		draw_mixed_hint("Projection", projection_mixed);
		if (ImGui::BeginCombo("Projection", current_projection_type_string))
		{
			for (int i = 0; i < 2; i++)
			{
				bool is_selected = !projection_mixed && projection == (scene_camera::projection_type)i;
				if (ImGui::Selectable(projection_type_strings[i], is_selected))
				{
					begin_property_edit_history();
					for (entity selected : selected_entities)
						selected.get_component<camera_component>().camera.set_projection_type((scene_camera::projection_type)i);
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (!projection_mixed && projection == scene_camera::projection_type::perspective)
		{
			float fov = glm::degrees(primary_component.camera.get_perspective_vertical_FOV());
			float near_clip = primary_component.camera.get_perspective_near_clip();
			float far_clip = primary_component.camera.get_perspective_far_clip();
			bool fov_mixed = false;
			bool near_mixed = false;
			bool far_mixed = false;
			for (entity selected : selected_entities)
			{
				const auto& camera = selected.get_component<camera_component>().camera;
				fov_mixed |= camera.get_perspective_vertical_FOV() != primary_component.camera.get_perspective_vertical_FOV();
				near_mixed |= camera.get_perspective_near_clip() != near_clip;
				far_mixed |= camera.get_perspective_far_clip() != far_clip;
			}

			draw_mixed_hint("Vertical FOV", fov_mixed);
			if (ImGui::DragFloat("Vertical FOV", &fov))
			{
				begin_property_edit_history();
				for (entity selected : selected_entities)
					selected.get_component<camera_component>().camera.set_perspective_vertical_FOV(glm::radians(fov));
			}
			draw_mixed_hint("Near", near_mixed);
			if (ImGui::DragFloat("Near", &near_clip))
			{
				begin_property_edit_history();
				for (entity selected : selected_entities)
					selected.get_component<camera_component>().camera.set_perspective_near_clip(near_clip);
			}
			draw_mixed_hint("Far", far_mixed);
			if (ImGui::DragFloat("Far", &far_clip))
			{
				begin_property_edit_history();
				for (entity selected : selected_entities)
					selected.get_component<camera_component>().camera.set_perspective_far_clip(far_clip);
			}
		}

		if (!projection_mixed && projection == scene_camera::projection_type::orthographic)
		{
			float size = primary_component.camera.get_orthographic_size();
			float near_clip = primary_component.camera.get_orthographic_near_clip();
			float far_clip = primary_component.camera.get_orthographic_far_clip();
			bool size_mixed = false;
			bool near_mixed = false;
			bool far_mixed = false;
			for (entity selected : selected_entities)
			{
				const auto& camera = selected.get_component<camera_component>().camera;
				size_mixed |= camera.get_orthographic_size() != size;
				near_mixed |= camera.get_orthographic_near_clip() != near_clip;
				far_mixed |= camera.get_orthographic_far_clip() != far_clip;
			}

			draw_mixed_hint("Size", size_mixed);
			if (ImGui::DragFloat("Size", &size))
			{
				begin_property_edit_history();
				for (entity selected : selected_entities)
					selected.get_component<camera_component>().camera.set_orthographic_size(size);
			}
			draw_mixed_hint("Near", near_mixed);
			if (ImGui::DragFloat("Near", &near_clip))
			{
				begin_property_edit_history();
				for (entity selected : selected_entities)
					selected.get_component<camera_component>().camera.set_orthographic_near_clip(near_clip);
			}
			draw_mixed_hint("Far", far_mixed);
			if (ImGui::DragFloat("Far", &far_clip))
			{
				begin_property_edit_history();
				for (entity selected : selected_entities)
					selected.get_component<camera_component>().camera.set_orthographic_far_clip(far_clip);
			}

			draw_mixed_hint("Fixed Aspect Ratio", fixed_aspect_mixed);
			if (ImGui::Checkbox("Fixed Aspect Ratio", &fixed_aspect))
			{
				begin_property_edit_history();
				for (entity selected : selected_entities)
					selected.get_component<camera_component>().fixed_aspect_ratio = fixed_aspect;
			}
		}

		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void scene_hierarchy_panel::draw_multi_script_component(const std::vector<entity>& selected_entities)
{
	ImGui::PushID("MultiScript");
	if (ImGui::TreeNodeEx("Script", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		entity primary_entity = selected_entities.front();
		std::string class_name = primary_entity.get_component<script_component>().class_name;
		bool class_mixed = false;
		for (entity selected : selected_entities)
			class_mixed |= selected.get_component<script_component>().class_name != class_name;

		draw_script_workspace_actions();
		ImGui::Separator();

		draw_mixed_hint("Class", class_mixed);
		const char* label = class_mixed ? "Mixed" : (class_name.empty() ? "None" : class_name.c_str());
		if (ImGui::BeginCombo("Class", label))
		{
			auto entity_classes = script_engine::get_entity_classes();
			for (const auto& [name, script_class] : entity_classes)
			{
				bool is_selected = !class_mixed && class_name == name;
				if (ImGui::Selectable(name.c_str(), is_selected))
				{
					begin_property_edit_history();
					for (entity selected : selected_entities)
						selected.get_component<script_component>().class_name = name;
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::TextDisabled("Script fields are edited per entity while runtime field mapping is entity-specific.");
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void scene_hierarchy_panel::draw_multi_sprite_renderer_component(const std::vector<entity>& selected_entities)
{
	ImGui::PushID("MultiSpriteRenderer");
	if (ImGui::TreeNodeEx("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		entity primary_entity = selected_entities.front();
		sprite_renderer_component& primary = primary_entity.get_component<sprite_renderer_component>();
		glm::vec4 color = primary.color;
		asset_handle texture = primary.texture;
		float tiling_factor = primary.tiling_factor;
		bool color_mixed = false;
		bool texture_mixed = false;
		bool tiling_mixed = false;

		for (entity selected : selected_entities)
		{
			const auto& component = selected.get_component<sprite_renderer_component>();
			color_mixed |= !same_vec4(component.color, color);
			texture_mixed |= component.texture != texture;
			tiling_mixed |= component.tiling_factor != tiling_factor;
		}

		draw_mixed_hint("Color", color_mixed);
		if (ImGui::ColorEdit4("Color", glm::value_ptr(color)))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<sprite_renderer_component>().color = color;
		}

		draw_mixed_hint("Texture", texture_mixed);
		std::string label = texture_mixed ? "Mixed" : asset_label(texture, asset_type::texture2D);
		const auto drag_drop_callback = [this, &selected_entities](asset_handle handle)
			{
				begin_property_edit_history();
				for (entity selected : selected_entities)
					selected.get_component<sprite_renderer_component>().texture = handle;
			};
		UI::drag_drop_target(asset_type::texture2D, drag_drop_callback, label.c_str(), true, glm::max<float>(100.0f, ImGui::CalcTextSize(label.c_str()).x + 20.0f), 0.0f);
		ImGui::SameLine();
		if (ImGui::Button("Clear Texture"))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<sprite_renderer_component>().texture = 0;
		}

		draw_mixed_hint("Tiling Factor", tiling_mixed);
		if (ImGui::DragFloat("Tiling Factor", &tiling_factor, 0.1f, 0.0f, 100.0f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<sprite_renderer_component>().tiling_factor = tiling_factor;
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void scene_hierarchy_panel::draw_multi_circle_renderer_component(const std::vector<entity>& selected_entities)
{
	ImGui::PushID("MultiCircleRenderer");
	if (ImGui::TreeNodeEx("Circle Renderer", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		entity primary_entity = selected_entities.front();
		circle_renderer_component& primary = primary_entity.get_component<circle_renderer_component>();
		glm::vec4 color = primary.color;
		float thickness = primary.thickness;
		float fade = primary.fade;
		bool color_mixed = false;
		bool thickness_mixed = false;
		bool fade_mixed = false;
		for (entity selected : selected_entities)
		{
			const auto& component = selected.get_component<circle_renderer_component>();
			color_mixed |= !same_vec4(component.color, color);
			thickness_mixed |= component.thickness != thickness;
			fade_mixed |= component.fade != fade;
		}

		draw_mixed_hint("Color", color_mixed);
		if (ImGui::ColorEdit4("Color", glm::value_ptr(color)))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<circle_renderer_component>().color = color;
		}
		draw_mixed_hint("Thickness", thickness_mixed);
		if (ImGui::DragFloat("Thickness", &thickness, 0.025f, 0.0f, 1.0f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<circle_renderer_component>().thickness = thickness;
		}
		draw_mixed_hint("Fade", fade_mixed);
		if (ImGui::DragFloat("Fade", &fade, 0.00025f, 0.0f, 1.0f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<circle_renderer_component>().fade = fade;
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void scene_hierarchy_panel::draw_multi_text_component(const std::vector<entity>& selected_entities)
{
	ImGui::PushID("MultiTextRenderer");
	if (ImGui::TreeNodeEx("Text Renderer", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		entity primary_entity = selected_entities.front();
		text_component& primary = primary_entity.get_component<text_component>();
		std::string text = primary.text_string;
		asset_handle font = primary.font;
		glm::vec4 color = primary.color;
		float kerning = primary.kerning;
		float line_spacing = primary.line_spacing;
		bool text_mixed = false;
		bool font_mixed = false;
		bool color_mixed = false;
		bool kerning_mixed = false;
		bool line_spacing_mixed = false;
		for (entity selected : selected_entities)
		{
			const auto& component = selected.get_component<text_component>();
			text_mixed |= component.text_string != text;
			font_mixed |= component.font != font;
			color_mixed |= !same_vec4(component.color, color);
			kerning_mixed |= component.kerning != kerning;
			line_spacing_mixed |= component.line_spacing != line_spacing;
		}

		draw_mixed_hint("Text", text_mixed);
		if (ImGui::InputTextMultiline("Text String", &text))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<text_component>().text_string = text;
		}
		draw_mixed_hint("Color", color_mixed);
		if (ImGui::ColorEdit4("Color", glm::value_ptr(color)))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<text_component>().color = color;
		}
		draw_mixed_hint("Font", font_mixed);
		std::string label = font_mixed ? "Mixed" : asset_label(font, asset_type::font);
		const auto drag_drop_callback = [this, &selected_entities](asset_handle handle)
			{
				begin_property_edit_history();
				for (entity selected : selected_entities)
					selected.get_component<text_component>().font = handle;
			};
		UI::drag_drop_target(asset_type::font, drag_drop_callback, label.c_str(), true, glm::max<float>(100.0f, ImGui::CalcTextSize(label.c_str()).x + 20.0f), 0.0f);
		ImGui::SameLine();
		if (ImGui::Button("Clear Font"))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<text_component>().font = 0;
		}
		draw_mixed_hint("Kerning", kerning_mixed);
		if (ImGui::DragFloat("Kerning", &kerning, 0.025f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<text_component>().kerning = kerning;
		}
		draw_mixed_hint("Line Spacing", line_spacing_mixed);
		if (ImGui::DragFloat("Line Spacing", &line_spacing, 0.025f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<text_component>().line_spacing = line_spacing;
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void scene_hierarchy_panel::draw_multi_rigidbody2D_component(const std::vector<entity>& selected_entities)
{
	ImGui::PushID("MultiRigidbody2D");
	if (ImGui::TreeNodeEx("Rigidbody 2D", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		entity primary_entity = selected_entities.front();
		rigidbody2D_component& primary = primary_entity.get_component<rigidbody2D_component>();
		rigidbody2D_component::body_type body_type = primary.type;
		float gravity_scale = primary.gravity_scale;
		bool fixed_rotation = primary.fixed_rotation;
		bool type_mixed = false;
		bool gravity_mixed = false;
		bool fixed_rotation_mixed = false;
		for (entity selected : selected_entities)
		{
			const auto& component = selected.get_component<rigidbody2D_component>();
			type_mixed |= component.type != body_type;
			gravity_mixed |= component.gravity_scale != gravity_scale;
			fixed_rotation_mixed |= component.fixed_rotation != fixed_rotation;
		}

		const char* body_type_strings[] = { "Static", "Dynamic", "Kinematic" };
		draw_mixed_hint("Body Type", type_mixed);
		if (ImGui::BeginCombo("Body Type", type_mixed ? "Mixed" : body_type_strings[(int)body_type]))
		{
			for (int i = 0; i < 3; i++)
			{
				bool is_selected = !type_mixed && body_type == (rigidbody2D_component::body_type)i;
				if (ImGui::Selectable(body_type_strings[i], is_selected))
				{
					begin_property_edit_history();
					for (entity selected : selected_entities)
						selected.get_component<rigidbody2D_component>().type = (rigidbody2D_component::body_type)i;
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		draw_mixed_hint("Gravity Scale", gravity_mixed);
		if (ImGui::DragFloat("Gravity Scale", &gravity_scale, 0.01f, 0.0f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<rigidbody2D_component>().gravity_scale = gravity_scale;
		}
		draw_mixed_hint("Fixed Rotation", fixed_rotation_mixed);
		if (ImGui::Checkbox("Fixed Rotation", &fixed_rotation))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<rigidbody2D_component>().fixed_rotation = fixed_rotation;
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void scene_hierarchy_panel::draw_multi_box_collider2D_component(const std::vector<entity>& selected_entities)
{
	ImGui::PushID("MultiBoxCollider2D");
	if (ImGui::TreeNodeEx("Box Collider 2D", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		entity primary_entity = selected_entities.front();
		box_collider2D_component& primary = primary_entity.get_component<box_collider2D_component>();
		std::string tag = primary.tag;
		bool sensor = primary.sensor;
		glm::vec2 offset = primary.offset;
		glm::vec2 size = primary.size;
		float density = primary.density;
		float friction = primary.friction;
		float restitution = primary.restitution;
		float restitution_threshold = primary.restitution_threshold;
		bool tag_mixed = false, sensor_mixed = false, offset_mixed = false, size_mixed = false;
		bool density_mixed = false, friction_mixed = false, restitution_mixed = false, threshold_mixed = false;
		for (entity selected : selected_entities)
		{
			const auto& component = selected.get_component<box_collider2D_component>();
			tag_mixed |= component.tag != tag;
			sensor_mixed |= component.sensor != sensor;
			offset_mixed |= !same_vec2(component.offset, offset);
			size_mixed |= !same_vec2(component.size, size);
			density_mixed |= component.density != density;
			friction_mixed |= component.friction != friction;
			restitution_mixed |= component.restitution != restitution;
			threshold_mixed |= component.restitution_threshold != restitution_threshold;
		}

		draw_mixed_hint("Tag", tag_mixed);
		if (ImGui::InputText("Tag", &tag))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<box_collider2D_component>().tag = tag;
		}
		draw_mixed_hint("Is Sensor", sensor_mixed);
		if (ImGui::Checkbox("Is Sensor", &sensor))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<box_collider2D_component>().sensor = sensor;
		}
		draw_mixed_hint("Offset", offset_mixed);
		if (ImGui::DragFloat2("Offset", glm::value_ptr(offset)))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<box_collider2D_component>().offset = offset;
		}
		draw_mixed_hint("Size", size_mixed);
		if (ImGui::DragFloat2("Size", glm::value_ptr(size)))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<box_collider2D_component>().size = size;
		}
		draw_mixed_hint("Density", density_mixed);
		if (ImGui::DragFloat("Density", &density, 0.01f, 0.0f, 1.0f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<box_collider2D_component>().density = density;
		}
		draw_mixed_hint("Friction", friction_mixed);
		if (ImGui::DragFloat("Friction", &friction, 0.01f, 0.0f, 1.0f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<box_collider2D_component>().friction = friction;
		}
		draw_mixed_hint("Restitution", restitution_mixed);
		if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<box_collider2D_component>().restitution = restitution;
		}
		draw_mixed_hint("Restitution Threshold", threshold_mixed);
		if (ImGui::DragFloat("Restitution Threshold", &restitution_threshold, 0.01f, 0.0f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<box_collider2D_component>().restitution_threshold = restitution_threshold;
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void scene_hierarchy_panel::draw_multi_circle_collider2D_component(const std::vector<entity>& selected_entities)
{
	ImGui::PushID("MultiCircleCollider2D");
	if (ImGui::TreeNodeEx("Circle Collider 2D", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		entity primary_entity = selected_entities.front();
		circle_collider2D_component& primary = primary_entity.get_component<circle_collider2D_component>();
		std::string tag = primary.tag;
		bool sensor = primary.sensor;
		glm::vec2 offset = primary.offset;
		float radius = primary.radius;
		float density = primary.density;
		float friction = primary.friction;
		float restitution = primary.restitution;
		float restitution_threshold = primary.restitution_threshold;
		bool tag_mixed = false, sensor_mixed = false, offset_mixed = false, radius_mixed = false;
		bool density_mixed = false, friction_mixed = false, restitution_mixed = false, threshold_mixed = false;
		for (entity selected : selected_entities)
		{
			const auto& component = selected.get_component<circle_collider2D_component>();
			tag_mixed |= component.tag != tag;
			sensor_mixed |= component.sensor != sensor;
			offset_mixed |= !same_vec2(component.offset, offset);
			radius_mixed |= component.radius != radius;
			density_mixed |= component.density != density;
			friction_mixed |= component.friction != friction;
			restitution_mixed |= component.restitution != restitution;
			threshold_mixed |= component.restitution_threshold != restitution_threshold;
		}

		draw_mixed_hint("Tag", tag_mixed);
		if (ImGui::InputText("Tag", &tag))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<circle_collider2D_component>().tag = tag;
		}
		draw_mixed_hint("Is Sensor", sensor_mixed);
		if (ImGui::Checkbox("Is Sensor", &sensor))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<circle_collider2D_component>().sensor = sensor;
		}
		draw_mixed_hint("Offset", offset_mixed);
		if (ImGui::DragFloat2("Offset", glm::value_ptr(offset)))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<circle_collider2D_component>().offset = offset;
		}
		draw_mixed_hint("Radius", radius_mixed);
		if (ImGui::DragFloat("Radius", &radius))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<circle_collider2D_component>().radius = radius;
		}
		draw_mixed_hint("Density", density_mixed);
		if (ImGui::DragFloat("Density", &density, 0.01f, 0.0f, 1.0f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<circle_collider2D_component>().density = density;
		}
		draw_mixed_hint("Friction", friction_mixed);
		if (ImGui::DragFloat("Friction", &friction, 0.01f, 0.0f, 1.0f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<circle_collider2D_component>().friction = friction;
		}
		draw_mixed_hint("Restitution", restitution_mixed);
		if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<circle_collider2D_component>().restitution = restitution;
		}
		draw_mixed_hint("Restitution Threshold", threshold_mixed);
		if (ImGui::DragFloat("Restitution Threshold", &restitution_threshold, 0.01f, 0.0f))
		{
			begin_property_edit_history();
			for (entity selected : selected_entities)
				selected.get_component<circle_collider2D_component>().restitution_threshold = restitution_threshold;
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void scene_hierarchy_panel::draw_components(entity entity_in)
{
	draw_property_section_title("Entity");
	if (entity_in.has_component<tag_component>())
	{
		auto& tag = entity_in.get_component<tag_component>().tag;

		if (ImGui::BeginTable("##EntityProperties", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 96.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Name");
			ImGui::TableNextColumn();
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strncpy_s(buffer, sizeof(buffer), tag.c_str(), sizeof(buffer));
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
			{
				m_context->m_unique_name_manager.remove_name(tag);
				tag = m_context->m_unique_name_manager.add_name(buffer);
			}

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("UUID");
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%llu", (unsigned long long)entity_in.get_UUID());

			if (entity_in.has_component<hierarchy_component>())
			{
				auto& hierarchy = entity_in.get_component<hierarchy_component>();
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Group");
				ImGui::TableNextColumn();
				ImGui::Checkbox("##IsGroup", &hierarchy.is_group);
			}

			ImGui::EndTable();
		}
	}

	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
		ImGui::OpenPopup("Add Component");
	ImGui::PopStyleColor(3);

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

	draw_property_section_title("Components");
	draw_component<transform_component>("Transform", entity_in, m_scene_change_callback, [](auto& component)
		{
			float spacing = ImGui::GetStyle().IndentSpacing;
			UI::draw_vec3_control("Translation", component.translation, 0, 100, spacing);
			glm::vec3 rotation = glm::degrees(component.rotation);
			UI::draw_vec3_control("Rotation", rotation, 0, 100, spacing);
			component.rotation = glm::radians(rotation);
			UI::draw_vec3_control("Scale", component.scale, 1.0f, 100, spacing);
		});
	ImGui::Spacing();
	draw_component<camera_component>("Camera", entity_in, m_scene_change_callback, [](auto& component)
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
	draw_component<script_component>("Script", entity_in, m_scene_change_callback, [entity_in, scene_in = m_context](auto& component) mutable
		{
			bool script_class_exists = script_engine::entity_class_exists(component.class_name);
			auto entity_classes = script_engine::get_entity_classes();
			if (ImGui::BeginTable("ScriptTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			{
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Workspace");
					ImGui::TableNextColumn();
					draw_script_workspace_actions();
				}

				{
					UI::scoped_style_color scope_color(ImGuiCol_Text, ImVec4(0.8f, 0.3f, 0.3f, 1.0f), !script_class_exists);
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
				}
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
							UI::draw_field_by_type(UI::script_field_draw::while_scene_running, field, entity_in, component.class_name, true);
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
							if (entity_fields.contains(name))
							{
								UI::draw_field_by_type(UI::script_field_draw::set_in_the_editor, field, entity_in, component.class_name, true);
							}
							else
							{
								UI::draw_field_by_type(UI::script_field_draw::with_base_value, field, entity_in, component.class_name, true);
							}
						}
					}
				}
			ImGui::EndTable();
			}
		});
	ImGui::Spacing();
	draw_component<sprite_renderer_component>("Sprite Renderer", entity_in, m_scene_change_callback, [](auto& component)
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
	draw_component<circle_renderer_component>("Circle Renderer", entity_in, m_scene_change_callback, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.color));
			ImGui::DragFloat("Thickness", &component.thickness, 0.025f, 0.0f, 1.0f);
			ImGui::DragFloat("Fade", &component.fade, 0.00025f, 0.0f, 1.0f);
		});
	ImGui::Spacing();
	draw_component<text_component>("Text Renderer", entity_in, m_scene_change_callback, [](auto& component)
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

			const auto drag_drop_callback = [&component](asset_handle handle)
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
	draw_component<rigidbody2D_component>("Rigidbody 2D", entity_in, m_scene_change_callback, [](auto& component)
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
	draw_component<box_collider2D_component>("Box Collider 2D", entity_in, m_scene_change_callback, [](auto& component)
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
	draw_component<circle_collider2D_component>("Circle Collider 2D", entity_in, m_scene_change_callback, [](auto& component)
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
	draw_component<audio_component>("Audio", entity_in, m_scene_change_callback, [](auto& component)
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
	const size_t selected_count = m_selection_contexts.empty() ? 0 : m_selection_contexts.size();
	const size_t component_count = count_selected_with_component<T>();
	if (component_count < selected_count)
	{
		if (ImGui::MenuItem(entry_name.c_str()))
		{
			notify_scene_change();
			add_component_to_selection<T>();
			ImGui::CloseCurrentPopup();
		}
	}
}

template<class T>
inline size_t scene_hierarchy_panel::count_selected_with_component() const
{
	size_t count = 0;
	for (entity selected : get_selected_entities())
		if (selected.has_component<T>())
			++count;
	return count;
}

template<class T>
inline void scene_hierarchy_panel::add_component_to_selection()
{
	for (entity selected : get_selected_entities())
		if (selected && !selected.has_component<T>())
			selected.add_component<T>();
}

template<class T>
inline void scene_hierarchy_panel::remove_component_from_selection()
{
	for (entity selected : get_selected_entities())
		if (selected && selected.has_component<T>())
			selected.remove_component<T>();
}

template<class T>
inline void scene_hierarchy_panel::draw_multi_component_summary(const char* name, size_t selected_count)
{
	const size_t component_count = count_selected_with_component<T>();
	if (component_count == 0)
		return;

	ImGui::PushID(name);
	if (ImGui::BeginTable("##MultiComponentSummary", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Coverage", ImGuiTableColumnFlags_WidthFixed, 96.0f);
		ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 176.0f);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(name);
		ImGui::TableNextColumn();
		if (component_count == selected_count)
			ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.48f, 1.0f), "All");
		else
			ImGui::TextColored(ImVec4(0.90f, 0.68f, 0.32f, 1.0f), "%zu/%zu", component_count, selected_count);
		ImGui::TableNextColumn();
		if (component_count < selected_count && ImGui::SmallButton("Add Missing"))
		{
			notify_scene_change();
			add_component_to_selection<T>();
		}
		if (component_count < selected_count)
			ImGui::SameLine();
		if constexpr (!std::is_same_v<T, transform_component>)
		{
			if (ImGui::SmallButton("Remove"))
			{
				notify_scene_change();
				remove_component_from_selection<T>();
			}
		}
		ImGui::EndTable();
	}
	ImGui::PopID();
}

_WHIP_END

#undef BEGIN_COMPONENT_TABLE_ROW
