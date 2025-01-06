#pragma once

#include <Whip/Core/core.h>
#include <Whip/Core/memory.h>
#include <Whip/Core/unique_name_manager.h>

#include "animation2D.h"

#include <vector>

_WHIP_START

class animation_manager
{
public:
	void update(timestep ts);
	void add_animation(ref<animation2D> animation);
	void remove_animation(ref<animation2D> animation);

	void clear();
	bool empty() const;

	bool has_been_ticked_this_frame() const;
	std::vector<ref<animation2D>>& get_animations() { return m_animations; }

	static animation_manager& get() { static animation_manager instance; return instance; }
	static unique_name_manager& get_animation_name_manager();
private:
	std::vector<ref<animation2D>> m_animations;
	uint64_t m_last_tick_count = 0;
};

_WHIP_END
