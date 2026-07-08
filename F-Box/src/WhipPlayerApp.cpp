#include "WhipPlayerLayer.h"

#include <Whip/Core/EntryPoint.h>
#include <Whip/Project/PlayerConfig.h>
#include <Whip/Utils/PlatformUtils.h>

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

		whip::PlayerConfig config;
		whip::PlayerConfigSerializer serializer(config);
		const std::filesystem::path executableDirectory = whip::Utils::GetExecutableDirectory();
		if (serializer.Deserialize(whip::PlayerConfigSerializer::GetDefaultConfigPath(executableDirectory)))
			return config.m_ProjectPath.is_absolute() ? config.m_ProjectPath : executableDirectory / config.m_ProjectPath;

		return {};
	}

	whip::PlayerConfig ResolvePlayerConfig(const whip::ApplicationCommandLineArgs& args)
	{
		whip::PlayerConfig config;
		whip::PlayerConfigSerializer serializer(config);
		const std::filesystem::path executableDirectory = whip::Utils::GetExecutableDirectory();
		serializer.Deserialize(whip::PlayerConfigSerializer::GetDefaultConfigPath(executableDirectory));

		std::filesystem::path commandLineProject = ResolveProjectPath(args);
		if (!commandLineProject.empty())
			config.m_ProjectPath = std::move(commandLineProject);
		if (config.m_ProjectPath.is_relative() && !config.m_ProjectPath.empty())
			config.m_ProjectPath = executableDirectory / config.m_ProjectPath;
		if (config.m_WindowTitle.empty())
			config.m_WindowTitle = !config.m_ProjectPath.empty() ? "Whip Player / " + config.m_ProjectPath.stem().string() : "Whip Player";
		if (HasCommandLineFlag(args, "--fullscreen"))
			config.m_Fullscreen = true;
		return config;
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
	const PlayerConfig config = ResolvePlayerConfig(args);
	whip::ApplicationSpecification spec;
	spec.m_Properties.m_Title = config.m_WindowTitle;
	spec.m_Properties.m_Width = config.m_WindowWidth;
	spec.m_Properties.m_Height = config.m_WindowHeight;
	spec.m_Properties.m_Fullscreen = config.m_Fullscreen;
	spec.m_WorkingDirectory = Utils::GetExecutableDirectory().string();
	spec.m_CommandLineArgs = args;
	return new WhipPlayer(spec);
}
