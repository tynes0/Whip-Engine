#include "whippch.h"
#include "UI_project.h"

#include <Whip/Core/Application.h>
#include <Whip/Asset/scene_importer.h>
#include <Whip/Project/project.h>
#include <Whip/Scene/scene.h>
#include <Whip/Utils/platform_utils.h>

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <vector>

_WHIP_START

namespace UI
{
	namespace
	{
		struct scene_entry
		{
			asset_handle handle = 0;
			asset_metadata metadata;
		};

		void copy_to_buffer(char* buffer, size_t buffer_size, const std::string& value)
		{
			std::memset(buffer, 0, buffer_size);
			std::strncpy(buffer, value.c_str(), buffer_size - 1);
		}

		std::string sanitize_scene_name(std::string value)
		{
			value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c)
				{
					return !std::isalnum(c) && c != '_' && c != '-' && c != ' ';
				}), value.end());

			for (char& c : value)
				if (c == ' ')
					c = '_';

			if (value.empty())
				value = "NewScene";

			return value;
		}

		std::filesystem::path make_unique_scene_path(const std::string& scene_name)
		{
			std::string safe_name = sanitize_scene_name(scene_name);
			std::filesystem::path relative_path = std::filesystem::path("Scenes") / (safe_name + ".whip");
			std::filesystem::path absolute_path = project::get_active_asset_directory() / relative_path;

			int suffix = 1;
			while (std::filesystem::exists(absolute_path))
			{
				relative_path = std::filesystem::path("Scenes") / (safe_name + "_" + std::to_string(suffix++) + ".whip");
				absolute_path = project::get_active_asset_directory() / relative_path;
			}

			return relative_path;
		}

		std::vector<scene_entry> collect_scene_entries()
		{
			std::vector<scene_entry> entries;
			ref<project> active_project = project::get_active();
			if (!active_project || !active_project->get_editor_asset_manager())
				return entries;

			const auto& scenes = active_project->get_editor_asset_manager()->get_asset_registry().get_filtered(asset_type::scene);
			entries.reserve(scenes.size());
			for (const auto& [handle, metadata] : scenes)
				entries.push_back({ handle, metadata });

			std::sort(entries.begin(), entries.end(), [](const scene_entry& left, const scene_entry& right)
				{
					return left.metadata.filepath.generic_string() < right.metadata.filepath.generic_string();
				});

			return entries;
		}

		bool same_relative_path(const std::filesystem::path& left, const std::filesystem::path& right)
		{
			return left.lexically_normal().generic_string() == right.lexically_normal().generic_string();
		}
	}

	UI_project::UI_project()
	{
	}

	void UI_project::set_finish_callback(const callback_type& callback)
	{
		m_callback = callback;
	}

	void UI_project::set_scene_callbacks(const scene_callback_type& open_scene_callback, const callback_type& close_scene_callback, const scene_path_callback_type& active_scene_path_callback)
	{
		m_open_scene_callback = open_scene_callback;
		m_close_scene_callback = close_scene_callback;
		m_active_scene_path_callback = active_scene_path_callback;
	}

	void UI_project::show(UI_type type, const callback_type& callback)
	{
		switch (type)
		{
		case whip::UI::UI_project::UI_none:
			break;
		case whip::UI::UI_project::UI_settings:
			break;
		default:
			type = UI_none;
			break;
		}
		if (callback && type != UI_none)
			set_finish_callback(callback);
		m_type = type;
		sync_from_active_project();
	}

	void UI_project::on_imgui_render()
	{
		if (m_type == UI_none)
			return;

		ref<project> active_project = project::get_active();
		if (!active_project)
		{
			m_type = UI_none;
			return;
		}

		if (active_project != m_last_active)
			sync_from_active_project();

		bool open = true;
		ImGui::SetNextWindowSize(ImVec2(820.0f, 560.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Project Settings", &open, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::TextUnformatted(m_name_buffer);
			ImGui::TextDisabled("%s", project::get_active_project_path().string().c_str());
			ImGui::Separator();

			if (ImGui::BeginTabBar("##ProjectSettingsTabs"))
			{
				if (ImGui::BeginTabItem("Project"))
				{
					draw_project_settings();
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Scenes"))
				{
					draw_scene_settings();
					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}
		}
		ImGui::End();

		draw_create_scene_popup();
		draw_delete_scene_popup();

		if (!open)
			m_type = UI_none;
	}

	void UI_project::sync_from_active_project()
	{
		ref<project> active_project = project::get_active();
		if (!active_project)
			return;

		const project_config& config = active_project->get_config();
		copy_to_buffer(m_name_buffer, max_buffer_size, config.name);
		copy_to_buffer(m_project_path_buffer, max_buffer_size, active_project->get_project_path().string());
		copy_to_buffer(m_asset_dir_buffer, max_buffer_size, config.asset_directory.string());
		copy_to_buffer(m_start_scene_buffer, max_buffer_size, std::to_string((uint64_t)config.start_scene));
		copy_to_buffer(m_script_module_path_buffer, max_buffer_size, config.script_module_path.string());
		m_last_active = active_project;
	}

	void UI_project::draw_project_settings()
	{
		ImGui::Spacing();
		if (ImGui::BeginTable("##ProjectSettingsTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 180.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Name");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputText("##ProjectName", m_name_buffer, max_buffer_size);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Project File");
			ImGui::TableNextColumn();
			ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputText("##ProjectPath", m_project_path_buffer, max_buffer_size);
			ImGui::EndDisabled();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Assets");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputText("##AssetDirectory", m_asset_dir_buffer, max_buffer_size);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Script Module");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputText("##ScriptModule", m_script_module_path_buffer, max_buffer_size);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Start Scene");
			ImGui::TableNextColumn();
			ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputText("##StartScene", m_start_scene_buffer, max_buffer_size);
			ImGui::EndDisabled();

			ImGui::EndTable();
		}

		ImGui::Spacing();
		if (ImGui::Button("Save Project", ImVec2(140.0f, 0.0f)))
			apply_project_settings();
		ImGui::SameLine();
		if (ImGui::Button("Revert", ImVec2(100.0f, 0.0f)))
			sync_from_active_project();
	}

	void UI_project::draw_scene_settings()
	{
		ref<project> active_project = project::get_active();
		if (!active_project)
			return;

		if (ImGui::Button("New Scene", ImVec2(120.0f, 0.0f)))
		{
			copy_to_buffer(m_new_scene_name_buffer, max_buffer_size, "NewScene");
			ImGui::OpenPopup("Create Scene");
		}

		ImGui::SameLine();
		if (ImGui::Button("Save Project", ImVec2(120.0f, 0.0f)))
			apply_project_settings();

		ImGui::Spacing();
		const std::vector<scene_entry> scenes = collect_scene_entries();
		if (scenes.empty())
		{
			ImGui::TextDisabled("No scenes have been imported yet.");
			return;
		}

		const std::filesystem::path active_scene_path = m_active_scene_path_callback ? m_active_scene_path_callback() : std::filesystem::path();
		bool open_delete_popup = false;
		if (ImGui::BeginTable("##SceneRegistryTable", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 360.0f)))
		{
			ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableSetupColumn("Start", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 190.0f);
			ImGui::TableHeadersRow();

			for (const scene_entry& entry : scenes)
			{
				const bool is_start_scene = active_project->get_config().start_scene == entry.handle;
				const bool is_active_scene = !active_scene_path.empty() && same_relative_path(active_scene_path, entry.metadata.filepath);
				const std::string scene_name = entry.metadata.filepath.stem().string();

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(scene_name.c_str());
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", entry.metadata.filepath.generic_string().c_str());
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", is_active_scene ? "Open" : "-");
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", is_start_scene ? "Yes" : "-");
				ImGui::TableNextColumn();

				ImGui::PushID((int)(uint64_t)entry.handle);
				if (ImGui::SmallButton("Open") && m_open_scene_callback)
					m_open_scene_callback(entry.handle);
				ImGui::SameLine();
				if (ImGui::SmallButton("Set Start"))
				{
					active_project->get_config().start_scene = entry.handle;
					apply_project_settings();
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Delete"))
				{
					m_pending_delete_scene = entry.handle;
					m_pending_delete_scene_path = entry.metadata.filepath;
					open_delete_popup = true;
				}
				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		if (open_delete_popup)
			ImGui::OpenPopup("Delete Scene");
	}

	void UI_project::draw_create_scene_popup()
	{
		if (ImGui::BeginPopupModal("Create Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::SetNextItemWidth(320.0f);
			ImGui::InputText("Name", m_new_scene_name_buffer, max_buffer_size);

			if (ImGui::Button("Create", ImVec2(120.0f, 0.0f)))
			{
				create_scene_from_popup();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	void UI_project::draw_delete_scene_popup()
	{
		if (ImGui::BeginPopupModal("Delete Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Delete scene?");
			ImGui::TextDisabled("%s", m_pending_delete_scene_path.generic_string().c_str());
			ImGui::Spacing();

			if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f)))
			{
				delete_pending_scene();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	void UI_project::apply_project_settings()
	{
		ref<project> active_project = project::get_active();
		if (!active_project)
			return;

		project_config& config = active_project->get_config();
		config.name = m_name_buffer;
		config.asset_directory = m_asset_dir_buffer;
		config.script_module_path = m_script_module_path_buffer;

		project::save_active();
		sync_from_active_project();

		if (m_callback)
			m_callback();
	}

	void UI_project::create_scene_from_popup()
	{
		ref<project> active_project = project::get_active();
		if (!active_project || !active_project->get_editor_asset_manager())
			return;

		std::filesystem::path relative_path = make_unique_scene_path(m_new_scene_name_buffer);
		std::filesystem::create_directories((project::get_active_asset_directory() / relative_path).parent_path());

		ref<scene> new_scene = make_ref<scene>();
		scene_importer::save_scene(new_scene, relative_path);
		asset_handle handle = active_project->get_editor_asset_manager()->import_asset(relative_path);

		if (handle != 0 && m_open_scene_callback)
			m_open_scene_callback(handle);

		if (m_callback)
			m_callback();
	}

	void UI_project::delete_pending_scene()
	{
		ref<project> active_project = project::get_active();
		if (!active_project || !active_project->get_editor_asset_manager() || m_pending_delete_scene == 0)
			return;

		const std::filesystem::path active_scene_path = m_active_scene_path_callback ? m_active_scene_path_callback() : std::filesystem::path();
		if (!active_scene_path.empty() && same_relative_path(active_scene_path, m_pending_delete_scene_path) && m_close_scene_callback)
			m_close_scene_callback();

		if (active_project->get_config().start_scene == m_pending_delete_scene)
			active_project->get_config().start_scene = 0;

		std::error_code remove_error;
		std::filesystem::remove(project::get_active_asset_directory() / m_pending_delete_scene_path, remove_error);
		if (remove_error)
			WHP_CORE_WARN("[Project Settings] Failed to delete scene file '{0}': {1}", m_pending_delete_scene_path.string(), remove_error.message());
		active_project->get_editor_asset_manager()->delete_asset(m_pending_delete_scene);
		project::save_active();

		m_pending_delete_scene = 0;
		m_pending_delete_scene_path.clear();
		sync_from_active_project();

		if (m_callback)
			m_callback();
	}
}

_WHIP_END
