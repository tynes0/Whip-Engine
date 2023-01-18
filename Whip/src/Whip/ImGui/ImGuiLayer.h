#pragma once

#include <Whip/Layer.h>

#include <Whip/Events/MouseEvent.h>
#include <Whip/Events/KeyEvent.h>
#include <Whip/Events/ApplicationEvent.h>

_WHIP_START

class WHIP_API ImGuiLayer : public Layer
{
private:
	float m_Time = 0;
public:
	ImGuiLayer();
	~ImGuiLayer();

	void begin();
	void end();

	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnImGuiRender() override;

};

#define GET_IM_IO ImGui::GetIO();

_WHIP_END