#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <glm/vec3.hpp>

_WHIP_START

namespace UI
{
	class UI_settings
	{
	public:
		UI_settings() = default;

		bool get_show_physics_colliders() const { return m_show_physics_colliders; }
		const glm::vec3& get_snap_values(uint32_t idx) const { WHP_CORE_ASSERT(idx < 3); return m_snap_values[idx]; }
		int get_step_frame() const { return m_step_frame; }

		void open_window() { m_open = true; }

		void on_imgui_render();
	private:
		bool m_show_physics_colliders = false;
		glm::vec3 m_snap_values[3] = { {0.5f, 0.5f, 0.5f}, {45.0f, 45.0f, 45.0f}, {0.5f, 0.5f, 0.5f} };
		int m_step_frame = 1;

		bool m_open = true;
	};
}

_WHIP_END
