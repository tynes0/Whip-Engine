#include <Whip-Editor/panels/ConsolePanel.h>

#include <array>
#include <cctype>
#include <string>
#include <vector>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <FileWatch.h>

#include <Whip/Core/Memory.h>
#include <Whip-Editor/UI/UIScopedStyle.h>

#include <algorithm>

_WHIP_START

struct ConsoleEntry
{
	Log::Level m_Level = Log::Level::Trace;
	std::string m_Timestamp;
	std::string m_Category;
	std::string m_Message;
	std::string m_SearchBlob;
};

struct ConsoleData
{
	std::vector<ConsoleEntry> m_Buffer;
	std::thread m_FileWatcherThread;
	std::mutex m_Mutex;
	std::uintmax_t m_LastStreamIndex = 0;
	std::uintmax_t m_ClearedStreamIndex = 0;
	std::atomic<bool> m_Running;
	std::filesystem::file_time_type m_LastWriteTime{};
	std::string m_TextFilter;
	std::string m_CategoryFilter;
	std::array<bool, 6> m_LevelFilter = { true, true, true, true, true, true };
	bool m_AutoScroll = true;
	bool m_Open = true;
	bool m_OpenDirty = false;

	static constexpr size_t MaxConsoleLines = 500;
	static constexpr size_t EraseCount = 100;
};

ConsoleData ConsoleState;

namespace
{
	std::string Trim(std::string value)
	{
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
			value.erase(value.begin());
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
			value.pop_back();
		return value;
	}

