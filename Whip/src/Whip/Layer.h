#pragma once

#include <Whip/Events/Event.h>
#include <Whip/Core/Timestep.h>

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
	virtual void OnUpdate(timestep ts) {}
	virtual void OnImGuiRender() {}
	virtual void OnEvent(Event& e) {}
	

	WHP_NODISCARD inline const std::string& GetName() const { return m_Name; }
};

typedef Layer* layerptr;

_WHIP_END