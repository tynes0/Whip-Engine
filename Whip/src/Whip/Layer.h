#pragma once

#include <Whip/Core.h>
#include <Whip/Events/Event.h>

_WHIP_START

class WHIP_API Layer
{
protected:
	std::string m_Name;
public:
	Layer(const std::string& name = "Layer");
	virtual ~Layer();

	virtual void OnAttach() {}
	virtual void OnDetach() {}
	virtual void OnUpdate() {}
	virtual void OnImGuiRender() {}
	virtual void OnEvent(Event& e) {}
	

	WHP_NODISCARD inline const std::string& GetName() const { return m_Name; }
};

typedef Layer* layerptr;

_WHIP_END