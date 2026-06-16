#include "WhipPch.h"
#include <Whip/UI/UIHelpers.h>

#include <Whip/UI/UIScopedStyle.h>

#include <Whip/Core/Input.h>
#include <Whip/Asset/AssetManager.h>

#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include <glm/glm.hpp>

_WHIP_START

namespace Utils
{
	static void CheckRange(float rangeMin, float rangeMax, float* valueOne, float* valueTwo)
	{
		if (*valueOne > rangeMax) *valueOne = rangeMax;
		else if (*valueOne < rangeMin) *valueOne = rangeMin;
		if (*valueTwo > rangeMax) *valueTwo = rangeMax;
		else if (*valueTwo < rangeMin) *valueTwo = rangeMin;
	}
}

namespace UI
{
	namespace
	{
		const ImVec4 AxisX = ImVec4{ 0.58f, 0.22f, 0.20f, 1.0f };
		const ImVec4 AxisXHover = ImVec4{ 0.70f, 0.30f, 0.26f, 1.0f };
		const ImVec4 AxisXActive = ImVec4{ 0.84f, 0.40f, 0.32f, 1.0f };
		const ImVec4 AxisY = ImVec4{ 0.32f, 0.50f, 0.27f, 1.0f };
		const ImVec4 AxisYHover = ImVec4{ 0.40f, 0.61f, 0.34f, 1.0f };
		const ImVec4 AxisYActive = ImVec4{ 0.50f, 0.70f, 0.42f, 1.0f };
		const ImVec4 AxisZ = ImVec4{ 0.34f, 0.36f, 0.50f, 1.0f };
		const ImVec4 AxisZHover = ImVec4{ 0.42f, 0.44f, 0.62f, 1.0f };
		const ImVec4 AxisZActive = ImVec4{ 0.52f, 0.54f, 0.74f, 1.0f };
	}

