#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>
#include <Whip/Render/Texture.h>

#include <Whip/Asset/Asset.h>

#include <filesystem>

_WHIP_START

struct MsdfData;

class Font : public Asset
{
public:
	Font(const std::filesystem::path& filepath, AssetHandle handle = AssetHandle{});
	~Font();

	const MsdfData* GetMsdfData() const { return m_Data; }
	Ref<Texture2D> GetAtlasTexture() const { return m_AtlasTexture; }

	static Ref<Font> GetDefault();

	virtual AssetType GetType() const override { return AssetType::Font; }

	static constexpr float DefaultPointSize = 32.0f;
private:
	MsdfData* m_Data;
	Ref<Texture2D> m_AtlasTexture;
};

_WHIP_END
