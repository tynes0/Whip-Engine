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
			const float hubWidth = std::clamp(viewport->WorkSize.x - 128.0f, 860.0f, 1180.0f);
			const float hubHeight = std::clamp(viewport->WorkSize.y - 150.0f, 560.0f, 720.0f);
			const ImVec2 hubSize{
				viewport->WorkSize.x < 940.0f ? viewport->WorkSize.x - 32.0f : hubWidth,
				viewport->WorkSize.y < 620.0f ? viewport->WorkSize.y - 32.0f : hubHeight
			};
			ImGui::SetNextWindowSize(hubSize, ImGuiCond_Always);
			ImGui::SetNextWindowPos(
				ImVec2(viewport->WorkPos.x + (viewport->WorkSize.x - hubSize.x) * 0.5f, viewport->WorkPos.y + (viewport->WorkSize.y - hubSize.y) * 0.5f),
				ImGuiCond_Always);

			const ImGuiWindowFlags windowFlags =
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.030f, 0.040f, 0.047f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.045f, 0.058f, 0.067f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.145f, 0.220f, 0.270f, 1.0f));
			ImGui::Begin("Whip Hub", nullptr, windowFlags);

			const ImVec2 windowPos = ImGui::GetWindowPos();
			const ImVec2 windowSize = ImGui::GetWindowSize();
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + 116.0f), IM_COL32(7, 14, 18, 255), 6.0f, ImDrawFlags_RoundCornersTop);
			drawList->AddRectFilledMultiColor(
				ImVec2(windowPos.x, windowPos.y),
				ImVec2(windowPos.x + windowSize.x, windowPos.y + 116.0f),
				IM_COL32(14, 26, 34, 255),
				IM_COL32(9, 16, 22, 255),
				IM_COL32(6, 11, 15, 255),
				IM_COL32(10, 22, 28, 255));
			drawList->AddLine(ImVec2(windowPos.x, windowPos.y + 116.0f), ImVec2(windowPos.x + windowSize.x, windowPos.y + 116.0f), IM_COL32(54, 78, 92, 210), 1.0f);

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 10.0f));
			ImGui::SetCursorPos(ImVec2(28.0f, 24.0f));
			ImGui::InvisibleButton("##WhipHubLogo", ImVec2(44.0f, 44.0f));
			const ImVec2 logoPos = ImGui::GetItemRectMin();
			drawList->AddRectFilled(logoPos, ImVec2(logoPos.x + 44.0f, logoPos.y + 44.0f), IM_COL32(18, 30, 37, 255), 5.0f);
			drawList->AddRect(logoPos, ImVec2(logoPos.x + 44.0f, logoPos.y + 44.0f), IM_COL32(73, 127, 159, 230), 5.0f, 0, 1.2f);
			drawList->AddLine(ImVec2(logoPos.x + 13.0f, logoPos.y + 14.0f), ImVec2(logoPos.x + 31.0f, logoPos.y + 22.0f), IM_COL32(232, 238, 240, 255), 4.0f);
			drawList->AddLine(ImVec2(logoPos.x + 31.0f, logoPos.y + 22.0f), ImVec2(logoPos.x + 15.0f, logoPos.y + 32.0f), IM_COL32(232, 238, 240, 255), 4.0f);
			drawList->AddLine(ImVec2(logoPos.x + 15.0f, logoPos.y + 32.0f), ImVec2(logoPos.x + 13.0f, logoPos.y + 14.0f), IM_COL32(105, 169, 202, 255), 3.0f);

			ImGui::SetCursorPos(ImVec2(86.0f, 22.0f));
			ImGui::TextUnformatted("Whip Hub");
			ImGui::SetCursorPosX(86.0f);
			ImGui::TextDisabled("Project launcher and workspace setup");
			ImGui::SetCursorPos(ImVec2(86.0f, 72.0f));
			DrawHubTag("Fast startup");
			ImGui::SameLine();
			DrawHubTag("C# workspace");
			ImGui::SameLine();
			DrawHubTag("Scene templates");

			const float contentTop = 140.0f;
			const float footerHeight = 48.0f;
			const float actionsWidth = 292.0f;
			const float contentHeight = windowSize.y - contentTop - footerHeight - 24.0f;
			ImGui::SetCursorPos(ImVec2(24.0f, contentTop));
			ImGui::BeginChild("WhipHubActions", ImVec2(actionsWidth, contentHeight), true, ImGuiWindowFlags_NoScrollbar);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 12.0f));
			ImGui::TextUnformatted("Start");
			ImGui::TextDisabled("Open an existing project or scaffold a new one.");
			ImGui::Spacing();

			ImGui::BeginDisabled(!m_LoadProjectCallback);
			if (DrawHubActionButton("Open Project", "Browse for a .whipproj file", true))
				OnProjectAction(m_LoadProjectCallback);
			ImGui::EndDisabled();

			ImGui::BeginDisabled(!m_CreateProjectCallback);
			if (DrawHubActionButton("New Project", "Choose template, scene, and C# workspace", false))
				OpenNewProjectWizard();
			ImGui::EndDisabled();

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::TextUnformatted("Templates");
			for (const ProjectTemplateOption& option : s_Templates)
			{
				ImGui::BulletText("%s", option.m_Name);
				ImGui::SameLine();
				ImGui::TextDisabled("%s", option.m_Description);
			}

			ImGui::PopStyleVar();
			ImGui::EndChild();

			ImGui::SameLine(0.0f, 16.0f);
			ImGui::BeginChild("WhipHubRecentProjects", ImVec2(0.0f, contentHeight), true);
			ImGui::TextUnformatted("Recent Projects");
			ImGui::SameLine();
			ImGui::TextDisabled("%d pinned in launcher", static_cast<int>(m_RecentProjects.size()));
			ImGui::Spacing();
			ImGui::Separator();

			if (m_RecentProjects.empty())
			{
				const ImVec2 emptyPos = ImGui::GetCursorScreenPos();
				const ImVec2 emptySize(ImGui::GetContentRegionAvail().x, 116.0f);
				drawList->AddRectFilled(emptyPos, ImVec2(emptyPos.x + emptySize.x, emptyPos.y + emptySize.y), IM_COL32(11, 18, 22, 255), 4.0f);
				drawList->AddRect(emptyPos, ImVec2(emptyPos.x + emptySize.x, emptyPos.y + emptySize.y), IM_COL32(42, 61, 72, 190), 4.0f);
				ImGui::Dummy(ImVec2(1.0f, 20.0f));
				ImGui::Indent(18.0f);
				ImGui::TextUnformatted("No recent projects yet.");
				ImGui::TextDisabled("Open or create a project to keep it here.");
				ImGui::Unindent(18.0f);
				ImGui::Dummy(ImVec2(1.0f, 32.0f));
			}
			else
			{
				for (size_t i = 0; i < m_RecentProjects.size(); ++i)
				{
					const std::filesystem::path& projectPath = m_RecentProjects[i];
					const std::string projectName = projectPath.stem().empty() ? projectPath.filename().string() : projectPath.stem().string();
					std::error_code error;
					const bool projectExists = std::filesystem::exists(projectPath, error);
					const float rowHeight = 72.0f;
					const ImVec2 rowPos = ImGui::GetCursorScreenPos();
					const float rowWidth = ImGui::GetContentRegionAvail().x;
					const ImVec2 rowEnd(rowPos.x + rowWidth, rowPos.y + rowHeight);
					drawList->AddRectFilled(rowPos, rowEnd, i % 2 == 0 ? IM_COL32(9, 15, 19, 255) : IM_COL32(13, 21, 25, 255), 4.0f);
					drawList->AddRect(rowPos, rowEnd, IM_COL32(40, 60, 72, 170), 4.0f);
					drawList->AddRectFilled(rowPos, ImVec2(rowPos.x + 4.0f, rowEnd.y), projectExists ? IM_COL32(73, 127, 159, 255) : IM_COL32(180, 104, 78, 255), 4.0f, ImDrawFlags_RoundCornersLeft);

					ImGui::PushID(static_cast<int>(i));
					ImGui::InvisibleButton("##RecentProjectRow", ImVec2(rowWidth, rowHeight));
					const bool rowHovered = ImGui::IsItemHovered();
					if (rowHovered)
						drawList->AddRect(rowPos, rowEnd, IM_COL32(91, 147, 180, 220), 4.0f, 0, 1.2f);
					if (rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && m_OpenRecentProjectCallback)
						OnProjectAction([this, projectPath]() { return m_OpenRecentProjectCallback(projectPath); });

					ImGui::SetCursorScreenPos(ImVec2(rowPos.x + 18.0f, rowPos.y + 12.0f));
					ImGui::TextUnformatted(projectName.c_str());
					ImGui::SetCursorScreenPos(ImVec2(rowPos.x + 18.0f, rowPos.y + 36.0f));
					ImGui::TextDisabled("%s", CompactPath(projectPath, 86).c_str());

					const char* stateText = projectExists ? "Ready" : "Missing";
					const ImVec4 stateColor = projectExists ? ImVec4(0.47f, 0.75f, 0.64f, 1.0f) : ImVec4(0.93f, 0.55f, 0.37f, 1.0f);
					ImGui::SetCursorScreenPos(ImVec2(rowEnd.x - 238.0f, rowPos.y + 15.0f));
					ImGui::TextColored(stateColor, "%s", stateText);

					ImGui::SetCursorScreenPos(ImVec2(rowEnd.x - 156.0f, rowPos.y + 19.0f));
					ImGui::BeginDisabled(!m_OpenRecentProjectCallback);
					if (ImGui::Button("Open", ImVec2(68.0f, 30.0f)))
						OnProjectAction([this, projectPath]() { return m_OpenRecentProjectCallback(projectPath); });
					ImGui::EndDisabled();
					ImGui::SameLine();
					ImGui::BeginDisabled(!m_ForgetRecentProjectCallback && !m_DeleteRecentProjectCallback);
					if (ImGui::Button("Manage", ImVec2(76.0f, 30.0f)))
						RequestRecentProjectDelete(projectPath);
					ImGui::EndDisabled();
					ImGui::PopID();
					ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowEnd.y + 8.0f));
					ImGui::Dummy(ImVec2(rowWidth, 1.0f));
				}
			}

			ImGui::EndChild();

			drawList->AddLine(ImVec2(windowPos.x, windowPos.y + windowSize.y - footerHeight), ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y - footerHeight), IM_COL32(48, 70, 82, 180), 1.0f);
			ImGui::SetCursorPos(ImVec2(24.0f, windowSize.y - 34.0f));
			if (!m_Status.empty())
				ImGui::TextColored(ImVec4(0.58f, 0.76f, 0.82f, 1.0f), "%s", m_Status.c_str());
			else
				ImGui::TextDisabled("Open a project to enter the editor workspace.");

			ImGui::PopStyleVar();
			DrawNewProjectWizard();
			DrawDeleteProjectModal();
			ImGui::End();
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar(4);
		}

	private:
		static constexpr std::array<ProjectTemplateOption, 3> s_Templates =
		{
			ProjectTemplateOption{ "Empty", "Folders, C# solution, and a camera-only startup scene." },
			ProjectTemplateOption{ "2D Starter", "Camera and a visible starter sprite for fast scene checks." },
			ProjectTemplateOption{ "Script Ready", "Starter Entity bound to the generated StarterEntity C# script." }
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

			const std::filesystem::path projectDirectory = settings.m_Location / SanitizePreviewToken(settings.m_Name);
			std::error_code error;
			if (std::filesystem::exists(projectDirectory, error) && !std::filesystem::is_empty(projectDirectory, error))
			{
				m_NewProjectError = "Project folder already exists and is not empty.";
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

		static void DrawHubTag(const char* label)
		{
			const ImVec2 cursor = ImGui::GetCursorScreenPos();
			const ImVec2 textSize = ImGui::CalcTextSize(label);
			const ImVec2 size(textSize.x + 18.0f, 24.0f);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y), IM_COL32(20, 34, 42, 220), 4.0f);
			drawList->AddRect(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y), IM_COL32(65, 105, 126, 180), 4.0f);
			ImGui::Dummy(size);
			drawList->AddText(ImVec2(cursor.x + 9.0f, cursor.y + 4.0f), IM_COL32(177, 198, 209, 255), label);
		}

		static bool DrawHubActionButton(const char* title, const char* description, bool primary)
		{
			const ImVec2 cursor = ImGui::GetCursorScreenPos();
			const ImVec2 size(ImGui::GetContentRegionAvail().x, 62.0f);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const ImU32 fill = primary ? IM_COL32(35, 80, 101, 255) : IM_COL32(17, 27, 33, 255);
			const ImU32 fillHovered = primary ? IM_COL32(44, 100, 126, 255) : IM_COL32(24, 38, 46, 255);
			const ImU32 border = primary ? IM_COL32(85, 150, 184, 230) : IM_COL32(55, 78, 92, 210);

			ImGui::InvisibleButton(title, size);
			const bool hovered = ImGui::IsItemHovered();
			const bool clicked = ImGui::IsItemClicked();
			drawList->AddRectFilled(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y), hovered ? fillHovered : fill, 4.0f);
			drawList->AddRect(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y), border, 4.0f, 0, hovered ? 1.4f : 1.0f);
			drawList->AddText(ImVec2(cursor.x + 16.0f, cursor.y + 12.0f), IM_COL32(235, 241, 244, 255), title);
			drawList->AddText(ImVec2(cursor.x + 16.0f, cursor.y + 34.0f), IM_COL32(144, 163, 174, 255), description);
			return clicked;
		}

		static std::string CompactPath(const std::filesystem::path& path, size_t maxCharacters)
		{
			std::string value = path.string();
			if (value.size() <= maxCharacters || maxCharacters < 12)
				return value;

			const size_t tailCount = maxCharacters - 4;
			return std::string("...\\") + value.substr(value.size() - tailCount);
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
