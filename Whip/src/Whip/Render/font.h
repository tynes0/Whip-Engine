#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <Whip/Render/Texture.h>

#include <Whip/Asset/asset.h>

#include <filesystem>

_WHIP_START

struct msdf_data;

class font : public asset
{
public:
	font(const std::filesystem::path& filepath, asset_handle handle = asset_handle{});
	~font();

	const msdf_data* get_msdf_data() const { return m_data; }
	ref<texture2D> get_atlas_texture() const { return m_atlas_texture; }

	static ref<font> get_default();

	virtual asset_type get_type() const override { return asset_type::font; }
private:
	msdf_data* m_data;
	ref<texture2D> m_atlas_texture;
};

_WHIP_END
