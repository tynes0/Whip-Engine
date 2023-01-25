#include <whippch.h>

#include "OpenGLContext.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

_WHIP_START


OpenGLContext::OpenGLContext(GLFWwindow* WinHandle) : m_WindowHandle(WinHandle)
{
	WHP_CORE_ASSERT(WinHandle, "Window Handle does not exist!");
}

void OpenGLContext::Init()
{
	glfwMakeContextCurrent(m_WindowHandle);
	int success = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	WHP_CORE_ASSERT(success, "Failed to initialize Glad! ");
}

void OpenGLContext::SwapBuffers()
{
	glfwSwapBuffers(m_WindowHandle);
}


_WHIP_END