using System.Runtime.CompilerServices;

namespace Whip
{
	static public class Logger
	{
		public enum LogLevel { Trace = 0, Debug, Info, Warn, Error, Critical }
		public static LogLevel Level { get; set; }

		public static bool CoreLogsEnabled = true;

		static Logger() 
		{
			Level = LogLevel.Trace;
			InternalCalls.Logger_SetLogger("Logger");
		}
		public static void SetLoggerName(string name)
		{
			InternalCalls.Logger_SetLogger(name);
		}

		public static void Log(string message) 
		{
			InternalCalls.Logger_PrintLog(message, Level);
		}

		public static void Log(string message, LogLevel level)
		{
			InternalCalls.Logger_PrintLog(message, level);
		}

		public static void Log(string loggerName, string message)
		{
			InternalCalls.Logger_PrintLogNamed(loggerName, message, Level);
		}

		public static void Log(string loggerName, string message, LogLevel level)
		{
			InternalCalls.Logger_PrintLogNamed(loggerName, message, level);
		}

		internal static void LogInternal(string message, LogLevel level = LogLevel.Trace)
		{
			if(CoreLogsEnabled)
				InternalCalls.Logger_InternalLog(message, level);
		}

		internal static void Assert(bool condition, string message, [CallerFilePath] string filepath = "", [CallerLineNumber] int line = 0)
		{
			InternalCalls.Logger_InternalAssert(condition, message, filepath, line);
		}
	}
}
