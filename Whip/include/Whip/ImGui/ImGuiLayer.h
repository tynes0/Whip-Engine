#pragma once

#include <Whip/Core/Layer.h>

#include <Whip/Events/MouseEvent.h>
#include <Whip/Events/KeyEvent.h>
#include <Whip/Events/ApplicationEvent.h>

_WHIP_START

class ImGuiLayer : public Layer
{
public:
	ImGuiLayer();
	~ImGuiLayer();


	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnEvent(Event& event) override;

	void Begin();
	void End();

	void BlockEvents(bool block) { m_BlockEvents = block; }
	bool IsBlockingEvents() const { return m_BlockEvents; }

	uint32_t GetActiveWidgetID() const;
private:
	void SetInitialStyle();
	void SetDarkThemeColor();

private:
	bool m_BlockEvents = true;
};

_WHIP_END
