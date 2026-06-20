#include <Whip-Editor/Helpers/IconManager.h>

_WHIP_START

IconManager::IconManager(bool loadDefault)
	: m_IconDatas{}
{
	if (!loadDefault)
		return;

	auto* buffer = m_IconDatas.As<Ref<Texture2D>>();

	buffer[frenum::index(Icon::Play).value()]			= TextureImporter::LoadTexture2D("resources/icons/play_icon.png");
	buffer[frenum::index(Icon::Simulate).value()]		= TextureImporter::LoadTexture2D("resources/icons/simulate_icon.png");
	buffer[frenum::index(Icon::Stop).value()]			= TextureImporter::LoadTexture2D("resources/icons/stop_icon.png");
	buffer[frenum::index(Icon::Pause).value()]			= TextureImporter::LoadTexture2D("resources/icons/pause_icon.png");
	buffer[frenum::index(Icon::StepForward).value()]		= TextureImporter::LoadTexture2D("resources/icons/step_icon.png");
	buffer[frenum::index(Icon::StepBack).value()]		= TextureImporter::LoadTexture2D("resources/icons/step_icon.png", FlipDirectionHorizontal);
	buffer[frenum::index(Icon::Directory).value()]		= TextureImporter::LoadTexture2D("resources/icons/content_browser/directory_icon.png");
	buffer[frenum::index(Icon::File).value()]			= TextureImporter::LoadTexture2D("resources/icons/content_browser/file_icon.png");
	buffer[frenum::index(Icon::Back).value()]			= TextureImporter::LoadTexture2D("resources/icons/return_icon.png");
}

Ref<Texture2D> IconManager::Load(Icon iconType, const std::filesystem::path& filepath, FlipDirection direction)
{
	auto* buffer = m_IconDatas.As<Ref<Texture2D>>();

	return buffer[frenum::index(iconType).value()] = TextureImporter::LoadTexture2D(filepath, direction);
}

Ref<Texture2D> IconManager::Load(Icon iconType, const Ref<Texture2D>& texture)
{
	auto* buffer = m_IconDatas.As<Ref<Texture2D>>();

	return buffer[frenum::index(iconType).value()] = texture;
}

Ref<Texture2D> IconManager::GetIcon(Icon iconType)
{
	if (!ValidIcon(iconType))
		return nullptr;
	auto* buffer = m_IconDatas.As<Ref<Texture2D>>();
	return buffer[frenum::index(iconType).value()];
}

IconManager& IconManager::Get()
{
	static IconManager instance{ true };
	return instance;
}

bool IconManager::ValidIcon(Icon iconType)
{
	return frenum::index(iconType).has_value();
}

_WHIP_END
