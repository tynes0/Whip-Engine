#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/Timestep.h>

#include <vector>

#include <imgui.h>

_WHIP_START

namespace UI
{
	struct UI_image_data
	{
		ImTextureID texture_id;
		ImVec2 size;
		ImVec2 uv1;
		ImVec2 uv2;
	};

	class UI_statistics
	{
	public:
		UI_statistics() = default;

		void add_image(ImTextureID texture_id, const ImVec2& size, const ImVec2& uv1, const ImVec2& uv2);

		void set_open(bool open);
		bool is_open() const { return m_open; }
		bool consume_open_dirty();
		void on_imgui_render(timestep ts);
	private:
		std::vector<UI_image_data> m_image_data;
		bool m_open = true;
		bool m_open_dirty = false;
	};
}

_WHIP_END
