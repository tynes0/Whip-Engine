#include <whippch.h>

#include "OpenGLContext.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

_WHIP_START
 
graphic_context* graphic_context::create(GLFWwindow* win_handle)
{
	return new opengl_context(win_handle);
}

opengl_context::opengl_context(GLFWwindow* win_handle) : m_window_handle(win_handle)
{
	WHP_CORE_ASSERT(win_handle, "Window Handle does not exist!");
}

void opengl_context::init()
{
	WHP_PROFILE_FUNCTION();

	glfwMakeContextCurrent(m_window_handle);
	int success = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	WHP_CORE_ASSERT(success, "Failed to initialize Glad! ");

	WHP_CORE_ASSERT(GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5), "Whip requires at least OpenGL version 4.5!");
}

void opengl_context::swap_buffers()
{
	WHP_PROFILE_FUNCTION();

	glfwSwapBuffers(m_window_handle);
}


_WHIP_END
