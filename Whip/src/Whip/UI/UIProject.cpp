#include "WhipPch.h"
#include <Whip/UI/UIProject.h>

#include <Whip/Core/Application.h>
#include <Whip/Utils/FileExtensions.h>
#include <Whip/Asset/SceneImporter.h>
#include <Whip/Project/Project.h>
#include <Whip/Scene/Scene.h>
#include <Whip/Utils/PlatformUtils.h>

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
		struct SceneEntry
		{
			AssetHandle handle = 0;
			AssetMetadata metadata;
		};

		void CopyToBuffer(char* buffer, size_t bufferSize, const std::string& value)
		{
			std::memset(buffer, 0, bufferSize);
			std::strncpy(buffer, value.c_str(), bufferSize - 1);
		}

		std::string SanitizeSceneName(std::string value)
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

		std::filesystem::path MakeUniqueScenePath(const std::string& sceneName)
		{
			std::string safeName = SanitizeSceneName(sceneName);
			std::filesystem::path relativePath = std::filesystem::path("Scenes") / (safeName + FileExtensions::Scene);
			std::filesystem::path absolutePath = Project::GetActiveAssetDirectory() / relativePath;

			int suffix = 1;
			while (std::filesystem::exists(absolutePath))
			{
				relativePath = std::filesystem::path("Scenes") / (safeName + "_" + std::to_string(suffix++) + FileExtensions::Scene);
				absolutePath = Project::GetActiveAssetDirectory() / relativePath;
			}

			return relativePath;
		}

		std::vector<SceneEntry> CollectSceneEntries()
		{
			std::vector<SceneEntry> entries;
			Ref<Project> activeProject = Project::GetActive();
			if (!activeProject || !activeProject->GetEditorAssetManager())
				return entries;

			const auto& scenes = activeProject->GetEditorAssetManager()->GetAssetRegistry().GetFiltered(AssetType::Scene);
			entries.reserve(scenes.size());
			for (const auto& [handle, metadata] : scenes)
				entries.push_back({ handle, metadata });

			std::sort(entries.begin(), entries.end(), [](const SceneEntry& left, const SceneEntry& right)
				{
					return left.metadata.m_Filepath.generic_string() < right.metadata.m_Filepath.generic_string();
				});

			return entries;
		}

		bool SameRelativePath(const std::filesystem::path& left, const std::filesystem::path& right)
		{
			return left.lexically_normal().generic_string() == right.lexically_normal().generic_string();
		}

		void DrawSectionHeader(const char* title)
		{
			ImGui::Spacing();
			ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", title);
			ImGui::Separator();
			ImGui::Spacing();
		}

		bool DrawSettingsNavItem(const char* label, bool selected)
		{
			const ImGuiStyle& style = ImGui::GetStyle();
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 8.0f));
			ImGui::PushStyleColor(ImGuiCol_Header, style.Colors[ImGuiCol_Header]);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style.Colors[ImGuiCol_HeaderHovered]);
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.Colors[ImGuiCol_HeaderActive]);
			const bool clicked = ImGui::Selectable(label, selected, 0, ImVec2(0.0f, 34.0f));
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();
			return clicked;
		}
	}

	UIProject::UIProject()
	{
	}

	void UIProject::SetFinishCallback(const CallbackType& callback)
	{
		m_Callback = callback;
	}

	void UIProject::SetBeforeChangeCallback(const CallbackType& callback)
	{
		m_BeforeChangeCallback = callback;
	}

	void UIProject::SetSceneCallbacks(const SceneCallbackType& openSceneCallback, const CallbackType& closeSceneCallback, const ScenePathCallbackType& activeScenePathCallback)
	{
		m_OpenSceneCallback = openSceneCallback;
		m_CloseSceneCallback = closeSceneCallback;
		m_ActiveScenePathCallback = activeScenePathCallback;
	}

	void UIProject::SetEditorSettingsDrawer(const CallbackType& drawer)
	{
		m_EditorSettingsDrawer = drawer;
	}

	void UIProject::Show(UIType type, const CallbackType& callback)
	{
		switch (type)
		{
		case whip::UI::UIProject::None:
			break;
		case whip::UI::UIProject::UISettings:
			break;
		default:
			type = None;
			break;
		}
		if (callback && type != None)
			SetFinishCallback(callback);
		m_Type = type;
		SyncFromActiveProject();
	}

	void UIProject::OnImGuiRender()
	{
		if (m_Type == None)
			return;

		Ref<Project> activeProject = Project::GetActive();
		if (!activeProject)
		{
			m_Type = None;
			return;
		}

		if (activeProject != m_LastActive)
			SyncFromActiveProject();

		bool open = true;
		ImGui::SetNextWindowSize(ImVec2(920.0f, 620.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Settings", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
		{
			ImGui::BeginChild("##SettingsNavigation", ImVec2(180.0f, 0.0f), true);
			ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), "%s", m_NameBuffer);
			ImGui::TextDisabled("%s", "Whip Project");
			ImGui::Separator();
			if (DrawSettingsNavItem("Project", m_ActiveSettingsTab == SettingsTab::Project))
				m_ActiveSettingsTab = SettingsTab::Project;
			if (DrawSettingsNavItem("Scenes", m_ActiveSettingsTab == SettingsTab::Scenes))
				m_ActiveSettingsTab = SettingsTab::Scenes;
			if (DrawSettingsNavItem("Editor", m_ActiveSettingsTab == SettingsTab::Editor))
				m_ActiveSettingsTab = SettingsTab::Editor;
			ImGui::EndChild();

			ImGui::SameLine();
			ImGui::BeginChild("##SettingsContent", ImVec2(0.0f, 0.0f), false);
			ImGui::TextDisabled("%s", Project::GetActiveProjectPath().string().c_str());
			ImGui::Spacing();

			switch (m_ActiveSettingsTab)
			{
			case SettingsTab::Project:
				DrawProjectSettings();
				break;
			case SettingsTab::Scenes:
				DrawSceneSettings();
				break;
			case SettingsTab::Editor:
				DrawEditorSettings();
				break;
			}
			ImGui::EndChild();
		}
		ImGui::End();

		if (!open)
			m_Type = None;
	}

	void UIProject::SyncFromActiveProject()
	{
		Ref<Project> activeProject = Project::GetActive();
		if (!activeProject)
			return;

		const ProjectConfig& config = activeProject->GetConfig();
		CopyToBuffer(m_NameBuffer, MaxBufferSize, config.m_Name);
		CopyToBuffer(m_ProjectPathBuffer, MaxBufferSize, activeProject->GetProjectPath().string());
		CopyToBuffer(m_AssetDirBuffer, MaxBufferSize, config.m_AssetDirectory.string());
		CopyToBuffer(m_StartSceneBuffer, MaxBufferSize, std::to_string((uint64_t)config.m_StartScene));
		CopyToBuffer(m_ScriptModulePathBuffer, MaxBufferSize, config.m_ScriptModulePath.string());
		m_LastActive = activeProject;
	}

	void UIProject::DrawProjectSettings()
	{
		DrawSectionHeader("Project Identity");
		if (ImGui::BeginTable("##ProjectSettingsTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 180.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Name");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputText("##ProjectName", m_NameBuffer, MaxBufferSize);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Project File");
			ImGui::TableNextColumn();
			ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputText("##ProjectPath", m_ProjectPathBuffer, MaxBufferSize);
			ImGui::EndDisabled();

			ImGui::EndTable();
		}

		DrawSectionHeader("Paths");
		if (ImGui::BeginTable("##ProjectPathSettingsTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 180.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Assets");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputText("##AssetDirectory", m_AssetDirBuffer, MaxBufferSize);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Script Module");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputText("##ScriptModule", m_ScriptModulePathBuffer, MaxBufferSize);

			ImGui::EndTable();
		}

		DrawSectionHeader("Startup");
		if (ImGui::BeginTable("##ProjectStartupSettingsTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 180.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Start Scene");
			ImGui::TableNextColumn();
			ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputText("##StartScene", m_StartSceneBuffer, MaxBufferSize);
			ImGui::EndDisabled();

			ImGui::EndTable();
		}

		ImGui::Spacing();
		if (ImGui::Button("Save Project", ImVec2(140.0f, 0.0f)))
			ApplyProjectSettings();
		ImGui::SameLine();
		if (ImGui::Button("Revert", ImVec2(100.0f, 0.0f)))
			SyncFromActiveProject();
	}

	void UIProject::DrawSceneSettings()
	{
		Ref<Project> activeProject = Project::GetActive();
		if (!activeProject)
			return;

		DrawSectionHeader("Scene Library");
		if (ImGui::Button("New Scene", ImVec2(120.0f, 0.0f)))
		{
			CopyToBuffer(m_NewSceneNameBuffer, MaxBufferSize, "NewScene");
			ImGui::OpenPopup("Create Scene");
		}

		ImGui::SameLine();
		if (ImGui::Button("Save Project", ImVec2(120.0f, 0.0f)))
			ApplyProjectSettings();

		ImGui::Spacing();
		const std::vector<SceneEntry> scenes = CollectSceneEntries();
		if (scenes.empty())
		{
			ImGui::TextDisabled("No scenes have been imported yet.");
			DrawCreateScenePopup();
			DrawDeleteScenePopup();
			return;
		}

		const std::filesystem::path activeScenePath = m_ActiveScenePathCallback ? m_ActiveScenePathCallback() : std::filesystem::path();
		bool openDeletePopup = false;
		if (ImGui::BeginTable("##SceneRegistryTable", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 360.0f)))
		{
			ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableSetupColumn("Start", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 190.0f);
			ImGui::TableHeadersRow();

			for (const SceneEntry& entry : scenes)
			{
				const bool isStartScene = activeProject->GetConfig().m_StartScene == entry.handle;
				const bool isActiveScene = !activeScenePath.empty() && SameRelativePath(activeScenePath, entry.metadata.m_Filepath);
				const std::string sceneName = entry.metadata.m_Filepath.stem().string();

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(sceneName.c_str());
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", entry.metadata.m_Filepath.generic_string().c_str());
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", isActiveScene ? "Open" : "-");
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", isStartScene ? "Yes" : "-");
				ImGui::TableNextColumn();

				ImGui::PushID((int)(uint64_t)entry.handle);
				if (ImGui::SmallButton("Open") && m_OpenSceneCallback)
					m_OpenSceneCallback(entry.handle);
				ImGui::SameLine();
				if (ImGui::SmallButton("Set Start"))
				{
					NotifyBeforeChange();
					activeProject->GetConfig().m_StartScene = entry.handle;
					ApplyProjectSettings(false);
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Delete"))
				{
					m_PendingDeleteScene = entry.handle;
					m_PendingDeleteScenePath = entry.metadata.m_Filepath;
					openDeletePopup = true;
				}
				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		if (openDeletePopup)
			ImGui::OpenPopup("Delete Scene");

		DrawCreateScenePopup();
		DrawDeleteScenePopup();
	}

	void UIProject::DrawEditorSettings()
	{
		if (m_EditorSettingsDrawer)
			m_EditorSettingsDrawer();
	}

	void UIProject::DrawCreateScenePopup()
	{
		if (ImGui::BeginPopupModal("Create Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::SetNextItemWidth(320.0f);
			ImGui::InputText("Name", m_NewSceneNameBuffer, MaxBufferSize);

			if (ImGui::Button("Create", ImVec2(120.0f, 0.0f)))
			{
				CreateSceneFromPopup();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	void UIProject::DrawDeleteScenePopup()
	{
		if (ImGui::BeginPopupModal("Delete Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Delete scene?");
			ImGui::TextDisabled("%s", m_PendingDeleteScenePath.generic_string().c_str());
			ImGui::Spacing();

			if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f)))
			{
				DeletePendingScene();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	void UIProject::NotifyBeforeChange()
	{
		if (m_BeforeChangeCallback)
			m_BeforeChangeCallback();
	}

	void UIProject::ApplyProjectSettings(bool notifyChange)
	{
		Ref<Project> activeProject = Project::GetActive();
		if (!activeProject)
			return;

		if (notifyChange)
			NotifyBeforeChange();

		ProjectConfig& config = activeProject->GetConfig();
		config.m_Name = m_NameBuffer;
		config.m_AssetDirectory = m_AssetDirBuffer;
		config.m_ScriptModulePath = m_ScriptModulePathBuffer;

		Project::SaveActive();
		SyncFromActiveProject();

		if (m_Callback)
			m_Callback();
	}

	void UIProject::CreateSceneFromPopup()
	{
		Ref<Project> activeProject = Project::GetActive();
		if (!activeProject || !activeProject->GetEditorAssetManager())
			return;

		NotifyBeforeChange();

		std::filesystem::path relativePath = MakeUniqueScenePath(m_NewSceneNameBuffer);
		std::filesystem::create_directories((Project::GetActiveAssetDirectory() / relativePath).parent_path());

		Ref<Scene> newScene = MakeRef<Scene>();
		SceneImporter::SaveScene(newScene, relativePath);
		AssetHandle handle = activeProject->GetEditorAssetManager()->ImportAsset(relativePath);

		if (handle != 0 && m_OpenSceneCallback)
			m_OpenSceneCallback(handle);

		if (m_Callback)
			m_Callback();
	}

	void UIProject::DeletePendingScene()
	{
		Ref<Project> activeProject = Project::GetActive();
		if (!activeProject || !activeProject->GetEditorAssetManager() || m_PendingDeleteScene == 0)
			return;

		NotifyBeforeChange();

		const std::filesystem::path activeScenePath = m_ActiveScenePathCallback ? m_ActiveScenePathCallback() : std::filesystem::path();
		if (!activeScenePath.empty() && SameRelativePath(activeScenePath, m_PendingDeleteScenePath) && m_CloseSceneCallback)
			m_CloseSceneCallback();

		if (activeProject->GetConfig().m_StartScene == m_PendingDeleteScene)
			activeProject->GetConfig().m_StartScene = 0;

		const std::filesystem::path absoluteScenePath = Project::GetActiveAssetDirectory() / m_PendingDeleteScenePath;
		std::error_code removeError;
		const bool removed = std::filesystem::remove(absoluteScenePath, removeError);
		if (removeError)
			WHP_CORE_WARN("[Project Settings] Failed to delete scene file '{0}': {1}", absoluteScenePath.string(), removeError.message());
		else if (!removed && std::filesystem::exists(absoluteScenePath))
			WHP_CORE_WARN("[Project Settings] Scene file was not deleted: {0}", absoluteScenePath.string());

		activeProject->GetEditorAssetManager()->DeleteAsset(m_PendingDeleteScene);
		activeProject->GetEditorAssetManager()->SerializeAssetRegistry();
		Project::SaveActive();

		m_PendingDeleteScene = 0;
		m_PendingDeleteScenePath.clear();
		SyncFromActiveProject();

		if (m_Callback)
			m_Callback();
	}
}

_WHIP_END
