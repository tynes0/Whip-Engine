#include <Whip-Editor/Panels/ConsolePanel.h>

#include <Whip-Editor/UI/UIScopedStyle.h>

#include <Whip/Debug/Instrumentor.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <FileWatch.h>

#include <array>
#include <string>
#include <vector>
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
	bool m_Focused = false;
	bool m_RequestSearchFocus = false;
	bool m_RequestScrollToBottom = true;
	size_t m_LastRenderedBufferSize = 0;
	size_t m_LastRenderedVisibleCount = 0;

	static constexpr size_t MaxConsoleLines = 500;
	static constexpr size_t EraseCount = 100;
};

namespace { ConsoleData ConsoleState; }

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
		std::ranges::transform(value, value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
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
		switch (level) // NOLINT(clang-diagnostic-switch-enum)
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
		switch (level) // NOLINT(clang-diagnostic-switch-enum)
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

	std::string SingleLineText(std::string text)
	{
		for (char& character : text)
		{
			if (character == '\r' || character == '\n' || character == '\t')
				character = ' ';
		}
		return text;
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

	void RequestScrollToBottom()
	{
		ConsoleState.m_RequestScrollToBottom = true;
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

	void CopyVisibleToClipboardUnlocked()
	{
		std::string clipboard;
		for (const ConsoleEntry& entry : ConsoleState.m_Buffer)
			if (EntryVisible(entry))
				clipboard += FormatEntryForClipboard(entry);
		ImGui::SetClipboardText(clipboard.c_str());
	}

	void DrawSelectableConsoleCell(const char* id, const std::string& displayText, const std::string& cellClipboard, const std::string& rowClipboard, const ImVec4* color = nullptr)
	{
		ImGui::PushID(id);
		if (color)
			ImGui::PushStyleColor(ImGuiCol_Text, *color);

		const std::string selectableText = displayText.empty() ? " " : displayText;
		ImGui::Selectable(selectableText.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(std::max(1.0f, ImGui::GetContentRegionAvail().x), 0.0f));

		if (color)
			ImGui::PopStyleColor();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				ImGui::SetClipboardText(cellClipboard.c_str());
		}

		if (ImGui::BeginPopupContextItem("##ConsoleCellCopyMenu"))
		{
			if (ImGui::MenuItem("Copy Cell"))
				ImGui::SetClipboardText(cellClipboard.c_str());
			if (ImGui::MenuItem("Copy Row"))
				ImGui::SetClipboardText(rowClipboard.c_str());
			if (ImGui::MenuItem("Copy Visible Logs"))
				CopyVisibleToClipboardUnlocked();
			ImGui::EndPopup();
		}

		ImGui::PopID();
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

		static constexpr std::string_view token = "level::";
		static constexpr size_t tokenLength = token.size();

		size_t currentPos = buffer.find(token);

		if (currentPos == std::string::npos)
			return;

		bool run = true;

		while (run)
		{
			if (ConsoleState.m_Buffer.size() >= ConsoleData::MaxConsoleLines)
				ConsoleState.m_Buffer.erase(ConsoleState.m_Buffer.begin(), ConsoleState.m_Buffer.begin() + ConsoleData::EraseCount);
			currentPos += tokenLength;

			size_t temp = buffer.find(',', currentPos);

			if (temp == std::string::npos || temp == buffer.size() - 1)
				return;

			Log::Level level = ParseLevelToken(buffer.substr(currentPos, temp - currentPos));
			currentPos = temp + 1;

			size_t nextTokenPos = buffer.find(token, currentPos);
			if (nextTokenPos == std::string::npos)
			{
				nextTokenPos = buffer.size();
				run = false;
			}

			std::string messageContent = buffer.substr(currentPos, nextTokenPos - currentPos);
			ConsoleState.m_Buffer.emplace_back(ParseEntry(level, messageContent));
			ConsoleState.m_RequestScrollToBottom = true;
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
	WHP_PROFILE_FUNCTION();
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
	WHP_PROFILE_FUNCTION();
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
	WHP_PROFILE_FUNCTION();
	std::lock_guard lock(ConsoleState.m_Mutex);

	if (!EditorLog::ShouldLog())
		return;
	if (!ConsoleState.m_Open)
	{
		ConsoleState.m_Focused = false;
		return;
	}

	static constexpr ImU32 TraceColor = IM_COL32(214, 208, 196, 255);
	static constexpr ImU32 DebugColor = IM_COL32(166, 154, 190, 255);
	static constexpr ImU32 InfoColor = IM_COL32(138, 184, 134, 255);
	static constexpr ImU32 WarnColor = IM_COL32(226, 180, 92, 255);
	static constexpr ImU32 ErrorColor = IM_COL32(222, 104, 104, 255);
	static constexpr ImU32 CriticalColor = IM_COL32(218, 116, 178, 255);

	static constexpr auto GetColor = [](Log::Level level) -> ImVec4
		{
			switch (level) // NOLINT(clang-diagnostic-switch-enum)
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
	ConsoleState.m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
	if (open != ConsoleState.m_Open)
	{
		ConsoleState.m_Open = open;
		ConsoleState.m_OpenDirty = true;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7.0f, 5.0f));

	if (ImGui::Button("Clear", ImVec2(74.0f, 0.0f)))
	{
		ConsoleState.m_Buffer.clear();
		ConsoleState.m_RequestScrollToBottom = true;
		SkipToEnd();
	}
	SameLineIfFits(EstimatedCheckboxWidth("Auto-scroll"));
	if (ImGui::Checkbox("Auto-scroll", &ConsoleState.m_AutoScroll) && ConsoleState.m_AutoScroll)
		ConsoleState.m_RequestScrollToBottom = true;
	SameLineIfFits(ImGui::CalcTextSize("000 logs").x);
	ImGui::TextDisabled("%zu logs", ConsoleState.m_Buffer.size());

	SameLineIfFits(EstimatedButtonWidth("Filters"));
	if (ImGui::SmallButton("Filters"))
		ImGui::OpenPopup("##ConsoleFiltersPopup");
	if (ImGui::BeginPopup("##ConsoleFiltersPopup"))
	{
		if (ImGui::SmallButton("All"))
		{
			SetAllLevelFilters(true);
			ConsoleState.m_TextFilter.clear();
			ConsoleState.m_CategoryFilter.clear();
			RequestScrollToBottom();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Warn+"))
		{
			SetWarningsAndErrorsFilter();
			RequestScrollToBottom();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Errors"))
		{
			SetErrorsFilter();
			RequestScrollToBottom();
		}

		ImGui::SeparatorText("Category");
		if (ImGui::SmallButton("Scripts"))
		{
			ConsoleState.m_CategoryFilter = "Script";
			RequestScrollToBottom();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Assets"))
		{
			ConsoleState.m_CategoryFilter = "Asset";
			RequestScrollToBottom();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Project"))
		{
			ConsoleState.m_CategoryFilter = "Project";
			RequestScrollToBottom();
		}
		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::InputTextWithHint("##ConsoleCategoryFilter", "Filter category", &ConsoleState.m_CategoryFilter))
			RequestScrollToBottom();

		ImGui::SeparatorText("Levels");
		for (size_t i = 0; i < ConsoleState.m_LevelFilter.size(); ++i)
		{
			if (i % 2 == 1)
				ImGui::SameLine(130.0f);
			if (ImGui::Checkbox(LevelName(static_cast<Log::Level>(i)), &ConsoleState.m_LevelFilter[i]))
				RequestScrollToBottom();
		}

		ImGui::EndPopup();
	}

	SameLineIfFits(EstimatedButtonWidth("Copy visible"));
	if (ImGui::SmallButton("Copy visible"))
		CopyVisibleToClipboardUnlocked();

	const size_t visibleCount = std::ranges::count_if(ConsoleState.m_Buffer, [](const ConsoleEntry& entry) { return EntryVisible(entry); });
	const bool consoleContentChanged = ConsoleState.m_Buffer.size() != ConsoleState.m_LastRenderedBufferSize || visibleCount != ConsoleState.m_LastRenderedVisibleCount;
	const bool shouldScrollToBottom = ConsoleState.m_AutoScroll && (ConsoleState.m_RequestScrollToBottom || consoleContentChanged);
	SameLineIfFits(ImGui::CalcTextSize("000 visible").x);
	ImGui::TextDisabled("%zu visible", visibleCount);

	ImGui::SetNextItemWidth(std::max(1.0f, ImGui::GetContentRegionAvail().x));
	if (ConsoleState.m_RequestSearchFocus)
	{
		ImGui::SetKeyboardFocusHere();
		ConsoleState.m_RequestSearchFocus = false;
	}
	if (ImGui::InputTextWithHint("##ConsoleTextFilter", "Search message, level, time", &ConsoleState.m_TextFilter))
		RequestScrollToBottom();

	ImGui::Separator();
	const ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable("##ConsoleTable", 4, tableFlags, ImVec2(0.0f, 0.0f)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 78.0f);
		ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 72.0f);
		ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthFixed, 1180.0f);
		ImGui::TableHeadersRow();

		size_t rowIndex = 0;
		size_t visibleRowIndex = 0;
		for (const ConsoleEntry& entry : ConsoleState.m_Buffer)
		{
			if (!EntryVisible(entry))
			{
				++rowIndex;
				continue;
			}

			const std::string rowClipboard = FormatEntryForClipboard(entry);
			ImGui::TableNextRow();
			ImGui::PushID(static_cast<int>(rowIndex));
			ImGui::TableNextColumn();
			const ImVec4 disabledColor = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
			DrawSelectableConsoleCell("time", entry.m_Timestamp, entry.m_Timestamp, rowClipboard, &disabledColor);
			ImGui::TableNextColumn();
			const ImVec4 levelColor = GetColor(entry.m_Level);
			DrawSelectableConsoleCell("level", LevelName(entry.m_Level), LevelName(entry.m_Level), rowClipboard, &levelColor);
			ImGui::TableNextColumn();
			DrawSelectableConsoleCell("category", entry.m_Category, entry.m_Category, rowClipboard);
			ImGui::TableNextColumn();
			const std::string message = SingleLineText(entry.m_Message);
			DrawSelectableConsoleCell("message", message, entry.m_Message, rowClipboard);
			ImGui::PopID();
			++visibleRowIndex;
			++rowIndex;
			if (shouldScrollToBottom && visibleRowIndex == visibleCount)
				ImGui::SetScrollHereY(1.0f);
		}
		ImGui::EndTable();
	}

	ConsoleState.m_RequestScrollToBottom = false;
	ConsoleState.m_LastRenderedBufferSize = ConsoleState.m_Buffer.size();
	ConsoleState.m_LastRenderedVisibleCount = visibleCount;

	ImGui::PopStyleVar(2);
	ImGui::End();
}

bool ConsolePanel::IsShortcutContextActive()
{
	return ConsoleState.m_Open && ConsoleState.m_Focused;
}

void ConsolePanel::Clear()
{
	std::lock_guard lock(ConsoleState.m_Mutex);
	ConsoleState.m_Buffer.clear();
	ConsoleState.m_RequestScrollToBottom = true;
	SkipToEnd();
}

void ConsolePanel::CopyVisible()
{
	WHP_PROFILE_FUNCTION();
	std::lock_guard lock(ConsoleState.m_Mutex);
	CopyVisibleToClipboardUnlocked();
}

void ConsolePanel::FocusSearch()
{
	ConsoleState.m_RequestSearchFocus = true;
}

void ConsolePanel::ClearFilters()
{
	std::lock_guard lock(ConsoleState.m_Mutex);
	ConsoleState.m_TextFilter.clear();
	ConsoleState.m_CategoryFilter.clear();
	SetAllLevelFilters(true);
}

void ConsolePanel::ShowAllLevels()
{
	std::lock_guard lock(ConsoleState.m_Mutex);
	SetAllLevelFilters(true);
}

void ConsolePanel::ShowWarningsAndErrors()
{
	std::lock_guard lock(ConsoleState.m_Mutex);
	SetWarningsAndErrorsFilter();
}

void ConsolePanel::ShowErrorsOnly()
{
	std::lock_guard lock(ConsoleState.m_Mutex);
	SetErrorsFilter();
}

void ConsolePanel::ToggleAutoScroll()
{
	ConsoleState.m_AutoScroll = !ConsoleState.m_AutoScroll;
	if (ConsoleState.m_AutoScroll)
		ConsoleState.m_RequestScrollToBottom = true;
}

std::vector<std::string> ConsolePanel::GetRecentMessages(size_t maxCount)
{
	WHP_PROFILE_FUNCTION();
	std::lock_guard lock(ConsoleState.m_Mutex);
	std::vector<std::string> messages;
	if (maxCount == 0 || ConsoleState.m_Buffer.empty())
		return messages;

	const size_t start = ConsoleState.m_Buffer.size() > maxCount ? ConsoleState.m_Buffer.size() - maxCount : 0;
	messages.reserve(ConsoleState.m_Buffer.size() - start);
	for (size_t i = start; i < ConsoleState.m_Buffer.size(); ++i)
	{
		const ConsoleEntry& entry = ConsoleState.m_Buffer[i];
		messages.push_back("[" + entry.m_Timestamp + "] " + LevelName(entry.m_Level) + " " + entry.m_Category + ": " + SingleLineText(entry.m_Message));
	}
	return messages;
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
