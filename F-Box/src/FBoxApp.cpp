#include "FBoxApp2D.h"

class FBox : public whip::Application
{
public:
	FBox(const whip::ApplicationSpecification& specification)
		: whip::Application(specification)
	{
		PushLayer(new FBoxApp2D());
	}
};

whip::Application* whip::CreateApplication(whip::ApplicationCommandLineArgs args)
{
	whip::ApplicationSpecification spec;
	spec.m_Properties.m_Title = "F-box";
	spec.m_Properties.m_Fullscreen = true;
	spec.m_CommandLineArgs = args;
	return new FBox(spec);
}
