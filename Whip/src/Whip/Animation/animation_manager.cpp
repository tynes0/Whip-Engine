#include "whippch.h"
#include "animation_manager.h"
#include <Whip/Core/Application.h>

_WHIP_START

static unique_name_manager s_name_manager;

void animation_manager::update(timestep ts)
{
	if (has_been_ticked_this_frame())
		return;

	m_last_tick_count = application::get().get_tick_count();

	for (auto& anim : m_animations)
	{
		if (!anim || !anim->is_playing() || anim->is_paused())
			continue;

		if (anim->m_frames.empty())
		{
			anim->stop();
			continue;
		}

		anim->m_elapsed_time += ts.get_seconds();

		if (anim->m_elapsed_time >= anim->m_frames[anim->m_current_frame].duration)
		{
			anim->m_current_frame++;

			if (anim->m_current_frame >= anim->m_frames.size())
			{
				anim->m_elapsed_time = 0.0f;
				if (anim->is_looping())
					anim->m_current_frame = 0;
				else
				{
					anim->stop();
					continue;
				}
			}

			anim->apply_frame(anim->m_frames[anim->m_current_frame].texture);
		}
	}
}

void animation_manager::add_animation(ref<animation2D> animation)
{
	m_animations.push_back(animation);
}

void animation_manager::remove_animation(ref<animation2D> animation)
{
	m_animations.erase(std::remove(m_animations.begin(), m_animations.end(), animation), m_animations.end());
}

void animation_manager::clear()
{
	m_animations.clear();
}

bool animation_manager::empty() const
{
	return m_animations.empty();
}

bool animation_manager::has_been_ticked_this_frame() const
{
	return m_last_tick_count == application::get().get_tick_count();
}

unique_name_manager& animation_manager::get_animation_name_manager()
{
	return s_name_manager;
}

_WHIP_END
