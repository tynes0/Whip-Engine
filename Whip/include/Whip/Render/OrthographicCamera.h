#pragma once

#include <Whip/Core/Core.h>

#include <glm/glm.hpp>

_WHIP_START

class OrthographicCamera
{
private:
	glm::mat4 m_ProjectionMatrix;
	glm::mat4 m_ViewMatrix;
	glm::mat4 m_ViewProjectionMatrix;
	glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
	float m_Rotation = 0.0f;
private:
	void RecalculateViewMatrix();
public:
	OrthographicCamera(float left, float right, float bottom, float top);

	void SetProjection(float left, float right, float bottom, float top);

	void SetPosition(const glm::vec3& position);
	void SetRotation(float rotation);

	WHP_NODISCARD const glm::vec3& GetPosition() const { return m_Position; }
	WHP_NODISCARD float GetRotation() const { return m_Rotation; }

	WHP_NODISCARD const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
	WHP_NODISCARD const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
	WHP_NODISCARD const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }
};

_WHIP_END
