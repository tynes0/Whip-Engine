#pragma once

#include <Whip/Core/Core.h>
#include "Asset.h"

#include <filesystem>

_WHIP_START

struct AssetMetadata
{
	AssetType m_Type = AssetType::None;
	std::filesystem::path m_Filepath;

	operator bool() const { return m_Type != AssetType::None; }
};

_WHIP_END
