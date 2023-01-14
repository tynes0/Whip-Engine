#include <Whip.h>


class ExLayer : public Whip::Layer
{
public:
	ExLayer() : Layer("Whip Layer") {}

	void OnUpdate() override
	{
		//WHP_INFO("ExLayer::Update");
	}
	void OnEvent(Whip::Event& event) override
	{
		//WHP_TRACE("{0}", event);
	}
};

class F_Box : public Whip::Application
{
public:
	F_Box()
	{
		PushLayer(new ExLayer());
		PushOverlay(new Whip::ImGuiLayer());
	}
	~F_Box()
	{

	}
};

Whip::Application* Whip::CreateApplication()
{
	return new F_Box();
}
