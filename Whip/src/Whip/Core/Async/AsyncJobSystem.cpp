#include <WhipPch.h>
#include <Whip/Core/AsyncJobSystem.h>

#include <algorithm>
#include <exception>

_WHIP_START

namespace Async
{
	namespace
	{
		bool IsTerminal(JobStatus status)
		{
			return status == JobStatus::Succeeded || status == JobStatus::Failed || status == JobStatus::Cancelled;
		}
	}

	JobState::JobState(std::string name)
		: m_Name(std::move(name))
	{
	}

	JobContext::JobContext(std::shared_ptr<JobState> state)
		: m_State(std::move(state))
	{
	}

	void JobContext::SetProgress(float progress)
	{
		if (!m_State)
			return;

		std::scoped_lock lock(m_State->m_Mutex);
		m_State->m_Progress = std::clamp(progress, 0.0f, 1.0f);
	}

	void JobContext::SetProgress(float progress, std::string message)
	{
		if (!m_State)
			return;

		std::scoped_lock lock(m_State->m_Mutex);
		m_State->m_Progress = std::clamp(progress, 0.0f, 1.0f);
		m_State->m_Message = std::move(message);
	}

	void JobContext::SetMessage(std::string message)
	{
		if (!m_State)
			return;

		std::scoped_lock lock(m_State->m_Mutex);
		m_State->m_Message = std::move(message);
	}

	bool JobContext::IsCancellationRequested() const
	{
		if (!m_State)
			return true;

		std::scoped_lock lock(m_State->m_Mutex);
		return m_State->m_CancelRequested;
	}

	JobHandle::JobHandle(std::shared_ptr<JobState> state)
		: m_State(std::move(state))
	{
	}

	bool JobHandle::IsValid() const
	{
		return static_cast<bool>(m_State);
	}

	bool JobHandle::IsDone() const
	{
		if (!m_State)
			return false;

		std::scoped_lock lock(m_State->m_Mutex);
		return IsTerminal(m_State->m_Status);
	}

	bool JobHandle::Succeeded() const
	{
		return Snapshot().m_Status == JobStatus::Succeeded;
	}

	bool JobHandle::Failed() const
	{
		return Snapshot().m_Status == JobStatus::Failed;
	}

	bool JobHandle::Cancelled() const
	{
		return Snapshot().m_Status == JobStatus::Cancelled;
	}

	void JobHandle::Cancel() const
	{
		if (!m_State)
			return;

		std::scoped_lock lock(m_State->m_Mutex);
		if (!IsTerminal(m_State->m_Status))
			m_State->m_CancelRequested = true;
	}

	void JobHandle::Wait() const
	{
		if (!m_State)
			return;

		std::unique_lock lock(m_State->m_Mutex);
		m_State->m_CompletedCondition.wait(lock, [this]() { return IsTerminal(m_State->m_Status); });
	}

	JobProgressSnapshot JobHandle::Snapshot() const
	{
		JobProgressSnapshot snapshot;
		if (!m_State)
			return snapshot;

		std::scoped_lock lock(m_State->m_Mutex);
		snapshot.m_Name = m_State->m_Name;
		snapshot.m_Message = m_State->m_Message;
		snapshot.m_Error = m_State->m_Error;
		snapshot.m_Progress = m_State->m_Progress;
		snapshot.m_Status = m_State->m_Status;
		snapshot.m_CancelRequested = m_State->m_CancelRequested;
		return snapshot;
	}

	JobSystem& JobSystem::Get()
	{
		static JobSystem system;
		return system;
	}

	JobSystem::JobSystem() = default;

	JobSystem::~JobSystem()
	{
		Shutdown();
	}

	JobHandle JobSystem::Submit(std::string name, JobFunction function)
	{
		WHP_CORE_ASSERT(function, "[AsyncJobSystem] Cannot submit an empty job.");

		auto state = std::make_shared<JobState>(std::move(name));
		{
			std::scoped_lock lock(m_Mutex);
			if (m_Shutdown)
			{
				m_Shutdown = false;
				m_Stopping = false;
			}
			m_Jobs.push_back({ state, std::move(function) });
			EnsureWorkers();
		}
		m_WorkAvailable.notify_one();
		return JobHandle(state);
	}

	void JobSystem::Shutdown()
	{
		std::vector<std::thread> workers;
		{
			std::scoped_lock lock(m_Mutex);
			if (m_Shutdown)
				return;

			m_Stopping = true;
			m_Shutdown = true;
			while (!m_Jobs.empty())
			{
				CompleteJob(m_Jobs.front().m_State, JobStatus::Cancelled);
				m_Jobs.pop_front();
			}
			workers.swap(m_Workers);
		}

		m_WorkAvailable.notify_all();
		for (std::thread& worker : workers)
			if (worker.joinable())
				worker.join();
	}

	size_t JobSystem::WorkerCount() const
	{
		std::scoped_lock lock(m_Mutex);
		return m_Workers.size();
	}

	void JobSystem::EnsureWorkers()
	{
		if (!m_Workers.empty())
			return;

		const uint32_t hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
		const uint32_t workerCount = std::clamp(hardwareThreads > 1 ? hardwareThreads - 1 : 1u, 1u, 4u);
		m_Workers.reserve(workerCount);
		for (uint32_t i = 0; i < workerCount; ++i)
			m_Workers.emplace_back([this]() { WorkerLoop(); });
	}

	void JobSystem::WorkerLoop()
	{
		for (;;)
		{
			QueuedJob job;
			{
				std::unique_lock lock(m_Mutex);
				m_WorkAvailable.wait(lock, [this]() { return m_Stopping || !m_Jobs.empty(); });
				if (m_Stopping && m_Jobs.empty())
					return;

				job = std::move(m_Jobs.front());
				m_Jobs.pop_front();
			}

			{
				std::scoped_lock lock(job.m_State->m_Mutex);
				if (job.m_State->m_CancelRequested)
				{
					job.m_State->m_Status = JobStatus::Cancelled;
					job.m_State->m_Progress = 1.0f;
					job.m_State->m_CompletedCondition.notify_all();
					continue;
				}

				job.m_State->m_Status = JobStatus::Running;
			}

			try
			{
				JobContext context(job.m_State);
				job.m_Function(context);

				const JobProgressSnapshot snapshot = JobHandle(job.m_State).Snapshot();
				CompleteJob(job.m_State, snapshot.m_CancelRequested ? JobStatus::Cancelled : JobStatus::Succeeded);
			}
			catch (const std::exception& exception)
			{
				CompleteJob(job.m_State, JobStatus::Failed, exception.what());
			}
			catch (...)
			{
				CompleteJob(job.m_State, JobStatus::Failed, "Unknown async job failure.");
			}
		}
	}

	void JobSystem::CompleteJob(const std::shared_ptr<JobState>& state, JobStatus status, std::string error)
	{
		if (!state)
			return;

		{
			std::scoped_lock lock(state->m_Mutex);
			if (IsTerminal(state->m_Status))
				return;

			state->m_Status = status;
			state->m_Error = std::move(error);
			state->m_Progress = status == JobStatus::Failed ? state->m_Progress : 1.0f;
			if (state->m_Message.empty())
			{
				switch (status)
				{
				case JobStatus::Succeeded: state->m_Message = "Done"; break;
				case JobStatus::Failed: state->m_Message = "Failed"; break;
				case JobStatus::Cancelled: state->m_Message = "Cancelled"; break;
				default: break;
				}
			}
		}
		state->m_CompletedCondition.notify_all();
	}
}

_WHIP_END
