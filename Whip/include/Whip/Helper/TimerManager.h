#pragma once
#include <Whip/Core/Core.h>

#include <functional>
#include <unordered_map>
#include <string>
#include <vector>

#include <Whip/Core/Timestep.h>
#include <Whip/Core/UUID.h>
#include <Whip/Core/Application.h>

_WHIP_START

using TimerId = uint64_t;

class TimerGroup
{
public:
	using Iterator = std::vector<TimerId>::iterator;
	using ConstIterator = std::vector<TimerId>::const_iterator;

	void Add(TimerId id);
	void Remove(TimerId id);
	void Pause();
	void Resume();
	void Stop();
	void Clear();
	bool Exists(TimerId id) const;
	bool Empty() const;

	Iterator begin();
	Iterator end();
	ConstIterator begin() const;
	ConstIterator end() const;
private:
	std::vector<TimerId> m_Timers;
};

class TimerGroupMap
{
public:
	TimerGroup& Get(const std::string& groupName);
	bool Remove(const std::string& groupName);
	void Clear();
	bool Exists(const std::string& groupName) const;
private:
	std::unordered_map<std::string, TimerGroup> m_TimerGroups;
};

class TimerManager
{
public:
	using FunctionType = std::function<void(void*)>;

	TimerManager() = default;
	~TimerManager() = default;

	TimerId SetTimeout(FunctionType func, float delayMs, void* userData = nullptr, int priority = 0);
	TimerId SetInterval(FunctionType func, float intervalMs, void* userData = nullptr, int priority = 0);
	bool WaitFor(TimerId tag, float delayMs, int priority = 0, TimerId* outId = nullptr);

	void PauseTimer(TimerId id);
	void ResumeTimer(TimerId id);
	void StopTimer(TimerId id);
	void Clear();
	float GetRemainingTime(TimerId id) const;
	bool Exists(TimerId id) const;
	bool IsPaused(TimerId id) const;
	bool HasBeenTickedThisFrame() const;

	void Tick(Timestep deltaTime);

	TimerGroupMap& GetGroupMap(ApplicationMode mode = ApplicationMode::Editor);

	static TimerManager& Get() { static TimerManager instance = TimerManager(); return instance; }
private:
	static TimerId GenerateId();

	struct TimerData
	{
		FunctionType m_Func;
		void* m_UserData = nullptr;
		float m_TimeLeft = 0.0f;
		float m_Interval = 0.0f;
		bool m_Repeat = false;
		bool m_Paused = false;
		bool m_Active = true;
		int m_Priority = 0;
	};

	std::unordered_map<TimerId, TimerData> m_Timers;
	TimerGroupMap m_TimerGroups;
	TimerGroupMap m_RuntimeTimerGroups;
	std::unordered_map<TimerId, bool> m_WaitForTimers;

	uint64_t m_LastTickCount = 0;

	friend class TimerGroup;
	friend class TimerGroupMap;
};

_WHIP_END
