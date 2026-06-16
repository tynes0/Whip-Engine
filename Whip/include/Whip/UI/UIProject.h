#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>
#include <Whip/Project/Project.h>

#include "UIPopupHandler.h"

#include <filesystem>
#include <functional>
#include <string>

_WHIP_START

namespace UI
{
	class UIProject
	{
	public:
		using CallbackType = std::function<void()>;
		using SceneCallbackType = std::function<void(AssetHandle)>;
		using ScenePathCallbackType = std::function<std::filesystem::path()>;

		enum UIType { None = 0, UISettings };

		static constexpr size_t MaxBufferSize = 128;

		UIProject();

		void SetFinishCallback(const CallbackType& callback);
		void SetBeforeChangeCallback(const CallbackType& callback);
		void SetSceneCallbacks(const SceneCallbackType& openSceneCallback, const CallbackType& closeSceneCallback, const ScenePathCallbackType& activeScenePathCallback);
		void SetEditorSettingsDrawer(const CallbackType& drawer);

		void Show(UIType type, const CallbackType& callback = CallbackType{});

		void OnImGuiRender();
	private:
		void SyncFromActiveProject();
		void DrawProjectSettings();
		void DrawSceneSettings();
		void DrawEditorSettings();
		void DrawCreateScenePopup();
		void DrawDeleteScenePopup();
		void NotifyBeforeChange();
		void ApplyProjectSettings(bool notifyChange = true);
		void CreateSceneFromPopup();
		void DeletePendingScene();

		enum class SettingsTab
		{
			Project = 0,
			Scenes,
			Editor
		};

		UIType m_Type = UIType::None;
		UIType m_TemporaryType = UIType::None;
		SettingsTab m_ActiveSettingsTab = SettingsTab::Project;

		CallbackType m_Callback;
		CallbackType m_BeforeChangeCallback;
		CallbackType m_EditorSettingsDrawer;
		SceneCallbackType m_OpenSceneCallback;
		CallbackType m_CloseSceneCallback;
		ScenePathCallbackType m_ActiveScenePathCallback;

		char m_NameBuffer[MaxBufferSize]{ 0 };
		char m_ProjectPathBuffer[MaxBufferSize]{ 0 };
		char m_AssetDirBuffer[MaxBufferSize]{ 0 };
		char m_StartSceneBuffer[MaxBufferSize]{ 0 };
		char m_ScriptModulePathBuffer[MaxBufferSize]{ 0 };
		char m_NewSceneNameBuffer[MaxBufferSize]{ 0 };

		AssetHandle m_PendingDeleteScene = 0;
		std::filesystem::path m_PendingDeleteScenePath;

		Ref<Project> m_LastActive = nullptr;
	};
}

_WHIP_END
