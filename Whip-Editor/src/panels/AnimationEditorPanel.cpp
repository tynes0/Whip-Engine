#include "AnimationEditorPanel.h"
#include <Whip/Core/Application.h>
#include <Whip/Utils/FileExtensions.h>
#include <Whip/Utils/Utility.h>
#include <Whip/Project/Project.h>
#include <Whip/Asset/AssetManager.h>
#include <Whip/Asset/AnimationImporter.h>
#include <Whip/UI/UIHelpers.h>
#include <Whip/Animation/AnimationManager.h>
#include <Whip/Animation/AnimationController.h>
#include <Whip/Utils/PlatformUtils.h>
#include "../Helpers/IconManager.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <limits>

_WHIP_START

namespace
{
	constexpr int NoTransitionSource = -1;
	constexpr int EntryTransitionSource = -2;
	constexpr int AnyStateTransitionSource = -3;

	ImVec2 Add(const ImVec2& left, const ImVec2& right)
	{
		return { left.x + right.x, left.y + right.y };
	}

	ImVec2 Subtract(const ImVec2& left, const ImVec2& right)
	{
		return { left.x - right.x, left.y - right.y };
	}

	ImVec2 Scale(const ImVec2& value, float factor)
	{
		return { value.x * factor, value.y * factor };
	}

	float Dot(const ImVec2& left, const ImVec2& right)
	{
		return left.x * right.x + left.y * right.y;
	}

	float DistanceToSegment(const ImVec2& point, const ImVec2& start, const ImVec2& end)
	{
		const ImVec2 segment = Subtract(end, start);
		const float segmentLengthSq = Dot(segment, segment);
		if (segmentLengthSq <= 0.0001f)
		{
			const ImVec2 delta = Subtract(point, start);
			return std::sqrt(Dot(delta, delta));
		}

		const float t = std::clamp(Dot(Subtract(point, start), segment) / segmentLengthSq, 0.0f, 1.0f);
		const ImVec2 closest = Add(start, Scale(segment, t));
		const ImVec2 delta = Subtract(point, closest);
		return std::sqrt(Dot(delta, delta));
	}

}

AnimationEditorPanel::AnimationEditorPanel()
{
}

AnimationEditorPanel::~AnimationEditorPanel() {}

