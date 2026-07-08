#pragma once

#include <Whip/Core/Core.h>

#include <cstdint>
#include <filesystem>
#include <string>

_WHIP_START

struct PlayerConfig
{
	std::filesystem::path m_ProjectPath;
	std::string m_WindowTitle = "Whip Player";
	uint32_t m_WindowWidth = 1280;
	uint32_t m_WindowHeight = 720;
	bool m_Fullscreen = false;
};

class PlayerConfigSerializer
{
public:
	explicit PlayerConfigSerializer(PlayerConfig& config);

	bool Serialize(const std::filesystem::path& filepath) const;
	bool Deserialize(const std::filesystem::path& filepath) const;

	static std::filesystem::path GetDefaultConfigPath(const std::filesystem::path& rootDirectory);

private:
	PlayerConfig& m_Config;
};

_WHIP_END
