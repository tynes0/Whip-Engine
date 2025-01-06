#pragma once
#include "Core.h"

#include <functional>
#include <unordered_map>
#include <string>
#include <vector>

#include "Timestep.h"
#include "UUID.h"
#include "memory.h"
#include "Application.h"

_WHIP_START

typedef uint64_t timer_id;

class timer_group
{
public:
	using iterator = std::vector<timer_id>::iterator;
	using const_iterator = std::vector<timer_id>::const_iterator;

	void add(timer_id id);
	void remove(timer_id id);
	void pause();
	void resume();
	void stop();
	void clear();
	bool exists(timer_id id) const;
	bool empty() const;

	iterator begin();
	iterator end();
	const_iterator begin() const;
	const_iterator end() const;
private:
	std::vector<timer_id> m_timers;
};

class timer_group_map
{
public:
	timer_group& get(const std::string& group_name);
	bool remove(const std::string& group_name);
	void clear();
	bool exists(const std::string& group_name) const;
private:
	std::unordered_map<std::string, timer_group> m_timer_groups;
};

class timer_manager
{
public:
	using function_type = std::function<void(void*)>;

	timer_manager();
	~timer_manager() = default;

	timer_id set_timeout(function_type func, float delay_ms, void* user_data = nullptr, int priority = 0);
	timer_id set_interval(function_type func, float interval_ms, void* user_data = nullptr, int priority = 0);
	bool wait_for(timer_id tag, float delay_ms, int priority = 0, timer_id* out_id = nullptr);

	void pause_timer(timer_id id);
	void resume_timer(timer_id id);
	void stop_timer(timer_id id);
	void clear();
	float get_remaining_time(timer_id id) const;
	bool exists(timer_id id) const;
	bool is_paused(timer_id id) const;
	bool has_been_ticked_this_frame() const;

	void tick(timestep delta_time);

	timer_group_map& get_group_map(application_mode mode = application_mode::editor);

	static timer_manager& get() { static timer_manager instance = timer_manager(); return instance; }
private:
	timer_id generate_id();

	struct timer_data
	{
		function_type func;
		void* user_data = nullptr;
		float time_left = 0.0f;
		float interval = 0.0f;
		bool repeat = false;
		bool paused = false;
		bool active = true;
		int priority = 0; 
	};

	std::unordered_map<timer_id, timer_data> m_timers;
	timer_group_map m_timer_groups;
	timer_group_map m_runtime_timer_groups;
	std::unordered_map<timer_id, bool> m_wait_for_timers;

	uint64_t m_last_tick_count = 0;

	friend class timer_group;
	friend class timer_group_map;
};

_WHIP_END
