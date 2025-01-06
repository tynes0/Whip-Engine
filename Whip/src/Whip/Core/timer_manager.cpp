#include "whippch.h"
#include "timer_manager.h"
#include "timestep.h"
#include "Application.h"

_WHIP_START

void timer_group::add(timer_id id)
{
	m_timers.push_back(id);
}

void timer_group::remove(timer_id id)
{
	m_timers.erase(std::remove(m_timers.begin(), m_timers.end(), id), m_timers.end());
}

void timer_group::pause()
{
	for (const auto& id : m_timers)
		timer_manager::get().pause_timer(id);
}

void timer_group::resume()
{
	for (const auto& id : m_timers)
		timer_manager::get().pause_timer(id);
}

void timer_group::stop()
{
	for (const auto& id : m_timers)
		timer_manager::get().pause_timer(id);
}

void timer_group::clear()
{
	for (auto& id : m_timers)
		timer_manager::get().m_timers.erase(id);
	m_timers.clear();
}

bool timer_group::exists(timer_id id) const
{
	return std::find(m_timers.begin(), m_timers.end(), id) != m_timers.end();
}

bool timer_group::empty() const
{
	return m_timers.empty();
}

timer_group::iterator timer_group::begin()
{
	return m_timers.begin();
}

timer_group::iterator timer_group::end()
{
	return m_timers.end();
}

timer_group::const_iterator timer_group::begin() const
{
	return m_timers.begin();
}

timer_group::const_iterator timer_group::end() const
{
	return m_timers.end();
}

timer_group& timer_group_map::get(const std::string& group_name)
{
	return m_timer_groups[group_name];
}

bool timer_group_map::remove(const std::string& group_name)
{
	return m_timer_groups.erase(group_name) != 0;
}

void timer_group_map::clear()
{
	for (auto& group : m_timer_groups)
		group.second.clear();
	m_timer_groups.clear();
}

bool timer_group_map::exists(const std::string& group_name) const
{
	return m_timer_groups.find(group_name) != m_timer_groups.end();
}

timer_manager::timer_manager() {}

timer_id timer_manager::set_timeout(function_type func, float delay_ms, void* user_data, int priority)
{
	timer_id id = generate_id();
	m_timers[id] = { func, user_data, delay_ms / 1000.0f, 0.0f, false, false, true, priority };
	return id;
}

timer_id timer_manager::set_interval(function_type func, float interval_ms, void* user_data, int priority)
{
	timer_id id = generate_id();
	m_timers[id] = { func, user_data, interval_ms / 1000.0f, interval_ms / 1000.0f, true, false, true, priority };
	return id;
}

bool timer_manager::wait_for(timer_id tag, float delay_ms, int priority, timer_id* out_id)
{
	auto it = m_wait_for_timers.find(tag);
	if (out_id)
		*out_id = 0;
	if (it != m_wait_for_timers.end() && it->second)
	{
		m_wait_for_timers.erase(tag);
		return true;
	}

	if (it == m_wait_for_timers.end())
	{
		timer_id id = set_timeout([this, tag](void*) { m_wait_for_timers[tag] = true; }, delay_ms, nullptr, priority);
		if (out_id)
			*out_id = id;
		m_wait_for_timers[tag] = false;
	}

	return false;
}

void timer_manager::pause_timer(timer_id id)
{
	auto it = m_timers.find(id);
	if (it != m_timers.end())
		it->second.paused = true;
}

void timer_manager::resume_timer(timer_id id)
{
	auto it = m_timers.find(id);
	if (it != m_timers.end())
		it->second.paused = false;
}

void timer_manager::stop_timer(timer_id id)
{
	auto it = m_timers.find(id);
	if (it != m_timers.end())
		it->second.active = false;
}

void timer_manager::clear()
{
	m_timers.clear();
	m_timer_groups.clear();
}

float timer_manager::get_remaining_time(timer_id id) const
{
	auto it = m_timers.find(id);
	return it != m_timers.end() ? it->second.time_left : -1.0f;
}

bool timer_manager::exists(timer_id id) const
{
	return m_timers.find(id) != m_timers.end();
}

bool timer_manager::is_paused(timer_id id) const
{
	auto it = m_timers.find(id);
	return it != m_timers.end() && it->second.paused;
}

bool timer_manager::has_been_ticked_this_frame() const
{
	return m_last_tick_count == application::get().get_tick_count();
}

void timer_manager::tick(timestep delta_time)
{
	if (!application::get().is_main_thread())
	{
		WHP_CORE_ERROR("[Timer] tick() can only be executed on the main thread!");
		return;
	}

	if (has_been_ticked_this_frame())
		return;

	m_last_tick_count = application::get().get_tick_count();
	float dt = delta_time.get_seconds();

	std::vector<std::pair<int, timer_id>> timers_to_remove;

	for (auto& [id, timer] : m_timers)
	{
		if (!timer.active)
		{
			timers_to_remove.push_back({ timer.priority, id });
			continue;
		}

		if (timer.paused)
			continue;

		timer.time_left -= dt;

		if (timer.time_left <= 0.0f)
		{
			if (timer.func)
				timer.func(timer.user_data);

			if (timer.repeat)
				timer.time_left = timer.interval;
			else
				timers_to_remove.push_back({ timer.priority, id });
		}
	}

	std::sort(timers_to_remove.begin(), timers_to_remove.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

	for (const auto& [_, id] : timers_to_remove)
		m_timers.erase(id);
}

timer_group_map& timer_manager::get_group_map(application_mode mode)
{
	switch (mode)
	{
	case whip::application_mode::editor: return m_timer_groups;
	case whip::application_mode::runtime: return m_runtime_timer_groups;
	default:
		WHP_CORE_ASSERT(false, "[Timer Mananger] Invalid application mode!");
		return m_timer_groups;
	}
}

timer_id timer_manager::generate_id()
{
	static timer_id next_id = 1;
	if (next_id == 0)
		next_id = 1;
	return next_id++;
}

_WHIP_END
