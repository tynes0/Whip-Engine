#include "console_panel.h"

#include <string>
#include <vector>

#include <imgui.h>
#include <FileWatch.h>

#include <Whip/Core/memory.h>
#include <Whip/UI/UI_scoped_style.h>

_WHIP_START

struct console_data 
{
	std::vector<std::pair<log::level, std::string>> buffer;
	std::thread file_watcher_thread;
	std::ifstream stream;
	std::mutex mtx;
	std::streamoff last_stream_index = 0;
	std::atomic<bool> running;
	std::atomic<bool> filestream_was_open;

	static constexpr size_t max_console_lines = 500;
	static constexpr size_t erase_count = 100;
};

console_data g_data;

namespace utils
{
	static void clear_filestream_context()
	{
		if (!g_data.stream.is_open())
			return; // not opened
		g_data.stream.clear();
		g_data.last_stream_index = 0;
	}

	static bool reopen()
	{
		if (g_data.stream.is_open())
			return true; // already opened
		g_data.stream.open(editor_log::get_log_filepath());
		return g_data.stream.is_open();
	}

	static void reset_buffer()
	{
		if (!g_data.stream)
		{
			if (g_data.filestream_was_open.load())
			{
				if (reopen())
					goto skip_error;
			}
			WHP_CLIENT_ERROR("[Console Panel] Filestream is undefined!");
			return;
		skip_error:;
		}

		g_data.stream.seekg(0, std::ios::end);
		std::streampos end_pos = g_data.stream.tellg();
		if (end_pos < g_data.last_stream_index)
		{
			clear_filestream_context();
			return;
		}
		g_data.stream.seekg(g_data.last_stream_index);

		std::streamoff buf_size = std::streamoff(end_pos) - g_data.last_stream_index;
		std::string buf;
		buf.resize(buf_size);
		g_data.stream.read(buf.data(), buf_size);

		g_data.last_stream_index = end_pos;

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
			log::level level = editor_log::whip_log_level_from_string(buf.substr(current_pos, temp - current_pos));
			current_pos = temp + 1;

			next_token_pos = buf.find(token, current_pos);
			if (next_token_pos == std::string::npos)
			{
				next_token_pos = buf.size();
				run = false;
			}

			std::string message_content = buf.substr(current_pos, next_token_pos - current_pos);

			g_data.buffer.emplace_back(level, message_content);
			current_pos = next_token_pos;
		}
	}

	static void monitor_flag() 
	{
		while (g_data.running.load()) 
		{
			if (editor_log::file_should_reset().load())
			{
				application::get().submit_to_main_thread(utils::reset_buffer);
				editor_log::file_should_reset().store(false);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
	}
}

void consol_panel::initialize()
{
	if (editor_log::should_log())
	{
		g_data.stream.open(editor_log::get_log_filepath());
		if (g_data.stream.is_open())
			g_data.filestream_was_open.store(true);
		else
			g_data.filestream_was_open.store(false);

		utils::reset_buffer();
		g_data.running.store(true);

		if (!g_data.file_watcher_thread.joinable())
			g_data.file_watcher_thread = std::thread(utils::monitor_flag);
	}
}

void consol_panel::shutdown()
{
	g_data.running.store(false);
	g_data.stream.close();
	if (g_data.file_watcher_thread.joinable())
		g_data.file_watcher_thread.join();

	g_data.buffer.clear();
	editor_log::log_state(false);
	editor_log::erase();
	if (std::filesystem::exists(editor_log::get_log_filepath()))
		std::filesystem::resize_file(editor_log::get_log_filepath(), 0);
}

void consol_panel::render_imgui_console() 
{
	std::lock_guard lock(g_data.mtx);

	if (!editor_log::should_log())
		return;

	static constexpr ImU32 TRACE_COLOR = IM_COL32(255, 255, 255, 255);
	static constexpr ImU32 DEBUG_COLOR = IM_COL32(50, 50, 255, 255);
	static constexpr ImU32 INFO_COLOR = IM_COL32(50, 255, 50, 255);
	static constexpr ImU32 WARN_COLOR = IM_COL32(200, 200, 50, 255);
	static constexpr ImU32 ERROR_COLOR = IM_COL32(255, 50, 50, 255);
	static constexpr ImU32 CRITICAL_COLOR = IM_COL32(180, 20, 150, 255);

	static constexpr auto get_color = [](log::level level) -> ImVec4
		{
			switch (level)
			{
			case whip::log::level::trace:		return ImVec4(ImColor(TRACE_COLOR));
			case whip::log::level::debug:		return ImVec4(ImColor(DEBUG_COLOR));
			case whip::log::level::info:		return ImVec4(ImColor(INFO_COLOR));
			case whip::log::level::warn:		return ImVec4(ImColor(WARN_COLOR));
			case whip::log::level::err:			return ImVec4(ImColor(ERROR_COLOR));
			case whip::log::level::critical:	return ImVec4(ImColor(CRITICAL_COLOR));
			default:							return ImVec4(ImColor(TRACE_COLOR));
			}
		};

	static auto draw_custom_text = [](const char* text, const ImVec2& pos, float size, ImU32 color)
		{
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			draw_list->AddText(ImGui::GetFont(), size, pos, color, text);
		};

	{
		UI::scoped_style_bold_font boldf(true);

		ImGui::Begin("Console");

		size_t idx = 1;
		for (auto it = g_data.buffer.rbegin(); it != g_data.buffer.rend(); it++)
		{
			const auto& text = *it;
			ImGui::TextColored(get_color(text.first), text.second.c_str());
		}

		ImGui::End();
	}
}

_WHIP_END
