#pragma once

#include "asset.h"

#include <filesystem>
#include <utility>

_WHIP_START

class entity_template_asset : public asset
{
public:
	entity_template_asset(asset_handle handle_in, std::filesystem::path filepath_in)
		: asset(handle_in), filepath(std::move(filepath_in)) {}

	virtual asset_type get_type() const override { return asset_type::entity; }

	std::filesystem::path filepath;
};

_WHIP_END
