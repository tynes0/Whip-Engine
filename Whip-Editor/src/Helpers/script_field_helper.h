#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Scripting/script_engine.h>

_WHIP_START

namespace UI
{
	enum class script_field_draw : uint8_t
	{
		while_scene_running,
		set_in_the_editor,
		with_base_value
	};

	template <script_field_type Sft, script_field_draw Sfdt>
	void draw_field(const script_field& field, entity ent, const std::string& class_name, bool in_table = false);
}

_WHIP_END
