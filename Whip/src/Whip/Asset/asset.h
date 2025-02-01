#pragma once


#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>

#include <cstdint>
#include <type_traits>
#include <string_view>

#include "../vendor/frenum/frenum.h"

_WHIP_START

using asset_handle = UUID;

enum class asset_type : uint16_t
{
	none = 0,
	scene,
	texture2D,
	audio,
	font,
	animation,
	entity,
};

MakeFrenumInNamespace(whip, asset_type, none, scene, texture2D, audio, font, animation, entity)

class asset
{
public:
	asset() {}
	asset(asset_handle handle_in) : handle(handle_in) {}

	asset_handle handle;

	virtual asset_type get_type() const = 0;
};

template <class T>
constexpr bool is_asset_v = std::is_base_of_v<asset, T>;

_WHIP_END
