#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <Whip/Core/Application.h>
#include <Whip/Project/project.h>

#include <string>

#include <imgui.h>

#include "UI_popup_handler.h"

_WHIP_START

namespace UI
{
	class UI_project_loader
	{
	public:
		void run()
		{
			// Pencerenin ortasýný hesapla
			auto& app = application::get();
			auto& window = app.get_window();

			ImVec2 window_size{ (float)window.get_width(), (float)window.get_height() };
			ImVec2 window_center{ window_size.x / 2.0f, window_size.y / 2.0f };

			// Pencerenin ortasýna yerleþtirilecek
			ImGui::SetNextWindowPos(window_center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

			// Proje yükleyici penceresi
			ImGui::Begin("Project Loader", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

			ImGui::Text("Bir seçenek belirleyin:");
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// "Yeni Proje Oluþtur" butonu
			if (ImGui::Button("Yeni Proje Oluþtur", ImVec2(200, 50)))
			{
				// Yeni proje oluþturma iþlemi (callback) 
				if (load_callback)
				{
					load_callback(); // Yeni proje oluþturma için fonksiyon çaðýrýlýr
				}
				m_loaded = true;
			}

			ImGui::Spacing();

			// "Varolan Projeyi Yükle" butonu
			if (ImGui::Button("Varolan Projeyi Yükle", ImVec2(200, 50)))
			{
				// Varolan proje yükleme iþlemi (callback)
				if (load_callback)
				{
					load_callback(); // Var olan proje yükleme için fonksiyon çaðýrýlýr
				}
				m_loaded = true;
			}

			ImGui::End();
		}

	private:
		bool m_loaded = false;
		void(*load_callback)();
	};
}

_WHIP_END
