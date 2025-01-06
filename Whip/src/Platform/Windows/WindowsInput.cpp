#include <whippch.h>
#include <Whip/Core/Input.h>
#include <Whip/Core/Application.h>

#include <GLFW/glfw3.h>

_WHIP_START

struct input_data
{
	static constexpr size_t MAX_KEYS = 512;
	static constexpr size_t MAX_BUTTONS = 8;
	std::bitset<MAX_KEYS> previous_key_states;
	std::bitset<MAX_BUTTONS> previous_mouse_button_states;
};

static input_data s_input_data;

WHP_NODISCARD bool input::is_key_pressed(int keycode)
{
	auto window = static_cast<GLFWwindow*>(application::get().get_window().get_native_window());
	int state = glfwGetKey(window, keycode);
	bool is_pressed = (state == GLFW_PRESS);

	// Eðer tuþa yeni basýldýysa durumu güncelle ve true döndür
	if (is_pressed && !s_input_data.previous_key_states[keycode])
	{
		s_input_data.previous_key_states[keycode] = true;
		return true;
	}

	// Eðer tuþa hala basýlýysa önceki durumu koruyun, ancak false döndürün
	s_input_data.previous_key_states[keycode] = is_pressed;
	return false;
}

WHP_NODISCARD bool input::is_key_released(int keycode)
{
	auto window = static_cast<GLFWwindow*>(application::get().get_window().get_native_window());
	int state = glfwGetKey(window, keycode);
	bool is_released = (state == GLFW_RELEASE);

	// Eðer tuþ yeni býrakýldýysa durumu güncelle ve true döndür
	if (is_released && s_input_data.previous_key_states[keycode])
	{
		s_input_data.previous_key_states[keycode] = false;
		return true;
	}

	// Eðer tuþ hala serbestse önceki durumu koruyun, ancak false döndürün
	s_input_data.previous_key_states[keycode] = !is_released;
	return false;
}


WHP_NODISCARD bool input::is_key_down(int keycode)
{
	auto window = static_cast<GLFWwindow*>(application::get().get_window().get_native_window());
	int state = glfwGetKey(window, keycode);
	bool is_pressed = (state == GLFW_PRESS);
	return is_pressed;
}

bool input::is_key_up(int keycode)
{
	auto window = static_cast<GLFWwindow*>(application::get().get_window().get_native_window());
	int state = glfwGetKey(window, keycode);
	bool is_released = (state == GLFW_RELEASE);
	return is_released;
}

WHP_NODISCARD bool input::is_mouse_button_pressed(int button)
{
	auto window = static_cast<GLFWwindow*>(application::get().get_window().get_native_window());
	int state = glfwGetMouseButton(window, button);
	bool is_pressed = (state == GLFW_PRESS);
	if (is_pressed && !s_input_data.previous_mouse_button_states[button])
	{
		s_input_data.previous_mouse_button_states[button] = true;
		return true;
	}
	return false;
}

WHP_NODISCARD bool input::is_mouse_button_released(int button)
{
	auto window = static_cast<GLFWwindow*>(application::get().get_window().get_native_window());
	int state = glfwGetMouseButton(window, button);
	bool is_released = (state == GLFW_RELEASE);
	if (is_released && s_input_data.previous_mouse_button_states[button])
	{
		s_input_data.previous_mouse_button_states[button] = false;
		return true;
	}
	return false;
}

WHP_NODISCARD bool input::is_mouse_button_down(int button)
{
	auto window = static_cast<GLFWwindow*>(application::get().get_window().get_native_window());
	int state = glfwGetMouseButton(window, button);
	bool is_pressed = (state == GLFW_PRESS);
	return is_pressed;
}

WHP_NODISCARD bool input::is_mouse_button_up(int button)
{
	auto window = static_cast<GLFWwindow*>(application::get().get_window().get_native_window());
	int state = glfwGetMouseButton(window, button);
	bool is_released = (state == GLFW_RELEASE);
	return is_released;
}

WHP_NODISCARD float input::get_mouse_X()
{
	auto posX = get_mouse_position().first;
	return posX;
}

WHP_NODISCARD float input::get_mouse_Y()
{
	auto posY = get_mouse_position().second;
	return posY;
}

WHP_NODISCARD std::pair<float, float> input::get_mouse_position()
{
	auto window = static_cast<GLFWwindow*>(application::get().get_window().get_native_window());
	double posX, posY;
	glfwGetCursorPos(window, &posX, &posY);
	return { (float)posX, (float)posY };
}

WHP_NODISCARD float input::get_scroll_delta()
{
	return application::get().get_window().get_scroll_delta();
}


_WHIP_END
