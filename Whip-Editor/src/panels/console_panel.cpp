#include "console_panel.h"

#include <array>
#include <cctype>
#include <string>
#include <vector>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <FileWatch.h>

#include <Whip/Core/memory.h>
#include <Whip/UI/UI_scoped_style.h>

#include <algorithm>

_WHIP_START

struct console_entry
{
	log::level level = log::level::trace;
	std::string timestamp;
	std::string category;
	std::string message;
	std::string search_blob;
};

struct console_data 
{
	std::vector<console_entry> buffer;
	std::thread file_watcher_thread;
	std::mutex mtx;
	std::uintmax_t last_stream_index = 0;
	std::uintmax_t cleared_stream_index = 0;
	std::atomic<bool> running;
	std::filesystem::file_time_type last_write_time{};
	std::string text_filter;
	std::string category_filter;
	std::array<bool, 6> level_filter = { true, true, true, true, true, true };
	bool auto_scroll = true;
	bool open = true;
	bool open_dirty = false;

	static constexpr size_t max_console_lines = 500;
	static constexpr size_t erase_count = 100;
};

console_data g_data;

namespace utils
{
	static std::string trim(std::string value)
	{
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
			value.erase(value.begin());
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
			value.pop_back();
		return value;
	}

	static std::string to_lower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value;
	}

	static const char* level_name(log::level level)
	{
		switch (level)
		{
		case log::level::trace:    return "Trace";
		case log::level::debug:    return "Debug";
		case log::level::info:     return "Info";
		case log::level::warning:  return "Warn";
		case log::level::error:    return "Error";
		case log::level::critical: return "Critical";
		default:                   return "Log";
		}
	}

	static size_t level_index(log::level level)
	{
		switch (level)
		{
		case log::level::trace:    return 0;
		case log::level::debug:    return 1;
		case log::level::info:     return 2;
		case log::level::warning:  return 3;
		case log::level::error:    return 4;
		case log::level::critical: return 5;
		default:                   return 0;
		}
	}

	static console_entry parse_entry(log::level level, std::string raw_message)
	{
		console_entry entry;
		entry.level = level;
		raw_message = trim(std::move(raw_message));

		size_t timestamp_end = std::string::npos;
		if (!raw_message.empty() && raw_message.front() == '[')
		{
			timestamp_end = raw_message.find(']');
			if (timestamp_end != std::string::npos)
				entry.timestamp = raw_message.substr(1, timestamp_end - 1);
		}

		const size_t category_start = timestamp_end == std::string::npos ? 0 : timestamp_end + 1;
		const size_t category_end = raw_message.find(':', category_start);
		if (category_end != std::string::npos)
		{
			entry.category = trim(raw_message.substr(category_start, category_end - category_start));
			entry.message = trim(raw_message.substr(category_end + 1));
		}
		else
		{
			entry.category = "General";
			entry.message = raw_message;
		}

		if (!entry.message.empty() && entry.message.front() == '[')
		{
			const size_t subcategory_end = entry.message.find(']');
			if (subcategory_end != std::string::npos && subcategory_end > 1)
			{
				const std::string subcategory = entry.message.substr(1, subcategory_end - 1);
				entry.category += "/" + subcategory;
			}
		}

		entry.search_blob = to_lower(entry.timestamp + " " + entry.category + " " + entry.message + " " + level_name(entry.level));
		return entry;
	}

	static bool entry_visible(const console_entry& entry)
	{
		const size_t idx = level_index(entry.level);
		if (idx >= g_data.level_filter.size() || !g_data.level_filter[idx])
			return false;

		const std::string text_filter = to_lower(g_data.text_filter);
		if (!text_filter.empty() && entry.search_blob.find(text_filter) == std::string::npos)
			return false;

		const std::string category_filter = to_lower(g_data.category_filter);
		if (!category_filter.empty() && to_lower(entry.category).find(category_filter) == std::string::npos)
			return false;

		return true;
	}

	static std::uintmax_t log_file_size()
	{
		std::error_code error;
		const auto& path = editor_log::get_log_filepath();
		if (!std::filesystem::exists(path, error))
			return 0;
		const std::uintmax_t size = std::filesystem::file_size(path, error);
		return error ? 0 : size;
	}

	static void skip_to_end()
	{
		const std::uintmax_t size = log_file_size();
		g_data.last_stream_index = size;
		g_data.cleared_stream_index = size;
		editor_log::file_should_reset().store(false);
	}

	static void reset_buffer()
	{
		const auto& log_path = editor_log::get_log_filepath();
		std::error_code error;
		if (!std::filesystem::exists(log_path, error))
			return;

		const std::uintmax_t end_pos = std::filesystem::file_size(log_path, error);
		if (error)
			return;

		if (end_pos < g_data.last_stream_index)
		{
			g_data.last_stream_index = 0;
			g_data.cleared_stream_index = 0;
		}

		if (g_data.last_stream_index < g_data.cleared_stream_index)
			g_data.last_stream_index = g_data.cleared_stream_index;

		if (end_pos <= g_data.last_stream_index)
			return;

		std::ifstream stream(log_path, std::ios::binary);
		if (!stream.is_open())
			return;

		const std::uintmax_t buf_size = end_pos - g_data.last_stream_index;
		std::string buf;
		buf.resize(static_cast<size_t>(buf_size));
		stream.seekg(static_cast<std::streamoff>(g_data.last_stream_index), std::ios::beg);
		stream.read(buf.data(), static_cast<std::streamsize>(buf.size()));
		const std::streamsize bytes_read = stream.gcount();

		g_data.last_stream_index = end_pos;
		if (bytes_read <= 0)
			return;
		if (static_cast<size_t>(bytes_read) < buf.size())
			buf.resize(static_cast<size_t>(bytes_read));

		static constexpr const char* token = "level::";
		static constexpr size_t token_length = sizeof(token) - 1;

		size_t temp = 0;
		size_t current_pos = buf.find(token);
		size_t next_token_pos = 0;

		if (current_pos == std::string::npos)
		{
			// ERROR
			return;
		}

		bool run = true;

		while (run)
		{
			if (g_data.buffer.size() >= console_data::max_console_lines)
				g_data.buffer.erase(g_data.buffer.begin(), g_data.buffer.begin() + console_data::erase_count);
			current_pos += token_length;

			temp = buf.find(',', current_pos);

			if (temp == std::string::npos || temp == buf.size() - 1)
			{
				// ERROR
				return;
			}

			auto opt_level = frenum::cast<log::level>(buf.substr(current_pos, temp - current_pos));
			log::level level = opt_level.has_value() ? *opt_level : log::level::trace;
			current_pos = temp + 1;

			next_token_pos = buf.find(token, current_pos);
			if (next_token_pos == std::string::npos)
			{
				next_token_pos = buf.size();
				run = false;
			}

			std::string message_content = buf.substr(current_pos, next_token_pos - current_pos);
			g_data.buffer.emplace_back(parse_entry(level, message_content));
			current_pos = next_token_pos;
		}
	}

	static void monitor_flag() 
	{
		while (g_data.running.load()) 
		{
			bool should_update = editor_log::file_should_reset().exchange(false);
			std::error_code error;
			const auto& log_path = editor_log::get_log_filepath();
			if (std::filesystem::exists(log_path, error))
			{
				auto write_time = std::filesystem::last_write_time(log_path, error);
				if (!error && write_time != g_data.last_write_time)
				{
					g_data.last_write_time = write_time;
					should_update = true;
				}
			}

			if (should_update)
				application::get().submit_to_main_thread(utils::reset_buffer);

			std::this_thread::sleep_for(std::chrono::milliseconds(80));
		}
	}
}

