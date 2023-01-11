#include <Whip.h>


class ExLayer : public _WHIP Layer
{
public:
	ExLayer() : Layer("Whip Layer") {}

	void OnUpdate() override
	{
		//WHP_INFO("ExLayer::Update");
	}
	void OnEvent(_WHIP Event& event) override
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
	}
	~F_Box()
	{

	}
};

Whip::Application* Whip::CreateApplication()
{
	return new F_Box();
}
