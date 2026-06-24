#include "WhipPlayerLayer.h"

#include <Whip/Core/EntryPoint.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace
{
	bool HasCommandLineFlag(const whip::ApplicationCommandLineArgs& args, std::string_view flag)
	{
		for (int i = 1; i < args.m_Count; ++i)
		{
			if (std::string_view(args[i]) == flag)
				return true;
		}

		return false;
	}

	std::filesystem::path ResolveProjectPath(const whip::ApplicationCommandLineArgs& args)
	{
		for (int i = 1; i < args.m_Count; ++i)
		{
			std::string_view arg = args[i];
			if (arg == "--project" && i + 1 < args.m_Count)
				return args[i + 1];

			if (!arg.starts_with("--"))
				return args[i];
		}

		return {};
	}

	std::string ResolveWindowTitle(const whip::ApplicationCommandLineArgs& args)
	{
		std::filesystem::path projectPath = ResolveProjectPath(args);
		if (!projectPath.empty())
			return "Whip Player / " + projectPath.stem().string();

		return "Whip Player";
	}
}

class WhipPlayer final : public whip::Application
{
public:
	explicit WhipPlayer(const whip::ApplicationSpecification& specification)
		: whip::Application(specification)
	{
		PushLayer(new WhipPlayerLayer());
	}
};

whip::Application* whip::CreateApplication(whip::ApplicationCommandLineArgs args)
{
	whip::ApplicationSpecification spec;
	spec.m_Properties.m_Title = ResolveWindowTitle(args);
	spec.m_Properties.m_Fullscreen = HasCommandLineFlag(args, "--fullscreen");
	spec.m_CommandLineArgs = args;
	return new WhipPlayer(spec);
}
