#pragma once

#include <Whip/Core/Layer.h>

#include <Whip/Events/MouseEvent.h>
#include <Whip/Events/KeyEvent.h>
#include <Whip/Events/ApplicationEvent.h>

_WHIP_START

class imgui_layer : public layer
{
private:
	float m_Time = 0;
public:
	imgui_layer();
	~imgui_layer();


	virtual void on_attach() override;
	virtual void on_detach() override;
	virtual void on_event(event& e) override;

	void begin();
	void end();
};

_WHIP_END