void AnimationEditorPanel::OnImGuiRender()
{
	if (!m_Open)
		return;
	bool open = m_Open;
	ImGui::Begin("Animation Editor", &open);
	if (open != m_Open)
		SetOpen(open);
	UpdatePreview();

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_ChildBg));

	ImGui::BeginChild("##AnimationEditorToolbar", ImVec2(0.0f, 58.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::AlignTextToFramePadding();
	DrawPlaybackControls(32.0f, 32.0f);
	ImGui::SameLine(0.0f, 16.0f);

	if (ImGui::Button("New", ImVec2(82.0f, 32.0f)))
	{
		m_CurrentAnimation.reset();
		Ref<Animation2D> newAnim = MakeRef<Animation2D>();
		newAnim->SetName("New Animation");
		m_CurrentAnimation = newAnim;
		m_SelectedFrameIndex = -1;
		StopPreview(false);
		std::string filepath = FileDialogs::SaveFile("Whip Animation (*.wanim)\0*.wanim\0", Project::GetActiveAssetDirectory().string().c_str());
		if (!filepath.empty())
		{
			std::filesystem::path animationPath(filepath);
			if (!FileExtensions::ExtensionEquals(animationPath, FileExtensions::Animation))
				animationPath.replace_extension(FileExtensions::Animation);
			m_CurrentAnimation->Serialize(animationPath);
			AssetMetadata metadata;
			metadata.m_Type = AssetType::Animation;
			metadata.m_Filepath = std::filesystem::relative(animationPath, Project::GetActiveAssetDirectory());
			Project::GetActive()->GetEditorAssetManager()->AddRegistry(m_CurrentAnimation->m_Handle, metadata);
			Project::GetActive()->GetEditorAssetManager()->SerializeAssetRegistry();
			if (m_RefreshAssetTreeCallback)
				m_RefreshAssetTreeCallback();
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Controller", ImVec2(96.0f, 32.0f)))
	{
		Ref<AnimationController> controller = MakeRef<AnimationController>();
		std::filesystem::path startDirectory = Project::GetActiveAssetDirectory() / "Animations";
		std::string filepath = FileDialogs::SaveFile("Whip Animation Controller (*.wac)\0*.wac\0", startDirectory.string().c_str());
		if (!filepath.empty())
		{
			std::filesystem::path controllerPath(filepath);
			if (!FileExtensions::ExtensionEquals(controllerPath, FileExtensions::AnimationController))
				controllerPath.replace_extension(FileExtensions::AnimationController);
			controller->Serialize(controllerPath);

			AssetMetadata metadata;
			metadata.m_Type = AssetType::AnimationController;
			metadata.m_Filepath = std::filesystem::relative(controllerPath, Project::GetActiveAssetDirectory());
			Project::GetActive()->GetEditorAssetManager()->AddRegistry(controller->m_Handle, metadata);
			Project::GetActive()->GetEditorAssetManager()->SerializeAssetRegistry();
			m_CurrentController = controller;
			m_SelectedControllerStateIndex = 0;
			m_SelectedControllerParameterIndex = -1;
			ClearSelectedControllerTransition();
			if (m_RefreshAssetTreeCallback)
				m_RefreshAssetTreeCallback();
		}
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!m_CurrentController);
	if (ImGui::Button("Save Ctrl", ImVec2(86.0f, 32.0f)))
		SaveCurrentController();
	ImGui::SameLine();
	if (ImGui::Button("Close Ctrl", ImVec2(90.0f, 32.0f)))
	{
		m_CurrentController = nullptr;
		m_SelectedControllerStateIndex = 0;
		m_SelectedControllerParameterIndex = -1;
		ClearSelectedControllerTransition();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!m_CurrentAnimation);
	if (ImGui::Button("Close", ImVec2(82.0f, 32.0f)))
	{
		m_CurrentAnimation = nullptr;
		m_SelectedFrameIndex = -1;
		StopPreview(false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Save", ImVec2(82.0f, 32.0f)))
	{
		const auto& metadata = AssetManager::GetAssetMetadata(m_CurrentAnimation->m_Handle);
		if (metadata)
			m_CurrentAnimation->Serialize(Project::GetActiveAssetDirectory() / metadata.m_Filepath);
	}

	ImGui::SameLine(0.0f, 14.0f);
	if (m_CurrentAnimation)
	{
		char buffer[256];
		memset(buffer, 0, sizeof(buffer));
		strncpy_s(buffer, sizeof(buffer), m_CurrentAnimation->GetName().c_str(), sizeof(buffer));
		ImGui::SetNextItemWidth(190.0f);
		if (ImGui::InputText("##AnimationName", buffer, sizeof(buffer)))
			m_CurrentAnimation->SetName(buffer);
		ImGui::SameLine();
	}
	ImGui::EndDisabled();

	ImGui::SetNextItemWidth(std::min(220.0f, ImGui::GetContentRegionAvail().x));
	if (ImGui::BeginCombo("##AnimationSelector", m_CurrentAnimation ? m_CurrentAnimation->GetName().data() : "Select Animation"))
	{
		const auto& reg = Project::GetActive()->GetEditorAssetManager()->GetAssetRegistry();
		reg.Foreach(AssetType::Animation, [&](const AssetRegistry::ValueType& value)
			{
				auto anim = AssetManager::GetAsset<Animation2D>(value.first);
				if (ImGui::Selectable(anim->GetName().c_str(), m_CurrentAnimation ? m_CurrentAnimation->m_Handle == value.first : false))
				{
					m_CurrentAnimation = anim;
					m_SelectedFrameIndex = -1;
					StopPreview(false);
				}
		});
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	DrawControllerSelector(std::min(220.0f, ImGui::GetContentRegionAvail().x));
	ImGui::EndChild();

	ImGui::Spacing();
	if (m_CurrentController)
	{
		DrawControllerEditor(std::max(260.0f, ImGui::GetContentRegionAvail().y));
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
		ImGui::End();
		return;
	}

	if (!m_CurrentAnimation)
	{
		ImGui::BeginChild("##AnimationEditorEmpty", ImVec2(0.0f, 0.0f), true);
		ImVec2 available = ImGui::GetContentRegionAvail();
		ImGui::SetCursorPos(ImVec2(16.0f, 16.0f));
		ImGui::TextDisabled("No animation selected");
		ImGui::SetCursorPos(ImVec2(16.0f, 48.0f));
		DrawAnimationDragDropArea(std::max(120.0f, available.x - 32.0f), std::max(80.0f, available.y - 64.0f));
		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
		ImGui::End();
		return;
	}

	ImGui::BeginChild("##AnimationEditorTimelineShell", ImVec2(0.0f, 168.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const auto& frames = m_CurrentAnimation->GetFrames();
	const float totalDuration = m_CurrentAnimation->GetDuration();
	const float effectiveFps = totalDuration > 0.0f ? static_cast<float>(frames.size()) / totalDuration : 0.0f;
	ImGui::TextDisabled("%zu frame(s)  |  %.3fs  |  %.1f FPS", frames.size(), totalDuration, effectiveFps);
	ImGui::SameLine();
	bool loop = m_CurrentAnimation->IsLooping();
	if (ImGui::Checkbox("Loop", &loop))
		m_CurrentAnimation->SetLoop(loop);
	DrawTimeline(ImGui::GetContentRegionAvail().x, 104.0f, 132.0f);
	ImGui::EndChild();

	ImGui::BeginChild("##AnimationEditorFrameTools", ImVec2(0.0f, 50.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	DrawFrameList(220.0f);
	ImGui::SameLine();
	DrawAddFrameButton(112.0f);
	ImGui::SameLine();
	DrawRemoveFrameButton(128.0f);
	ImGui::EndChild();

	const float lowerHeight = std::max(220.0f, ImGui::GetContentRegionAvail().y);
	ImGui::BeginChild("##AnimationEditorPreview", ImVec2(180.0f, lowerHeight), true);
	DrawPreviewPane(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("##AnimationEditorFrameInspector", ImVec2(0.0f, lowerHeight), true);
	DrawFrameEditor(ImGui::GetContentRegionAvail().x);
	ImGui::EndChild();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);
	ImGui::End();
}

void AnimationEditorPanel::SetOpen(bool open)
{
	if (m_Open == open)
		return;
	m_Open = open;
	m_OpenDirty = true;
}

bool AnimationEditorPanel::ConsumeOpenDirty()
{
	const bool dirty = m_OpenDirty;
	m_OpenDirty = false;
	return dirty;
}

void AnimationEditorPanel::DrawAnimationDragDropArea(float width, float height)
{
	const auto dragDropCallback = [this](AssetHandle handle)
		{
			m_CurrentAnimation = AssetManager::GetAsset<Animation2D>(handle);
			m_SelectedFrameIndex = -1;
			StopPreview(false);
		};

	UI::DragDropTarget(AssetType::Animation, dragDropCallback, "Select Animation", true, width, height, true);
}

void AnimationEditorPanel::DrawPlaybackControls(float width, float height)
{
	ImVec2 size(width, height);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));
	const bool hasFrames = m_CurrentAnimation && !m_CurrentAnimation->GetFrames().empty();

	auto drawButton = [&](Icon iconType, const char* tooltip) -> bool
		{
			const std::string buttonId = "##AnimationControl" + std::to_string(static_cast<int>(iconType));
			ImGui::InvisibleButton(buttonId.c_str(), size);
			bool clicked = ImGui::IsItemClicked();
			bool hovered = ImGui::IsItemHovered();
			bool active = ImGui::IsItemActive();
			ImVec2 min = ImGui::GetItemRectMin();
			ImVec2 max = ImGui::GetItemRectMax();
			ImU32 bg = active ? IM_COL32(94, 62, 34, 255) : hovered ? IM_COL32(48, 42, 34, 255) : IM_COL32(30, 28, 24, 255);
			drawList->AddRectFilled(min, max, bg, 5.0f);
			drawList->AddRect(min, max, hovered ? IM_COL32(118, 92, 58, 255) : IM_COL32(64, 56, 44, 255), 5.0f);

			if (frenum::contains(iconType))
			{
				Ref<Texture2D> texture = IconManager::Get().GetIcon(iconType);
				const float iconSize = 17.0f;
				ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
				drawList->AddImage(
					UI::ToImGuiTextureId(texture->GetRendererId()),
					ImVec2(center.x - iconSize * 0.5f, center.y - iconSize * 0.5f),
					ImVec2(center.x + iconSize * 0.5f, center.y + iconSize * 0.5f),
					ImVec2(0, 1),
					ImVec2(1, 0),
					IM_COL32(240, 232, 216, 255));
			}
			else
				drawList->AddText(ImVec2(min.x + 8.0f, min.y + 6.0f), IM_COL32(240, 232, 216, 255), frenum::to_string<Icon>(iconType).c_str());

			if (hovered)
				ImGui::SetTooltip("%s", tooltip);
			return clicked;
		};

	if (drawButton(Icon::StepBack, "Previous frame"))
	{
		StepPreview(-1);
	}
	ImGui::SameLine();
	if (drawButton(Icon::Play, m_PreviewPaused ? "Resume preview" : "Play preview"))
	{
		if (hasFrames)
		{
			if (m_SelectedFrameIndex < 0 || m_SelectedFrameIndex >= (int)m_CurrentAnimation->GetFrames().size())
				m_SelectedFrameIndex = 0;
			m_PreviewPlaying = true;
			m_PreviewPaused = false;
			m_PreviewElapsed = 0.0f;
		}
	}
	ImGui::SameLine();
	if (drawButton(Icon::Pause, "Pause preview"))
	{
		if (m_PreviewPlaying)
			m_PreviewPaused = true;
	}
	ImGui::SameLine();
	if (drawButton(Icon::Stop, "Stop preview"))
	{
		StopPreview(true);
	}
	ImGui::SameLine();
	if (drawButton(Icon::StepForward, "Next frame"))
	{
		StepPreview(1);
	}

	ImGui::PopStyleVar();
}

void AnimationEditorPanel::DrawNewButton(float width, float leftPadding)
{
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (leftPadding + width));
	if (ImGui::Button("New", ImVec2(width, 0.0f)))
	{
		m_CurrentAnimation.reset();
		Ref<Animation2D> newAnim = MakeRef<Animation2D>();
		newAnim->SetName("New Animation");
		m_CurrentAnimation = newAnim;
		m_SelectedFrameIndex = -1;
		std::string filepath = FileDialogs::SaveFile("Whip Animation (*.wanim)\0*.wanim\0", Project::GetActiveAssetDirectory().string().c_str());
		if (!filepath.empty())
		{
			std::filesystem::path animationPath(filepath);
			if (!FileExtensions::ExtensionEquals(animationPath, FileExtensions::Animation))
				animationPath.replace_extension(FileExtensions::Animation);
			m_CurrentAnimation->Serialize(animationPath);
			AssetMetadata metadata;
			metadata.m_Type = AssetType::Animation;
			metadata.m_Filepath = std::filesystem::relative(animationPath, Project::GetActiveAssetDirectory());
			Project::GetActive()->GetEditorAssetManager()->AddRegistry(m_CurrentAnimation->m_Handle, metadata);
			Project::GetActive()->GetEditorAssetManager()->SerializeAssetRegistry();
			if (m_RefreshAssetTreeCallback)
				m_RefreshAssetTreeCallback();
		}
	}
}

void AnimationEditorPanel::DrawCloseButton(float width, float leftPadding)
{
	if (!m_CurrentAnimation)
		return;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (leftPadding + width));
	if (ImGui::Button("Close", ImVec2(width, 0.0f)))
	{
		m_CurrentAnimation = nullptr;
		m_SelectedFrameIndex = -1;
		StopPreview(false);
	}
}

void AnimationEditorPanel::DrawSaveButton(float width, float leftPadding)
{
	if (!m_CurrentAnimation)
		return;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (leftPadding + width));
	if (ImGui::Button("Save", ImVec2(width, 0.0f)))
	{
		const auto& metadata = AssetManager::GetAssetMetadata(m_CurrentAnimation->m_Handle);
		if (metadata)
			m_CurrentAnimation->Serialize(Project::GetActiveAssetDirectory() / metadata.m_Filepath);
		else
		{
		}
	}
}

void AnimationEditorPanel::DrawNameInput(float width, float leftPadding)
{
	if (!m_CurrentAnimation)
		return;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (leftPadding + width));
	char buffer[256];
	memset(buffer, 0, sizeof(buffer));
	strncpy_s(buffer, sizeof(buffer), m_CurrentAnimation->GetName().c_str(), sizeof(buffer));
	ImGui::SetNextItemWidth(width);
	if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
		m_CurrentAnimation->SetName(buffer);
}

void AnimationEditorPanel::DrawAnimationSelector(float width, float leftPadding)
{
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (width + leftPadding));
	ImGui::SetNextItemWidth(width);
	if (ImGui::BeginCombo("##AnimationSelector", m_CurrentAnimation ? m_CurrentAnimation->GetName().data() : "Select Animation"))
	{
		const auto& reg = Project::GetActive()->GetEditorAssetManager()->GetAssetRegistry();

		reg.Foreach(AssetType::Animation, [&](const AssetRegistry::ValueType& value)
			{
				auto anim = AssetManager::GetAsset<Animation2D>(value.first);
				if (ImGui::Selectable(anim->GetName().c_str(), m_CurrentAnimation ? m_CurrentAnimation->m_Handle == value.first : false, 0, ImVec2(width - ImGui::GetStyle().WindowPadding.x, 0.0f)))
				{
					m_CurrentAnimation = AssetManager::GetAsset<Animation2D>(value.first);
					m_SelectedFrameIndex = -1;
					StopPreview(false);
				}
			});
		ImGui::EndCombo();
	}
}

void AnimationEditorPanel::DrawControllerSelector(float width)
{
	if (width <= 0.0f)
		return;

	std::string preview = "Select Controller";
	if (m_CurrentController)
	{
		const AssetMetadata& metadata = AssetManager::GetAssetMetadata(m_CurrentController->m_Handle);
		preview = metadata ? metadata.m_Filepath.filename().string() : "Controller";
	}

	ImGui::SetNextItemWidth(width);
	if (ImGui::BeginCombo("##ControllerSelector", preview.c_str()))
	{
		const auto& reg = Project::GetActive()->GetEditorAssetManager()->GetAssetRegistry();
		reg.Foreach(AssetType::AnimationController, [&](const AssetRegistry::ValueType& value)
			{
				const std::string label = value.second.m_Filepath.filename().string();
				const bool selected = m_CurrentController && m_CurrentController->m_Handle == value.first;
				if (ImGui::Selectable(label.c_str(), selected))
				{
					m_CurrentController = AssetManager::GetAsset<AnimationController>(value.first);
					m_SelectedControllerStateIndex = 0;
					m_SelectedControllerParameterIndex = -1;
					ClearSelectedControllerTransition();
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			});
		ImGui::EndCombo();
	}
}

void AnimationEditorPanel::DrawControllerEditor(float height)
{
	if (!m_CurrentController)
		return;

	if (m_CurrentController->GetStates().empty())
	{
		m_CurrentController->AddState("Entry");
		m_CurrentController->SetDefaultState("Entry");
	}

	m_SelectedControllerStateIndex = std::clamp(m_SelectedControllerStateIndex, 0, (int)m_CurrentController->GetStates().size() - 1);

	const float availableWidth = ImGui::GetContentRegionAvail().x;
	const float parameterWidth = std::min(270.0f, availableWidth * 0.28f);
	const float inspectorWidth = std::min(360.0f, availableWidth * 0.34f);
	const float graphWidth = std::max(260.0f, availableWidth - parameterWidth - inspectorWidth - ImGui::GetStyle().ItemSpacing.x * 2.0f);

	DrawControllerParameters(parameterWidth, height);
	ImGui::SameLine();
	DrawControllerGraph(graphWidth, height);
	ImGui::SameLine();
	DrawControllerStateInspector(inspectorWidth, height);
}

void AnimationEditorPanel::DrawControllerParameters(float width, float height)
{
	ImGui::BeginChild("##ControllerParameters", ImVec2(width, height), true);
	ImGui::TextDisabled("Parameters");
	ImGui::Separator();

	auto addParameter = [this](AnimationParameterType type)
		{
			AnimationControllerParameter& parameter = m_CurrentController->AddParameter(frenum::to_string(type).data(), type);
			m_SelectedControllerParameterIndex = (int)m_CurrentController->GetParameters().size() - 1;
			if (type == AnimationParameterType::Bool || type == AnimationParameterType::Trigger)
				parameter.m_DefaultBool = false;
		};

	if (ImGui::Button("+ Bool"))
		addParameter(AnimationParameterType::Bool);
	ImGui::SameLine();
	if (ImGui::Button("+ Int"))
		addParameter(AnimationParameterType::Int);
	if (ImGui::Button("+ Float"))
		addParameter(AnimationParameterType::Float);
	ImGui::SameLine();
	if (ImGui::Button("+ Trigger"))
		addParameter(AnimationParameterType::Trigger);

	ImGui::Spacing();
	auto& parameters = m_CurrentController->GetParameters();
	for (size_t i = 0; i < parameters.size(); ++i)
	{
		ImGui::PushID((int)i);
		const bool selected = m_SelectedControllerParameterIndex == (int)i;
		if (ImGui::Selectable(parameters[i].m_Name.c_str(), selected))
			m_SelectedControllerParameterIndex = (int)i;
		ImGui::PopID();
	}

	if (m_SelectedControllerParameterIndex >= 0 && m_SelectedControllerParameterIndex < (int)parameters.size())
	{
		ImGui::Separator();
		AnimationControllerParameter& parameter = parameters[m_SelectedControllerParameterIndex];
		std::string oldName = parameter.m_Name;
		if (ImGui::InputText("Name", &parameter.m_Name))
		{
			if (parameter.m_Name.empty())
				parameter.m_Name = oldName;
			else if (parameter.m_Name != oldName)
			{
				for (AnimationControllerState& state : m_CurrentController->GetStates())
				{
					for (AnimationControllerTransition& transition : state.m_Transitions)
					{
						for (AnimationControllerCondition& condition : transition.m_Conditions)
						{
							if (condition.m_Parameter == oldName)
								condition.m_Parameter = parameter.m_Name;
						}
					}
				}
				for (AnimationControllerTransition& transition : m_CurrentController->GetAnyStateTransitions())
				{
					for (AnimationControllerCondition& condition : transition.m_Conditions)
					{
						if (condition.m_Parameter == oldName)
							condition.m_Parameter = parameter.m_Name;
					}
				}
			}
		}

		const std::array<AnimationParameterType, 4> parameterTypes =
		{
			AnimationParameterType::Bool,
			AnimationParameterType::Int,
			AnimationParameterType::Float,
			AnimationParameterType::Trigger
		};
		std::string typePreview = frenum::to_string(parameter.m_Type).data();
		if (ImGui::BeginCombo("Type", typePreview.c_str()))
		{
			for (AnimationParameterType type : parameterTypes)
			{
				std::string typeLabel = frenum::to_string(type).data();
				const bool selected = parameter.m_Type == type;
				if (ImGui::Selectable(typeLabel.c_str(), selected))
					parameter.m_Type = type;
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (parameter.m_Type == AnimationParameterType::Bool)
			ImGui::Checkbox("Default", &parameter.m_DefaultBool);
		else if (parameter.m_Type == AnimationParameterType::Int)
			ImGui::DragInt("Default", &parameter.m_DefaultInt, 1.0f);
		else if (parameter.m_Type == AnimationParameterType::Float)
			ImGui::DragFloat("Default", &parameter.m_DefaultFloat, 0.01f);

		if (ImGui::Button("Remove Parameter", ImVec2(-1.0f, 0.0f)))
		{
			const std::string removedName = parameter.m_Name;
			for (AnimationControllerState& state : m_CurrentController->GetStates())
			{
				for (AnimationControllerTransition& transition : state.m_Transitions)
				{
					std::erase_if(transition.m_Conditions, [&removedName](const AnimationControllerCondition& condition)
						{
							return condition.m_Parameter == removedName;
						});
				}
			}
			for (AnimationControllerTransition& transition : m_CurrentController->GetAnyStateTransitions())
			{
				std::erase_if(transition.m_Conditions, [&removedName](const AnimationControllerCondition& condition)
					{
						return condition.m_Parameter == removedName;
					});
			}
			parameters.erase(parameters.begin() + m_SelectedControllerParameterIndex);
			m_SelectedControllerParameterIndex = -1;
		}
	}

	ImGui::EndChild();
}

void AnimationEditorPanel::DrawControllerGraph(float width, float height)
{
	ImGui::BeginChild("##ControllerGraph", ImVec2(width, height), true);
	ImGui::TextDisabled("Controller Graph");
	ImGui::SameLine();
	if (ImGui::Button("+ State"))
	{
		AnimationControllerState& state = m_CurrentController->AddState("State");
		m_SelectedControllerStateIndex = (int)m_CurrentController->GetStates().size() - 1;
		if (m_CurrentController->GetDefaultState().empty())
			m_CurrentController->SetDefaultState(state.m_Name);
		ClearSelectedControllerTransition();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(m_CurrentController->GetStates().size() <= 1);
	if (ImGui::Button("Remove"))
	{
		const std::string removedName = m_CurrentController->GetStates()[m_SelectedControllerStateIndex].m_Name;
		m_CurrentController->RemoveState(removedName);
		m_SelectedControllerStateIndex = std::clamp(m_SelectedControllerStateIndex, 0, (int)m_CurrentController->GetStates().size() - 1);
		ClearSelectedControllerTransition();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Set Default"))
	{
		m_CurrentController->SetDefaultState(m_CurrentController->GetStates()[m_SelectedControllerStateIndex].m_Name);
		ClearSelectedControllerTransition();
	}
	ImGui::SameLine();
	if (ImGui::Button("Zoom -"))
		m_ControllerGraphZoom = std::max(0.45f, m_ControllerGraphZoom - 0.1f);
	ImGui::SameLine();
	if (ImGui::Button("Zoom +"))
		m_ControllerGraphZoom = std::min(2.0f, m_ControllerGraphZoom + 0.1f);
	ImGui::SameLine();
	if (ImGui::Button("Reset View"))
	{
		m_ControllerGraphZoom = 1.0f;
		m_ControllerGraphPan = { 28.0f, 28.0f };
	}
	ImGui::Separator();

	auto& states = m_CurrentController->GetStates();
	const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 17, 15, 255), 4.0f);
	drawList->AddRect(canvasMin, canvasMax, IM_COL32(76, 68, 54, 170), 4.0f);

	ImGui::InvisibleButton("##ControllerGraphCanvas", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	const bool canvasHovered = ImGui::IsItemHovered();
	const ImVec2 mousePos = ImGui::GetIO().MousePos;
	if (canvasHovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
	{
		const ImVec2 delta = ImGui::GetIO().MouseDelta;
		m_ControllerGraphPan.x += delta.x;
		m_ControllerGraphPan.y += delta.y;
	}
	if (canvasHovered && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f)
		m_ControllerGraphZoom = std::clamp(m_ControllerGraphZoom + ImGui::GetIO().MouseWheel * 0.08f, 0.45f, 2.0f);

	const float zoom = m_ControllerGraphZoom;
	const float nodeWidth = 174.0f;
	const float nodeHeight = 82.0f;
	const float specialWidth = 136.0f;
	const float specialHeight = 58.0f;
	const float pinRadius = std::max(3.5f, 5.5f * zoom);
	const ImVec2 nodeSize(nodeWidth * zoom, nodeHeight * zoom);
	const ImVec2 specialSize(specialWidth * zoom, specialHeight * zoom);

	auto worldToScreen = [&](const glm::vec2& world) -> ImVec2
		{
			return { canvasMin.x + m_ControllerGraphPan.x + world.x * zoom, canvasMin.y + m_ControllerGraphPan.y + world.y * zoom };
		};

	auto stateInputPin = [&](size_t index) -> ImVec2
		{
			const glm::vec2& position = states[index].m_GraphPosition;
			return worldToScreen({ position.x, position.y + nodeHeight * 0.5f });
		};

	auto stateOutputPin = [&](size_t index) -> ImVec2
		{
			const glm::vec2& position = states[index].m_GraphPosition;
			return worldToScreen({ position.x + nodeWidth, position.y + nodeHeight * 0.5f });
		};

	const int columns = std::max(1, (int)((canvasSize.x / std::max(zoom, 0.1f) - 280.0f) / 230.0f));
	for (size_t i = 0; i < states.size(); ++i)
	{
		const int row = (int)i / columns;
		const int column = (int)i % columns;
		if (states[i].m_GraphPosition.x == 0.0f && states[i].m_GraphPosition.y == 0.0f)
			states[i].m_GraphPosition = { 240.0f + column * 230.0f, 42.0f + row * 126.0f };
	}

	float exitColumn = 540.0f;
	for (const AnimationControllerState& state : states)
		exitColumn = std::max(exitColumn, state.m_GraphPosition.x + nodeWidth + 190.0f);

	const glm::vec2 entryPosition{ 22.0f, 58.0f };
	const glm::vec2 anyStatePosition{ 22.0f, 160.0f };
	const glm::vec2 exitPosition{ exitColumn, 108.0f };
	const ImVec2 entryOutput = worldToScreen({ entryPosition.x + specialWidth, entryPosition.y + specialHeight * 0.5f });
	const ImVec2 anyStateOutput = worldToScreen({ anyStatePosition.x + specialWidth, anyStatePosition.y + specialHeight * 0.5f });
	const ImVec2 exitInput = worldToScreen({ exitPosition.x, exitPosition.y + specialHeight * 0.5f });

	auto findStateIndex = [&](std::string_view stateName) -> int
		{
			for (size_t i = 0; i < states.size(); ++i)
			{
				if (states[i].m_Name == stateName)
					return (int)i;
			}
			return -1;
		};

	auto selectTransition = [&](int sourceStateIndex, int transitionIndex)
		{
			m_SelectedTransitionSourceStateIndex = sourceStateIndex;
			m_SelectedTransitionIndex = transitionIndex;
			if (sourceStateIndex >= 0)
				m_SelectedControllerStateIndex = sourceStateIndex;
		};

	auto clearPendingConnection = [&]()
		{
			m_PendingTransitionSourceStateIndex = NoTransitionSource;
		};

	auto createConnection = [&](int targetStateIndex, bool targetExit)
		{
			if (m_PendingTransitionSourceStateIndex == NoTransitionSource)
				return;

			const std::string target = targetExit ? std::string(AnimationController::ExitStateName) : states[targetStateIndex].m_Name;
			if (m_PendingTransitionSourceStateIndex == EntryTransitionSource)
			{
				if (!targetExit)
				{
					m_CurrentController->SetDefaultState(target);
					m_SelectedControllerStateIndex = targetStateIndex;
					ClearSelectedControllerTransition();
				}
				clearPendingConnection();
				return;
			}

			AnimationControllerTransition transition;
			transition.m_TargetState = target;
			if (m_PendingTransitionSourceStateIndex == AnyStateTransitionSource)
			{
				transition.m_HasExitTime = false;
				auto& transitions = m_CurrentController->GetAnyStateTransitions();
				transitions.push_back(transition);
				selectTransition(AnyStateTransitionSource, (int)transitions.size() - 1);
				clearPendingConnection();
				return;
			}

			if (m_PendingTransitionSourceStateIndex >= 0 && m_PendingTransitionSourceStateIndex < (int)states.size())
			{
				auto& transitions = states[m_PendingTransitionSourceStateIndex].m_Transitions;
				transitions.push_back(transition);
				selectTransition(m_PendingTransitionSourceStateIndex, (int)transitions.size() - 1);
			}
			clearPendingConnection();
		};

	auto transitionTargetPin = [&](const AnimationControllerTransition& transition, ImVec2& targetPin) -> bool
		{
			if (transition.m_TargetState == AnimationController::ExitStateName)
			{
				targetPin = exitInput;
				return true;
			}

			const int targetIndex = findStateIndex(transition.m_TargetState);
			if (targetIndex < 0)
				return false;
			targetPin = stateInputPin((size_t)targetIndex);
			return true;
		};

	auto drawTransition = [&](int sourceStateIndex, int transitionIndex, const ImVec2& sourcePin, const AnimationControllerTransition& transition)
		{
			ImVec2 targetPin;
			if (!transitionTargetPin(transition, targetPin))
				return;

			const bool selected = m_SelectedTransitionSourceStateIndex == sourceStateIndex && m_SelectedTransitionIndex == transitionIndex;
			const bool hovered = canvasHovered && DistanceToSegment(mousePos, sourcePin, targetPin) <= 9.0f;
			const ImU32 color = selected ? IM_COL32(122, 196, 255, 255) : hovered ? IM_COL32(235, 196, 118, 255) : IM_COL32(184, 132, 72, 220);
			const float tangent = std::max(58.0f * zoom, std::abs(targetPin.x - sourcePin.x) * 0.42f);
			drawList->AddBezierCubic(sourcePin, ImVec2(sourcePin.x + tangent, sourcePin.y), ImVec2(targetPin.x - tangent, targetPin.y), targetPin, color, selected ? 3.2f : 2.0f);
			drawList->AddCircleFilled(targetPin, selected ? pinRadius + 1.0f : pinRadius, color);

			if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				selectTransition(sourceStateIndex, transitionIndex);
		};

	drawList->PushClipRect(canvasMin, canvasMax, true);
	const int defaultStateIndex = findStateIndex(m_CurrentController->GetDefaultState());
	if (defaultStateIndex >= 0)
	{
		const ImVec2 targetPin = stateInputPin((size_t)defaultStateIndex);
		const float tangent = std::max(54.0f * zoom, std::abs(targetPin.x - entryOutput.x) * 0.36f);
		drawList->AddBezierCubic(entryOutput, ImVec2(entryOutput.x + tangent, entryOutput.y), ImVec2(targetPin.x - tangent, targetPin.y), targetPin, IM_COL32(92, 168, 236, 235), 2.4f);
		drawList->AddCircleFilled(targetPin, pinRadius, IM_COL32(92, 168, 236, 235));
	}

	const auto& anyStateTransitions = m_CurrentController->GetAnyStateTransitions();
	for (size_t transitionIndex = 0; transitionIndex < anyStateTransitions.size(); ++transitionIndex)
		drawTransition(AnyStateTransitionSource, (int)transitionIndex, anyStateOutput, anyStateTransitions[transitionIndex]);

	for (size_t stateIndex = 0; stateIndex < states.size(); ++stateIndex)
	{
		const ImVec2 sourcePin = stateOutputPin(stateIndex);
		for (size_t transitionIndex = 0; transitionIndex < states[stateIndex].m_Transitions.size(); ++transitionIndex)
			drawTransition((int)stateIndex, (int)transitionIndex, sourcePin, states[stateIndex].m_Transitions[transitionIndex]);
	}

	auto drawSpecialNode = [&](const char* label, const glm::vec2& worldPosition, ImU32 fill, ImU32 border, int sourceIndex, bool hasInput)
		{
			const ImVec2 nodeMin = worldToScreen(worldPosition);
			const ImVec2 nodeMax(nodeMin.x + specialSize.x, nodeMin.y + specialSize.y);
			drawList->AddRectFilled(nodeMin, nodeMax, fill, 6.0f);
			drawList->AddRect(nodeMin, nodeMax, border, 6.0f, 0, 1.6f);
			drawList->AddText(ImVec2(nodeMin.x + 12.0f * zoom, nodeMin.y + 18.0f * zoom), IM_COL32(238, 230, 214, 255), label);

			if (sourceIndex != NoTransitionSource)
			{
				const ImVec2 outPin(nodeMax.x, nodeMin.y + specialSize.y * 0.5f);
				drawList->AddCircleFilled(outPin, pinRadius, IM_COL32(228, 184, 104, 255));
				ImGui::SetCursorScreenPos(ImVec2(outPin.x - 9.0f, outPin.y - 9.0f));
				ImGui::PushID(sourceIndex);
				ImGui::InvisibleButton("##SpecialOutPin", ImVec2(18.0f, 18.0f));
				if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
					m_PendingTransitionSourceStateIndex = sourceIndex;
				ImGui::PopID();
			}

			if (hasInput)
			{
				const ImVec2 inPin(nodeMin.x, nodeMin.y + specialSize.y * 0.5f);
				drawList->AddCircleFilled(inPin, pinRadius, IM_COL32(116, 190, 138, 255));
				ImGui::SetCursorScreenPos(ImVec2(inPin.x - 9.0f, inPin.y - 9.0f));
				ImGui::PushID("ExitInput");
				ImGui::InvisibleButton("##SpecialInPin", ImVec2(18.0f, 18.0f));
				if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
					createConnection(-1, true);
				ImGui::PopID();
			}
		};

	drawSpecialNode("Entry", entryPosition, IM_COL32(36, 54, 66, 255), IM_COL32(92, 168, 236, 235), EntryTransitionSource, false);
	drawSpecialNode("Any State", anyStatePosition, IM_COL32(58, 42, 31, 255), IM_COL32(232, 166, 85, 235), AnyStateTransitionSource, false);
	drawSpecialNode("Exit", exitPosition, IM_COL32(47, 38, 42, 255), IM_COL32(204, 116, 128, 235), NoTransitionSource, true);

	for (size_t i = 0; i < states.size(); ++i)
	{
		AnimationControllerState& state = states[i];
		const ImVec2 nodeMin = worldToScreen(state.m_GraphPosition);
		const ImVec2 nodeMax(nodeMin.x + nodeSize.x, nodeMin.y + nodeSize.y);
		const bool selected = m_SelectedControllerStateIndex == (int)i && m_SelectedTransitionIndex < 0;
		const bool isDefault = state.m_Name == m_CurrentController->GetDefaultState();
		const ImU32 fill = selected ? IM_COL32(86, 66, 48, 255) : IM_COL32(44, 39, 34, 255);
		const ImU32 border = isDefault ? IM_COL32(92, 168, 236, 245) : selected ? IM_COL32(230, 206, 168, 230) : IM_COL32(104, 88, 66, 190);
		drawList->AddRectFilled(nodeMin, nodeMax, fill, 6.0f);
		drawList->AddRect(nodeMin, nodeMax, border, 6.0f, 0, isDefault ? 2.2f : 1.2f);
		drawList->AddText(ImVec2(nodeMin.x + 12.0f * zoom, nodeMin.y + 10.0f * zoom), IM_COL32(238, 230, 214, 255), state.m_Name.c_str());

		std::string clipLabel = state.m_MotionType == AnimationMotionType::BlendTree1D ? "Blend Tree 1D" : "No Clip";
		if (state.m_MotionType == AnimationMotionType::Clip && state.m_Clip != 0 && AssetManager::IsAssetHandleValid(state.m_Clip) && AssetManager::GetAssetType(state.m_Clip) == AssetType::Animation)
			clipLabel = AssetManager::GetAssetMetadata(state.m_Clip).m_Filepath.filename().string();
		drawList->AddText(ImVec2(nodeMin.x + 12.0f * zoom, nodeMin.y + 36.0f * zoom), IM_COL32(178, 168, 148, 255), clipLabel.c_str());

		if (isDefault)
			drawList->AddText(ImVec2(nodeMin.x + 12.0f * zoom, nodeMin.y + 58.0f * zoom), IM_COL32(106, 182, 248, 255), "Default");

		ImGui::SetCursorScreenPos(nodeMin);
		ImGui::PushID((int)i);
		ImGui::InvisibleButton("##ControllerStateNode", nodeSize);
		if (ImGui::IsItemClicked())
		{
			m_SelectedControllerStateIndex = (int)i;
			ClearSelectedControllerTransition();
		}
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			const ImVec2 delta = ImGui::GetIO().MouseDelta;
			state.m_GraphPosition.x += delta.x / zoom;
			state.m_GraphPosition.y += delta.y / zoom;
		}

		const ImVec2 inputPin = stateInputPin(i);
		drawList->AddCircleFilled(inputPin, pinRadius, IM_COL32(116, 190, 138, 255));
		ImGui::SetCursorScreenPos(ImVec2(inputPin.x - 9.0f, inputPin.y - 9.0f));
		ImGui::InvisibleButton("##StateInPin", ImVec2(18.0f, 18.0f));
		if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			createConnection((int)i, false);

		const ImVec2 outputPin = stateOutputPin(i);
		drawList->AddCircleFilled(outputPin, pinRadius, IM_COL32(228, 184, 104, 255));
		ImGui::SetCursorScreenPos(ImVec2(outputPin.x - 9.0f, outputPin.y - 9.0f));
		ImGui::InvisibleButton("##StateOutPin", ImVec2(18.0f, 18.0f));
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			m_PendingTransitionSourceStateIndex = (int)i;
		ImGui::PopID();
	}

	if (m_PendingTransitionSourceStateIndex != NoTransitionSource)
	{
		ImVec2 sourcePin = entryOutput;
		if (m_PendingTransitionSourceStateIndex == AnyStateTransitionSource)
			sourcePin = anyStateOutput;
		else if (m_PendingTransitionSourceStateIndex >= 0 && m_PendingTransitionSourceStateIndex < (int)states.size())
			sourcePin = stateOutputPin((size_t)m_PendingTransitionSourceStateIndex);

		const float tangent = std::max(48.0f * zoom, std::abs(mousePos.x - sourcePin.x) * 0.35f);
		drawList->AddBezierCubic(sourcePin, ImVec2(sourcePin.x + tangent, sourcePin.y), ImVec2(mousePos.x - tangent, mousePos.y), mousePos, IM_COL32(122, 196, 255, 230), 2.4f);
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
			clearPendingConnection();
	}

	drawList->PopClipRect();
	ImGui::SetCursorScreenPos(canvasMax);
	ImGui::EndChild();
}

AnimationControllerTransition* AnimationEditorPanel::GetSelectedControllerTransition()
{
	if (!m_CurrentController || m_SelectedTransitionIndex < 0)
		return nullptr;

	if (m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource)
	{
		auto& transitions = m_CurrentController->GetAnyStateTransitions();
		if (m_SelectedTransitionIndex >= (int)transitions.size())
			return nullptr;
		return &transitions[m_SelectedTransitionIndex];
	}

	auto& states = m_CurrentController->GetStates();
	if (m_SelectedTransitionSourceStateIndex < 0 || m_SelectedTransitionSourceStateIndex >= (int)states.size())
		return nullptr;

	auto& transitions = states[m_SelectedTransitionSourceStateIndex].m_Transitions;
	if (m_SelectedTransitionIndex >= (int)transitions.size())
		return nullptr;
	return &transitions[m_SelectedTransitionIndex];
}

void AnimationEditorPanel::ClearSelectedControllerTransition()
{
	m_SelectedTransitionSourceStateIndex = NoTransitionSource;
	m_SelectedTransitionIndex = -1;
}

void AnimationEditorPanel::DrawControllerTransitionInspector(AnimationControllerTransition& transition, bool allowExitTarget)
{
	auto& states = m_CurrentController->GetStates();

	if (ImGui::BeginCombo("Target", transition.m_TargetState.empty() ? "None" : transition.m_TargetState.c_str()))
	{
		for (const AnimationControllerState& targetState : states)
		{
			const bool selected = transition.m_TargetState == targetState.m_Name;
			if (ImGui::Selectable(targetState.m_Name.c_str(), selected))
				transition.m_TargetState = targetState.m_Name;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		if (allowExitTarget)
		{
			const bool selected = transition.m_TargetState == AnimationController::ExitStateName;
			if (ImGui::Selectable(AnimationController::ExitStateName.data(), selected))
				transition.m_TargetState = std::string(AnimationController::ExitStateName);
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Checkbox("Has Exit Time", &transition.m_HasExitTime);
	ImGui::BeginDisabled(!transition.m_HasExitTime);
	ImGui::DragFloat("Exit Time", &transition.m_ExitTime, 0.01f, 0.0f, 10.0f, "%.2f");
	ImGui::EndDisabled();
	ImGui::DragFloat("Duration", &transition.m_Duration, 0.01f, 0.0f, 10.0f, "%.2f");

	ImGui::Separator();
	ImGui::TextDisabled("Conditions");
	if (ImGui::Button("+ Condition", ImVec2(-1.0f, 0.0f)) && !m_CurrentController->GetParameters().empty())
	{
		AnimationControllerCondition condition;
		condition.m_Parameter = m_CurrentController->GetParameters().front().m_Name;
		transition.m_Conditions.push_back(condition);
	}

	for (size_t conditionIndex = 0; conditionIndex < transition.m_Conditions.size(); ++conditionIndex)
	{
		ImGui::PushID((int)conditionIndex);
		AnimationControllerCondition& condition = transition.m_Conditions[conditionIndex];
		if (ImGui::BeginCombo("Parameter", condition.m_Parameter.empty() ? "None" : condition.m_Parameter.c_str()))
		{
			for (const AnimationControllerParameter& parameter : m_CurrentController->GetParameters())
			{
				const bool selected = condition.m_Parameter == parameter.m_Name;
				if (ImGui::Selectable(parameter.m_Name.c_str(), selected))
					condition.m_Parameter = parameter.m_Name;
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		const std::array<AnimationConditionMode, 6> conditionModes =
		{
			AnimationConditionMode::If,
			AnimationConditionMode::IfNot,
			AnimationConditionMode::Greater,
			AnimationConditionMode::Less,
			AnimationConditionMode::Equals,
			AnimationConditionMode::NotEquals
		};
		std::string modePreview = frenum::to_string(condition.m_Mode).data();
		if (ImGui::BeginCombo("Mode", modePreview.c_str()))
		{
			for (AnimationConditionMode mode : conditionModes)
			{
				std::string modeLabel = frenum::to_string(mode).data();
				const bool selected = condition.m_Mode == mode;
				if (ImGui::Selectable(modeLabel.c_str(), selected))
					condition.m_Mode = mode;
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		const auto parameterIt = std::find_if(m_CurrentController->GetParameters().begin(), m_CurrentController->GetParameters().end(), [&condition](const AnimationControllerParameter& parameter)
			{
				return parameter.m_Name == condition.m_Parameter;
			});
		if (parameterIt != m_CurrentController->GetParameters().end())
		{
			if (parameterIt->m_Type == AnimationParameterType::Float)
				ImGui::DragFloat("Threshold", &condition.m_Threshold, 0.01f);
			else if (parameterIt->m_Type == AnimationParameterType::Int)
				ImGui::DragInt("Value", &condition.m_IntValue, 1.0f);
			else
				ImGui::Checkbox("Value", &condition.m_BoolValue);
		}

		if (ImGui::Button("Remove Condition", ImVec2(-1.0f, 0.0f)))
		{
			transition.m_Conditions.erase(transition.m_Conditions.begin() + conditionIndex);
			ImGui::PopID();
			break;
		}
		ImGui::Separator();
		ImGui::PopID();
	}
}

void AnimationEditorPanel::DrawControllerStateInspector(float width, float height)
{
	ImGui::BeginChild("##ControllerStateInspector", ImVec2(width, height), true);

	auto& states = m_CurrentController->GetStates();
	if (states.empty())
	{
		ImGui::EndChild();
		return;
	}

	m_SelectedControllerStateIndex = std::clamp(m_SelectedControllerStateIndex, 0, (int)states.size() - 1);

	if (AnimationControllerTransition* selectedTransition = GetSelectedControllerTransition())
	{
		ImGui::TextDisabled("Transition Inspector");
		ImGui::Separator();
		if (m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource)
			ImGui::TextDisabled("Source: Any State");
		else if (m_SelectedTransitionSourceStateIndex >= 0 && m_SelectedTransitionSourceStateIndex < (int)states.size())
			ImGui::TextDisabled("Source: %s", states[m_SelectedTransitionSourceStateIndex].m_Name.c_str());

		DrawControllerTransitionInspector(*selectedTransition, true);
		if (ImGui::Button("Remove Transition", ImVec2(-1.0f, 0.0f)))
		{
			if (m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource)
			{
				auto& transitions = m_CurrentController->GetAnyStateTransitions();
				if (m_SelectedTransitionIndex >= 0 && m_SelectedTransitionIndex < (int)transitions.size())
					transitions.erase(transitions.begin() + m_SelectedTransitionIndex);
			}
			else if (m_SelectedTransitionSourceStateIndex >= 0 && m_SelectedTransitionSourceStateIndex < (int)states.size())
			{
				auto& transitions = states[m_SelectedTransitionSourceStateIndex].m_Transitions;
				if (m_SelectedTransitionIndex >= 0 && m_SelectedTransitionIndex < (int)transitions.size())
					transitions.erase(transitions.begin() + m_SelectedTransitionIndex);
			}
			ClearSelectedControllerTransition();
		}
		ImGui::EndChild();
		return;
	}
	if (m_SelectedTransitionIndex >= 0)
		ClearSelectedControllerTransition();

	ImGui::TextDisabled("State Inspector");
	ImGui::Separator();
	AnimationControllerState& state = states[m_SelectedControllerStateIndex];

	const std::string oldName = state.m_Name;
	if (ImGui::InputText("Name", &state.m_Name))
	{
		if (state.m_Name.empty())
			state.m_Name = oldName;
		else if (state.m_Name != oldName)
		{
			if (m_CurrentController->GetDefaultState() == oldName)
				m_CurrentController->SetDefaultState(state.m_Name);
			for (AnimationControllerState& sourceState : states)
			{
				for (AnimationControllerTransition& transition : sourceState.m_Transitions)
				{
					if (transition.m_TargetState == oldName)
						transition.m_TargetState = state.m_Name;
				}
			}
			for (AnimationControllerTransition& transition : m_CurrentController->GetAnyStateTransitions())
			{
				if (transition.m_TargetState == oldName)
					transition.m_TargetState = state.m_Name;
			}
		}
	}

	const std::array<AnimationMotionType, 2> motionTypes =
	{
		AnimationMotionType::Clip,
		AnimationMotionType::BlendTree1D
	};
	std::string motionPreview = frenum::to_string(state.m_MotionType).data();
	if (ImGui::BeginCombo("Motion", motionPreview.c_str()))
	{
		for (AnimationMotionType motionType : motionTypes)
		{
			std::string label = frenum::to_string(motionType).data();
			const bool selected = state.m_MotionType == motionType;
			if (ImGui::Selectable(label.c_str(), selected))
				state.m_MotionType = motionType;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (state.m_MotionType == AnimationMotionType::Clip)
	{
		std::string clipLabel = "Drop Animation";
		bool isClipValid = false;
		if (state.m_Clip != 0)
		{
			if (AssetManager::IsAssetHandleValid(state.m_Clip) && AssetManager::GetAssetType(state.m_Clip) == AssetType::Animation)
			{
				clipLabel = AssetManager::GetAssetMetadata(state.m_Clip).m_Filepath.filename().string();
				isClipValid = true;
			}
			else
				clipLabel = "Invalid";
		}

		const auto clipDropCallback = [&state](AssetHandle handle)
			{
				state.m_Clip = handle;
			};
		UI::DragDropTarget(AssetType::Animation, clipDropCallback, clipLabel.c_str(), true, std::max(140.0f, width - 42.0f), 0.0f);
		if (isClipValid)
		{
			ImGui::SameLine();
			if (ImGui::Button("X##ClearStateClip"))
				state.m_Clip = 0;
		}
	}
	else
	{
		if (ImGui::BeginCombo("Blend Parameter", state.m_BlendParameter.empty() ? "None" : state.m_BlendParameter.c_str()))
		{
			for (const AnimationControllerParameter& parameter : m_CurrentController->GetParameters())
			{
				if (parameter.m_Type != AnimationParameterType::Float && parameter.m_Type != AnimationParameterType::Int && parameter.m_Type != AnimationParameterType::Bool)
					continue;

				const bool selected = state.m_BlendParameter == parameter.m_Name;
				if (ImGui::Selectable(parameter.m_Name.c_str(), selected))
					state.m_BlendParameter = parameter.m_Name;
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (ImGui::Button("+ Blend Child", ImVec2(-1.0f, 0.0f)))
			state.m_BlendChildren.push_back({});

		for (size_t childIndex = 0; childIndex < state.m_BlendChildren.size(); ++childIndex)
		{
			ImGui::PushID((int)childIndex);
			AnimationBlendChild& child = state.m_BlendChildren[childIndex];
			std::string childClipLabel = "Drop Animation";
			if (child.m_Clip != 0 && AssetManager::IsAssetHandleValid(child.m_Clip) && AssetManager::GetAssetType(child.m_Clip) == AssetType::Animation)
				childClipLabel = AssetManager::GetAssetMetadata(child.m_Clip).m_Filepath.filename().string();

			const auto childDropCallback = [&child](AssetHandle handle)
				{
					child.m_Clip = handle;
				};
			UI::DragDropTarget(AssetType::Animation, childDropCallback, childClipLabel.c_str(), true, std::max(120.0f, width - 42.0f), 0.0f);
			ImGui::DragFloat("Threshold", &child.m_Threshold, 0.01f);
			ImGui::DragFloat("Child Speed", &child.m_Speed, 0.01f, 0.0f, 20.0f, "%.2f");
			if (ImGui::Button("Remove Child", ImVec2(-1.0f, 0.0f)))
			{
				state.m_BlendChildren.erase(state.m_BlendChildren.begin() + childIndex);
				ImGui::PopID();
				break;
			}
			ImGui::Separator();
			ImGui::PopID();
		}
	}
	ImGui::DragFloat("Speed", &state.m_Speed, 0.01f, 0.0f, 20.0f, "%.2f");
	ImGui::Checkbox("Loop", &state.m_Loop);

	ImGui::Separator();
	ImGui::TextDisabled("Transitions");
	if (ImGui::Button("+ Transition", ImVec2(-1.0f, 0.0f)))
	{
		AnimationControllerTransition transition;
		for (const AnimationControllerState& targetState : states)
		{
			if (targetState.m_Name != state.m_Name)
			{
				transition.m_TargetState = targetState.m_Name;
				break;
			}
		}
		if (transition.m_TargetState.empty())
			transition.m_TargetState = std::string(AnimationController::ExitStateName);
		state.m_Transitions.push_back(transition);
		m_SelectedTransitionSourceStateIndex = m_SelectedControllerStateIndex;
		m_SelectedTransitionIndex = (int)state.m_Transitions.size() - 1;
	}

	for (size_t transitionIndex = 0; transitionIndex < state.m_Transitions.size(); ++transitionIndex)
	{
		ImGui::PushID((int)transitionIndex);
		AnimationControllerTransition& transition = state.m_Transitions[transitionIndex];
		std::string header = "-> " + (transition.m_TargetState.empty() ? std::string("None") : transition.m_TargetState);
		if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button("Select In Graph", ImVec2(-1.0f, 0.0f)))
			{
				m_SelectedTransitionSourceStateIndex = m_SelectedControllerStateIndex;
				m_SelectedTransitionIndex = (int)transitionIndex;
			}
			DrawControllerTransitionInspector(transition, true);

			if (ImGui::Button("Remove Transition", ImVec2(-1.0f, 0.0f)))
			{
				state.m_Transitions.erase(state.m_Transitions.begin() + transitionIndex);
				ClearSelectedControllerTransition();
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	ImGui::Separator();
	ImGui::TextDisabled("Any State");
	if (ImGui::Button("+ Any State Transition", ImVec2(-1.0f, 0.0f)))
	{
		AnimationControllerTransition transition;
		transition.m_HasExitTime = false;
		transition.m_TargetState = state.m_Name;
		auto& transitions = m_CurrentController->GetAnyStateTransitions();
		transitions.push_back(transition);
		m_SelectedTransitionSourceStateIndex = AnyStateTransitionSource;
		m_SelectedTransitionIndex = (int)transitions.size() - 1;
	}
	const auto& anyTransitions = m_CurrentController->GetAnyStateTransitions();
	for (size_t transitionIndex = 0; transitionIndex < anyTransitions.size(); ++transitionIndex)
	{
		const AnimationControllerTransition& transition = anyTransitions[transitionIndex];
		std::string label = "Any -> " + (transition.m_TargetState.empty() ? std::string("None") : transition.m_TargetState);
		if (ImGui::Selectable(label.c_str(), m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource && m_SelectedTransitionIndex == (int)transitionIndex))
		{
			m_SelectedTransitionSourceStateIndex = AnyStateTransitionSource;
			m_SelectedTransitionIndex = (int)transitionIndex;
		}
	}

	ImGui::EndChild();
}

void AnimationEditorPanel::SaveCurrentController()
{
	if (!m_CurrentController)
		return;

	const AssetMetadata& metadata = AssetManager::GetAssetMetadata(m_CurrentController->m_Handle);
	if (metadata)
		m_CurrentController->Serialize(Project::GetActiveAssetDirectory() / metadata.m_Filepath);
}

void AnimationEditorPanel::DrawTimeline(float width, float timelineHeight, float totalHeight)
{
	if (!m_CurrentAnimation)
		return;
	UI::DrawTimelineWithNodes(m_CurrentAnimation, 4.0f, width, timelineHeight, totalHeight, 120.0f, &m_SelectedFrameIndex);
}

void AnimationEditorPanel::DrawFrameList(float width)
{
	if (!m_CurrentAnimation)
		return;
	auto& frames = m_CurrentAnimation->GetFrames();

	ImGui::SetNextItemWidth(width);
	if (ImGui::BeginCombo("##FrameList", m_SelectedFrameIndex != -1 ? ("Frame " + std::to_string(m_SelectedFrameIndex)).c_str() : "Select Frame"))
	{
		for (size_t i = 0; i < frames.size(); ++i)
		{
			std::string label = "Frame " + std::to_string(i);
			if (ImGui::Selectable(label.c_str(), m_SelectedFrameIndex == i, 0, ImVec2(width - ImGui::GetStyle().WindowPadding.x, 0.0f)))
				m_SelectedFrameIndex = (int)i;
		}
		ImGui::EndCombo();
	}
}

void AnimationEditorPanel::DrawAddFrameButton(float width)
{
	if (!m_CurrentAnimation)
		return;
	if (ImGui::Button("Add Frame", ImVec2(width, 0.0f)))
	{
		AnimationFrame frame;
		frame.m_Duration = 0.1f;
		m_CurrentAnimation->AddFrame(frame);
		m_SelectedFrameIndex = int(m_CurrentAnimation->GetFrames().size() - 1);
		StopPreview(false);
	}
}

void AnimationEditorPanel::DrawRemoveFrameButton(float width)
{
	if (!m_CurrentAnimation || m_SelectedFrameIndex < 0)
		return;
	if (ImGui::Button("Remove Frame", ImVec2(width, 0.0f)))
	{
		m_CurrentAnimation->RemoveFrame(m_SelectedFrameIndex);
		if (m_CurrentAnimation->GetFrames().empty())
			m_SelectedFrameIndex = -1;
		else
			m_SelectedFrameIndex = std::min(m_SelectedFrameIndex, (int)m_CurrentAnimation->GetFrames().size() - 1);
		StopPreview(false);
	}
}

void AnimationEditorPanel::DrawPreviewPane(float width, float height)
{
	ImGui::TextDisabled("Preview");
	ImGui::Separator();

	if (!m_CurrentAnimation || m_SelectedFrameIndex < 0 || m_SelectedFrameIndex >= (int)m_CurrentAnimation->GetFrames().size())
	{
		ImGui::TextDisabled("Select a frame.");
		return;
	}

	const AnimationFrame& frame = m_CurrentAnimation->GetFrames()[m_SelectedFrameIndex];
	if (!frame.m_Texture)
	{
		ImGui::TextDisabled("No texture assigned.");
		return;
	}

	Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(frame.m_Texture);
	if (!texture || !texture->IsLoaded())
	{
		ImGui::TextDisabled("Texture is not loaded.");
		return;
	}

	const float maxPreview = std::max(48.0f, std::min(width, height - 42.0f));
	const float aspect = texture->GetHeight() > 0 ? static_cast<float>(texture->GetWidth()) / static_cast<float>(texture->GetHeight()) : 1.0f;
	ImVec2 previewSize(maxPreview, maxPreview);
	if (aspect > 1.0f)
		previewSize.y = previewSize.x / aspect;
	else
		previewSize.x = previewSize.y * aspect;

	const float centerOffset = std::max(0.0f, (width - previewSize.x) * 0.5f);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);
	UI::Image(UI::ToImGuiTextureId(texture->GetRendererId()), previewSize, ImVec2(0, 1), ImVec2(1, 0));
	ImGui::TextDisabled("%ux%u", texture->GetWidth(), texture->GetHeight());
}

void AnimationEditorPanel::DrawFrameEditor(float width)
{
	if (!m_CurrentAnimation)
		return;

	if (m_SelectedFrameIndex < 0 || m_SelectedFrameIndex >= (int)m_CurrentAnimation->GetFrames().size())
	{
		ImGui::TextDisabled("Select a frame to edit texture and duration.");
		return;
	}

	auto& frame = m_CurrentAnimation->GetFrames()[m_SelectedFrameIndex];
	const auto dragDropCallback = [&frame](AssetHandle handle)
		{
			frame.m_Texture = handle;
		};

	if (ImGui::BeginTable("##AnimationFrameInspectorTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 110.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Texture");
		ImGui::TableNextColumn();
		const std::string textureLabel = frame.m_Texture ? AssetManager::GetAssetMetadata(frame.m_Texture).m_Filepath.generic_string() : "Drop texture";
		UI::DragDropTarget(AssetType::Texture2D, dragDropCallback, textureLabel.c_str(), true, std::max(160.0f, width - 140.0f), 0.0f);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Duration");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1.0f);
		static constexpr float minValue = 0.0f;
		ImGui::DragScalar("##DurationSeconds", ImGuiDataType_Float, &frame.m_Duration, 0.01f, &minValue, nullptr, "%.3f s");

		ImGui::EndTable();
	}

	const float keyTime = m_CurrentAnimation->GetFrameStartTime((size_t)m_SelectedFrameIndex);
	ImGui::Separator();
	ImGui::TextDisabled("Events");
	if (ImGui::Button("+ Event"))
		m_CurrentAnimation->GetEvents().push_back({ keyTime, "Event" });

	auto& events = m_CurrentAnimation->GetEvents();
	for (size_t eventIndex = 0; eventIndex < events.size(); ++eventIndex)
	{
		ImGui::PushID((int)eventIndex);
		ImGui::DragFloat("Time", &events[eventIndex].m_Time, 0.01f, 0.0f);
		ImGui::InputText("Name", &events[eventIndex].m_Name);
		if (ImGui::Button("Remove Event", ImVec2(-1.0f, 0.0f)))
		{
			events.erase(events.begin() + eventIndex);
			ImGui::PopID();
			break;
		}
		ImGui::Separator();
		ImGui::PopID();
	}

	ImGui::TextDisabled("Property Tracks");
	if (ImGui::Button("+ Translation"))
		m_CurrentAnimation->GetTranslationKeys().push_back({ keyTime, glm::vec3{ 0.0f } });
	ImGui::SameLine();
	if (ImGui::Button("+ Rotation"))
		m_CurrentAnimation->GetRotationKeys().push_back({ keyTime, glm::vec3{ 0.0f } });
	if (ImGui::Button("+ Scale"))
		m_CurrentAnimation->GetScaleKeys().push_back({ keyTime, glm::vec3{ 1.0f } });
	ImGui::SameLine();
	if (ImGui::Button("+ Color"))
		m_CurrentAnimation->GetColorKeys().push_back({ keyTime, glm::vec4{ 1.0f } });

	auto drawVec3Keys = [](const char* label, std::vector<AnimationVec3Key>& keys)
		{
			if (!ImGui::TreeNode(label))
				return;
			for (size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex)
			{
				ImGui::PushID((int)keyIndex);
				ImGui::DragFloat("Time", &keys[keyIndex].m_Time, 0.01f, 0.0f);
				ImGui::DragFloat3("Value", &keys[keyIndex].m_Value.x, 0.01f);
				if (ImGui::Button("Remove Key", ImVec2(-1.0f, 0.0f)))
				{
					keys.erase(keys.begin() + keyIndex);
					ImGui::PopID();
					break;
				}
				ImGui::Separator();
				ImGui::PopID();
			}
			ImGui::TreePop();
		};

	auto drawVec4Keys = [](const char* label, std::vector<AnimationVec4Key>& keys)
		{
			if (!ImGui::TreeNode(label))
				return;
			for (size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex)
			{
				ImGui::PushID((int)keyIndex);
				ImGui::DragFloat("Time", &keys[keyIndex].m_Time, 0.01f, 0.0f);
				ImGui::ColorEdit4("Value", &keys[keyIndex].m_Value.x);
				if (ImGui::Button("Remove Key", ImVec2(-1.0f, 0.0f)))
				{
					keys.erase(keys.begin() + keyIndex);
					ImGui::PopID();
					break;
				}
				ImGui::Separator();
				ImGui::PopID();
			}
			ImGui::TreePop();
		};

	drawVec3Keys("Translation Keys", m_CurrentAnimation->GetTranslationKeys());
	drawVec3Keys("Rotation Keys", m_CurrentAnimation->GetRotationKeys());
	drawVec3Keys("Scale Keys", m_CurrentAnimation->GetScaleKeys());
	drawVec4Keys("Color Keys", m_CurrentAnimation->GetColorKeys());
}

void AnimationEditorPanel::UpdatePreview()
{
	if (!m_PreviewPlaying || m_PreviewPaused || !m_CurrentAnimation)
		return;

	auto& frames = m_CurrentAnimation->GetFrames();
	if (frames.empty())
	{
		StopPreview(false);
		return;
	}

	if (m_SelectedFrameIndex < 0 || m_SelectedFrameIndex >= (int)frames.size())
		m_SelectedFrameIndex = 0;

	m_PreviewElapsed += ImGui::GetIO().DeltaTime;
	const float currentDuration = std::max(frames[m_SelectedFrameIndex].m_Duration, 0.033f);
	if (m_PreviewElapsed < currentDuration)
		return;

	m_PreviewElapsed = 0.0f;
	const int nextFrame = m_SelectedFrameIndex + 1;
	if (nextFrame < (int)frames.size())
	{
		m_SelectedFrameIndex = nextFrame;
		return;
	}

	if (m_CurrentAnimation->IsLooping())
		m_SelectedFrameIndex = 0;
	else
		StopPreview(false);
}

void AnimationEditorPanel::StepPreview(int direction)
{
	if (!m_CurrentAnimation)
		return;

	auto& frames = m_CurrentAnimation->GetFrames();
	if (frames.empty())
		return;

	if (m_SelectedFrameIndex < 0 || m_SelectedFrameIndex >= (int)frames.size())
		m_SelectedFrameIndex = direction < 0 ? (int)frames.size() - 1 : 0;
	else
	{
		const int frameCount = (int)frames.size();
		m_SelectedFrameIndex = (m_SelectedFrameIndex + direction + frameCount) % frameCount;
	}

	m_PreviewElapsed = 0.0f;
}

void AnimationEditorPanel::StopPreview(bool resetSelection)
{
	m_PreviewPlaying = false;
	m_PreviewPaused = false;
	m_PreviewElapsed = 0.0f;

	if (resetSelection && m_CurrentAnimation && !m_CurrentAnimation->GetFrames().empty())
		m_SelectedFrameIndex = 0;
}

_WHIP_END