void console_panel::initialize()
{
	if (editor_log::should_log())
	{
		std::error_code error;
		g_data.last_write_time = std::filesystem::last_write_time(editor_log::get_log_filepath(), error);

		utils::reset_buffer();
		g_data.running.store(true);

		if (!g_data.file_watcher_thread.joinable())
			g_data.file_watcher_thread = std::thread(utils::monitor_flag);
	}
}

void console_panel::shutdown()
{
	g_data.running.store(false);
	if (g_data.file_watcher_thread.joinable())
		g_data.file_watcher_thread.join();

	g_data.buffer.clear();
	g_data.last_stream_index = 0;
	g_data.cleared_stream_index = 0;
	editor_log::log_state(false);
	editor_log::erase();
	if (std::filesystem::exists(editor_log::get_log_filepath()))
	{
		std::error_code error;
		std::filesystem::resize_file(editor_log::get_log_filepath(), 0, error);
	}
}

void console_panel::on_imgui_render()
{
	std::lock_guard lock(g_data.mtx);

	if (!editor_log::should_log())
		return;
	if (!g_data.open)
		return;

	static constexpr ImU32 TRACE_COLOR = IM_COL32(214, 208, 196, 255);
	static constexpr ImU32 DEBUG_COLOR = IM_COL32(166, 154, 190, 255);
	static constexpr ImU32 INFO_COLOR = IM_COL32(138, 184, 134, 255);
	static constexpr ImU32 WARN_COLOR = IM_COL32(226, 180, 92, 255);
	static constexpr ImU32 ERROR_COLOR = IM_COL32(222, 104, 104, 255);
	static constexpr ImU32 CRITICAL_COLOR = IM_COL32(218, 116, 178, 255);

	static constexpr auto get_color = [](log::level level) -> ImVec4
		{
			switch (level)
			{
			case whip::log::level::trace:		return ImVec4(ImColor(TRACE_COLOR));
			case whip::log::level::debug:		return ImVec4(ImColor(DEBUG_COLOR));
			case whip::log::level::info:		return ImVec4(ImColor(INFO_COLOR));
			case whip::log::level::warning:		return ImVec4(ImColor(WARN_COLOR));
			case whip::log::level::error:		return ImVec4(ImColor(ERROR_COLOR));
			case whip::log::level::critical:	return ImVec4(ImColor(CRITICAL_COLOR));
			default:							return ImVec4(ImColor(TRACE_COLOR));
			}
		};

	bool open = g_data.open;
	ImGui::Begin("Console", &open);
	if (open != g_data.open)
	{
		g_data.open = open;
		g_data.open_dirty = true;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));

	if (ImGui::Button("Clear", ImVec2(74.0f, 0.0f)))
	{
		g_data.buffer.clear();
		utils::skip_to_end();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &g_data.auto_scroll);
	ImGui::SameLine();
	ImGui::TextDisabled("%zu logs", g_data.buffer.size());

	ImGui::SetNextItemWidth(std::max(180.0f, ImGui::GetContentRegionAvail().x * 0.58f));
	ImGui::InputTextWithHint("##ConsoleTextFilter", "Filter message, level, time", &g_data.text_filter);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(std::max(160.0f, ImGui::GetContentRegionAvail().x));
	ImGui::InputTextWithHint("##ConsoleCategoryFilter", "Filter category", &g_data.category_filter);

	for (size_t i = 0; i < g_data.level_filter.size(); ++i)
	{
		if (i > 0)
			ImGui::SameLine();
		ImGui::Checkbox(utils::level_name(static_cast<log::level>(i)), &g_data.level_filter[i]);
	}

	ImGui::Separator();
	ImGui::BeginChild("##ConsoleScroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

	if (ImGui::BeginTable("##ConsoleTable", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 82.0f);
		ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 82.0f);
		ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 190.0f);
		ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		for (const console_entry& entry : g_data.buffer)
		{
			if (!utils::entry_visible(entry))
				continue;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", entry.timestamp.c_str());
			ImGui::TableNextColumn();
			ImGui::TextColored(get_color(entry.level), "%s", utils::level_name(entry.level));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(entry.category.c_str());
			ImGui::TableNextColumn();
			ImGui::TextWrapped("%s", entry.message.c_str());
		}
		ImGui::EndTable();
	}

	if (g_data.auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
		ImGui::SetScrollHereY(1.0f);

	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::End();
}

void console_panel::set_open(bool open)
{
	std::lock_guard lock(g_data.mtx);
	if (g_data.open == open)
		return;
	g_data.open = open;
	g_data.open_dirty = true;
}

bool console_panel::is_open()
{
	std::lock_guard lock(g_data.mtx);
	return g_data.open;
}

bool console_panel::consume_open_dirty()
{
	std::lock_guard lock(g_data.mtx);
	const bool dirty = g_data.open_dirty;
	g_data.open_dirty = false;
	return dirty;
}

_WHIP_END
