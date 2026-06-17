#include "WhipPch.h"
#include <Whip/Helper/TimerManager.h>
#include <Whip/Core/Timestep.h>
#include <Whip/Core/Application.h>

_WHIP_START

void TimerGroup::Add(TimerId id)
{
	m_Timers.push_back(id);
}

void TimerGroup::Remove(TimerId id)
{
	std::erase(m_Timers, id);
}

void TimerGroup::Pause()
{
	for (const auto& id : m_Timers)
		TimerManager::Get().PauseTimer(id);
}

void TimerGroup::Resume()
{
	for (const auto& id : m_Timers)
		TimerManager::Get().ResumeTimer(id);
}

void TimerGroup::Stop()
{
	for (const auto& id : m_Timers)
		TimerManager::Get().StopTimer(id);
}

void TimerGroup::Clear()
{
	for (auto& id : m_Timers)
		TimerManager::Get().m_Timers.erase(id);
	m_Timers.clear();
}

bool TimerGroup::Exists(TimerId id) const
{
	return std::ranges::find(m_Timers, id) != m_Timers.end();
}

bool TimerGroup::Empty() const
{
	return m_Timers.empty();
}

TimerGroup::Iterator TimerGroup::begin()
{
	return m_Timers.begin();
}

TimerGroup::Iterator TimerGroup::end()
{
	return m_Timers.end();
}

TimerGroup::ConstIterator TimerGroup::begin() const
{
	return m_Timers.begin();
}

TimerGroup::ConstIterator TimerGroup::end() const
{
	return m_Timers.end();
}

TimerGroup& TimerGroupMap::Get(const std::string& groupName)
{
	return m_TimerGroups[groupName];
}

bool TimerGroupMap::Remove(const std::string& groupName)
{
	return m_TimerGroups.erase(groupName) != 0;
}

void TimerGroupMap::Clear()
{
	for (auto& group : m_TimerGroups)
		group.second.Clear();
	m_TimerGroups.clear();
}

bool TimerGroupMap::Exists(const std::string& groupName) const
{
	return m_TimerGroups.contains(groupName);
}

TimerId TimerManager::SetTimeout(FunctionType func, float delayMs, void* userData, int priority)
{
	TimerId id = GenerateId();
	m_Timers[id] = {
		.m_Func = std::move(func),
		.m_UserData = userData,
		.m_TimeLeft = delayMs / 1000.0f,
		.m_Interval = 0.0f,
		.m_Repeat = false,
		.m_Paused = false,
		.m_Active = true,
		.m_Priority = priority
	};
	return id;
}

TimerId TimerManager::SetInterval(FunctionType func, float intervalMs, void* userData, int priority)
{
	TimerId id = GenerateId();
	m_Timers[id] = {
		.m_Func = std::move(func),
		.m_UserData = userData,
		.m_TimeLeft = intervalMs / 1000.0f,
		.m_Interval = intervalMs / 1000.0f,
		.m_Repeat = true,
		.m_Paused = false,
		.m_Active = true,
		.m_Priority = priority
	};
	return id;
}

bool TimerManager::WaitFor(TimerId tag, float delayMs, int priority, TimerId* outId)
{
	auto it = m_WaitForTimers.find(tag);
	if (outId)
		*outId = 0;
	if (it != m_WaitForTimers.end() && it->second)
	{
		m_WaitForTimers.erase(tag);
		return true;
	}

	if (it == m_WaitForTimers.end())
	{
		TimerId id = SetTimeout([this, tag](void*) { m_WaitForTimers[tag] = true; }, delayMs, nullptr, priority);
		if (outId)
			*outId = id;
		m_WaitForTimers[tag] = false;
	}

	return false;
}

void TimerManager::PauseTimer(TimerId id)
{
	auto it = m_Timers.find(id);
	if (it != m_Timers.end())
		it->second.m_Paused = true;
}

void TimerManager::ResumeTimer(TimerId id)
{
	auto it = m_Timers.find(id);
	if (it != m_Timers.end())
		it->second.m_Paused = false;
}

void TimerManager::StopTimer(TimerId id)
{
	auto it = m_Timers.find(id);
	if (it != m_Timers.end())
		it->second.m_Active = false;
}

void TimerManager::Clear()
{
	m_Timers.clear();
	m_TimerGroups.Clear();
}

float TimerManager::GetRemainingTime(TimerId id) const
{
	auto it = m_Timers.find(id);
	return it != m_Timers.end() ? it->second.m_TimeLeft : -1.0f;
}

bool TimerManager::Exists(TimerId id) const
{
	return m_Timers.contains(id);
}

bool TimerManager::IsPaused(TimerId id) const
{
	auto it = m_Timers.find(id);
	return it != m_Timers.end() && it->second.m_Paused;
}

bool TimerManager::HasBeenTickedThisFrame() const
{
	return m_LastTickCount == Application::Get().GetTickCount();
}

void TimerManager::Tick(Timestep deltaTime)
{
	if (!Application::Get().IsMainThread())
	{
		WHP_CORE_ERROR("[Timer] Tick() can only be executed on the main thread!");
		return;
	}

	if (HasBeenTickedThisFrame())
		return;

	m_LastTickCount = Application::Get().GetTickCount();
	float dt = deltaTime.GetSeconds();

	std::vector<std::pair<int, TimerId>> timersToRemove;

	for (auto& [id, timer] : m_Timers)
	{
		if (!timer.m_Active)
		{
			timersToRemove.emplace_back(timer.m_Priority, id);
			continue;
		}

		if (timer.m_Paused)
			continue;

		timer.m_TimeLeft -= dt;

		if (timer.m_TimeLeft <= 0.0f)
		{
			if (timer.m_Func)
				timer.m_Func(timer.m_UserData);

			if (timer.m_Repeat)
				timer.m_TimeLeft = timer.m_Interval;
			else
				timersToRemove.emplace_back(timer.m_Priority, id);
		}
	}

	std::ranges::sort(timersToRemove, [](const auto& a, const auto& b) { return a.first > b.first; });

	for (const auto& [priority, id] : timersToRemove)
		m_Timers.erase(id);
}

TimerGroupMap& TimerManager::GetGroupMap(ApplicationMode mode)
{
	switch (mode)
	{
	case ApplicationMode::Editor: return m_TimerGroups;
	case ApplicationMode::Runtime: return m_RuntimeTimerGroups;
	}

	WHP_CORE_ASSERT(false, "[Timer Mananger] Invalid Application mode!");
	return m_TimerGroups;
}

TimerId TimerManager::GenerateId()
{
	static TimerId nextId = 1;
	if (nextId == 0)
		nextId = 1;
	return nextId++;
}

_WHIP_END
