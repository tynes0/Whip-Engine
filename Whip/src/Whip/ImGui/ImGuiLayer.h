#include <Whip/Layer.h>


_WHIP_START

class WHIP_API ImGuiLayer : public Layer
{
private:
	float m_Time = 0;
public:
	ImGuiLayer();
	~ImGuiLayer();


	void OnAttach();
	void OnDetach();
	void OnUpdate();
	void OnEvent(Event& event);
};

_WHIP_END