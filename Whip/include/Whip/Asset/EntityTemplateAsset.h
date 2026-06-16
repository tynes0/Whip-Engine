#pragma once

#include "Asset.h"

#include <filesystem>
#include <utility>

_WHIP_START

class EntityTemplateAsset : public Asset
{
public:
	EntityTemplateAsset(AssetHandle handleIn, std::filesystem::path filepathIn)
		: Asset(handleIn), m_Filepath(std::move(filepathIn)) {}

	virtual AssetType GetType() const override { return AssetType::Entity; }

	std::filesystem::path m_Filepath;
};

_WHIP_END
