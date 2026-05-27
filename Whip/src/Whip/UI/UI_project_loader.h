#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <Whip/Core/Application.h>
#include <Whip/Project/project.h>
#include <Whip/Utils/platform_utils.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "UI_popup_handler.h"

_WHIP_START

namespace UI
{
	struct project_template_option
	{
		const char* name;
		const char* description;
	};

	struct project_create_settings
	{
		std::string name;
		std::filesystem::path location;
		std::string initial_scene_name;
		int template_index = 0;
		bool create_start_scene = true;
		bool open_after_create = true;
	};

	class UI_project_loader
	{
	public:
		using callback = std::function<bool()>;
		using create_project_callback = std::function<bool(const project_create_settings&)>;
		using project_callback = std::function<bool(const std::filesystem::path&)>;

		void set_create_project_callback(create_project_callback create_callback)
		{
			m_create_project_callback = std::move(create_callback);
		}

		void set_load_project_callback(callback load_callback)
		{
			m_load_project_callback = std::move(load_callback);
		}

		void set_open_recent_project_callback(project_callback callback)
		{
			m_open_recent_project_callback = std::move(callback);
		}

		void set_forget_recent_project_callback(project_callback callback)
		{
			m_forget_recent_project_callback = std::move(callback);
		}

		void set_delete_recent_project_callback(project_callback callback)
		{
			m_delete_recent_project_callback = std::move(callback);
		}

		void set_recent_projects(std::vector<std::filesystem::path> recent_projects)
		{
			m_recent_projects = std::move(recent_projects);
		}

		void set_loaded(bool loaded)
		{
			m_loaded = loaded;
		}

		bool loaded() const { return m_loaded; }
		void reset()
		{
			m_loaded = false;
			m_status.clear();
		}

		void run()
		{
			if (m_loaded)
				return;

			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			const ImVec2 hub_size{
				viewport->WorkSize.x < 760.0f ? viewport->WorkSize.x - 32.0f : 700.0f,
				viewport->WorkSize.y < 480.0f ? viewport->WorkSize.y - 32.0f : 420.0f
			};
			ImGui::SetNextWindowSize(hub_size, ImGuiCond_Always);
			ImGui::SetNextWindowPos(
				ImVec2(viewport->WorkPos.x + (viewport->WorkSize.x - hub_size.x) * 0.5f, viewport->WorkPos.y + (viewport->WorkSize.y - hub_size.y) * 0.5f),
				ImGuiCond_Always);

			const ImGuiWindowFlags window_flags =
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
			ImGui::Begin("Whip Hub", nullptr, window_flags);

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 10.0f));
			ImGui::TextUnformatted("Whip Hub");
			ImGui::SameLine();
			ImGui::TextDisabled("Project Workspace");
			ImGui::Separator();

			const float actions_width = 220.0f;
			ImGui::BeginChild("WhipHubActions", ImVec2(actions_width, 0.0f), true);
			ImGui::TextUnformatted("Project");
			ImGui::Spacing();

			ImGui::BeginDisabled(!m_load_project_callback);
			if (ImGui::Button("Open Project", ImVec2(-1.0f, 40.0f)))
				on_project_action(m_load_project_callback);
			ImGui::EndDisabled();

			ImGui::BeginDisabled(!m_create_project_callback);
			if (ImGui::Button("New Project", ImVec2(-1.0f, 40.0f)))
				open_new_project_wizard();
			ImGui::EndDisabled();

			if (!m_status.empty())
			{
				ImGui::Separator();
				ImGui::TextWrapped("%s", m_status.c_str());
			}

			ImGui::EndChild();
			ImGui::SameLine();

			ImGui::BeginChild("WhipHubRecentProjects", ImVec2(0.0f, 0.0f), true);
			ImGui::TextUnformatted("Recent Projects");
			ImGui::Separator();

