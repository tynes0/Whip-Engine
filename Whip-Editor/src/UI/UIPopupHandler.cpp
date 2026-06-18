#include <WhipPch.h>
#include <Whip-Editor/UI/UIPopupHandler.h>

#include <Whip-Editor/UI/UIHelpers.h>
#include <Whip/Core/Application.h>

#include <imgui.h>

_WHIP_START

UI::PopupHandler& UI::PopupHandler::Add(const FunctionType& func)
{
	m_DrawList.emplace_back(func);
	return *this;
}

UI::PopupHandler& UI::PopupHandler::Add(const FunctionTypeWithData& func)
{
	//m_DrawListWithData.emplace_back(func);
	return *this;
}

UI::PopupHandler& UI::PopupHandler::AddButton(const FunctionType& callback, const std::string& label, float width, float height, bool visible)
{
	m_DrawList.emplace_back([=]() 
		{  
			if (visible)
			{
				if (ImGui::Button(label.c_str(), ImVec2(width, height)))
					callback();
			}
			else
			{
				if (ImGui::InvisibleButton(label.c_str(), ImVec2(width, height)))
					callback();
			}
		});
	return *this;
}

UI::PopupHandler& UI::PopupHandler::AddButton(const FunctionTypeWithData& callback, const std::string& label, float width, float height, bool visible)
{
	/*m_DrawListWithData.emplace_back([=](RawBuffer data)
		{
			if (visible)
			{
				if (ImGui::Button(label.c_str(), ImVec2(width, height)))
					callback(data);
			}
			else
			{
				if (ImGui::InvisibleButton(label.c_str(), ImVec2(width, height)))
					callback(data);
			}
		});*/
	return *this;
}

UI::PopupHandler& UI::PopupHandler::AddDragDrop(AssetType type, const std::function<void(AssetHandle)>& callback, const char* label, float width, float height, bool visible, const std::function<void()>& errorCallback)
{
	m_DrawList.emplace_back([=]() 
		{
			UI::DragDropTarget(type, callback, label, true, width, height, visible, errorCallback);
		});
	return *this;
}

UI::PopupHandler& UI::PopupHandler::AddDualHandleSlider(float sliderMin, float sliderMax, float* valueOne, float* valueTwo, float sliderWidth, float sliderHeight, bool showTexts)
{
	m_DrawList.emplace_back([=]()
		{
			UI::DrawDualHandleSlider(sliderMin, sliderMax, valueOne, valueTwo, sliderWidth, sliderHeight, showTexts);
		});
	return *this;
}

UI::PopupHandler& UI::PopupHandler::SameLine()
{
	m_DrawList.emplace_back([]() { ImGui::SameLine(); });
	return *this;
}

void UI::PopupHandler::OnImGuiRender()
{
	if (m_ShowState && !m_PopupName.empty())
	{
		auto& app = Application::Get();
		auto& window = app.GetWindow();

		ImVec2 windowSize{ (float)window.GetWidth(), (float)window.GetHeight() };
		ImVec2 windowPos{ (float)window.GetPosition().first, (float)window.GetPosition().second };
		ImVec2 popupPos = ImVec2{ ((windowSize.x - m_Width) * 0.5f) + windowPos.x, ((windowSize.y - m_Height) * 0.5f) + windowPos.y };

		ImGui::SetNextWindowSize(ImVec2(m_Width, m_Height));
		ImGui::SetNextWindowPos(popupPos);
		ImGui::OpenPopup(m_PopupName.c_str());

		if (ImGui::BeginPopupModal(m_PopupName.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::PushTextWrapPos(m_Width);
			for (const auto& func : m_DrawList)
				func();

			if (m_ShowState && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered() && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) 
			{
				m_ShowState = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopTextWrapPos();
			ImGui::EndPopup();
		}

	}
}

void UI::PopupHandler::LoadUserData(RawBuffer userData)
{
	m_UserData = userData;
}

_WHIP_END
