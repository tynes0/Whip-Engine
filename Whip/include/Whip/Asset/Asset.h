#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>

#include <cstdint>
#include <type_traits>

#include <frenum.h>

_WHIP_START

using AssetHandle = UUID;

enum class AssetType : uint16_t
{
	None = 0,
	Scene,
	Texture2D,
	Audio,
	Font,
	Animation,
	AnimationController,
	Entity
};

MakeFrenumInNamespace(whip, AssetType, None, Scene, Texture2D, Audio, Font, Animation, AnimationController, Entity)

class Asset
{
public:
	Asset() = default;
	Asset(AssetHandle handleIn) : m_Handle(handleIn) {}
	virtual ~Asset() = default;

	virtual AssetType GetType() const = 0;

	AssetHandle m_Handle;
};

template<typename T>
constexpr bool IsAssetV = std::is_base_of_v<Asset, T>;

_WHIP_END
