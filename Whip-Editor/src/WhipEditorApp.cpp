#include <Whip-Editor/EditorLayer.h>

#include <Whip/Utils/PlatformUtils.h>

_WHIP_START

class WhipEditor : public Application
{
public:
	WhipEditor(const ApplicationSpecification& spec) : Application(spec)
	{
		PushLayer(new EditorLayer());
	}
};

Application* CreateApplication(ApplicationCommandLineArgs args)
{
	ApplicationSpecification spec;
	spec.m_Properties.m_Title = "Whip Editor";
	spec.m_Properties.m_Fullscreen = true;
	spec.m_Properties.m_CustomTitlebar = true;
	spec.m_WorkingDirectory = Utils::GetExecutableDirectory().string();
	spec.m_CommandLineArgs = args;
	return new WhipEditor(spec);
}

_WHIP_END
