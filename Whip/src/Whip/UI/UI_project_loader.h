#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <Whip/Core/Application.h>
#include <Whip/Project/project.h>

#include <functional>
#include <string>
#include <utility>

#include <imgui.h>

#include "UI_popup_handler.h"

_WHIP_START

namespace UI
{
	class UI_project_loader
	{
	public:
		using callback = std::function<void()>;

		void set_create_project_callback(callback create_callback)
		{
			m_create_project_callback = std::move(create_callback);
		}

		void set_load_project_callback(callback load_callback)
		{
			m_load_project_callback = std::move(load_callback);
		}

		bool loaded() const { return m_loaded; }
		void reset() { m_loaded = false; }

		void run()
		{
			// Center the loader window.
			auto& app = application::get();
			auto& window = app.get_window();

			ImVec2 window_size{ (float)window.get_width(), (float)window.get_height() };
			ImVec2 window_center{ window_size.x / 2.0f, window_size.y / 2.0f };

			ImGui::SetNextWindowPos(window_center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

			ImGui::Begin("Project Loader", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

			ImGui::Text("Choose an option:");
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Create New Project", ImVec2(200, 50)))
			{
				on_project_action(m_create_project_callback);
			}

			ImGui::Spacing();

			if (ImGui::Button("Load Existing Project", ImVec2(200, 50)))
			{
				on_project_action(m_load_project_callback);
			}

			ImGui::End();
		}

	private:
		void on_project_action(const callback& action)
		{
			if (action)
				action();

			m_loaded = true;
		}

		bool m_loaded = false;
		callback m_create_project_callback;
		callback m_load_project_callback;
	};
}

_WHIP_END
