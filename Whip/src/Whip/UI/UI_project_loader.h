#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <Whip/Core/Application.h>
#include <Whip/Project/project.h>

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
	class UI_project_loader
	{
	public:
		using callback = std::function<bool()>;
		using project_callback = std::function<bool(const std::filesystem::path&)>;

		void set_create_project_callback(callback create_callback)
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
				on_project_action(m_create_project_callback);
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
			else if (ImGui::BeginTable("RecentProjectsTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Project", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 88.0f);

				for (size_t i = 0; i < m_recent_projects.size(); ++i)
				{
					const std::filesystem::path& project_path = m_recent_projects[i];
					const std::string project_name = project_path.stem().empty() ? project_path.filename().string() : project_path.stem().string();
					const std::string button_id = "Open##recent_project_" + std::to_string(i);

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
				}

				ImGui::EndTable();
			}

			ImGui::EndChild();
			ImGui::PopStyleVar();
			ImGui::End();
			ImGui::PopStyleVar();
		}

	private:
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

		bool m_loaded = false;
		callback m_create_project_callback;
		callback m_load_project_callback;
		project_callback m_open_recent_project_callback;
		std::vector<std::filesystem::path> m_recent_projects;
		std::string m_status;
	};
}

_WHIP_END
