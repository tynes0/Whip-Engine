#include <WhipPch.h>

#include "OpenGLContext.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

_WHIP_START
 
GraphicContext* GraphicContext::Create(GLFWwindow* windowHandle)
{
	return MakeRawTagged<OpenGLContext>(memory::MemoryTag::Renderer, windowHandle);
}

OpenGLContext::OpenGLContext(GLFWwindow* windowHandle) : m_WindowHandle(windowHandle)
{
	WHP_CORE_ASSERT(windowHandle, "Window Handle does not exist!");
}

void OpenGLContext::Init()
{
	WHP_PROFILE_FUNCTION();

	glfwMakeContextCurrent(m_WindowHandle);
	int success = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	WHP_CORE_ASSERT(success, "Failed to initialize Glad! ");

	WHP_CORE_ASSERT(GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5), "Whip requires at least OpenGL version 4.5!");
}

void OpenGLContext::SwapBuffers()
{
	WHP_PROFILE_FUNCTION();

	glfwSwapBuffers(m_WindowHandle);
}


_WHIP_END
