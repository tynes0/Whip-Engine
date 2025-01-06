#include "whippch.h"
#include "UI_project.h"

#include <Whip/Core/Application.h>
#include <Whip/Project/project.h>
#include <Whip/Utils/platform_utils.h>

#include <imgui.h>

#include <cstring>

_WHIP_START

namespace UI
{
	UI_project::UI_project()
	{
	}

	void UI_project::set_finish_callback(const callback_type& callback)
	{
		m_callback = callback;
	}

	void UI_project::show(UI_type type, const callback_type& callback)
	{
		switch (type)
		{
		case whip::UI::UI_project::UI_none:
			break;
		case whip::UI::UI_project::UI_settings:
			break;
		default:
			type = UI_none;
			break;
		}
		if (callback && type != UI_none)
			set_finish_callback(callback);
		m_type = type;
	}

	void UI_project::on_imgui_render()
	{
		if (m_type != UI_none)
		{
			
		}
	}
}

_WHIP_END
