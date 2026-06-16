#include "WhipPch.h"
#include <Whip/Asset/TextureImporter.h>
#include <Whip/Asset/AssetManager.h>

#include <Whip/Project/Project.h>

#include <stb_image.h>

_WHIP_START

Ref<Texture2D> TextureImporter::ImportTexture2D(AssetHandle handle, const AssetMetadata& metadata)
{
	Ref<Texture2D> result = LoadTexture2D(Project::GetActiveAssetDirectory() / metadata.m_Filepath);
	result->m_Handle = handle;
	return result;
}

Ref<Texture2D> TextureImporter::LoadTexture2D(const std::filesystem::path& path, FlipDirection direction)
{
	WHP_PROFILE_FUNCTION();
	int width, height, channels;
	RawBuffer data;

	{
		WHP_PROFILE_SCOPE("stbi_load - TextureImporter::ImportTexture2D");
		std::string pathStr = path.string();
		data.m_Data = stbi_load(pathStr.c_str(), &width, &height, &channels, STBI_rgb_alpha);
		channels = 4;
	}

	if (data.m_Data == nullptr)
	{
		WHP_CORE_ERROR("[Asset Manager] Could not load Texture from filepath: {}", path.string());
		return nullptr;
	}

	// TODO: think about this
	data.m_Size = static_cast<uint64_t>(width * height * channels);

	TextureSpecification spec;
	spec.m_Width = width;
	spec.m_Height = height;
	switch (channels)
	{
	case 3:
		spec.m_Format = ImageFormat::Rgb8;
		break;
	case 4:
		spec.m_Format = ImageFormat::Rgba8;
		break;
	}

	if(!(direction & FlipDirectionVertical))
		Texture2D::FlipTextureBuffer(data, width, height, channels, FlipDirectionVertical); // base case
	if (direction & FlipDirectionHorizontal)
		Texture2D::FlipTextureBuffer(data, width, height, channels, FlipDirectionHorizontal);

	Ref<Texture2D> Texture = Texture2D::Create(spec, data);
	data.Release();
	return Texture;
}

_WHIP_END
