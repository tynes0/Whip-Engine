#include "icon_manager.h"

_WHIP_START

icon_manager::icon_manager(bool load_default) : m_icon_datas{}
{
	if (!load_default)
		return;

	auto* buf = m_icon_datas.as<ref<texture2D>>();

	buf[frenum::index(icon::play).value()]			= texture_importer::load_texture2D("resources/icons/play_icon.png");
	buf[frenum::index(icon::simulate).value()]		= texture_importer::load_texture2D("resources/icons/simulate_icon.png");
	buf[frenum::index(icon::stop).value()]			= texture_importer::load_texture2D("resources/icons/stop_icon.png");
	buf[frenum::index(icon::pause).value()]			= texture_importer::load_texture2D("resources/icons/pause_icon.png");
	buf[frenum::index(icon::step_forward).value()]	= texture_importer::load_texture2D("resources/icons/step_icon.png");
	buf[frenum::index(icon::step_back).value()]		= texture_importer::load_texture2D("resources/icons/step_icon.png", flip_direction_horizontal);
	buf[frenum::index(icon::directory).value()]		= texture_importer::load_texture2D("resources/icons/content_browser/directory_icon.png");
	buf[frenum::index(icon::file).value()]			= texture_importer::load_texture2D("resources/icons/content_browser/file_icon.png");
	buf[frenum::index(icon::back).value()]			= texture_importer::load_texture2D("resources/icons/return_icon.png");
}


ref<texture2D> icon_manager::load(icon icon_t, const std::filesystem::path& filepath, flip_direction_t direction)
{
	auto* buf = m_icon_datas.as<ref<texture2D>>();

	return buf[frenum::index(icon_t).value()] = texture_importer::load_texture2D(filepath, direction);
}

ref<texture2D> icon_manager::load(icon icon_t, ref<texture2D> tex)
{
	auto* buf = m_icon_datas.as<ref<texture2D>>();

	return buf[frenum::index(icon_t).value()] = tex;
}

ref<texture2D> icon_manager::get_icon(icon icon_t)
{
	if (!valid_icon(icon_t))
		return nullptr;
	auto* buf = m_icon_datas.as<ref<texture2D>>();
	return buf[frenum::index(icon_t).value()];
}

icon_manager& icon_manager::get()
{
	static icon_manager instance{ true };
	return instance;
}

bool icon_manager::valid_icon(icon icon_t)
{
	return frenum::index(icon_t).has_value();
}

_WHIP_END
