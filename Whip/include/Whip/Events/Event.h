#pragma once

#include <Whip/Core/Core.h>
#include <string>
#include <functional>

_WHIP_START

// WindowLostFocus - WindowMoved - AppTick - AppUpdate - AppRender <<-- not implamented yet

enum class EventType
{
	None = 0,
	
	WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved, WindowDrop,
	
	AppTick, AppUpdate, AppRender,
	
	KeyPressed, KeyReleased, KeyTyped,
	
	MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
};

enum EventCategory
{
	EventCategoryNone = 0,
	EventCategoryApplication		= WHP_BIT(0),
	EventCategoryInput			= WHP_BIT(1),
	EventCategoryKeyboard		= WHP_BIT(2),
	EventCategoryMouse			= WHP_BIT(3),
	EventCategoryMouseButton		= WHP_BIT(4)
};

#define EVENT_CLASS_TYPE(type)				WHP_NODISCARD static EventType GetStaticType() { return EventType::##type; }\
											WHP_NODISCARD virtual EventType GetEventType() const override { return GetStaticType(); }\
											WHP_NODISCARD const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category)		WHP_NODISCARD virtual int GetCategoryFlags() const override { return category; }

#define EVENT_TO_STRING						WHP_NODISCARD std::string ToString() const override

class Event
{
public:
	virtual ~Event() = default;

	WHP_NODISCARD virtual EventType GetEventType() const = 0;
	WHP_NODISCARD virtual const char* GetName() const = 0;
	WHP_NODISCARD virtual int GetCategoryFlags() const = 0;
	WHP_NODISCARD virtual std::string ToString() const { return GetName(); }
	WHP_NODISCARD Event* Get() { return this; }

	WHP_NODISCARD inline bool IsInCategory(EventCategory category)
	{
		return GetCategoryFlags() & category;
	}

	bool m_Handled = false;
};

class EventDispatcher
{
	template <class T>
	using EventFn = std::function<bool(T&)>;
	
	Event& m_Event;
public:
	EventDispatcher(Event& event) : m_Event(event) {}

	template <class T>
	typename std::enable_if<std::is_base_of<Event, T>::value, bool>::type Dispatch(EventFn<T> func)
	{
		if (m_Event.GetEventType() == T::GetStaticType())
		{
			m_Event.m_Handled |= func(DREF(T*) & m_Event);
			return true;
		}
		return false;
	}
};

WHP_NODISCARD inline std::ostream& operator<<(std::ostream& out, const Event& event)
{
	return out << event.ToString();
}

_WHIP_END