	void DragDropTarget(AssetType type, const std::function<void(AssetHandle)>& callback, const char* label, bool drawButton, float xSize, float ySize, bool visible, const std::function<void()>& errorCallback)
	{
		if (type == AssetType::None)
		{
			WHP_CORE_ERROR("[Asset Manager] Undefined type!");
			return;
		}
		ImVec2 cursorPos = ImGui::GetCursorPos();

		if(drawButton)
		{
			if (visible)
			{
				ImGui::Button(label, ImVec2(xSize, ySize));
			}
			else
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::InvisibleButton(label, ImVec2(xSize, ySize), ImGuiButtonFlags_AllowOverlap);
				ImGui::PopItemFlag();
			}
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				AssetHandle handle = *(AssetHandle*)payload->Data;
				if (AssetManager::GetAssetType(handle) == type)
				{
					callback(handle);
				}
				else
				{
					if (errorCallback)
						errorCallback();
					else
						WHP_CORE_WARN("[Asset Manager] Wrong Asset type!");
				}
			}
			ImGui::EndDragDropTarget();
		}
		if(!visible)
			ImGui::SetCursorPos(cursorPos);
	}

	void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue, float columnWidth, float spacing, bool noText)
	{
		ImGui::PushID(label.c_str());

		if (!noText)
		{
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, columnWidth);
			ImGui::Text(label.c_str());
			ImGui::NextColumn();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
		ImGui::PushMultiItemsWidths(3, ImGui::GetWindowWidth() - columnWidth - buttonSize.x * 3 - spacing);

		ImGui::PushStyleColor(ImGuiCol_Button, AxisX);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AxisXHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, AxisXActive);
		{
			ScopedStyleColor scopeColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			ScopedStyleBoldFont boldFont;
			if (ImGui::Button("X", buttonSize))
				values.x = resetValue;
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, AxisY);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AxisYHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, AxisYActive);
		{
			ScopedStyleColor scopeColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			ScopedStyleBoldFont boldFont;
			if (ImGui::Button("Y", buttonSize))
				values.y = resetValue;
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, AxisZ);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AxisZHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, AxisZActive);
		{
			ScopedStyleColor scopeColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			ScopedStyleBoldFont boldFont;
			if (ImGui::Button("Z", buttonSize))
				values.z = resetValue;
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}
	bool DrawFieldVec2Control(const std::string& labelForId, glm::vec2& values, float resetValue, float columnWidth)
	{
		ImGui::PushID(labelForId.c_str());

		bool changed = false;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		columnWidth -= buttonSize.x * 2;

		ImGui::PushStyleColor(ImGuiCol_Button, AxisX);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AxisXHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, AxisXActive);
		{
			ScopedStyleColor scopeColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			ScopedStyleBoldFont boldFont;
			if (ImGui::Button("X", buttonSize))
			{
				values.x = resetValue;
				changed = true;
			}
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushItemWidth(columnWidth / 2);
		if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f"))
			changed = true;
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, AxisY);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AxisYHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, AxisYActive);
		{
			ScopedStyleColor scopeColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			ScopedStyleBoldFont boldFont;
			if (ImGui::Button("Y", buttonSize))
			{
				values.y = resetValue;
				changed = true;
			}
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f"))
			changed = true;
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::PopID();
		return changed;
	}

	bool DrawFieldVec3Control(const std::string& labelForId, glm::vec3& values, float resetValue, float columnWidth)
	{
		ImGui::PushID(labelForId.c_str());

		bool changed = false;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		columnWidth -= buttonSize.x * 3;

		ImGui::PushStyleColor(ImGuiCol_Button, AxisX);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AxisXHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, AxisXActive);
		{
			ScopedStyleColor scopeColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			ScopedStyleBoldFont boldFont;
			if (ImGui::Button("X", buttonSize))
			{
				values.x = resetValue;
				changed = true;
			}
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushItemWidth(columnWidth / 3);
		if(ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f"))
			changed = true;
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, AxisY);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AxisYHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, AxisYActive);
		{
			ScopedStyleColor scopeColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			ScopedStyleBoldFont boldFont;
			if (ImGui::Button("Y", buttonSize))
			{
				values.y = resetValue;
				changed = true;
			}
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushItemWidth(columnWidth / 3);
		if(ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f"))
			changed = true;
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, AxisZ);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AxisZHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, AxisZActive);
		{
			ScopedStyleColor scopeColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			ScopedStyleBoldFont boldFont;
			if (ImGui::Button("Z", buttonSize))
			{
				values.z = resetValue;
				changed = true;
			}
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		if(ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f"))
			changed = true;
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::PopID();
		return changed;
	}

	void DrawDualHandleSlider(float sliderMin, float sliderMax, float* valueOne, float* valueTwo, float sliderWidth, float sliderHeight, bool showTexts)
	{
		Utils::CheckRange(sliderMin, sliderMax, valueOne, valueTwo);

		ImVec2 sliderPos = ImGui::GetCursorScreenPos();
		if(sliderWidth == 0.0f)
			sliderWidth = ImGui::GetContentRegionAvail().x;
		if(sliderHeight == 0.0f)
			sliderHeight = 20.0f;

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImU32 bgColor = IM_COL32(30, 29, 26, 255);
		ImU32 fillColor = IM_COL32(156, 104, 52, 255);
		ImU32 handleColor = IM_COL32(222, 212, 194, 220);
		float rangeStartX = sliderPos.x + (*valueOne - sliderMin) / (sliderMax - sliderMin) * sliderWidth;
		float rangeEndX = sliderPos.x + (*valueTwo - sliderMin) / (sliderMax - sliderMin) * sliderWidth;

		drawList->AddRectFilled(sliderPos, ImVec2(sliderPos.x + sliderWidth, sliderPos.y + sliderHeight), bgColor, 1.0f);
		drawList->AddRectFilled(ImVec2(rangeStartX, sliderPos.y), ImVec2(rangeEndX, sliderPos.y + sliderHeight), fillColor, 1.0f);
		drawList->AddRectFilled(ImVec2(rangeStartX - 4, sliderPos.y), ImVec2(rangeStartX + 4, sliderPos.y + sliderHeight), handleColor, 2.0f);
		drawList->AddRectFilled(ImVec2(rangeEndX - 4, sliderPos.y), ImVec2(rangeEndX + 4, sliderPos.y + sliderHeight), handleColor, 2.0f);

		ImGui::InvisibleButton("##slider", ImVec2(sliderWidth, sliderHeight));
		if (ImGui::IsItemActive())
		{
			ImVec2 mousePos = ImGui::GetIO().MousePos;
			float normalizedPos = (mousePos.x - sliderPos.x) / sliderWidth;
			float value = sliderMin + normalizedPos * (sliderMax - sliderMin);

			if (ImGui::IsMouseDown(0))
			{
				if (fabsf(value - *valueOne) < fabsf(value - *valueTwo))
					*valueOne = ImClamp(value, sliderMin, *valueTwo);
				else
					*valueTwo = ImClamp(value, *valueOne, sliderMax);
			}
		}

		if(showTexts)
			ImGui::Text("Selected Range: %.2f - %.2f", *valueOne, *valueTwo);
	}

	void DrawTimelineWithNodes(Ref<Animation2D> anim, float initialDrawRange, float timelineWidth, float timelineHeight, float totalHeight, float maxValue, int* selectedIndex)
	{
		static constexpr float MaxInitialRange = 20.0f;
		static constexpr float ZoomMin = 0.25f;
		static constexpr float ZoomMax = 20.0f;
		static constexpr float ZoomSpeed = 0.1f;
		static constexpr float MajorInterval = 1.0f;
		static constexpr float MinorInterval = 0.1f;
		static constexpr float MiniInterval = 0.01f;

		static constexpr ImU32 MajorLineColor = IM_COL32(192, 182, 160, 190);
		static constexpr ImU32 MinorLineColor = IM_COL32(112, 100, 82, 130);
		static constexpr ImU32 MiniLineColor = IM_COL32(82, 74, 62, 105);
		static constexpr ImU32 SecondsTextColor = IM_COL32(195, 186, 170, 220);
		static constexpr ImU32 WindowBgColor = IM_COL32(17, 16, 14, 255);
		static constexpr ImU32 TimelineBgColor = IM_COL32(48, 43, 36, 255);
		static constexpr ImU32 NodeColor = IM_COL32(216, 118, 82, 255);
		static constexpr ImU32 SelectedNodeColor = IM_COL32(224, 164, 84, 255);

		static constexpr ImVec2 NodeSize(12, 12);
		static constexpr float NodeRadius = NodeSize.x / 2.0f;

		static float zoomLevel = 1.0f;
		static float offsetTime = 0.0f;

		float zoomThreshold = initialDrawRange * 0.6f;


		static constexpr auto drawMajorLine = [](ImDrawList* drawList, float x, float cursorY, float timelineHeight, float time)
			{
				drawList->AddLine(ImVec2(x, cursorY), ImVec2(x, cursorY + timelineHeight), MajorLineColor);
				char label[16];
				snprintf(label, sizeof(label), "%.0f", time);
				drawList->AddText(ImVec2(x + 2, cursorY + timelineHeight + 4), SecondsTextColor, label);
			};

		static constexpr auto drawMinorLine = [](ImDrawList* drawList, float x, float cursorY, float timelineHeight)
			{
				drawList->AddLine(ImVec2(x, cursorY), ImVec2(x, cursorY + timelineHeight * 0.4f), MinorLineColor);
			};

		static constexpr auto drawMiniLine = [](ImDrawList* drawList, float x, float cursorY, float timelineHeight)
			{
				drawList->AddLine(ImVec2(x, cursorY), ImVec2(x, cursorY + timelineHeight * 0.2f), MiniLineColor);
			};

		const auto dragDropCallback = [anim, selectedIndex](AssetHandle handle)
			{
				if (!anim)
					return;

				AnimationFrame newFrame = {};
				newFrame.m_Texture = handle;
				newFrame.m_Duration = 0.1f;
				anim->AddFrame(newFrame);
				if (selectedIndex)
					*selectedIndex = static_cast<int>(anim->GetFrames().size() - 1);
			};

		if (timelineWidth == 0.0f)
			timelineWidth = ImGui::GetContentRegionAvail().x;
		if (timelineHeight == 0.0f)
			timelineHeight = 100.0f;
		if (totalHeight == 0.0f)
			totalHeight = timelineHeight + 50.0f;

		const float animationDuration = anim ? anim->GetDuration() : 0.0f;
		const float drawRange = std::min(MaxInitialRange, std::max(initialDrawRange, animationDuration + 0.5f));

		ImGui::BeginChild("TimelineRegion", ImVec2(0, totalHeight), true);

		// drag - drop
		{
			ImVec2 windowSize = ImGui::GetWindowSize();
			ImGuiStyle style = ImGui::GetStyle();

			windowSize.x -= style.WindowPadding.x + style.ItemSpacing.x * 2;
			windowSize.y -= style.WindowPadding.y + style.ItemSpacing.y * 2;

			DragDropTarget(AssetType::Texture2D, dragDropCallback, "DragDropTexture", true, windowSize.x, windowSize.y, false);
		}

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 cursorStart = ImGui::GetCursorScreenPos();

		// background colors
		ImVec2 childMin = ImGui::GetWindowPos();
		drawList->AddRectFilled(childMin, ImVec2(childMin.x + ImGui::GetWindowWidth(), childMin.y + ImGui::GetWindowHeight()), WindowBgColor, ImGui::GetStyle().WindowRounding);
		drawList->AddRectFilled(cursorStart, ImVec2(cursorStart.x + timelineWidth, cursorStart.y + timelineHeight), TimelineBgColor, 3.0f);
		drawList->AddRect(cursorStart, ImVec2(cursorStart.x + timelineWidth, cursorStart.y + timelineHeight), IM_COL32(92, 80, 62, 170), 3.0f);

		// timeline moves
		if (ImGui::IsWindowHovered() && Input::IsKeyDown(Key::LeftControl))
		{
			float scrollDelta = Input::GetScrollDelta();
			if (scrollDelta != 0.0f)
			{
				float mouseX = Input::GetMouseX() - cursorStart.x;
				float zoomCenter = offsetTime + (mouseX / timelineWidth) * (drawRange / zoomLevel);

				float newZoomLevel = std::clamp(zoomLevel * (1.0f + scrollDelta * ZoomSpeed), ZoomMin, ZoomMax);
				const float newScaledMaxTime = drawRange / newZoomLevel;
				offsetTime = std::clamp(zoomCenter - (zoomCenter - offsetTime) * (zoomLevel / newZoomLevel), 0.0f, std::max(0.0f, drawRange - newScaledMaxTime));
				zoomLevel = newZoomLevel;
			}
			if (ImGui::IsMouseDragging(0))
			{
				float deltaX = ImGui::GetMouseDragDelta().x;
				ImGui::ResetMouseDragDelta();

				float deltaTime = (deltaX / timelineWidth) * (drawRange / zoomLevel);
				offsetTime = std::max(0.0f, offsetTime - deltaTime);
			}
		}

		float scaledMaxTime = drawRange / zoomLevel;
		offsetTime = std::clamp(offsetTime, 0.0f, std::max(0.0f, drawRange - scaledMaxTime));
		float startTime = std::floor(offsetTime / MinorInterval) * MinorInterval;
		float endTime = std::ceil((offsetTime + scaledMaxTime) / MinorInterval) * MinorInterval;
		float interval = zoomLevel < zoomThreshold ? MinorInterval : MiniInterval;

		for (float time = startTime; time <= endTime; time += interval)
		{
			float x = cursorStart.x + ((time - offsetTime) / scaledMaxTime) * timelineWidth;

			if (x < cursorStart.x || x > cursorStart.x + timelineWidth)
				continue;

			if (zoomLevel < zoomThreshold)
			{
				if (static_cast<int>(std::round(time * 10.0f)) % static_cast<int>(std::round(MajorInterval * 10.0f)) == 0)
					drawMajorLine(drawList, x, cursorStart.y, timelineHeight, time);
				else
					drawMinorLine(drawList, x, cursorStart.y, timelineHeight);
			}
			else
			{
				if (static_cast<int>(std::round(time * 100.0f)) % static_cast<int>(std::round(MajorInterval * 100.0f)) == 0)
					drawMajorLine(drawList, x, cursorStart.y, timelineHeight, time);
				else if(static_cast<int>(std::round(time * 100.0f)) % static_cast<int>(std::round(MinorInterval * 100.0f)) == 0)
					drawMinorLine(drawList, x, cursorStart.y, timelineHeight);
				else
					drawMiniLine(drawList, x, cursorStart.y, timelineHeight);
			}
		}

		// node renders
		if (anim)
		{
			auto& frames = anim->GetFrames();

			for (size_t i = 0; i < frames.size(); ++i)
			{
				const float frameStart = anim->GetFrameStartTime(i);
				const float frameDuration = std::max(frames[i].m_Duration, 0.0f);
				const float frameEnd = frameStart + frameDuration;
				const float startScaled = (frameStart - offsetTime) / scaledMaxTime;
				const float endScaled = (frameEnd - offsetTime) / scaledMaxTime;
				const bool selected = selectedIndex && *selectedIndex == static_cast<int>(i);

				const float startX = cursorStart.x + std::clamp(startScaled, 0.0f, 1.0f) * timelineWidth;
				const float endX = cursorStart.x + std::clamp(endScaled, 0.0f, 1.0f) * timelineWidth;
				const float minVisibleX = cursorStart.x;
				const float maxVisibleX = cursorStart.x + timelineWidth;
				if (endX >= minVisibleX && startX <= maxVisibleX)
				{
					const ImVec2 segmentMin(startX, cursorStart.y + 32.0f);
					const ImVec2 segmentMax(std::max(startX + 3.0f, endX), cursorStart.y + timelineHeight - 24.0f);
					const ImU32 segmentColor = selected ? IM_COL32(178, 118, 70, 210) : (i % 2 == 0 ? IM_COL32(88, 72, 54, 190) : IM_COL32(74, 62, 48, 190));
					drawList->AddRectFilled(segmentMin, segmentMax, segmentColor, 4.0f);
					drawList->AddRect(segmentMin, segmentMax, selected ? IM_COL32(245, 216, 176, 230) : IM_COL32(118, 102, 78, 170), 4.0f);

					ImGui::SetCursorScreenPos(segmentMin);
					ImGui::InvisibleButton(("frame_segment" + std::to_string(i)).c_str(), ImVec2(std::max(8.0f, segmentMax.x - segmentMin.x), segmentMax.y - segmentMin.y));
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Frame %zu: %.3fs - %.3fs", i, frameStart, frameEnd);
						if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && selectedIndex)
							*selectedIndex = static_cast<int>(i);
					}
				}

				float scaledTime = (frameEnd - offsetTime) / scaledMaxTime;
				if (scaledTime < 0.0f || scaledTime > 1.0f)
					continue;

				float xPos = cursorStart.x + scaledTime * timelineWidth;
				ImVec2 nodePos = ImVec2(xPos, cursorStart.y + timelineHeight / 2);

				drawList->AddCircleFilled(nodePos, NodeRadius, selected ? SelectedNodeColor : NodeColor);
				drawList->AddCircle(nodePos, NodeRadius + 1.5f, selected ? IM_COL32(245, 216, 176, 210) : IM_COL32(38, 34, 28, 220), 16, 1.5f);
				ImGui::SetCursorScreenPos(ImVec2(nodePos.x - NodeRadius, nodePos.y - NodeRadius));
				ImGui::InvisibleButton(("node_drag" + std::to_string(i)).c_str(), NodeSize);

				if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
				{
					float deltaX = ImGui::GetMouseDragDelta().x;
					ImGui::ResetMouseDragDelta();

					float deltaTime = (deltaX / timelineWidth) * scaledMaxTime;

					frames[i].m_Duration = std::clamp(frames[i].m_Duration + deltaTime, 0.0f, maxValue);
					ImGui::SetTooltip("Frame %zu duration: %.3fs", i, frames[i].m_Duration);
				}
				else
				{
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Drag to resize frame %zu: %.3fs", i, frames[i].m_Duration);
						if (Input::IsMouseButtonDown(Mouse::Button0))
							if (selectedIndex)
								*selectedIndex = static_cast<int>(i);
					}
				}
			}
		}
		ImGui::EndChild();
	}
}

_WHIP_END
