#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>
#include <Whip/Core/Application.h>
#include <Whip/Project/Project.h>
#include <Whip/Utils/PlatformUtils.h>

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

#include "UIPopupHandler.h"

_WHIP_START

namespace UI
{
	struct ProjectTemplateOption
	{
		const char* m_Name;
		const char* m_Description;
	};

	struct ProjectCreateSettings
	{
		std::string m_Name;
		std::filesystem::path m_Location;
		std::string m_InitialSceneName;
		int m_TemplateIndex = 0;
		bool m_CreateStartScene = true;
		bool m_OpenAfterCreate = true;
	};

	class UIProjectLoader
	{
	public:
		using Callback = std::function<bool()>;
		using CreateProjectCallback = std::function<bool(const ProjectCreateSettings&)>;
		using ProjectCallback = std::function<bool(const std::filesystem::path&)>;

		void SetCreateProjectCallback(CreateProjectCallback createCallback)
		{
			m_CreateProjectCallback = std::move(createCallback);
		}

		void SetLoadProjectCallback(Callback loadCallback)
		{
			m_LoadProjectCallback = std::move(loadCallback);
		}

		void SetOpenRecentProjectCallback(ProjectCallback callback)
		{
			m_OpenRecentProjectCallback = std::move(callback);
		}

		void SetForgetRecentProjectCallback(ProjectCallback callback)
		{
			m_ForgetRecentProjectCallback = std::move(callback);
		}

		void SetDeleteRecentProjectCallback(ProjectCallback callback)
		{
			m_DeleteRecentProjectCallback = std::move(callback);
		}

		void SetRecentProjects(std::vector<std::filesystem::path> recentProjects)
		{
			m_RecentProjects = std::move(recentProjects);
		}

		void SetLoaded(bool loaded)
		{
			m_Loaded = loaded;
		}

		void SetStatus(std::string status)
		{
			m_Status = std::move(status);
		}

		bool Loaded() const { return m_Loaded; }
		void Reset()
		{
			m_Loaded = false;
			m_Status.clear();
		}

		void Run()
		{
			if (m_Loaded)
				return;

			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			const ImVec2 hubSize{
				viewport->WorkSize.x < 760.0f ? viewport->WorkSize.x - 32.0f : 700.0f,
				viewport->WorkSize.y < 480.0f ? viewport->WorkSize.y - 32.0f : 420.0f
			};
			ImGui::SetNextWindowSize(hubSize, ImGuiCond_Always);
			ImGui::SetNextWindowPos(
				ImVec2(viewport->WorkPos.x + (viewport->WorkSize.x - hubSize.x) * 0.5f, viewport->WorkPos.y + (viewport->WorkSize.y - hubSize.y) * 0.5f),
				ImGuiCond_Always);

			const ImGuiWindowFlags windowFlags =
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
			ImGui::Begin("Whip Hub", nullptr, windowFlags);

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 10.0f));
			ImGui::TextUnformatted("Whip Hub");
			ImGui::SameLine();
			ImGui::TextDisabled("Project Workspace");
			ImGui::Separator();

			const float actionsWidth = 220.0f;
			ImGui::BeginChild("WhipHubActions", ImVec2(actionsWidth, 0.0f), true);
			ImGui::TextUnformatted("Project");
			ImGui::Spacing();

			ImGui::BeginDisabled(!m_LoadProjectCallback);
			if (ImGui::Button("Open Project", ImVec2(-1.0f, 40.0f)))
				OnProjectAction(m_LoadProjectCallback);
			ImGui::EndDisabled();

			ImGui::BeginDisabled(!m_CreateProjectCallback);
			if (ImGui::Button("New Project", ImVec2(-1.0f, 40.0f)))
				OpenNewProjectWizard();
			ImGui::EndDisabled();

			if (!m_Status.empty())
			{
				ImGui::Separator();
				ImGui::TextWrapped("%s", m_Status.c_str());
			}

			ImGui::EndChild();
			ImGui::SameLine();

			ImGui::BeginChild("WhipHubRecentProjects", ImVec2(0.0f, 0.0f), true);
			ImGui::TextUnformatted("Recent Projects");
			ImGui::Separator();

