#pragma once

#include <Whip/Events/Event.h>
#include <Whip/Core/Timestep.h>

_WHIP_START

class WHIP_API layer
{
protected:
	std::string m_name;
public:
	layer(const std::string& name = "Layer");
	virtual ~layer();

	virtual void on_attach() {}
	virtual void on_detach() {}
	virtual void on_update(timestep ts) {}
	virtual void on_imgui_render() {}
	virtual void on_event(event& e) {}
	

	WHP_NODISCARD inline const std::string& get_name() const { return m_name; }
};

typedef layer* layerptr;

_WHIP_END