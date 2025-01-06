#pragma once

#include <Whip/Core/Layer.h>

#include <Whip/Events/MouseEvent.h>
#include <Whip/Events/KeyEvent.h>
#include <Whip/Events/ApplicationEvent.h>

_WHIP_START

class imgui_layer : public layer
{
public:
	imgui_layer();
	~imgui_layer();


	virtual void on_attach() override;
	virtual void on_detach() override;
	virtual void on_event(event& e) override;

	void begin();
	void end();

	void block_events(bool block) { m_block_events = block; }

	uint32_t get_active_widgetID() const;
private:
	void set_initial_style();
	void set_dark_theme_color();

private:
	bool m_block_events = true;
};

_WHIP_END