			if (m_RecentProjects.empty())
			{
				ImGui::TextWrapped("No recent projects yet.");
			}
			else if (ImGui::BeginTable("RecentProjectsTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Project", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Open", ImGuiTableColumnFlags_WidthFixed, 78.0f);
				ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed, 78.0f);

				for (size_t i = 0; i < m_RecentProjects.size(); ++i)
				{
					const std::filesystem::path& projectPath = m_RecentProjects[i];
					const std::string projectName = projectPath.stem().empty() ? projectPath.filename().string() : projectPath.stem().string();
					const std::string buttonId = "Open##recent_project_" + std::to_string(i);
					const std::string deleteButtonId = "Delete##recent_project_" + std::to_string(i);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(projectName.c_str());
					ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x);
					ImGui::TextDisabled("%s", projectPath.string().c_str());
					ImGui::PopTextWrapPos();

					ImGui::TableSetColumnIndex(1);
					ImGui::BeginDisabled(!m_OpenRecentProjectCallback);
					if (ImGui::Button(buttonId.c_str(), ImVec2(-1.0f, 0.0f)))
						OnProjectAction([this, projectPath]() { return m_OpenRecentProjectCallback(projectPath); });
					ImGui::EndDisabled();

					ImGui::TableSetColumnIndex(2);
					ImGui::BeginDisabled(!m_ForgetRecentProjectCallback && !m_DeleteRecentProjectCallback);
					if (ImGui::Button(deleteButtonId.c_str(), ImVec2(-1.0f, 0.0f)))
						RequestRecentProjectDelete(projectPath);
					ImGui::EndDisabled();
				}

