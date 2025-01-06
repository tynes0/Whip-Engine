namespace Whip
{
	static public class Timer
	{
		public static bool WaitFor(ulong tag, float delayMs)
		{
			return InternalCalls.Timer_WaitFor(tag, delayMs);
		}
		public static ulong SetTimeout(TimerCallback callback, float delayMs)
		{
			return InternalCalls.Timer_SetTimeout(callback, delayMs, null);
		}
		public static ulong SetTimeout(TimerCallback callback, float delayMs, object userData)
		{
			return InternalCalls.Timer_SetTimeout(callback, delayMs, userData);
		}
		public static ulong SetInterval(TimerCallback callback, float intervalMs)
		{
			return InternalCalls.Timer_SetInterval(callback, intervalMs, null);
		}
		public static ulong SetInterval(TimerCallback callback, float intervalMs, object userData)
		{
			return InternalCalls.Timer_SetInterval(callback, intervalMs, userData);
		}

		public static void Pause(ulong id)
		{
			InternalCalls.Timer_PauseTimer(id);
		}

		public static void Resume(ulong id)
		{
			InternalCalls.Timer_ResumeTimer(id);
		}

		public static void Stop(ulong id)
		{
			InternalCalls.Timer_StopTimer(id);
		}

		public static void Clear()
		{
			InternalCalls.Timer_Clear();
		}

		public static bool Exists(ulong id)
		{
			return InternalCalls.Timer_Exists(id);
		}
	}
}