	std::string ToLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
	}

	Log::Level ParseLevelToken(std::string token)
	{
		token = ToLower(Trim(std::move(token)));
		if (token == "debug")
			return Log::Level::Debug;
		if (token == "info")
			return Log::Level::Info;
		if (token == "warn" || token == "warning")
			return Log::Level::Warning;
		if (token == "error")
			return Log::Level::Error;
		if (token == "critical")
			return Log::Level::Critical;
		if (token == "off")
			return Log::Level::Off;
		return Log::Level::Trace;
	}

	const char* LevelName(Log::Level level)
	{
		switch (level)
		{
		case Log::Level::Trace:    return "Trace";
		case Log::Level::Debug:    return "Debug";
		case Log::Level::Info:     return "Info";
		case Log::Level::Warning:  return "Warn";
		case Log::Level::Error:    return "Error";
		case Log::Level::Critical: return "Critical";
		default:                   return "Log";
		}
	}

	size_t LevelIndex(Log::Level level)
	{
		switch (level)
		{
		case Log::Level::Trace:    return 0;
		case Log::Level::Debug:    return 1;
		case Log::Level::Info:     return 2;
		case Log::Level::Warning:  return 3;
		case Log::Level::Error:    return 4;
		case Log::Level::Critical: return 5;
		default:                   return 0;
		}
	}

	float EstimatedButtonWidth(const char* label)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		return ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
	}

	float EstimatedCheckboxWidth(const char* label)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		return ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(label).x;
	}

	void SameLineIfFits(float nextItemWidth)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		if (ImGui::GetContentRegionAvail().x >= nextItemWidth + style.ItemSpacing.x)
			ImGui::SameLine();
	}

	ConsoleEntry ParseEntry(Log::Level level, std::string rawMessage)
	{
		ConsoleEntry entry;
		entry.m_Level = level;
		rawMessage = Trim(std::move(rawMessage));

		size_t timestampEnd = std::string::npos;
		if (!rawMessage.empty() && rawMessage.front() == '[')
		{
			timestampEnd = rawMessage.find(']');
			if (timestampEnd != std::string::npos)
				entry.m_Timestamp = rawMessage.substr(1, timestampEnd - 1);
		}

		const size_t categoryStart = timestampEnd == std::string::npos ? 0 : timestampEnd + 1;
		const size_t categoryEnd = rawMessage.find(':', categoryStart);
		if (categoryEnd != std::string::npos)
		{
			entry.m_Category = Trim(rawMessage.substr(categoryStart, categoryEnd - categoryStart));
			entry.m_Message = Trim(rawMessage.substr(categoryEnd + 1));
		}
		else
		{
			entry.m_Category = "General";
			entry.m_Message = rawMessage;
		}

		if (!entry.m_Message.empty() && entry.m_Message.front() == '[')
		{
			const size_t subcategoryEnd = entry.m_Message.find(']');
			if (subcategoryEnd != std::string::npos && subcategoryEnd > 1)
			{
				const std::string subcategory = entry.m_Message.substr(1, subcategoryEnd - 1);
				entry.m_Category += "/" + subcategory;
			}
		}

		entry.m_SearchBlob = ToLower(entry.m_Timestamp + " " + entry.m_Category + " " + entry.m_Message + " " + LevelName(entry.m_Level));
		return entry;
	}

	bool EntryVisible(const ConsoleEntry& entry)
	{
		const size_t index = LevelIndex(entry.m_Level);
		if (index >= ConsoleState.m_LevelFilter.size() || !ConsoleState.m_LevelFilter[index])
			return false;

		const std::string textFilter = ToLower(ConsoleState.m_TextFilter);
		if (!textFilter.empty() && entry.m_SearchBlob.find(textFilter) == std::string::npos)
			return false;

		const std::string categoryFilter = ToLower(ConsoleState.m_CategoryFilter);
		if (!categoryFilter.empty() && ToLower(entry.m_Category).find(categoryFilter) == std::string::npos)
			return false;

		return true;
	}

	void SetAllLevelFilters(bool enabled)
	{
		for (bool& levelEnabled : ConsoleState.m_LevelFilter)
			levelEnabled = enabled;
	}

	void SetWarningsAndErrorsFilter()
	{
		SetAllLevelFilters(false);
		ConsoleState.m_LevelFilter[LevelIndex(Log::Level::Warning)] = true;
		ConsoleState.m_LevelFilter[LevelIndex(Log::Level::Error)] = true;
		ConsoleState.m_LevelFilter[LevelIndex(Log::Level::Critical)] = true;
	}

	void SetErrorsFilter()
	{
		SetAllLevelFilters(false);
		ConsoleState.m_LevelFilter[LevelIndex(Log::Level::Error)] = true;
		ConsoleState.m_LevelFilter[LevelIndex(Log::Level::Critical)] = true;
	}

	std::string FormatEntryForClipboard(const ConsoleEntry& entry)
	{
		return "[" + entry.m_Timestamp + "] " + LevelName(entry.m_Level) + " " + entry.m_Category + ": " + entry.m_Message + "\n";
	}

	std::uintmax_t LogFileSize()
	{
		std::error_code error;
		const auto& path = EditorLog::GetLogFilepath();
		if (!std::filesystem::exists(path, error))
			return 0;
		const std::uintmax_t size = std::filesystem::file_size(path, error);
		return error ? 0 : size;
	}

	void SkipToEnd()
	{
		const std::uintmax_t size = LogFileSize();
		ConsoleState.m_LastStreamIndex = size;
		ConsoleState.m_ClearedStreamIndex = size;
		EditorLog::FileShouldReset().store(false);
	}

	void ResetBuffer()
	{
		const auto& logPath = EditorLog::GetLogFilepath();
		std::error_code error;
		if (!std::filesystem::exists(logPath, error))
			return;

		const std::uintmax_t endPos = std::filesystem::file_size(logPath, error);
		if (error)
			return;

		if (endPos < ConsoleState.m_LastStreamIndex)
		{
			ConsoleState.m_LastStreamIndex = 0;
			ConsoleState.m_ClearedStreamIndex = 0;
		}

		if (ConsoleState.m_LastStreamIndex < ConsoleState.m_ClearedStreamIndex)
			ConsoleState.m_LastStreamIndex = ConsoleState.m_ClearedStreamIndex;

		if (endPos <= ConsoleState.m_LastStreamIndex)
			return;

		std::ifstream stream(logPath, std::ios::binary);
		if (!stream.is_open())
			return;

		const std::uintmax_t bufSize = endPos - ConsoleState.m_LastStreamIndex;
		std::string buffer;
		buffer.resize(static_cast<size_t>(bufSize));
		stream.seekg(static_cast<std::streamoff>(ConsoleState.m_LastStreamIndex), std::ios::beg);
		stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		const std::streamsize bytesRead = stream.gcount();

		ConsoleState.m_LastStreamIndex = endPos;
		if (bytesRead <= 0)
			return;
		if (static_cast<size_t>(bytesRead) < buffer.size())
			buffer.resize(static_cast<size_t>(bytesRead));

		static constexpr const char* token = "level::";
		static constexpr size_t tokenLength = sizeof(token) - 1;

		size_t temp = 0;
		size_t currentPos = buffer.find(token);
		size_t nextTokenPos = 0;

		if (currentPos == std::string::npos)
			return;

		bool run = true;

		while (run)
		{
			if (ConsoleState.m_Buffer.size() >= ConsoleData::MaxConsoleLines)
				ConsoleState.m_Buffer.erase(ConsoleState.m_Buffer.begin(), ConsoleState.m_Buffer.begin() + ConsoleData::EraseCount);
			currentPos += tokenLength;

			temp = buffer.find(',', currentPos);

			if (temp == std::string::npos || temp == buffer.size() - 1)
				return;

			Log::Level level = ParseLevelToken(buffer.substr(currentPos, temp - currentPos));
			currentPos = temp + 1;

			nextTokenPos = buffer.find(token, currentPos);
			if (nextTokenPos == std::string::npos)
			{
				nextTokenPos = buffer.size();
				run = false;
			}

			std::string messageContent = buffer.substr(currentPos, nextTokenPos - currentPos);
			ConsoleState.m_Buffer.emplace_back(ParseEntry(level, messageContent));
			currentPos = nextTokenPos;
		}
	}

	void MonitorFlag()
	{
		while (ConsoleState.m_Running.load())
		{
			bool shouldUpdate = EditorLog::FileShouldReset().exchange(false);
			std::error_code error;
			const auto& logPath = EditorLog::GetLogFilepath();
			if (std::filesystem::exists(logPath, error))
			{
				auto writeTime = std::filesystem::last_write_time(logPath, error);
				if (!error && writeTime != ConsoleState.m_LastWriteTime)
				{
					ConsoleState.m_LastWriteTime = writeTime;
					shouldUpdate = true;
				}
			}

			if (shouldUpdate)
				Application::Get().SubmitToMainThread(ResetBuffer);

			std::this_thread::sleep_for(std::chrono::milliseconds(80));
		}
	}
}