				ImGui::EndTable();
			}

			ImGui::EndChild();
			ImGui::PopStyleVar();
			DrawNewProjectWizard();
			DrawDeleteProjectModal();
			ImGui::End();
			ImGui::PopStyleVar();
		}

	private:
		static constexpr std::array<ProjectTemplateOption, 3> s_Templates =
		{
			ProjectTemplateOption{ "Empty", "Folders, C# solution, and a clean startup scene." },
			ProjectTemplateOption{ "2D Starter", "C# solution, Camera, and starter sprite." },
			ProjectTemplateOption{ "Script Ready", "Starter scene with a script-bound entity." }
		};

		void OpenNewProjectWizard()
		{
			if (std::strlen(m_NewProjectName) == 0)
				CopyToBuffer(m_NewProjectName, sizeof(m_NewProjectName), "Untitled");
			if (std::strlen(m_NewProjectSceneName) == 0)
				CopyToBuffer(m_NewProjectSceneName, sizeof(m_NewProjectSceneName), "Main");
			if (std::strlen(m_NewProjectLocation) == 0)
			{
				std::filesystem::path defaultLocation = std::filesystem::current_path().parent_path();
				if (defaultLocation.empty())
					defaultLocation = std::filesystem::current_path();
				std::string location = defaultLocation.string();
				CopyToBuffer(m_NewProjectLocation, sizeof(m_NewProjectLocation), location);
			}
			m_NewProjectError.clear();
			m_NewProjectModalOpen = true;
			m_NewProjectPopupRequested = true;
			m_NewProjectPopupRequestFrame = ImGui::GetFrameCount();
		}

		void RequestRecentProjectDelete(const std::filesystem::path& path)
		{
			m_ProjectDeletePath = path;
			m_ProjectDeleteError.clear();
			m_ProjectDeleteModalOpen = true;
			m_ProjectDeletePopupRequested = true;
			m_ProjectDeletePopupRequestFrame = ImGui::GetFrameCount();
		}

		bool ValidateNewProject(ProjectCreateSettings& settings)
		{
			settings.m_Name = m_NewProjectName;
			settings.m_Location = m_NewProjectLocation;
			settings.m_InitialSceneName = m_NewProjectSceneName;
			settings.m_TemplateIndex = m_NewProjectTemplateIndex;
			settings.m_CreateStartScene = m_NewProjectCreateStartScene;
			settings.m_OpenAfterCreate = m_NewProjectOpenAfterCreate;

			if (settings.m_Name.empty())
			{
				m_NewProjectError = "Project name is required.";
				return false;
			}
			if (settings.m_Location.empty())
			{
				m_NewProjectError = "Project location is required.";
				return false;
			}
			if (settings.m_CreateStartScene && settings.m_InitialSceneName.empty())
			{
				m_NewProjectError = "Initial scene name is required.";
				return false;
			}
			return true;
		}

		static std::string SanitizePreviewToken(std::string value)
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

		static void DrawFieldLabel(const char* label)
		{
			ImGui::TextColored(ImVec4(0.62f, 0.66f, 0.65f, 1.0f), "%s", label);
		}

		static void CopyToBuffer(char* destination, size_t destinationSize, const std::string& value)
		{
			if (destinationSize == 0)
				return;

			std::memset(destination, 0, destinationSize);
			std::strncpy(destination, value.c_str(), destinationSize - 1);
		}

		bool DrawTemplateCard(int index, float width)
		{
			const bool selected = m_NewProjectTemplateIndex == index;
			const ImVec2 cursor = ImGui::GetCursorScreenPos();
			const ImVec2 size(width, 74.0f);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const ImU32 bg = ImGui::ColorConvertFloat4ToU32(selected ? ImVec4(0.13f, 0.23f, 0.21f, 1.0f) : ImVec4(0.08f, 0.09f, 0.09f, 1.0f));
			const ImU32 border = ImGui::ColorConvertFloat4ToU32(selected ? ImVec4(0.36f, 0.82f, 0.72f, 1.0f) : ImVec4(0.17f, 0.19f, 0.19f, 1.0f));
			drawList->AddRectFilled(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y), bg, 7.0f);
			drawList->AddRect(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y), border, 7.0f, 0, selected ? 1.6f : 1.0f);
			if (selected)
				drawList->AddRectFilled(cursor, ImVec2(cursor.x + 4.0f, cursor.y + size.y), ImGui::ColorConvertFloat4ToU32(ImVec4(0.36f, 0.82f, 0.72f, 1.0f)), 7.0f, ImDrawFlags_RoundCornersLeft);

			ImGui::InvisibleButton((std::string("##TemplateCard") + std::to_string(index)).c_str(), size);
			const bool clicked = ImGui::IsItemClicked();
			if (clicked)
				m_NewProjectTemplateIndex = index;

			const ImVec2 titlePos(cursor.x + 16.0f, cursor.y + 12.0f);
			const ImVec2 descriptionPos(cursor.x + 16.0f, cursor.y + 34.0f);
			drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), titlePos, ImGui::GetColorU32(ImGuiCol_Text), s_Templates[index].m_Name);
			drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), descriptionPos, ImGui::GetColorU32(ImGuiCol_TextDisabled), s_Templates[index].m_Description, nullptr, size.x - 30.0f);
			return clicked;
		}

		void DrawNewProjectWizard()
		{
			if (!m_NewProjectModalOpen)
				return;

			if (m_NewProjectPopupRequested)
			{
				if (ImGui::GetFrameCount() <= m_NewProjectPopupRequestFrame)
					return;

				ImGui::OpenPopup("New Project");
				m_NewProjectPopupRequested = false;
			}
			ImGui::SetNextWindowSize(ImVec2(980.0f, 640.0f), ImGuiCond_Appearing);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 10.0f));
			if (ImGui::BeginPopupModal("New Project", &m_NewProjectModalOpen, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar))
			{
				const ImVec2 windowPos = ImGui::GetWindowPos();
				const ImVec2 windowSize = ImGui::GetWindowSize();
				const float contentTop = 108.0f;
				const float contentHeight = windowSize.y - 204.0f;
				const float templateWidth = 280.0f;
				const float detailsX = 340.0f;
				const float detailsWidth = windowSize.x - detailsX - 28.0f;
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				drawList->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + 84.0f), ImGui::ColorConvertFloat4ToU32(ImVec4(0.06f, 0.07f, 0.07f, 1.0f)), 8.0f, ImDrawFlags_RoundCornersTop);
				drawList->AddLine(ImVec2(windowPos.x, windowPos.y + 84.0f), ImVec2(windowPos.x + windowSize.x, windowPos.y + 84.0f), ImGui::ColorConvertFloat4ToU32(ImVec4(0.18f, 0.20f, 0.20f, 1.0f)));

				ImGui::SetCursorPos(ImVec2(24.0f, 18.0f));
				ImGui::TextUnformatted("Create Whip Project");
				ImGui::SetCursorPosX(24.0f);
				ImGui::TextDisabled("A ready-to-build Whip project with a C# solution.");

				ImGui::SetCursorPos(ImVec2(24.0f, contentTop));
				ImGui::BeginChild("##TemplateRail", ImVec2(templateWidth, contentHeight), false);
				DrawFieldLabel("Template");
				ImGui::Spacing();
				const float cardWidth = ImGui::GetContentRegionAvail().x;
				for (int i = 0; i < static_cast<int>(s_Templates.size()); ++i)
					DrawTemplateCard(i, cardWidth);
				ImGui::EndChild();

				ImGui::SetCursorPos(ImVec2(detailsX, contentTop));
				ImGui::BeginChild("##ProjectDetails", ImVec2(detailsWidth, contentHeight), false);
				DrawFieldLabel("Project Details");
				ImGui::Spacing();

				DrawFieldLabel("Project Name");
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputText("##NewProjectName", m_NewProjectName, sizeof(m_NewProjectName));

				DrawFieldLabel("Location");
				const float browseWidth = 104.0f;
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth - ImGui::GetStyle().ItemSpacing.x);
				ImGui::InputText("##NewProjectLocation", m_NewProjectLocation, sizeof(m_NewProjectLocation));
				ImGui::SameLine();
				if (ImGui::Button("Browse##NewProjectBrowse", ImVec2(browseWidth, 0.0f)))
				{
					std::string folder = FileDialogs::OpenFolder();
					if (!folder.empty())
						CopyToBuffer(m_NewProjectLocation, sizeof(m_NewProjectLocation), folder);
				}

				ImGui::Checkbox("Create startup scene", &m_NewProjectCreateStartScene);
				ImGui::BeginDisabled(!m_NewProjectCreateStartScene);
				DrawFieldLabel("Startup Scene");
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputText("##NewProjectStartupScene", m_NewProjectSceneName, sizeof(m_NewProjectSceneName));
				ImGui::EndDisabled();
				ImGui::Checkbox("Open after create", &m_NewProjectOpenAfterCreate);

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				DrawFieldLabel("Generated C# Workspace");
				const std::string projectStem = SanitizePreviewToken(m_NewProjectName);
				ImGui::TextDisabled("Assets/Scripts/%s.sln", projectStem.c_str());
				ImGui::TextDisabled("Assets/Scripts/%s.csproj", projectStem.c_str());
				ImGui::TextDisabled("Assets/Scripts/Whip-ScriptCore/Whip-ScriptCore.csproj");

				if (!m_NewProjectError.empty())
				{
					ImGui::Spacing();
					ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "%s", m_NewProjectError.c_str());
				}
				ImGui::EndChild();

				drawList->AddRectFilled(ImVec2(windowPos.x, windowPos.y + windowSize.y - 64.0f), ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), ImGui::ColorConvertFloat4ToU32(ImVec4(0.055f, 0.06f, 0.06f, 1.0f)), 8.0f, ImDrawFlags_RoundCornersBottom);
				drawList->AddLine(ImVec2(windowPos.x, windowPos.y + windowSize.y - 64.0f), ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y - 64.0f), ImGui::ColorConvertFloat4ToU32(ImVec4(0.18f, 0.20f, 0.20f, 1.0f)));
				ImGui::SetCursorPos(ImVec2(windowSize.x - 296.0f, windowSize.y - 44.0f));
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.49f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.65f, 0.58f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.40f, 1.0f));
				if (ImGui::Button("Create Project", ImVec2(156.0f, 30.0f)))
				{
					ProjectCreateSettings settings;
					if (ValidateNewProject(settings))
					{
						m_Status.clear();
						if (m_CreateProjectCallback && m_CreateProjectCallback(settings))
						{
							m_Loaded = settings.m_OpenAfterCreate;
							if (!settings.m_OpenAfterCreate)
								m_Status = "Project created.";
							m_NewProjectModalOpen = false;
							m_NewProjectPopupRequested = false;
							m_NewProjectPopupRequestFrame = -1;
							ImGui::CloseCurrentPopup();
						}
						else
						{
							m_NewProjectError = "Project could not be created.";
							m_Status = m_NewProjectError;
						}
					}
				}
				ImGui::PopStyleColor(3);
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(112.0f, 30.0f)))
				{
					m_NewProjectModalOpen = false;
					m_NewProjectPopupRequested = false;
					m_NewProjectPopupRequestFrame = -1;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
			else if (!ImGui::IsPopupOpen("New Project"))
			{
				m_NewProjectModalOpen = false;
				m_NewProjectPopupRequested = false;
				m_NewProjectPopupRequestFrame = -1;
			}
			ImGui::PopStyleVar(2);
		}

		void DrawDeleteProjectModal()
		{
			if (!m_ProjectDeleteModalOpen)
				return;

			if (m_ProjectDeletePopupRequested)
			{
				if (ImGui::GetFrameCount() <= m_ProjectDeletePopupRequestFrame)
					return;

				ImGui::OpenPopup("Delete Project");
				m_ProjectDeletePopupRequested = false;
			}

			ImGui::SetNextWindowSize(ImVec2(500.0f, 0.0f), ImGuiCond_Appearing);
			if (ImGui::BeginPopupModal("Delete Project", &m_ProjectDeleteModalOpen, ImGuiWindowFlags_AlwaysAutoResize))
			{
				const std::string projectName = m_ProjectDeletePath.stem().empty() ? m_ProjectDeletePath.filename().string() : m_ProjectDeletePath.stem().string();
				ImGui::TextUnformatted("Delete project?");
				ImGui::TextDisabled("%s", projectName.c_str());
				ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + 460.0f);
				ImGui::TextWrapped("%s", m_ProjectDeletePath.string().c_str());
				ImGui::PopTextWrapPos();
				ImGui::Spacing();
				ImGui::TextWrapped("Forget removes it from the recent list. Delete Project removes the project folder from disk.");

				if (!m_ProjectDeleteError.empty())
				{
					ImGui::Spacing();
					ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "%s", m_ProjectDeleteError.c_str());
				}

				ImGui::Spacing();
				ImGui::BeginDisabled(!m_ForgetRecentProjectCallback);
				if (ImGui::Button("Forget", ImVec2(118.0f, 0.0f)))
				{
					if (OnProjectManagementAction(m_ForgetRecentProjectCallback, m_ProjectDeletePath, "Project forgotten."))
					{
						m_ProjectDeleteModalOpen = false;
						ImGui::CloseCurrentPopup();
					}
					else
					{
						m_ProjectDeleteError = "Project could not be removed from recents.";
					}
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::BeginDisabled(!m_DeleteRecentProjectCallback);
				if (ImGui::Button("Delete Project", ImVec2(140.0f, 0.0f)))
				{
					if (OnProjectManagementAction(m_DeleteRecentProjectCallback, m_ProjectDeletePath, "Project deleted."))
					{
						m_ProjectDeleteModalOpen = false;
						ImGui::CloseCurrentPopup();
					}
					else
					{
						m_ProjectDeleteError = "Project could not be deleted.";
					}
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(118.0f, 0.0f)))
				{
					m_ProjectDeleteModalOpen = false;
					m_ProjectDeletePopupRequested = false;
					m_ProjectDeletePopupRequestFrame = -1;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
			else if (!ImGui::IsPopupOpen("Delete Project"))
			{
				m_ProjectDeleteModalOpen = false;
				m_ProjectDeletePopupRequested = false;
				m_ProjectDeletePopupRequestFrame = -1;
			}
		}

		void OnProjectAction(const Callback& action)
		{
			if (!action)
				return;

			m_Status.clear();
			if (action())
				m_Loaded = true;
		}

		bool OnProjectManagementAction(const ProjectCallback& action, const std::filesystem::path& path, const char* successStatus)
		{
			if (!action)
				return false;

			m_Status.clear();
			if (!action(path))
				return false;

			m_Status = successStatus;
			return true;
		}

		bool m_Loaded = false;
		CreateProjectCallback m_CreateProjectCallback;
		Callback m_LoadProjectCallback;
		ProjectCallback m_OpenRecentProjectCallback;
		ProjectCallback m_ForgetRecentProjectCallback;
		ProjectCallback m_DeleteRecentProjectCallback;
		std::vector<std::filesystem::path> m_RecentProjects;
		std::string m_Status;
		bool m_NewProjectModalOpen = false;
		bool m_NewProjectPopupRequested = false;
		int m_NewProjectPopupRequestFrame = -1;
		char m_NewProjectName[128] = "Untitled";
		char m_NewProjectLocation[260] = "";
		char m_NewProjectSceneName[128] = "Main";
		int m_NewProjectTemplateIndex = 1;
		bool m_NewProjectCreateStartScene = true;
		bool m_NewProjectOpenAfterCreate = true;
		std::string m_NewProjectError;
		std::filesystem::path m_ProjectDeletePath;
		bool m_ProjectDeleteModalOpen = false;
		bool m_ProjectDeletePopupRequested = false;
		int m_ProjectDeletePopupRequestFrame = -1;
		std::string m_ProjectDeleteError;
	};
}

_WHIP_END
