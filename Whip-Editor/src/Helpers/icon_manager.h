#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Texture/texture_manager.h>
#include <Whip/Core/buffer.h>
#include <Whip/Core/utility.h>

#include "../vendor/frenum/frenum.h"

#include <cstdint>
#include <filesystem>



_WHIP_START

FrenumClassInNamespace(whip, icon, uint16_t, directory, file, back, play, stop, simulate, pause, step_forward, step_back)

class icon_manager
{
public:
	icon_manager(bool load_default);
	ref<texture2D> load(icon icon_t, const std::filesystem::path& filepath, flip_direction_t direction = flip_direction_none);
	ref<texture2D> load(icon icon_t, ref<texture2D> tex);
	ref<texture2D> get_icon(icon icon_t);

	static icon_manager& get();
private:
	bool valid_icon(icon icon_t);

	stack_buffer<frenum::size<icon>() * sizeof(ref<texture2D>), 16> m_icon_datas;
};

_WHIP_END
