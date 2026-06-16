#include "WhipPch.h"
#include <Whip/Render/OrthographicCameraController.h>

_WHIP_START

OrthographicCameraController::OrthographicCameraController(float aspectRatio, bool rotation)
	:m_AspectRatio(aspectRatio), m_Bounds({ -m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel }), m_Camera(m_Bounds.left, m_Bounds.right, m_Bounds.bottom, m_Bounds.top), m_Rotation(rotation) {}

void OrthographicCameraController::OnUpdate(Timestep ts)
{
	WHP_PROFILE_FUNCTION();

	if (Input::IsKeyDown(Key::A))
	{
		m_CameraPosition.x -= cos(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
		m_CameraPosition.y -= sin(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
	}
	if (Input::IsKeyDown(Key::D))
	{
		m_CameraPosition.x += cos(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
		m_CameraPosition.y += sin(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
	}
	if (Input::IsKeyDown(Key::S))
	{
		m_CameraPosition.x -= -sin(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
		m_CameraPosition.y -= cos(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
	}
	if (Input::IsKeyDown(Key::W))
	{
		m_CameraPosition.x += -sin(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
		m_CameraPosition.y += cos(glm::radians(m_CameraRotation)) * m_CameraTranslationSpeed * ts;
	}
	if(m_Rotation)
	{
		if (Input::IsKeyDown(Key::Q))
			m_CameraRotation += m_CameraRotationSpeed * ts;
		if (Input::IsKeyDown(Key::E))
			m_CameraRotation -= m_CameraRotationSpeed * ts;
		if (m_CameraRotation > 180.0f)
			m_CameraRotation -= 360.0f;
		if (m_CameraRotation <= -180.0f)
			m_CameraRotation += 360.0f;
		m_Camera.SetRotation(m_CameraRotation);
	}
	m_Camera.SetPosition(m_CameraPosition);
	m_CameraTranslationSpeed = (m_ZoomLevel * m_CameraTranslationSpeedStabil);
}

void OrthographicCameraController::OnEvent(Event& event)
{
	WHP_PROFILE_FUNCTION();

	EventDispatcher dispatcher(event);
	dispatcher.Dispatch<MouseScrolledEvent>([this]<typename ...Args>(Args&&... args) -> decltype(auto) { return this->OnMouseScrolled(std::forward<decltype(args)>(args)...); });
	dispatcher.Dispatch<WindowResizeEvent>([this](auto&&... args) -> decltype(auto) { return this->OnWindowResized(std::forward<decltype(args)>(args)...); });
}

void OrthographicCameraController::OnResize(float width, float height)
{
	m_AspectRatio = width / height;
	CalculateView();
}

void OrthographicCameraController::SetZoomLevel(float zoomLevel)
{
	m_ZoomLevel = zoomLevel;
	CalculateView();
}

void OrthographicCameraController::CalculateView()
{
	m_Bounds = { -m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel };
	m_Camera.SetProjection(m_Bounds.left, m_Bounds.right, m_Bounds.bottom, m_Bounds.top);
}

bool OrthographicCameraController::OnMouseScrolled(MouseScrolledEvent& event)
{
	WHP_PROFILE_FUNCTION();
	m_ZoomLevel -= event.GetOffsetY() * 0.15f;
	m_ZoomLevel = (m_ZoomLevel > 0.1f) ? m_ZoomLevel : 0.1f;
	CalculateView();
	return false;
}

bool OrthographicCameraController::OnWindowResized(WindowResizeEvent& event)
{
	WHP_PROFILE_FUNCTION();
	OnResize((float)event.GetWidth(), (float)event.GetHeight());
	return false;
}

_WHIP_END
