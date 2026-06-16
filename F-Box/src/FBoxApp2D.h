#pragma once

#include <Whip.h>

class FBoxApp2D : public whip::Layer
{
public:
	FBoxApp2D();
	virtual ~FBoxApp2D() = default;

	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnUpdate(whip::Timestep timestep) override;
	virtual void OnImGuiRender() override;
	virtual void OnEvent(whip::event& eventIn) override;

private:
	bool LoadProject(const std::filesystem::path& projectPath);
	void LoadStartScene();

	// Scene
	whip::Ref<whip::Scene> m_RuntimeScene;
	whip::Ref<whip::Framebuffer> m_Framebuffer;

	// Viewport
	glm::vec2 m_ViewportSize = { 1280.0f, 720.0f };
	bool m_SceneLoaded = false;
};

