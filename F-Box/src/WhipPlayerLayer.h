#pragma once

#include <Whip.h>

class WhipPlayerLayer final : public whip::Layer
{
public:
	WhipPlayerLayer();
	~WhipPlayerLayer() override = default;

	void OnAttach() override;
	void OnDetach() override;
	void OnUpdate(whip::Timestep timestep) override;
	void OnImGuiRender() override;
	void OnEvent(whip::Event& eventIn) override;

private:
	std::filesystem::path ResolveProjectPath() const;
	void SetFatalError(std::string title, std::string detail);
	bool LoadProject(const std::filesystem::path& projectPath);
	bool LoadStartScene();
	bool LoadRuntimeScene(whip::AssetHandle sceneHandle);
	bool UnloadRuntimeScene();
	void StopRuntimeScene();
	void ProcessRuntimeSceneTransition();

private:
	whip::Ref<whip::Scene> m_RuntimeScene;
	whip::Ref<whip::Framebuffer> m_Framebuffer;
	glm::vec2 m_ViewportSize = { 1280.0f, 720.0f };
	whip::AssetHandle m_ActiveSceneHandle = 0;
	std::string m_FatalErrorTitle;
	std::string m_FatalErrorDetail;
	std::filesystem::path m_ConfigPath;
	std::filesystem::path m_LogFilePath;
	bool m_SceneLoaded = false;
};