void ConsolePanel::Initialize()
{
	if (EditorLog::ShouldLog())
	{
		std::error_code error;
		ConsoleState.m_LastWriteTime = std::filesystem::last_write_time(EditorLog::GetLogFilepath(), error);

		ResetBuffer();
		ConsoleState.m_Running.store(true);

		if (!ConsoleState.m_FileWatcherThread.joinable())
			ConsoleState.m_FileWatcherThread = std::thread(MonitorFlag);
	}
}

void ConsolePanel::Shutdown()
{
	ConsoleState.m_Running.store(false);
	if (ConsoleState.m_FileWatcherThread.joinable())
		ConsoleState.m_FileWatcherThread.join();

	ConsoleState.m_Buffer.clear();
	ConsoleState.m_LastStreamIndex = 0;
	ConsoleState.m_ClearedStreamIndex = 0;
	EditorLog::LogState(false);
	EditorLog::Erase();
	if (std::filesystem::exists(EditorLog::GetLogFilepath()))
	{
		std::error_code error;
		std::filesystem::resize_file(EditorLog::GetLogFilepath(), 0, error);
	}
}

void ConsolePanel::OnImGuiRender()
{
	std::lock_guard lock(ConsoleState.m_Mutex);

	if (!EditorLog::ShouldLog())
		return;
	if (!ConsoleState.m_Open)
		return;

	static constexpr ImU32 TraceColor = IM_COL32(214, 208, 196, 255);
	static constexpr ImU32 DebugColor = IM_COL32(166, 154, 190, 255);
	static constexpr ImU32 InfoColor = IM_COL32(138, 184, 134, 255);
	static constexpr ImU32 WarnColor = IM_COL32(226, 180, 92, 255);
	static constexpr ImU32 ErrorColor = IM_COL32(222, 104, 104, 255);
	static constexpr ImU32 CriticalColor = IM_COL32(218, 116, 178, 255);

	static constexpr auto GetColor = [](Log::Level level) -> ImVec4
		{
			switch (level)
			{
			case whip::Log::Level::Trace:		return ImVec4(ImColor(TraceColor));
			case whip::Log::Level::Debug:		return ImVec4(ImColor(DebugColor));
			case whip::Log::Level::Info:		return ImVec4(ImColor(InfoColor));
			case whip::Log::Level::Warning:		return ImVec4(ImColor(WarnColor));
			case whip::Log::Level::Error:		return ImVec4(ImColor(ErrorColor));
			case whip::Log::Level::Critical:	return ImVec4(ImColor(CriticalColor));
			default:							return ImVec4(ImColor(TraceColor));
			}
		};

	bool open = ConsoleState.m_Open;
	ImGui::Begin("Console", &open);
	if (open != ConsoleState.m_Open)
	{
		ConsoleState.m_Open = open;
		ConsoleState.m_OpenDirty = true;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));

	if (ImGui::Button("Clear", ImVec2(74.0f, 0.0f)))
	{
		ConsoleState.m_Buffer.clear();
		SkipToEnd();
	}
	SameLineIfFits(EstimatedCheckboxWidth("Auto-scroll"));
	ImGui::Checkbox("Auto-scroll", &ConsoleState.m_AutoScroll);
	SameLineIfFits(ImGui::CalcTextSize("000 logs").x);
	ImGui::TextDisabled("%zu logs", ConsoleState.m_Buffer.size());
	SameLineIfFits(EstimatedButtonWidth("All"));
	if (ImGui::SmallButton("All"))
	{
		SetAllLevelFilters(true);
		ConsoleState.m_TextFilter.clear();
		ConsoleState.m_CategoryFilter.clear();
	}
	SameLineIfFits(EstimatedButtonWidth("Warn+"));
	if (ImGui::SmallButton("Warn+"))
		SetWarningsAndErrorsFilter();
	SameLineIfFits(EstimatedButtonWidth("Errors"));
	if (ImGui::SmallButton("Errors"))
		SetErrorsFilter();
	SameLineIfFits(EstimatedButtonWidth("Scripts"));
	if (ImGui::SmallButton("Scripts"))
		ConsoleState.m_CategoryFilter = "Script";
	SameLineIfFits(EstimatedButtonWidth("Assets"));
	if (ImGui::SmallButton("Assets"))
		ConsoleState.m_CategoryFilter = "Asset";
	SameLineIfFits(EstimatedButtonWidth("Project"));
	if (ImGui::SmallButton("Project"))
		ConsoleState.m_CategoryFilter = "Project";
	SameLineIfFits(EstimatedButtonWidth("Copy visible"));
	if (ImGui::SmallButton("Copy visible"))
	{
		std::string clipboard;
		for (const ConsoleEntry& entry : ConsoleState.m_Buffer)
			if (EntryVisible(entry))
				clipboard += FormatEntryForClipboard(entry);
		ImGui::SetClipboardText(clipboard.c_str());
	}

	SameLineIfFits(220.0f);
	const float filterReserve = 170.0f + ImGui::GetStyle().ItemSpacing.x;
	const float textFilterAvail = ImGui::GetContentRegionAvail().x;
	const float textFilterMin = std::min(180.0f, textFilterAvail);
	ImGui::SetNextItemWidth(std::max(textFilterMin, std::min(textFilterAvail, textFilterAvail - filterReserve)));
	ImGui::InputTextWithHint("##ConsoleTextFilter", "Filter message, level, time", &ConsoleState.m_TextFilter);
	SameLineIfFits(170.0f);
	const float categoryFilterAvail = ImGui::GetContentRegionAvail().x;
	ImGui::SetNextItemWidth(std::max(1.0f, categoryFilterAvail));
	ImGui::InputTextWithHint("##ConsoleCategoryFilter", "Filter category", &ConsoleState.m_CategoryFilter);

	for (size_t i = 0; i < ConsoleState.m_LevelFilter.size(); ++i)
	{
		if (i > 0)
			SameLineIfFits(EstimatedCheckboxWidth(LevelName(static_cast<Log::Level>(i))));
		ImGui::Checkbox(LevelName(static_cast<Log::Level>(i)), &ConsoleState.m_LevelFilter[i]);
	}
	const size_t visibleCount = std::ranges::count_if(ConsoleState.m_Buffer, [](const ConsoleEntry& entry) { return EntryVisible(entry); });
	SameLineIfFits(ImGui::CalcTextSize("000 visible").x);
	ImGui::TextDisabled("%zu visible", visibleCount);

	ImGui::Separator();
	ImGui::BeginChild("##ConsoleScroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

	if (ImGui::BeginTable("##ConsoleTable", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 82.0f);
		ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 82.0f);
		ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 190.0f);
		ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		for (const ConsoleEntry& entry : ConsoleState.m_Buffer)
		{
			if (!EntryVisible(entry))
				continue;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", entry.m_Timestamp.c_str());
			ImGui::TableNextColumn();
			ImGui::TextColored(GetColor(entry.m_Level), "%s", LevelName(entry.m_Level));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(entry.m_Category.c_str());
			ImGui::TableNextColumn();
			ImGui::TextWrapped("%s", entry.m_Message.c_str());
		}
		ImGui::EndTable();
	}

	if (ConsoleState.m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
		ImGui::SetScrollHereY(1.0f);

	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::End();
}

void ConsolePanel::SetOpen(bool open)
{
	std::lock_guard lock(ConsoleState.m_Mutex);
	if (ConsoleState.m_Open == open)
		return;
	ConsoleState.m_Open = open;
	ConsoleState.m_OpenDirty = true;
}

bool ConsolePanel::IsOpen()
{
	std::lock_guard lock(ConsoleState.m_Mutex);
	return ConsoleState.m_Open;
}

bool ConsolePanel::ConsumeOpenDirty()
{
	std::lock_guard lock(ConsoleState.m_Mutex);
	const bool dirty = ConsoleState.m_OpenDirty;
	ConsoleState.m_OpenDirty = false;
	return dirty;
}

_WHIP_END
