#pragma once

#include <Whip/Core/Core.h>
#include <string>
#include <functional>

_WHIP_START

// window_lost_focus - window_moved - app_tick - app_update - app_render <<-- not implamented yet

enum class event_type
{
	none = 0,
	
	window_close, window_resize, window_focus, window_lost_focus, window_moved, window_drop,
	
	app_tick, app_update, app_render,
	
	key_pressed, key_released, key_typed,
	
	mouse_button_pressed, mouse_button_released, mouse_moved, mouse_scrolled
};

enum event_category
{
	none = 0,
	event_category_application		= WHP_BIT(0),
	event_category_input			= WHP_BIT(1),
	event_category_keyboard			= WHP_BIT(2),
	event_category_mouse			= WHP_BIT(3),
	event_category_mouse_button		= WHP_BIT(4)
};

#define EVENT_CLASS_TYPE(type)				WHP_NODISCARD static event_type get_static_type() { return event_type::##type; }\
											WHP_NODISCARD virtual event_type get_event_type() const override { return get_static_type(); }\
											WHP_NODISCARD const char* get_name() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category)		WHP_NODISCARD virtual int get_category_flags() const override { return category; }

#define EVENT_TO_STRING						WHP_NODISCARD std::string to_string() const override

class event
{
public:
	virtual ~event() = default;

	WHP_NODISCARD virtual event_type get_event_type() const = 0;
	WHP_NODISCARD virtual const char* get_name() const = 0;
	WHP_NODISCARD virtual int get_category_flags() const = 0;
	WHP_NODISCARD virtual std::string to_string() const { return get_name(); }
	WHP_NODISCARD event* get() { return this; }

	WHP_NODISCARD inline bool is_in_category(event_category category)
	{
		return get_category_flags() & category;
	}

	bool handled = false;
};

class event_dispatcher
{
	template <class T>
	using event_fn = std::function<bool(T&)>;
private:
	event& m_event;
public:
	event_dispatcher(event& evnt) : m_event(evnt) {}

	template <class T>
	typename std::enable_if<std::is_base_of<event, T>::value, bool>::type dispatch(event_fn<T> func)
	{
		if (m_event.get_event_type() == T::get_static_type())
		{
			m_event.handled |= func(DREF(T*) & m_event);
			return true;
		}
		return false;
	}
};

WHP_NODISCARD inline std::ostream& operator<<(std::ostream& out, const event& evnt)
{
	return out << evnt.to_string();
}

_WHIP_END