			if (m_recent_projects.empty())
			{
				ImGui::TextWrapped("No recent projects yet.");
			}
			else if (ImGui::BeginTable("RecentProjectsTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Project", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Open", ImGuiTableColumnFlags_WidthFixed, 78.0f);
				ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed, 78.0f);

				for (size_t i = 0; i < m_recent_projects.size(); ++i)
				{
					const std::filesystem::path& project_path = m_recent_projects[i];
					const std::string project_name = project_path.stem().empty() ? project_path.filename().string() : project_path.stem().string();
					const std::string button_id = "Open##recent_project_" + std::to_string(i);
					const std::string delete_button_id = "Delete##recent_project_" + std::to_string(i);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(project_name.c_str());
					ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x);
					ImGui::TextDisabled("%s", project_path.string().c_str());
					ImGui::PopTextWrapPos();

					ImGui::TableSetColumnIndex(1);
					ImGui::BeginDisabled(!m_open_recent_project_callback);
					if (ImGui::Button(button_id.c_str(), ImVec2(-1.0f, 0.0f)))
						on_project_action([this, project_path]() { return m_open_recent_project_callback(project_path); });
					ImGui::EndDisabled();

					ImGui::TableSetColumnIndex(2);
					ImGui::BeginDisabled(!m_forget_recent_project_callback && !m_delete_recent_project_callback);
					if (ImGui::Button(delete_button_id.c_str(), ImVec2(-1.0f, 0.0f)))
						request_recent_project_delete(project_path);
					ImGui::EndDisabled();
				}

