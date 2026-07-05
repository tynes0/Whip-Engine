#pragma once

#include <Whip/Core/Core.h>

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

_WHIP_START

namespace Async
{
	enum class JobStatus : uint8_t
	{
		Pending = 0,
		Running,
		Succeeded,
		Failed,
		Cancelled
	};

	struct JobProgressSnapshot
	{
		std::string m_Name;
		std::string m_Message;
		std::string m_Error;
		float m_Progress = 0.0f;
		JobStatus m_Status = JobStatus::Pending;
		bool m_CancelRequested = false;
	};

	struct JobState
	{
		explicit JobState(std::string name);

		std::mutex m_Mutex;
		std::condition_variable m_CompletedCondition;
		std::string m_Name;
		std::string m_Message;
		std::string m_Error;
		float m_Progress = 0.0f;
		JobStatus m_Status = JobStatus::Pending;
		bool m_CancelRequested = false;
	};

	class JobContext
	{
	public:
		explicit JobContext(std::shared_ptr<JobState> state);

		void SetProgress(float progress);
		void SetProgress(float progress, std::string message);
		void SetMessage(std::string message);
		bool IsCancellationRequested() const;

	private:
		std::shared_ptr<JobState> m_State;
	};

	class JobHandle
	{
	public:
		JobHandle() = default;
		explicit JobHandle(std::shared_ptr<JobState> state);

		bool IsValid() const;
		bool IsDone() const;
		bool Succeeded() const;
		bool Failed() const;
		bool Cancelled() const;
		void Cancel() const;
		void Wait() const;
		JobProgressSnapshot Snapshot() const;

	private:
		std::shared_ptr<JobState> m_State;
	};

	class JobSystem
	{
	public:
		using JobFunction = std::function<void(JobContext&)>;

		static JobSystem& Get();

		JobHandle Submit(std::string name, JobFunction function);
		void Shutdown();
		size_t WorkerCount() const;

	private:
		JobSystem();
		~JobSystem();

		void EnsureWorkers();
		void WorkerLoop();
		void CompleteJob(const std::shared_ptr<JobState>& state, JobStatus status, std::string error = {});

		struct QueuedJob
		{
			std::shared_ptr<JobState> m_State;
			JobFunction m_Function;
		};

		mutable std::mutex m_Mutex;
		std::condition_variable m_WorkAvailable;
		std::deque<QueuedJob> m_Jobs;
		std::vector<std::thread> m_Workers;
		bool m_Stopping = false;
		bool m_Shutdown = false;
	};
}

_WHIP_END
