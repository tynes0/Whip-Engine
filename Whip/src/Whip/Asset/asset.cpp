#include <whippch.h>
#include "asset.h"

_WHIP_START

std::string_view asset_type_to_string(asset_type type)
{
	switch (type)
	{
	case whip::asset_type::none:		return "asset_type::none";
	case whip::asset_type::scene:		return "asset_type::scene";
	case whip::asset_type::texture2D:	return "asset_type::texture2D";
	case whip::asset_type::audio:		return "asset_type::audio";
	case whip::asset_type::font:		return "asset_type::font";
	case whip::asset_type::animation:	return "asset_type::animation";
	case whip::asset_type::entity:		return "asset_type::entity";
	}
	return "asset_type::<invalid>";
}

asset_type asset_type_from_string(std::string_view asset_t)
{
	if (asset_t == "asset_type::none")
		return asset_type::none;
	if (asset_t == "asset_type::scene")
		return asset_type::scene;
	if (asset_t == "asset_type::texture2D")
		return asset_type::texture2D;
	if (asset_t == "asset_type::audio")
		return asset_type::audio;
	if (asset_t == "asset_type::font")
		return asset_type::font;
	if (asset_t == "asset_type::animation")
		return asset_type::animation;
	if (asset_t == "asset_type::entity")
		return asset_type::entity;
	return asset_type::none;
}

_WHIP_END