				ImGui::EndTable();
			}

			ImGui::EndChild();
			ImGui::PopStyleVar();
			draw_new_project_wizard();
			draw_delete_project_modal();
			ImGui::End();
			ImGui::PopStyleVar();
		}

	private:
		static constexpr std::array<project_template_option, 3> templates =
		{
			project_template_option{ "Empty", "Folders, C# solution, and a clean startup scene." },
			project_template_option{ "2D Starter", "C# solution, camera, and starter sprite." },
			project_template_option{ "Script Ready", "Starter scene with a script-bound entity." }
		};

		void open_new_project_wizard()
		{
			if (std::strlen(m_new_project_name) == 0)
				copy_to_buffer(m_new_project_name, sizeof(m_new_project_name), "Untitled");
			if (std::strlen(m_new_project_scene_name) == 0)
				copy_to_buffer(m_new_project_scene_name, sizeof(m_new_project_scene_name), "Main");
			if (std::strlen(m_new_project_location) == 0)
			{
				std::filesystem::path default_location = std::filesystem::current_path().parent_path();
				if (default_location.empty())
					default_location = std::filesystem::current_path();
				std::string location = default_location.string();
				copy_to_buffer(m_new_project_location, sizeof(m_new_project_location), location);
			}
			m_new_project_error.clear();
			m_new_project_modal_open = true;
			m_new_project_popup_requested = true;
			m_new_project_popup_request_frame = ImGui::GetFrameCount();
		}

		void request_recent_project_delete(const std::filesystem::path& path)
		{
			m_project_delete_path = path;
			m_project_delete_error.clear();
			m_project_delete_modal_open = true;
			m_project_delete_popup_requested = true;
			m_project_delete_popup_request_frame = ImGui::GetFrameCount();
		}

		bool validate_new_project(project_create_settings& settings)
		{
			settings.name = m_new_project_name;
			settings.location = m_new_project_location;
			settings.initial_scene_name = m_new_project_scene_name;
			settings.template_index = m_new_project_template_index;
			settings.create_start_scene = m_new_project_create_start_scene;
			settings.open_after_create = m_new_project_open_after_create;

			if (settings.name.empty())
			{
				m_new_project_error = "Project name is required.";
				return false;
			}
			if (settings.location.empty())
			{
				m_new_project_error = "Project location is required.";
				return false;
			}
			if (settings.create_start_scene && settings.initial_scene_name.empty())
			{
				m_new_project_error = "Initial scene name is required.";
				return false;
			}
			return true;
		}

		static std::string sanitize_preview_token(std::string value)
		{
			value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c)
				{
					return !std::isalnum(c) && c != '_' && c != '-' && c != ' ';
				}), value.end());

			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
				value.erase(value.begin());
			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
				value.pop_back();
			if (value.empty())
				value = "Untitled";
			for (char& c : value)
				if (c == ' ')
					c = '_';
			return value;
		}

		static void draw_field_label(const char* label)
		{
			ImGui::TextColored(ImVec4(0.62f, 0.66f, 0.65f, 1.0f), "%s", label);
		}

		static void copy_to_buffer(char* destination, size_t destination_size, const std::string& value)
		{
			if (destination_size == 0)
				return;

			std::memset(destination, 0, destination_size);
			std::strncpy(destination, value.c_str(), destination_size - 1);
		}

		bool draw_template_card(int index, float width)
		{
			const bool selected = m_new_project_template_index == index;
			const ImVec2 cursor = ImGui::GetCursorScreenPos();
			const ImVec2 size(width, 74.0f);
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			const ImU32 bg = ImGui::ColorConvertFloat4ToU32(selected ? ImVec4(0.13f, 0.23f, 0.21f, 1.0f) : ImVec4(0.08f, 0.09f, 0.09f, 1.0f));
			const ImU32 border = ImGui::ColorConvertFloat4ToU32(selected ? ImVec4(0.36f, 0.82f, 0.72f, 1.0f) : ImVec4(0.17f, 0.19f, 0.19f, 1.0f));
			draw_list->AddRectFilled(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y), bg, 7.0f);
			draw_list->AddRect(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y), border, 7.0f, 0, selected ? 1.6f : 1.0f);
			if (selected)
				draw_list->AddRectFilled(cursor, ImVec2(cursor.x + 4.0f, cursor.y + size.y), ImGui::ColorConvertFloat4ToU32(ImVec4(0.36f, 0.82f, 0.72f, 1.0f)), 7.0f, ImDrawFlags_RoundCornersLeft);

			ImGui::InvisibleButton((std::string("##TemplateCard") + std::to_string(index)).c_str(), size);
			const bool clicked = ImGui::IsItemClicked();
			if (clicked)
				m_new_project_template_index = index;

			const ImVec2 title_pos(cursor.x + 16.0f, cursor.y + 12.0f);
			const ImVec2 description_pos(cursor.x + 16.0f, cursor.y + 34.0f);
			draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), title_pos, ImGui::GetColorU32(ImGuiCol_Text), templates[index].name);
			draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), description_pos, ImGui::GetColorU32(ImGuiCol_TextDisabled), templates[index].description, nullptr, size.x - 30.0f);
			return clicked;
		}

		void draw_new_project_wizard()
		{
			if (!m_new_project_modal_open)
				return;

			if (m_new_project_popup_requested)
			{
				if (ImGui::GetFrameCount() <= m_new_project_popup_request_frame)
					return;

				ImGui::OpenPopup("New Project");
				m_new_project_popup_requested = false;
			}
			ImGui::SetNextWindowSize(ImVec2(820.0f, 560.0f), ImGuiCond_Appearing);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 10.0f));
			if (ImGui::BeginPopupModal("New Project", &m_new_project_modal_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar))
			{
				const ImVec2 window_pos = ImGui::GetWindowPos();
				const ImVec2 window_size = ImGui::GetWindowSize();
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				draw_list->AddRectFilled(window_pos, ImVec2(window_pos.x + window_size.x, window_pos.y + 76.0f), ImGui::ColorConvertFloat4ToU32(ImVec4(0.06f, 0.07f, 0.07f, 1.0f)), 8.0f, ImDrawFlags_RoundCornersTop);
				draw_list->AddLine(ImVec2(window_pos.x, window_pos.y + 76.0f), ImVec2(window_pos.x + window_size.x, window_pos.y + 76.0f), ImGui::ColorConvertFloat4ToU32(ImVec4(0.18f, 0.20f, 0.20f, 1.0f)));

				ImGui::SetCursorPos(ImVec2(24.0f, 18.0f));
				ImGui::TextUnformatted("Create Whip Project");
				ImGui::SetCursorPosX(24.0f);
				ImGui::TextDisabled("A ready-to-build Whip project with a C# solution.");

				ImGui::SetCursorPos(ImVec2(24.0f, 98.0f));
				ImGui::BeginChild("##TemplateRail", ImVec2(260.0f, 378.0f), false);
				draw_field_label("Template");
				ImGui::Spacing();
				const float card_width = ImGui::GetContentRegionAvail().x;
				for (int i = 0; i < static_cast<int>(templates.size()); ++i)
					draw_template_card(i, card_width);
				ImGui::EndChild();

				ImGui::SetCursorPos(ImVec2(306.0f, 98.0f));
				ImGui::BeginChild("##ProjectDetails", ImVec2(490.0f, 378.0f), false);
				draw_field_label("Project Details");
				ImGui::Spacing();

				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputText("Project Name", m_new_project_name, sizeof(m_new_project_name));

				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 94.0f);
				ImGui::InputText("Location", m_new_project_location, sizeof(m_new_project_location));
				ImGui::SameLine();
				if (ImGui::Button("Browse", ImVec2(84.0f, 0.0f)))
				{
					std::string folder = file_dialogs::open_folder();
					if (!folder.empty())
						copy_to_buffer(m_new_project_location, sizeof(m_new_project_location), folder);
				}

				ImGui::Checkbox("Create startup scene", &m_new_project_create_start_scene);
				ImGui::BeginDisabled(!m_new_project_create_start_scene);
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputText("Startup Scene", m_new_project_scene_name, sizeof(m_new_project_scene_name));
				ImGui::EndDisabled();
				ImGui::Checkbox("Open after create", &m_new_project_open_after_create);

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				draw_field_label("Generated C# Workspace");
				const std::string project_stem = sanitize_preview_token(m_new_project_name);
				ImGui::TextDisabled("Assets/Scripts/%s.sln", project_stem.c_str());
				ImGui::TextDisabled("Assets/Scripts/%s.csproj", project_stem.c_str());
				ImGui::TextDisabled("Assets/Scripts/Whip-ScriptCore/Whip-ScriptCore.csproj");

				if (!m_new_project_error.empty())
				{
					ImGui::Spacing();
					ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "%s", m_new_project_error.c_str());
				}
				ImGui::EndChild();

				draw_list->AddRectFilled(ImVec2(window_pos.x, window_pos.y + window_size.y - 64.0f), ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y), ImGui::ColorConvertFloat4ToU32(ImVec4(0.055f, 0.06f, 0.06f, 1.0f)), 8.0f, ImDrawFlags_RoundCornersBottom);
				draw_list->AddLine(ImVec2(window_pos.x, window_pos.y + window_size.y - 64.0f), ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y - 64.0f), ImGui::ColorConvertFloat4ToU32(ImVec4(0.18f, 0.20f, 0.20f, 1.0f)));
				ImGui::SetCursorPos(ImVec2(window_size.x - 270.0f, window_size.y - 44.0f));
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.49f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.65f, 0.58f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.40f, 1.0f));
				if (ImGui::Button("Create Project", ImVec2(136.0f, 30.0f)))
				{
					project_create_settings settings;
					if (validate_new_project(settings))
					{
						m_status.clear();
						if (m_create_project_callback && m_create_project_callback(settings))
						{
							m_loaded = settings.open_after_create;
							if (!settings.open_after_create)
								m_status = "Project created.";
							m_new_project_modal_open = false;
							m_new_project_popup_requested = false;
							m_new_project_popup_request_frame = -1;
							ImGui::CloseCurrentPopup();
						}
						else
						{
							m_new_project_error = "Project could not be created.";
							m_status = m_new_project_error;
						}
					}
				}
				ImGui::PopStyleColor(3);
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(112.0f, 30.0f)))
				{
					m_new_project_modal_open = false;
					m_new_project_popup_requested = false;
					m_new_project_popup_request_frame = -1;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
			else if (!ImGui::IsPopupOpen("New Project"))
			{
				m_new_project_modal_open = false;
				m_new_project_popup_requested = false;
				m_new_project_popup_request_frame = -1;
			}
			ImGui::PopStyleVar(2);
		}

		void draw_delete_project_modal()
		{
			if (!m_project_delete_modal_open)
				return;

			if (m_project_delete_popup_requested)
			{
				if (ImGui::GetFrameCount() <= m_project_delete_popup_request_frame)
					return;

				ImGui::OpenPopup("Delete Project");
				m_project_delete_popup_requested = false;
			}

			ImGui::SetNextWindowSize(ImVec2(500.0f, 0.0f), ImGuiCond_Appearing);
			if (ImGui::BeginPopupModal("Delete Project", &m_project_delete_modal_open, ImGuiWindowFlags_AlwaysAutoResize))
			{
				const std::string project_name = m_project_delete_path.stem().empty() ? m_project_delete_path.filename().string() : m_project_delete_path.stem().string();
				ImGui::TextUnformatted("Delete project?");
				ImGui::TextDisabled("%s", project_name.c_str());
				ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + 460.0f);
				ImGui::TextWrapped("%s", m_project_delete_path.string().c_str());
				ImGui::PopTextWrapPos();
				ImGui::Spacing();
				ImGui::TextWrapped("Forget removes it from the recent list. Delete Project removes the project folder from disk.");

				if (!m_project_delete_error.empty())
				{
					ImGui::Spacing();
					ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "%s", m_project_delete_error.c_str());
				}

				ImGui::Spacing();
				ImGui::BeginDisabled(!m_forget_recent_project_callback);
				if (ImGui::Button("Forget", ImVec2(118.0f, 0.0f)))
				{
					if (on_project_management_action(m_forget_recent_project_callback, m_project_delete_path, "Project forgotten."))
					{
						m_project_delete_modal_open = false;
						ImGui::CloseCurrentPopup();
					}
					else
					{
						m_project_delete_error = "Project could not be removed from recents.";
					}
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::BeginDisabled(!m_delete_recent_project_callback);
				if (ImGui::Button("Delete Project", ImVec2(140.0f, 0.0f)))
				{
					if (on_project_management_action(m_delete_recent_project_callback, m_project_delete_path, "Project deleted."))
					{
						m_project_delete_modal_open = false;
						ImGui::CloseCurrentPopup();
					}
					else
					{
						m_project_delete_error = "Project could not be deleted.";
					}
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(118.0f, 0.0f)))
				{
					m_project_delete_modal_open = false;
					m_project_delete_popup_requested = false;
					m_project_delete_popup_request_frame = -1;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
			else if (!ImGui::IsPopupOpen("Delete Project"))
			{
				m_project_delete_modal_open = false;
				m_project_delete_popup_requested = false;
				m_project_delete_popup_request_frame = -1;
			}
		}

		void on_project_action(const callback& action)
		{
			if (!action)
				return;

			m_status.clear();
			if (action())
				m_loaded = true;
			else
				m_status = "Project was not opened.";
		}

		bool on_project_management_action(const project_callback& action, const std::filesystem::path& path, const char* success_status)
		{
			if (!action)
				return false;

			m_status.clear();
			if (!action(path))
				return false;

			m_status = success_status;
			return true;
		}

		bool m_loaded = false;
		create_project_callback m_create_project_callback;
		callback m_load_project_callback;
		project_callback m_open_recent_project_callback;
		project_callback m_forget_recent_project_callback;
		project_callback m_delete_recent_project_callback;
		std::vector<std::filesystem::path> m_recent_projects;
		std::string m_status;
		bool m_new_project_modal_open = false;
		bool m_new_project_popup_requested = false;
		int m_new_project_popup_request_frame = -1;
		char m_new_project_name[128] = "Untitled";
		char m_new_project_location[260] = "";
		char m_new_project_scene_name[128] = "Main";
		int m_new_project_template_index = 1;
		bool m_new_project_create_start_scene = true;
		bool m_new_project_open_after_create = true;
		std::string m_new_project_error;
		std::filesystem::path m_project_delete_path;
		bool m_project_delete_modal_open = false;
		bool m_project_delete_popup_requested = false;
		int m_project_delete_popup_request_frame = -1;
		std::string m_project_delete_error;
	};
}

_WHIP_END
