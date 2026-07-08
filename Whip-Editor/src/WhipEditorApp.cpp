#include <Whip-Editor/EditorLayer.h>

#include <Whip/Core/Memory.h>
#include <Whip/Utils/PlatformUtils.h>

_WHIP_START

class WhipEditor : public Application
{
public:
	WhipEditor(const ApplicationSpecification& spec) : Application(spec)
	{
		PushLayer(MakeRawTagged<EditorLayer>(memory::MemoryTag::Editor));
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
	spec.m_Mode = ApplicationMode::Editor;
	return MakeRawTagged<WhipEditor>(memory::MemoryTag::Editor, spec);
}

_WHIP_END
