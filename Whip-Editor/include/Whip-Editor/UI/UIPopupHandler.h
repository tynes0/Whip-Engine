#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Helper/Buffer.h>
#include <Whip/Asset/Asset.h>

#include <functional>
#include <string>

_WHIP_START

namespace UI
{
	class PopupHandler
	{

	public:
		using FunctionType = std::function<void()>;
		using FunctionTypeWithData = std::function<void(RawBuffer)>;

		PopupHandler() = default;
		PopupHandler(const std::string& name) : m_PopupName(name) {}
	
		PopupHandler& SetPopupName(const std::string& popupName) { m_PopupName = popupName; return *this; }
		PopupHandler& SetSize(float width, float height) { m_Width = width; m_Height = height; return *this; }
		PopupHandler& SetWidth(float width) { m_Width = width; return *this; }
		PopupHandler& SetHeight(float height) { m_Height = height; return *this; }
		PopupHandler& SetShowState(bool state) { m_ShowState = state; return *this; }

		PopupHandler& Add(const FunctionType& func);
		PopupHandler& Add(const FunctionTypeWithData& func);
		PopupHandler& AddButton(const FunctionType& callback, const std::string& label, float width = 0.0f, float height = 0.0f, bool visible = true);
		PopupHandler& AddButton(const FunctionTypeWithData& callback, const std::string& label, float width = 0.0f, float height = 0.0f, bool visible = true);
		PopupHandler& AddDragDrop(AssetType type, const std::function<void(AssetHandle)>& callback, const char* label, float width, float height, bool visible = true, const std::function<void()>& errorCallback = nullptr);
		PopupHandler& AddDualHandleSlider(float sliderMin, float sliderMax, float* valueOne, float* valueTwo, float sliderWidth = 0.0f, float sliderHeight = 0.0f, bool showTexts = true);
		PopupHandler& SameLine();

		const std::string& GetPopupName() const { return m_PopupName; }
		bool GetShowState() const { return m_ShowState; }
		float GetWidth() const { return m_Width; }
		float GetHeight() const { return m_Height; }
	
		void OnImGuiRender();
		void LoadUserData(RawBuffer userData);

		static constexpr float DefaultWidth = 100.0f;
		static constexpr float DefaultHeight = 100.0f;
	private:
		std::string m_PopupName;
		std::vector<FunctionType> m_DrawList;
		std::vector<FunctionType> m_DrawListWithData;
		float m_Width = DefaultWidth, m_Height = DefaultHeight;
		bool m_ShowState = false;
		RawBuffer m_UserData = nullptr;
	};
}

_WHIP_END
