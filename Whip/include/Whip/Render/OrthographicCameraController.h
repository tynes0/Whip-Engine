#pragma once

#include <Whip/Render/OrthographicCamera.h>

#include <Whip/Core/Timestep.h>

#include <Whip/Events/ApplicationEvent.h>
#include <Whip/Events/MouseEvent.h>

#include <Whip/Core/Input.h>

_WHIP_START

struct OrthographicCameraBounds
{
	float left, right, bottom, top;

	float GetWidth() { return right - left; }
	float GetHeight() { return top - bottom; }
};

class OrthographicCameraController
{
public:
	OrthographicCameraController(float aspectRatio, bool rotation = false);

	void OnUpdate(Timestep ts);
	void OnEvent(Event& event);

	void OnResize(float width, float height);
	
	float GetZoomLevel() const { return m_ZoomLevel; }
	bool IsRotatible() const { return m_Rotation; }
	float GetCameraRotation() const { return m_CameraRotation; }
	float GetCameraTranslationSpeed() const { return m_CameraTranslationSpeedStabil; }
	float GetCameraRotationSpeed() const { return m_CameraRotationSpeed; }

	const OrthographicCameraBounds& GetBounds() const { return m_Bounds; }

	void SetZoomLevel(float zoomLevel);
	void SetRotatability(bool rotation) { m_Rotation = rotation; }
	void SetCameraRotation(float rotation) { m_CameraRotation = rotation; }
	void SetCameraTranslationSpeed(float translationSpeed) { m_CameraTranslationSpeedStabil = translationSpeed; }
	void SetCameraRotationSpeed(float rotationSpeed) { m_CameraRotationSpeed = rotationSpeed; }
	void SetCameraPosition(const glm::vec3& position) { m_CameraPosition = position;  m_Camera.SetPosition(m_CameraPosition); }
	
	OrthographicCamera& GetCamera() { return m_Camera; }
	const OrthographicCamera& GetCamera() const { return m_Camera; }
private:
	void CalculateView();

	bool OnMouseScrolled(MouseScrolledEvent& event);
	bool OnWindowResized(WindowResizeEvent& event);

	float m_AspectRatio;
	float m_ZoomLevel = 1.0f;

	OrthographicCameraBounds m_Bounds;
	OrthographicCamera m_Camera;

	bool m_Rotation;

	glm::vec3 m_CameraPosition = { 0.0f, 0.0f, 0.0f };
	float m_CameraRotation = 0.0f;

	float m_CameraTranslationSpeedStabil = 2.0f;
	float m_CameraTranslationSpeed = m_CameraTranslationSpeedStabil;
	float m_CameraRotationSpeed = 120.0f;
};

_WHIP_END
