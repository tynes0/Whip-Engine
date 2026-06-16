#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Animation/Animation2D.h>

#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include <type_traits>

#include <glm/vec3.hpp>
#include <imgui.h>

_WHIP_START

namespace UI
{

	WHP_NODISCARD inline ImTextureID ToImGuiTextureId(uint64_t rendererId)
	{
		return rendererId;
	}

	WHP_NODISCARD inline ImTextureID ToImGuiTextureId(uint32_t rendererId)
	{
		return ToImGuiTextureId(static_cast<uint64_t>(rendererId));
	}

	inline void Image(ImTextureID textureId, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tintCol = ImVec4(1, 1, 1, 1), const ImVec4& borderCol = ImVec4(0, 0, 0, 0))
	{
		ImGui::Image(textureId, size, uv0, uv1, tintCol, borderCol);
	}

	WHP_NODISCARD inline bool ImageButton(const char* id, ImTextureID textureId, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), int framePadding = -1, const ImVec4& bgCol = ImVec4(0, 0, 0, 0), const ImVec4& tintCol = ImVec4(1, 1, 1, 1))
	{
#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 18900
		WHP_UNUSED(framePadding);
		return ImGui::ImageButton(id, textureId, size, uv0, uv1, bgCol, tintCol);
#else
		WHP_UNUSED(id);
		return ImGui::ImageButton(textureId, size, uv0, uv1, framePadding, bgCol, tintCol);
#endif
	}

	void DragDropTarget(AssetType type, const std::function<void(AssetHandle)>& callback, const char* label, bool drawButton = true, float xSize = 0.0f, float ySize = 0.0f, bool visible = true, const std::function<void()>& errorCallback = nullptr);

	void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f, float spacing = 0.0f, bool noText = false);
	bool DrawFieldVec2Control(const std::string& labelForId, glm::vec2& values, float resetValue = 0.0f, float columnWidth = 100.0f);
	bool DrawFieldVec3Control(const std::string& labelForId, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f);

	void DrawDualHandleSlider(float sliderMin, float sliderMax, float* valueOne, float* valueTwo, float sliderWidth = 0.0f, float sliderHeight = 0.0f, bool showTexts = true);
	void DrawTimelineWithNodes(Ref<Animation2D> anim, float initialDrawRange = 10.0f, float timelineWidth = 200.0f, float timelineHeight = 100.0f, float totalHeight = 150.0f, float maxValue = FLT_MAX, int* selectedIndex = nullptr);
}

_WHIP_END
