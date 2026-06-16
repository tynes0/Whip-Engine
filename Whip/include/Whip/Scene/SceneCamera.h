#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Render/Camera.h>

_WHIP_START

class SceneCamera : public Camera
{
public:
	enum class ProjectionType { Perspective = 0, Orthographic = 1 };
public:
	SceneCamera();
	virtual ~SceneCamera() = default;

	void SetViewportSize(uint32_t width, uint32_t height);

	void SetPerspective(float verticalFOV, float nearClip, float farClip);
	void SetOrthographic(float size, float nearClip, float farClip);

	float GetPerspectiveVerticalFOV() const { return m_PerspectiveFOV; }
	float GetPerspectiveNearClip() const { return m_PerspectiveNear; }	
	float GetPerspectiveFarClip() const { return m_PerspectiveFar; }	
	float GetOrthographicSize() const { return m_OrthographicSize; }
	float GetOrthographicNearClip() const { return m_OrthographicNear; }
	float GetOrthographicFarClip() const { return m_OrthographicFar; }

	void SetPerspectiveVerticalFOV(float verticalFOV);
	void SetPerspectiveNearClip(float nearClip);
	void SetPerspectiveFarClip(float farClip);
	void SetOrthographicSize(float size);
	void SetOrthographicNearClip(float nearClip);
	void SetOrthographicFarClip(float farClip);

	ProjectionType GetProjectionType() const { return m_ProjectionType; }
	void SetProjectionType(ProjectionType type);
private:
	void RecalculateProjection();
private:
	ProjectionType m_ProjectionType = ProjectionType::Orthographic;
	float m_PerspectiveFOV = glm::radians(45.0f);
	float m_PerspectiveNear = 0.01f;
	float m_PerspectiveFar = 1000.0f;
	float m_OrthographicSize = 10.0f;
	float m_OrthographicNear = -1.0f;
	float m_OrthographicFar = 1.0f;
	float m_AspectRatio = 0.0f;
};

_WHIP_END
