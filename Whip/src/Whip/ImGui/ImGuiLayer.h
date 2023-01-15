#include <Whip/Layer.h>

#include <Whip/Events/MouseEvent.h>
#include <Whip/Events/KeyEvent.h>
#include <Whip/Events/ApplicationEvent.h>

_WHIP_START

class WHIP_API ImGuiLayer : public Layer
{
private:
	float m_Time = 0;
private:
	bool OnMouseButtonPressedEvent(MouseButtonPressedEvent& event);
	bool OnMouseButtonReleasedEvent(MouseButtonReleasedEvent& event);
	bool OnMouseMovedEvent(MouseMovedEvent& event);
	bool OnMouseScrolledEvent(MouseScrolledEvent& event);
	
	bool OnKeyPressedEvent(KeyPressedEvent& event);
	bool OnKeyReleasedEvent(KeyReleasedEvent& event);
	bool OnKeyTypedEvent(/**/);
	
	bool OnWindowResizeEvent(WindowResizeEvent& event);

public:
	ImGuiLayer();
	~ImGuiLayer();


	void OnAttach();
	void OnDetach();
	void OnUpdate();
	void OnEvent(Event& event);
};

_WHIP_END