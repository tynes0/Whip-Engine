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

#ifdef WHP_ENABLE_ASSERTS
	int version_major;
	int version_minor;
	glGetIntegerv(GL_MAJOR_VERSION, &version_major);
	glGetIntegerv(GL_MINOR_VERSION, &version_minor);

	WHP_CORE_ASSERT(version_major > 4 || (version_major == 4 && version_minor >= 5), "Whip requires at least OpenGL version!");
#endif // WHP_ENABLE_ASSERTS
}

void opengl_context::swap_buffers()
{
	WHP_PROFILE_FUNCTION();

	glfwSwapBuffers(m_window_handle);
}


_WHIP_END