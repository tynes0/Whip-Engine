#include <WhipPch.h>
#include "OpenGLRenderAPI.h"

#include <glad/glad.h>

_WHIP_START

void OpenglMessageCallback(unsigned source, unsigned type, unsigned id, unsigned severity, int length, const char* message, const void* userParam)
{
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH:         WHP_CORE_CRITICAL(message); return;
	case GL_DEBUG_SEVERITY_MEDIUM:       WHP_CORE_ERROR(message); return;
	case GL_DEBUG_SEVERITY_LOW:          WHP_CORE_WARN(message); return;
	case GL_DEBUG_SEVERITY_NOTIFICATION: WHP_CORE_TRACE(message); return;
	}

	WHP_CORE_ASSERT(false, "Unknown severity level!");
}

void OpenGLRenderAPI::Init()
{
	WHP_PROFILE_FUNCTION();

#ifdef WHP_DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(OpenglMessageCallback, nullptr);

	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
#endif

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LINE_SMOOTH);
}

void OpenGLRenderAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	glViewport(x, y, width, height);
}

void OpenGLRenderAPI::SetClearColor(const glm::vec4& color)
{
	glClearColor(color.r, color.g, color.b, color.a);
}

void OpenGLRenderAPI::Clear()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderAPI::SetDepthTest(bool enabled)
{
	if (enabled)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
}

void OpenGLRenderAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount)
{
	vertexArray->Bind();
	uint32_t count = (indexCount != 0) ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
	glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
}

void OpenGLRenderAPI::DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
{
	vertexArray->Bind();
	glDrawArrays(GL_LINES, 0, vertexCount);
}

void OpenGLRenderAPI::SetLineWidth(float width)
{
	float range[2];
	glGetFloatv(GL_LINE_WIDTH_RANGE, range);

	if (width < range[0])
		width = range[0];
	else if (width > range[1])
		width = range[1];

	glLineWidth(width);
}

_WHIP_END
