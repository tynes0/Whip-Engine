#include "AnimationEditorPanel.h"
#include <Whip/Core/Application.h>
#include <Whip/Utils/FileExtensions.h>
#include <Whip/Utils/Utility.h>
#include <Whip/Project/Project.h>
#include <Whip/Asset/AssetManager.h>
#include <Whip/Asset/AssetUtils.h>
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
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <limits>
#include <unordered_set>

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

	float Length(const ImVec2& value)
	{
		return std::sqrt(Dot(value, value));
	}

	ImVec2 Normalize(const ImVec2& value)
	{
		const float length = Length(value);
		if (length <= 0.0001f)
			return { 1.0f, 0.0f };
		return { value.x / length, value.y / length };
	}

	ImVec2 Perpendicular(const ImVec2& value)
	{
		return { -value.y, value.x };
	}

	std::string FormatCompactFloat(float value)
	{
		char buffer[32]{};
		std::snprintf(buffer, sizeof(buffer), "%.2f", value);
		return buffer;
	}

	bool IsTextureSourceFile(const std::filesystem::path& filepath)
	{
		return Utils::TryGetAssetTypeFromFileExtension(filepath.extension()) == AssetType::Texture2D;
	}

	bool IsRelativePathInsideRoot(const std::filesystem::path& relativePath)
	{
		if (relativePath.empty())
			return false;

		for (const std::filesystem::path& part : relativePath)
		{
			if (part == "..")
				return false;
		}

		return true;
	}

	template<typename TKey>
	void SortKeysByTime(std::vector<TKey>& keys)
	{
		std::sort(keys.begin(), keys.end(), [](const TKey& left, const TKey& right)
			{
				return left.m_Time < right.m_Time;
			});
	}

	ImGuiKey ToImGuiKey(KeyCode key)
	{
		switch (key)
		{
		case Key::A: return ImGuiKey_A;
		case Key::B: return ImGuiKey_B;
		case Key::C: return ImGuiKey_C;
		case Key::D: return ImGuiKey_D;
		case Key::E: return ImGuiKey_E;
		case Key::F: return ImGuiKey_F;
		case Key::G: return ImGuiKey_G;
		case Key::H: return ImGuiKey_H;
		case Key::I: return ImGuiKey_I;
		case Key::J: return ImGuiKey_J;
		case Key::K: return ImGuiKey_K;
		case Key::L: return ImGuiKey_L;
		case Key::M: return ImGuiKey_M;
		case Key::N: return ImGuiKey_N;
		case Key::O: return ImGuiKey_O;
		case Key::P: return ImGuiKey_P;
		case Key::Q: return ImGuiKey_Q;
		case Key::R: return ImGuiKey_R;
		case Key::S: return ImGuiKey_S;
		case Key::T: return ImGuiKey_T;
		case Key::U: return ImGuiKey_U;
		case Key::V: return ImGuiKey_V;
		case Key::W: return ImGuiKey_W;
		case Key::X: return ImGuiKey_X;
		case Key::Y: return ImGuiKey_Y;
		case Key::Z: return ImGuiKey_Z;
		case Key::Delete: return ImGuiKey_Delete;
		case Key::Backspace: return ImGuiKey_Backspace;
		case Key::Insert: return ImGuiKey_Insert;
		case Key::Enter: return ImGuiKey_Enter;
		case Key::Tab: return ImGuiKey_Tab;
		case Key::Space: return ImGuiKey_Space;
		case Key::Escape: return ImGuiKey_Escape;
		default: return ImGuiKey_None;
		}
	}

	bool ShortcutPressed(const UI::UISettings& settings, UI::EditorShortcutAction action)
	{
		if (settings.HasShortcutConflict(action))
			return false;

		const UI::ShortcutBinding binding = settings.GetShortcutBinding(action);
		const ImGuiKey imguiKey = ToImGuiKey(binding.m_Key);
		if (imguiKey == ImGuiKey_None)
			return false;

		const ImGuiIO& io = ImGui::GetIO();
		if (binding.m_Ctrl != io.KeyCtrl || binding.m_Shift != io.KeyShift || binding.m_Alt != io.KeyAlt)
			return false;

		return ImGui::IsKeyPressed(imguiKey, false);
	}

}

AnimationEditorPanel::AnimationEditorPanel()
{
}

AnimationEditorPanel::~AnimationEditorPanel() {}

void AnimationEditorPanel::OnImGuiRender()
{
	if (!m_Open)
	{
		m_ShortcutContextActive = false;
		return;
	}
	bool open = m_Open;
	if (m_FullscreenRequested)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
		m_FullscreenRequested = false;
	}
	if (m_FocusRequested)
	{
		ImGui::SetNextWindowFocus();
		m_FocusRequested = false;
	}
	ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 260.0f), ImVec2(FLT_MAX, FLT_MAX));
	const std::string windowTitle = GetWindowTitle();
	ImGui::Begin(windowTitle.c_str(), &open);
	m_ShortcutContextActive = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_DockHierarchy);
	if (open != m_Open)
		SetOpen(open);
	UpdatePreview();

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_ChildBg));

	ImGui::BeginChild("##AnimationEditorToolbar", ImVec2(0.0f, 96.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	auto drawModeButton = [&](AnimationEditorMode mode, const char* label)
		{
			const bool selected = m_EditorMode == mode;
			if (selected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.43f, 0.55f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.34f, 0.50f, 0.64f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.38f, 0.50f, 1.0f));
			}
			const bool clicked = ImGui::Button(label, ImVec2(132.0f, 30.0f));
			if (selected)
				ImGui::PopStyleColor(3);
			return clicked;
		};

	if (drawModeButton(AnimationEditorMode::Clip, "Animation Clip"))
		m_EditorMode = AnimationEditorMode::Clip;
	ImGui::SameLine();
	if (drawModeButton(AnimationEditorMode::Controller, "Controller"))
	{
		m_EditorMode = AnimationEditorMode::Controller;
		StopPreview(false);
	}
	ImGui::SameLine(0.0f, 16.0f);
	std::string controllerStatus = "No controller";
	if (m_CurrentController)
	{
		const AssetMetadata& metadata = AssetManager::GetAssetMetadata(m_CurrentController->m_Handle);
		controllerStatus = metadata ? metadata.m_Filepath.filename().string() : "Memory controller";
	}
	ImGui::TextDisabled("Clip: %s  |  Controller: %s", m_CurrentAnimation ? m_CurrentAnimation->GetName().c_str() : "None", controllerStatus.c_str());
	ImGui::SameLine();
	if (ImGui::SmallButton(m_CompactMode ? "Expand" : "Mini"))
		m_CompactMode = !m_CompactMode;
	ImGui::SameLine();
	if (ImGui::SmallButton("Full"))
		m_FullscreenRequested = true;
	ImGui::Separator();

	if (m_EditorMode == AnimationEditorMode::Clip)
	{
		ImGui::AlignTextToFramePadding();
		DrawPlaybackControls(30.0f, 30.0f);
		ImGui::SameLine(0.0f, 14.0f);

		if (ImGui::Button("New Clip", ImVec2(92.0f, 30.0f)))
		{
			m_EditorMode = AnimationEditorMode::Clip;
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
		ImGui::BeginDisabled(!m_CurrentAnimation);
		if (ImGui::Button("Save Clip", ImVec2(86.0f, 30.0f)))
		{
			const auto& metadata = AssetManager::GetAssetMetadata(m_CurrentAnimation->m_Handle);
			if (metadata)
				m_CurrentAnimation->Serialize(Project::GetActiveAssetDirectory() / metadata.m_Filepath);
		}
		ImGui::SameLine();
		if (ImGui::Button("Close Clip", ImVec2(88.0f, 30.0f)))
		{
			m_CurrentAnimation = nullptr;
			m_SelectedFrameIndex = -1;
			StopPreview(false);
		}

		if (m_CurrentAnimation)
		{
			ImGui::SameLine(0.0f, 12.0f);
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strncpy_s(buffer, sizeof(buffer), m_CurrentAnimation->GetName().c_str(), sizeof(buffer));
			ImGui::SetNextItemWidth(std::min(190.0f, std::max(96.0f, ImGui::GetContentRegionAvail().x * 0.34f)));
			if (ImGui::InputText("##AnimationName", buffer, sizeof(buffer)))
				m_CurrentAnimation->SetName(buffer);
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::SetNextItemWidth(std::min(240.0f, std::max(120.0f, ImGui::GetContentRegionAvail().x)));
		if (ImGui::BeginCombo("##AnimationSelector", m_CurrentAnimation ? m_CurrentAnimation->GetName().data() : "Select Animation"))
		{
			const auto& reg = Project::GetActive()->GetEditorAssetManager()->GetAssetRegistry();
			reg.Foreach(AssetType::Animation, [&](const AssetRegistry::ValueType& value)
				{
					auto anim = AssetManager::GetAsset<Animation2D>(value.first);
					if (ImGui::Selectable(anim->GetName().c_str(), m_CurrentAnimation ? m_CurrentAnimation->m_Handle == value.first : false))
					{
						m_EditorMode = AnimationEditorMode::Clip;
						m_CurrentAnimation = anim;
						m_SelectedFrameIndex = -1;
						StopPreview(false);
					}
				});
			ImGui::EndCombo();
		}
	}
	else
	{
		if (ImGui::Button("New Controller", ImVec2(122.0f, 30.0f)))
		{
			m_EditorMode = AnimationEditorMode::Controller;
			StopPreview(false);
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
		if (ImGui::Button("Save Controller", ImVec2(122.0f, 30.0f)))
			SaveCurrentController();
		ImGui::SameLine();
		if (ImGui::Button("Close Controller", ImVec2(126.0f, 30.0f)))
		{
			m_CurrentController = nullptr;
			m_SelectedControllerStateIndex = 0;
			m_SelectedControllerParameterIndex = -1;
			ClearSelectedControllerTransition();
		}
		ImGui::EndDisabled();

		ImGui::SameLine(0.0f, 12.0f);
		DrawControllerSelector(std::min(280.0f, std::max(140.0f, ImGui::GetContentRegionAvail().x)));
	}
	ImGui::EndChild();

	if (m_CompactMode)
	{
		DrawCompactSummary();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
		ImGui::End();
		return;
	}

	ImGui::Spacing();
	if (m_EditorMode == AnimationEditorMode::Controller)
	{
		if (m_CurrentController)
			DrawControllerEditor(std::max(260.0f, ImGui::GetContentRegionAvail().y));
		else
		{
			ImGui::BeginChild("##ControllerEditorEmpty", ImVec2(0.0f, 0.0f), true);
			ImVec2 available = ImGui::GetContentRegionAvail();
			ImGui::SetCursorPos(ImVec2(16.0f, 16.0f));
			ImGui::TextDisabled("No animation controller selected");
			ImGui::SetCursorPos(ImVec2(16.0f, 48.0f));
			DrawControllerDragDropArea(std::max(120.0f, available.x - 32.0f), std::max(80.0f, available.y - 64.0f));
			ImGui::EndChild();
		}
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
	ImGui::SameLine();
	DrawImportFramesButton(136.0f);
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

bool AnimationEditorPanel::OpenAsset(AssetHandle handle)
{
	if (handle == 0 || !Project::GetActive() || !AssetManager::IsAssetHandleValid(handle))
		return false;

	const AssetType type = AssetManager::GetAssetType(handle);
	if (type == AssetType::Animation)
	{
		Ref<Animation2D> animation = AssetManager::GetAsset<Animation2D>(handle);
		if (!animation)
			return false;

		m_EditorMode = AnimationEditorMode::Clip;
		m_CurrentAnimation = animation;
		m_SelectedFrameIndex = -1;
		StopPreview(false);
		SetOpen(true);
		m_FocusRequested = true;
		return true;
	}

	if (type == AssetType::AnimationController)
	{
		Ref<AnimationController> controller = AssetManager::GetAsset<AnimationController>(handle);
		if (!controller)
			return false;

		m_EditorMode = AnimationEditorMode::Controller;
		m_CurrentController = controller;
		m_SelectedControllerStateIndex = 0;
		m_SelectedControllerParameterIndex = -1;
		ClearSelectedControllerTransition();
		StopPreview(false);
		SetOpen(true);
		m_FocusRequested = true;
		return true;
	}

	return false;
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
			m_EditorMode = AnimationEditorMode::Clip;
			m_CurrentAnimation = AssetManager::GetAsset<Animation2D>(handle);
			m_SelectedFrameIndex = -1;
			StopPreview(false);
		};

	UI::DragDropTarget(AssetType::Animation, dragDropCallback, "Select Animation", true, width, height, true);
}

void AnimationEditorPanel::DrawControllerDragDropArea(float width, float height)
{
	const auto dragDropCallback = [this](AssetHandle handle)
		{
			m_EditorMode = AnimationEditorMode::Controller;
			m_CurrentController = AssetManager::GetAsset<AnimationController>(handle);
			m_SelectedControllerStateIndex = 0;
			m_SelectedControllerParameterIndex = -1;
			ClearSelectedControllerTransition();
			StopPreview(false);
		};

	UI::DragDropTarget(AssetType::AnimationController, dragDropCallback, "Select Controller", true, width, height, true);
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
					m_EditorMode = AnimationEditorMode::Controller;
					m_CurrentController = AssetManager::GetAsset<AnimationController>(value.first);
					m_SelectedControllerStateIndex = 0;
					m_SelectedControllerParameterIndex = -1;
					ClearSelectedControllerTransition();
					StopPreview(false);
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
	ImGui::BeginDisabled(m_CurrentController->GetStates().empty());
	if (ImGui::Button("Duplicate"))
	{
		auto& states = m_CurrentController->GetStates();
		const AnimationControllerState sourceState = states[m_SelectedControllerStateIndex];
		AnimationControllerState& duplicate = m_CurrentController->AddState(sourceState.m_Name + " Copy", sourceState.m_Clip);
		const std::string uniqueName = duplicate.m_Name;
		duplicate = sourceState;
		duplicate.m_Name = uniqueName;
		duplicate.m_GraphPosition += glm::vec2{ 44.0f, 44.0f };
		duplicate.m_Transitions.clear();
		m_SelectedControllerStateIndex = (int)m_CurrentController->GetStates().size() - 1;
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
	if (ImGui::Button("Auto Layout"))
		AutoLayoutControllerGraph();
	ImGui::SameLine();
	if (ImGui::Button("Frame Graph"))
		m_FrameControllerGraphRequested = true;
	ImGui::SameLine();
	ImGui::Checkbox("Snap", &m_ControllerGraphSnapToGrid);
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

	const ImVec2 mousePos = ImGui::GetIO().MousePos;
	const bool canvasHovered =
		ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
		ImGui::IsMouseHoveringRect(canvasMin, canvasMax, true);
	if (canvasHovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
	{
		const ImVec2 delta = ImGui::GetIO().MouseDelta;
		m_ControllerGraphPan.x += delta.x;
		m_ControllerGraphPan.y += delta.y;
	}
	if (canvasHovered && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f)
		m_ControllerGraphZoom = std::clamp(m_ControllerGraphZoom + ImGui::GetIO().MouseWheel * 0.08f, 0.45f, 2.0f);

	const float nodeWidth = 174.0f;
	const float nodeHeight = 82.0f;
	const float specialWidth = 136.0f;
	const float specialHeight = 58.0f;
	float zoom = m_ControllerGraphZoom;
	auto snapPosition = [&](glm::vec2& position)
		{
			if (!m_ControllerGraphSnapToGrid)
				return;
			constexpr float grid = 16.0f;
			position.x = std::round(position.x / grid) * grid;
			position.y = std::round(position.y / grid) * grid;
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

	glm::vec2& entryPosition = m_CurrentController->GetEntryGraphPosition();
	glm::vec2& anyStatePosition = m_CurrentController->GetAnyStateGraphPosition();
	glm::vec2& exitPosition = m_CurrentController->GetExitGraphPosition();
	if (exitPosition.x <= 0.0f && exitPosition.y <= 0.0f)
		exitPosition = { exitColumn, 108.0f };

	if (m_FrameControllerGraphRequested)
	{
		glm::vec2 minBounds{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
		glm::vec2 maxBounds{ -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
		auto includeNode = [&](const glm::vec2& position, float nodeW, float nodeH)
			{
				minBounds.x = std::min(minBounds.x, position.x);
				minBounds.y = std::min(minBounds.y, position.y);
				maxBounds.x = std::max(maxBounds.x, position.x + nodeW);
				maxBounds.y = std::max(maxBounds.y, position.y + nodeH);
			};

		includeNode(entryPosition, specialWidth, specialHeight);
		includeNode(anyStatePosition, specialWidth, specialHeight);
		includeNode(exitPosition, specialWidth, specialHeight);
		for (const AnimationControllerState& state : states)
			includeNode(state.m_GraphPosition, nodeWidth, nodeHeight);

		const float boundsWidth = std::max(1.0f, maxBounds.x - minBounds.x);
		const float boundsHeight = std::max(1.0f, maxBounds.y - minBounds.y);
		const float fitZoom = std::min((canvasSize.x - 80.0f) / boundsWidth, (canvasSize.y - 80.0f) / boundsHeight);
		m_ControllerGraphZoom = std::clamp(fitZoom, 0.45f, 2.0f);
		zoom = m_ControllerGraphZoom;
		m_ControllerGraphPan =
		{
			(canvasSize.x - boundsWidth * zoom) * 0.5f - minBounds.x * zoom,
			(canvasSize.y - boundsHeight * zoom) * 0.5f - minBounds.y * zoom
		};
		m_FrameControllerGraphRequested = false;
	}

	const float pinRadius = std::max(3.5f, 5.5f * zoom);
	const ImVec2 nodeSize(nodeWidth * zoom, nodeHeight * zoom);
	const ImVec2 specialSize(specialWidth * zoom, specialHeight * zoom);

	auto worldToScreen = [&](const glm::vec2& world) -> ImVec2
		{
			return { canvasMin.x + m_ControllerGraphPan.x + world.x * zoom, canvasMin.y + m_ControllerGraphPan.y + world.y * zoom };
		};
	auto screenToWorld = [&](const ImVec2& screen) -> glm::vec2
		{
			return
			{
				(screen.x - canvasMin.x - m_ControllerGraphPan.x) / zoom,
				(screen.y - canvasMin.y - m_ControllerGraphPan.y) / zoom
			};
		};

	const float gridStep = 32.0f * zoom;
	const ImU32 gridColor = IM_COL32(72, 66, 54, 80);
	for (float x = std::fmod(m_ControllerGraphPan.x, gridStep); x < canvasSize.x; x += gridStep)
		drawList->AddLine(ImVec2(canvasMin.x + x, canvasMin.y), ImVec2(canvasMin.x + x, canvasMax.y), gridColor);
	for (float y = std::fmod(m_ControllerGraphPan.y, gridStep); y < canvasSize.y; y += gridStep)
		drawList->AddLine(ImVec2(canvasMin.x, canvasMin.y + y), ImVec2(canvasMax.x, canvasMin.y + y), gridColor);

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
				const bool alreadyExists = std::any_of(transitions.begin(), transitions.end(), [&target](const AnimationControllerTransition& existingTransition)
					{
						return existingTransition.m_TargetState == target;
					});
				if (alreadyExists)
				{
					clearPendingConnection();
					return;
				}
				transitions.push_back(transition);
				selectTransition(AnyStateTransitionSource, (int)transitions.size() - 1);
				clearPendingConnection();
				return;
			}

			if (m_PendingTransitionSourceStateIndex >= 0 && m_PendingTransitionSourceStateIndex < (int)states.size())
			{
				if (!targetExit && m_PendingTransitionSourceStateIndex == targetStateIndex)
				{
					clearPendingConnection();
					return;
				}
				auto& transitions = states[m_PendingTransitionSourceStateIndex].m_Transitions;
				const bool alreadyExists = std::any_of(transitions.begin(), transitions.end(), [&target](const AnimationControllerTransition& existingTransition)
					{
						return existingTransition.m_TargetState == target;
					});
				if (alreadyExists)
				{
					clearPendingConnection();
					return;
				}
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

	bool edgeHovered = false;
	auto drawTransition = [&](int sourceStateIndex, int transitionIndex, const ImVec2& sourcePin, const AnimationControllerTransition& transition)
		{
			ImVec2 targetPin;
			if (!transitionTargetPin(transition, targetPin))
				return;

			const bool selected = m_SelectedTransitionSourceStateIndex == sourceStateIndex && m_SelectedTransitionIndex == transitionIndex;
			const bool hovered = canvasHovered && DistanceToSegment(mousePos, sourcePin, targetPin) <= 9.0f;
			edgeHovered |= hovered;
			const ImU32 color = selected ? IM_COL32(122, 196, 255, 255) : hovered ? IM_COL32(235, 196, 118, 255) : IM_COL32(184, 132, 72, 220);
			const float tangent = std::max(58.0f * zoom, std::abs(targetPin.x - sourcePin.x) * 0.42f);
			drawList->AddBezierCubic(sourcePin, ImVec2(sourcePin.x + tangent, sourcePin.y), ImVec2(targetPin.x - tangent, targetPin.y), targetPin, color, selected ? 3.2f : 2.0f);
			drawList->AddCircleFilled(targetPin, selected ? pinRadius + 1.0f : pinRadius, color);

			const ImVec2 midpoint = Scale(Add(sourcePin, targetPin), 0.5f);
			const ImVec2 direction = Normalize(Subtract(targetPin, sourcePin));
			const ImVec2 normal = Perpendicular(direction);
			const float arrowSize = 7.0f * zoom;
			drawList->AddTriangleFilled(
				Add(midpoint, Scale(direction, arrowSize)),
				Add(Add(midpoint, Scale(direction, -arrowSize)), Scale(normal, arrowSize * 0.58f)),
				Add(Add(midpoint, Scale(direction, -arrowSize)), Scale(normal, -arrowSize * 0.58f)),
				color);

			std::string badge;
			if (!transition.m_Conditions.empty())
				badge += std::to_string(transition.m_Conditions.size()) + " cond";
			if (transition.m_HasExitTime)
				badge += (badge.empty() ? "" : " | ") + std::string("exit ") + FormatCompactFloat(transition.m_ExitTime);
			if (transition.m_Duration > 0.0f)
				badge += (badge.empty() ? "" : " | ") + FormatCompactFloat(transition.m_Duration) + "s";
			if (!badge.empty())
			{
				const ImVec2 textSize = ImGui::CalcTextSize(badge.c_str());
				const ImVec2 badgeMin(midpoint.x - textSize.x * 0.5f - 6.0f, midpoint.y - textSize.y - 18.0f);
				const ImVec2 badgeMax(badgeMin.x + textSize.x + 12.0f, badgeMin.y + textSize.y + 6.0f);
				drawList->AddRectFilled(badgeMin, badgeMax, IM_COL32(30, 27, 22, 235), 4.0f);
				drawList->AddRect(badgeMin, badgeMax, color, 4.0f);
				drawList->AddText(ImVec2(badgeMin.x + 6.0f, badgeMin.y + 3.0f), IM_COL32(238, 230, 214, 255), badge.c_str());
			}

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

	auto drawSpecialNode = [&](const char* label, glm::vec2& worldPosition, ImU32 fill, ImU32 border, int sourceIndex, bool hasInput)
		{
			const ImVec2 nodeMin = worldToScreen(worldPosition);
			const ImVec2 nodeMax(nodeMin.x + specialSize.x, nodeMin.y + specialSize.y);
			drawList->AddRectFilled(nodeMin, nodeMax, fill, 6.0f);
			drawList->AddRect(nodeMin, nodeMax, border, 6.0f, 0, 1.6f);
			drawList->AddText(ImVec2(nodeMin.x + 12.0f * zoom, nodeMin.y + 18.0f * zoom), IM_COL32(238, 230, 214, 255), label);

			const float leftPinGutter = hasInput ? 16.0f * zoom : 0.0f;
			const float rightPinGutter = sourceIndex != NoTransitionSource ? 16.0f * zoom : 0.0f;
			ImGui::SetCursorScreenPos(ImVec2(nodeMin.x + leftPinGutter, nodeMin.y));
			ImGui::PushID(label);
			ImGui::InvisibleButton("##SpecialNodeBody", ImVec2(std::max(24.0f, specialSize.x - leftPinGutter - rightPinGutter), specialSize.y));
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
				ClearSelectedControllerTransition();
			if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && m_PendingTransitionSourceStateIndex == NoTransitionSource)
			{
				const ImVec2 delta = ImGui::GetIO().MouseDelta;
				worldPosition.x += delta.x / zoom;
				worldPosition.y += delta.y / zoom;
			}
			if (ImGui::IsItemDeactivated())
				snapPosition(worldPosition);
			ImGui::PopID();

			if (sourceIndex != NoTransitionSource)
			{
				const ImVec2 outPin(nodeMax.x, nodeMin.y + specialSize.y * 0.5f);
				drawList->AddCircleFilled(outPin, pinRadius, IM_COL32(228, 184, 104, 255));
				ImGui::SetCursorScreenPos(ImVec2(outPin.x - 9.0f, outPin.y - 9.0f));
				ImGui::PushID(sourceIndex);
				ImGui::InvisibleButton("##SpecialOutPin", ImVec2(18.0f, 18.0f), ImGuiButtonFlags_AllowOverlap);
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
				ImGui::InvisibleButton("##SpecialInPin", ImVec2(18.0f, 18.0f), ImGuiButtonFlags_AllowOverlap);
				if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
					createConnection(-1, true);
				ImGui::PopID();
			}
		};

	drawSpecialNode("Entry", entryPosition, IM_COL32(36, 54, 66, 255), IM_COL32(92, 168, 236, 235), EntryTransitionSource, false);
	drawSpecialNode("Any State", anyStatePosition, IM_COL32(58, 42, 31, 255), IM_COL32(232, 166, 85, 235), AnyStateTransitionSource, false);
	drawSpecialNode("Exit", exitPosition, IM_COL32(47, 38, 42, 255), IM_COL32(204, 116, 128, 235), NoTransitionSource, true);

	int stateToRemove = -1;
	int stateToDuplicate = -1;
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

		const float pinGutter = 16.0f * zoom;
		ImGui::SetCursorScreenPos(ImVec2(nodeMin.x + pinGutter, nodeMin.y));
		ImGui::PushID((int)i);
		ImGui::InvisibleButton("##ControllerStateNode", ImVec2(std::max(24.0f, nodeSize.x - pinGutter * 2.0f), nodeSize.y));
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
		if (ImGui::IsItemDeactivated())
			snapPosition(state.m_GraphPosition);
		if (ImGui::BeginPopupContextItem("##StateNodeContext"))
		{
			if (ImGui::MenuItem("Set Default"))
			{
				m_CurrentController->SetDefaultState(state.m_Name);
				ClearSelectedControllerTransition();
			}
			if (ImGui::MenuItem("Duplicate"))
				stateToDuplicate = (int)i;
			ImGui::BeginDisabled(states.size() <= 1);
			if (ImGui::MenuItem("Remove"))
				stateToRemove = (int)i;
			ImGui::EndDisabled();
			ImGui::EndPopup();
		}

		const ImVec2 inputPin = stateInputPin(i);
		drawList->AddCircleFilled(inputPin, pinRadius, IM_COL32(116, 190, 138, 255));
		ImGui::SetCursorScreenPos(ImVec2(inputPin.x - 9.0f, inputPin.y - 9.0f));
		ImGui::InvisibleButton("##StateInPin", ImVec2(18.0f, 18.0f), ImGuiButtonFlags_AllowOverlap);
		if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			createConnection((int)i, false);

		const ImVec2 outputPin = stateOutputPin(i);
		drawList->AddCircleFilled(outputPin, pinRadius, IM_COL32(228, 184, 104, 255));
		ImGui::SetCursorScreenPos(ImVec2(outputPin.x - 9.0f, outputPin.y - 9.0f));
		ImGui::InvisibleButton("##StateOutPin", ImVec2(18.0f, 18.0f), ImGuiButtonFlags_AllowOverlap);
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			m_PendingTransitionSourceStateIndex = (int)i;
		ImGui::PopID();
	}

	if (stateToDuplicate >= 0 && stateToDuplicate < (int)states.size())
	{
		const AnimationControllerState sourceState = states[stateToDuplicate];
		AnimationControllerState& duplicate = m_CurrentController->AddState(sourceState.m_Name + " Copy", sourceState.m_Clip);
		const std::string uniqueName = duplicate.m_Name;
		duplicate = sourceState;
		duplicate.m_Name = uniqueName;
		duplicate.m_GraphPosition += glm::vec2{ 44.0f, 44.0f };
		duplicate.m_Transitions.clear();
		m_SelectedControllerStateIndex = (int)m_CurrentController->GetStates().size() - 1;
		ClearSelectedControllerTransition();
	}
	if (stateToRemove >= 0 && stateToRemove < (int)m_CurrentController->GetStates().size() && m_CurrentController->GetStates().size() > 1)
	{
		const std::string removedName = m_CurrentController->GetStates()[stateToRemove].m_Name;
		m_CurrentController->RemoveState(removedName);
		m_SelectedControllerStateIndex = std::clamp(m_SelectedControllerStateIndex, 0, (int)m_CurrentController->GetStates().size() - 1);
		ClearSelectedControllerTransition();
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

	if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !edgeHovered && !ImGui::IsAnyItemHovered())
		ClearSelectedControllerTransition();
	if (canvasHovered && ImGui::IsKeyPressed(ImGuiKey_Delete) && GetSelectedControllerTransition())
		RemoveSelectedControllerTransition();
	if (canvasHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered() && Length(ImGui::GetMouseDragDelta(ImGuiMouseButton_Right)) < 2.0f)
		ImGui::OpenPopup("##ControllerGraphCanvasContext");
	if (ImGui::BeginPopup("##ControllerGraphCanvasContext"))
	{
		if (ImGui::MenuItem("Add State Here"))
		{
			AnimationControllerState& state = m_CurrentController->AddState("State");
			state.m_GraphPosition = screenToWorld(mousePos);
			snapPosition(state.m_GraphPosition);
			m_SelectedControllerStateIndex = (int)m_CurrentController->GetStates().size() - 1;
			ClearSelectedControllerTransition();
		}
		if (ImGui::MenuItem("Frame Graph"))
			m_FrameControllerGraphRequested = true;
		if (ImGui::MenuItem("Auto Layout"))
			AutoLayoutControllerGraph();
		if (ImGui::MenuItem("Reset View"))
		{
			m_ControllerGraphZoom = 1.0f;
			m_ControllerGraphPan = { 28.0f, 28.0f };
		}
		ImGui::EndPopup();
	}

	drawList->PopClipRect();
	ImGui::SetCursorScreenPos(canvasMin);
	ImGui::Dummy(canvasSize);
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

void AnimationEditorPanel::RemoveSelectedControllerTransition()
{
	if (!m_CurrentController || m_SelectedTransitionIndex < 0)
		return;

	if (m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource)
	{
		auto& transitions = m_CurrentController->GetAnyStateTransitions();
		if (m_SelectedTransitionIndex >= 0 && m_SelectedTransitionIndex < (int)transitions.size())
			transitions.erase(transitions.begin() + m_SelectedTransitionIndex);
		ClearSelectedControllerTransition();
		return;
	}

	auto& states = m_CurrentController->GetStates();
	if (m_SelectedTransitionSourceStateIndex < 0 || m_SelectedTransitionSourceStateIndex >= (int)states.size())
	{
		ClearSelectedControllerTransition();
		return;
	}

	auto& transitions = states[m_SelectedTransitionSourceStateIndex].m_Transitions;
	if (m_SelectedTransitionIndex >= 0 && m_SelectedTransitionIndex < (int)transitions.size())
		transitions.erase(transitions.begin() + m_SelectedTransitionIndex);
	ClearSelectedControllerTransition();
}

void AnimationEditorPanel::AutoLayoutControllerGraph()
{
	if (!m_CurrentController)
		return;

	m_CurrentController->GetEntryGraphPosition() = { 22.0f, 58.0f };
	m_CurrentController->GetAnyStateGraphPosition() = { 22.0f, 160.0f };

	auto& states = m_CurrentController->GetStates();
	constexpr float nodeWidth = 174.0f;
	constexpr float columnGap = 96.0f;
	constexpr float rowGap = 50.0f;
	const int columns = std::max(1, (int)std::ceil(std::sqrt((float)states.size())));
	for (size_t i = 0; i < states.size(); ++i)
	{
		const int row = (int)i / columns;
		const int column = (int)i % columns;
		states[i].m_GraphPosition = { 250.0f + column * (nodeWidth + columnGap), 42.0f + row * (82.0f + rowGap) };
	}

	float exitColumn = 620.0f;
	for (const AnimationControllerState& state : states)
		exitColumn = std::max(exitColumn, state.m_GraphPosition.x + nodeWidth + 190.0f);
	m_CurrentController->GetExitGraphPosition() = { exitColumn, 108.0f };
	m_FrameControllerGraphRequested = true;
}

void AnimationEditorPanel::DrawCompactSummary()
{
	ImGui::BeginChild("##AnimationEditorCompactSummary", ImVec2(0.0f, 0.0f), true);
	if (m_EditorMode == AnimationEditorMode::Clip)
	{
		ImGui::TextUnformatted("Animation Clip");
		if (!m_CurrentAnimation)
		{
			ImGui::TextDisabled("No animation selected.");
			DrawAnimationDragDropArea(std::max(160.0f, ImGui::GetContentRegionAvail().x), 64.0f);
		}
		else
		{
			const auto& metadata = AssetManager::GetAssetMetadata(m_CurrentAnimation->m_Handle);
			ImGui::TextDisabled("%s", metadata ? metadata.m_Filepath.generic_string().c_str() : "Memory animation");
			ImGui::Text("Frames: %zu", m_CurrentAnimation->GetFrames().size());
			ImGui::Text("Duration: %.3fs", m_CurrentAnimation->GetDuration());
			ImGui::Text("Loop: %s", m_CurrentAnimation->IsLooping() ? "true" : "false");
		}
	}
	else
	{
		ImGui::TextUnformatted("Animation Controller");
		if (!m_CurrentController)
		{
			ImGui::TextDisabled("No controller selected.");
			DrawControllerDragDropArea(std::max(160.0f, ImGui::GetContentRegionAvail().x), 64.0f);
		}
		else
		{
			const auto& metadata = AssetManager::GetAssetMetadata(m_CurrentController->m_Handle);
			ImGui::TextDisabled("%s", metadata ? metadata.m_Filepath.generic_string().c_str() : "Memory controller");
			ImGui::Text("States: %zu", m_CurrentController->GetStates().size());
			ImGui::Text("Parameters: %zu", m_CurrentController->GetParameters().size());
			ImGui::Text("Any State transitions: %zu", m_CurrentController->GetAnyStateTransitions().size());
		}
	}
	ImGui::EndChild();
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
			if (ImGui::Selectable("Exit", selected))
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
	if (ImGui::Button("Instant"))
	{
		transition.m_Duration = 0.0f;
		transition.m_HasExitTime = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("Quick Blend"))
	{
		transition.m_Duration = 0.15f;
		transition.m_HasExitTime = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("Exit Blend"))
	{
		transition.m_Duration = 0.15f;
		transition.m_HasExitTime = true;
		transition.m_ExitTime = 0.85f;
	}

	ImGui::Separator();
	ImGui::TextDisabled("Conditions");
	if (ImGui::Button("+ Condition", ImVec2(-1.0f, 0.0f)) && !m_CurrentController->GetParameters().empty())
	{
		AnimationControllerCondition condition;
		condition.m_Parameter = m_CurrentController->GetParameters().front().m_Name;
		transition.m_Conditions.push_back(condition);
	}
	ImGui::BeginDisabled(transition.m_Conditions.empty());
	if (ImGui::Button("Clear Conditions", ImVec2(-1.0f, 0.0f)))
		transition.m_Conditions.clear();
	ImGui::EndDisabled();

	int conditionToDuplicate = -1;
	int conditionToMoveUp = -1;
	int conditionToMoveDown = -1;
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

		ImGui::BeginDisabled(conditionIndex == 0);
		if (ImGui::Button("Move Up"))
			conditionToMoveUp = (int)conditionIndex;
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(conditionIndex + 1 >= transition.m_Conditions.size());
		if (ImGui::Button("Move Down"))
			conditionToMoveDown = (int)conditionIndex;
		ImGui::EndDisabled();
		if (ImGui::Button("Duplicate Condition", ImVec2(-1.0f, 0.0f)))
			conditionToDuplicate = (int)conditionIndex;
		if (ImGui::Button("Remove Condition", ImVec2(-1.0f, 0.0f)))
		{
			transition.m_Conditions.erase(transition.m_Conditions.begin() + conditionIndex);
			ImGui::PopID();
			break;
		}
		ImGui::Separator();
		ImGui::PopID();
	}
	if (conditionToMoveUp > 0 && conditionToMoveUp < (int)transition.m_Conditions.size())
		std::swap(transition.m_Conditions[conditionToMoveUp], transition.m_Conditions[conditionToMoveUp - 1]);
	if (conditionToMoveDown >= 0 && conditionToMoveDown + 1 < (int)transition.m_Conditions.size())
		std::swap(transition.m_Conditions[conditionToMoveDown], transition.m_Conditions[conditionToMoveDown + 1]);
	if (conditionToDuplicate >= 0 && conditionToDuplicate < (int)transition.m_Conditions.size())
		transition.m_Conditions.insert(transition.m_Conditions.begin() + conditionToDuplicate + 1, transition.m_Conditions[conditionToDuplicate]);
}

void AnimationEditorPanel::DrawControllerValidation()
{
	if (!m_CurrentController)
		return;

	std::vector<std::string> issues;
	const auto& states = m_CurrentController->GetStates();
	const auto& parameters = m_CurrentController->GetParameters();

	auto stateExists = [&](std::string_view name)
		{
			return std::any_of(states.begin(), states.end(), [name](const AnimationControllerState& state)
				{
					return state.m_Name == name;
				});
		};

	auto parameterExists = [&](std::string_view name)
		{
			return std::any_of(parameters.begin(), parameters.end(), [name](const AnimationControllerParameter& parameter)
				{
					return parameter.m_Name == name;
				});
		};

	auto isAnimationClipValid = [](AssetHandle handle)
		{
			return handle != 0 && AssetManager::IsAssetHandleValid(handle) && AssetManager::GetAssetType(handle) == AssetType::Animation;
		};

	if (states.empty())
		issues.emplace_back("Controller has no states.");
	if (m_CurrentController->GetDefaultState().empty() || !stateExists(m_CurrentController->GetDefaultState()))
		issues.emplace_back("Default state is missing or invalid.");

	std::unordered_set<std::string> stateNames;
	for (const AnimationControllerState& state : states)
	{
		if (state.m_Name.empty())
			issues.emplace_back("A state has an empty name.");
		else if (!stateNames.insert(state.m_Name).second)
			issues.emplace_back("Duplicate state name: " + state.m_Name);

		if (state.m_MotionType == AnimationMotionType::Clip)
		{
			if (!isAnimationClipValid(state.m_Clip))
				issues.emplace_back("State '" + state.m_Name + "' has no valid animation clip.");
		}
		else
		{
			if (state.m_BlendParameter.empty() || !parameterExists(state.m_BlendParameter))
				issues.emplace_back("Blend state '" + state.m_Name + "' has an invalid blend parameter.");
			if (state.m_BlendChildren.empty())
				issues.emplace_back("Blend state '" + state.m_Name + "' has no child clips.");
			for (const AnimationBlendChild& child : state.m_BlendChildren)
			{
				if (!isAnimationClipValid(child.m_Clip))
					issues.emplace_back("Blend state '" + state.m_Name + "' has an invalid child clip.");
			}
		}

		std::unordered_set<std::string> transitionTargets;
		for (const AnimationControllerTransition& transition : state.m_Transitions)
		{
			if (transition.m_TargetState.empty())
				issues.emplace_back("State '" + state.m_Name + "' has a transition with no target.");
			else if (transition.m_TargetState != AnimationController::ExitStateName && !stateExists(transition.m_TargetState))
				issues.emplace_back("State '" + state.m_Name + "' targets missing state '" + transition.m_TargetState + "'.");
			if (!transition.m_TargetState.empty() && !transitionTargets.insert(transition.m_TargetState).second)
				issues.emplace_back("State '" + state.m_Name + "' has duplicate transitions to '" + transition.m_TargetState + "'.");
			for (const AnimationControllerCondition& condition : transition.m_Conditions)
			{
				if (condition.m_Parameter.empty() || !parameterExists(condition.m_Parameter))
					issues.emplace_back("Transition from '" + state.m_Name + "' has an invalid condition parameter.");
			}
		}
	}

	for (const AnimationControllerTransition& transition : m_CurrentController->GetAnyStateTransitions())
	{
		if (transition.m_TargetState.empty())
			issues.emplace_back("Any State has a transition with no target.");
		else if (transition.m_TargetState != AnimationController::ExitStateName && !stateExists(transition.m_TargetState))
			issues.emplace_back("Any State targets missing state '" + transition.m_TargetState + "'.");
		if (transition.m_Conditions.empty())
			issues.emplace_back("Any State transition to '" + transition.m_TargetState + "' has no conditions and may fire immediately.");
		for (const AnimationControllerCondition& condition : transition.m_Conditions)
		{
			if (condition.m_Parameter.empty() || !parameterExists(condition.m_Parameter))
				issues.emplace_back("Any State transition has an invalid condition parameter.");
		}
	}

	const ImGuiTreeNodeFlags flags = issues.empty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen;
	if (ImGui::TreeNodeEx("Validation", flags))
	{
		if (issues.empty())
			ImGui::TextDisabled("No issues found.");
		else
		{
			for (const std::string& issue : issues)
				ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.28f, 1.0f), "%s", issue.c_str());
		}
		ImGui::TreePop();
	}
	ImGui::Separator();
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
	DrawControllerValidation();

	if (AnimationControllerTransition* selectedTransition = GetSelectedControllerTransition())
	{
		ImGui::TextDisabled("Transition Inspector");
		ImGui::Separator();
		if (m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource)
			ImGui::TextDisabled("Source: Any State");
		else if (m_SelectedTransitionSourceStateIndex >= 0 && m_SelectedTransitionSourceStateIndex < (int)states.size())
			ImGui::TextDisabled("Source: %s", states[m_SelectedTransitionSourceStateIndex].m_Name.c_str());

		if (ImGui::Button("Duplicate Transition", ImVec2(-1.0f, 0.0f)))
		{
			const AnimationControllerTransition duplicate = *selectedTransition;
			if (m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource)
			{
				auto& transitions = m_CurrentController->GetAnyStateTransitions();
				transitions.push_back(duplicate);
				m_SelectedTransitionIndex = (int)transitions.size() - 1;
			}
			else if (m_SelectedTransitionSourceStateIndex >= 0 && m_SelectedTransitionSourceStateIndex < (int)states.size())
			{
				auto& transitions = states[m_SelectedTransitionSourceStateIndex].m_Transitions;
				transitions.push_back(duplicate);
				m_SelectedTransitionIndex = (int)transitions.size() - 1;
			}
			ImGui::EndChild();
			return;
		}
		DrawControllerTransitionInspector(*selectedTransition, true);
		if (ImGui::Button("Remove Transition", ImVec2(-1.0f, 0.0f)))
			RemoveSelectedControllerTransition();
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
		if (ImGui::Button("Sort Blend Children", ImVec2(-1.0f, 0.0f)))
		{
			std::sort(state.m_BlendChildren.begin(), state.m_BlendChildren.end(), [](const AnimationBlendChild& left, const AnimationBlendChild& right)
				{
					return left.m_Threshold < right.m_Threshold;
				});
		}

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

	int transitionToMoveUp = -1;
	int transitionToMoveDown = -1;
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
			ImGui::BeginDisabled(transitionIndex == 0);
			if (ImGui::Button("Move Up"))
				transitionToMoveUp = (int)transitionIndex;
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(transitionIndex + 1 >= state.m_Transitions.size());
			if (ImGui::Button("Move Down"))
				transitionToMoveDown = (int)transitionIndex;
			ImGui::EndDisabled();
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
	if (transitionToMoveUp > 0 && transitionToMoveUp < (int)state.m_Transitions.size())
	{
		std::swap(state.m_Transitions[transitionToMoveUp], state.m_Transitions[transitionToMoveUp - 1]);
		m_SelectedTransitionSourceStateIndex = m_SelectedControllerStateIndex;
		m_SelectedTransitionIndex = transitionToMoveUp - 1;
	}
	if (transitionToMoveDown >= 0 && transitionToMoveDown + 1 < (int)state.m_Transitions.size())
	{
		std::swap(state.m_Transitions[transitionToMoveDown], state.m_Transitions[transitionToMoveDown + 1]);
		m_SelectedTransitionSourceStateIndex = m_SelectedControllerStateIndex;
		m_SelectedTransitionIndex = transitionToMoveDown + 1;
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
	auto& anyTransitions = m_CurrentController->GetAnyStateTransitions();
	int anyTransitionToMoveUp = -1;
	int anyTransitionToMoveDown = -1;
	for (size_t transitionIndex = 0; transitionIndex < anyTransitions.size(); ++transitionIndex)
	{
		ImGui::PushID((int)transitionIndex);
		const AnimationControllerTransition& transition = anyTransitions[transitionIndex];
		std::string label = "Any -> " + (transition.m_TargetState.empty() ? std::string("None") : transition.m_TargetState);
		if (ImGui::Selectable(label.c_str(), m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource && m_SelectedTransitionIndex == (int)transitionIndex, 0, ImVec2(std::max(80.0f, width - 112.0f), 0.0f)))
		{
			m_SelectedTransitionSourceStateIndex = AnyStateTransitionSource;
			m_SelectedTransitionIndex = (int)transitionIndex;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(transitionIndex == 0);
		if (ImGui::SmallButton("Up"))
			anyTransitionToMoveUp = (int)transitionIndex;
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(transitionIndex + 1 >= anyTransitions.size());
		if (ImGui::SmallButton("Down"))
			anyTransitionToMoveDown = (int)transitionIndex;
		ImGui::EndDisabled();
		ImGui::PopID();
	}
	if (anyTransitionToMoveUp > 0 && anyTransitionToMoveUp < (int)anyTransitions.size())
	{
		std::swap(anyTransitions[anyTransitionToMoveUp], anyTransitions[anyTransitionToMoveUp - 1]);
		m_SelectedTransitionSourceStateIndex = AnyStateTransitionSource;
		m_SelectedTransitionIndex = anyTransitionToMoveUp - 1;
	}
	if (anyTransitionToMoveDown >= 0 && anyTransitionToMoveDown + 1 < (int)anyTransitions.size())
	{
		std::swap(anyTransitions[anyTransitionToMoveDown], anyTransitions[anyTransitionToMoveDown + 1]);
		m_SelectedTransitionSourceStateIndex = AnyStateTransitionSource;
		m_SelectedTransitionIndex = anyTransitionToMoveDown + 1;
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

void AnimationEditorPanel::DrawImportFramesButton(float width)
{
	if (!m_CurrentAnimation)
		return;

	if (ImGui::Button("Import Folder", ImVec2(width, 0.0f)))
		ImportTextureFolderFrames();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Import supported texture files from an asset folder as animation frames.");
}

void AnimationEditorPanel::DrawPreviewPane(float width, float height)
{
	ImGui::TextDisabled("Preview");
	ImGui::SameLine();
	ImGui::Checkbox("Onion", &m_ShowOnionSkin);
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
		std::string texturePath = "Unknown texture";
		if (AssetManager::IsAssetHandleValid(frame.m_Texture))
			texturePath = AssetManager::GetAssetMetadata(frame.m_Texture).m_Filepath.generic_string();
		ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.28f, 1.0f), "Texture is not loaded.");
		ImGui::TextWrapped("%s", texturePath.c_str());
		if (ImGui::Button("Reload Texture", ImVec2(-1.0f, 0.0f)))
			Project::GetActive()->GetEditorAssetManager()->UnloadAsset(frame.m_Texture);
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
	const ImVec2 imageMin = ImGui::GetCursorScreenPos();
	const ImVec2 imageMax(imageMin.x + previewSize.x, imageMin.y + previewSize.y);
	ImGui::InvisibleButton("##AnimationPreviewImage", previewSize);

	auto drawFrameTexture = [&](int frameIndex, const ImVec2& offset, ImU32 tint)
		{
			if (frameIndex < 0 || frameIndex >= (int)m_CurrentAnimation->GetFrames().size())
				return;

			const AssetHandle textureHandle = m_CurrentAnimation->GetFrames()[frameIndex].m_Texture;
			if (!textureHandle || !AssetManager::IsAssetHandleValid(textureHandle) || AssetManager::GetAssetType(textureHandle) != AssetType::Texture2D)
				return;

			Ref<Texture2D> onionTexture = AssetManager::GetAsset<Texture2D>(textureHandle);
			if (!onionTexture || !onionTexture->IsLoaded())
				return;

			ImGui::GetWindowDrawList()->AddImage(
				UI::ToImGuiTextureId(onionTexture->GetRendererId()),
				Add(imageMin, offset),
				Add(imageMax, offset),
				ImVec2(0, 1),
				ImVec2(1, 0),
				tint);
		};

	if (m_ShowOnionSkin)
	{
		drawFrameTexture(m_SelectedFrameIndex - 1, ImVec2(-7.0f, 0.0f), IM_COL32(120, 172, 255, 76));
		drawFrameTexture(m_SelectedFrameIndex + 1, ImVec2(7.0f, 0.0f), IM_COL32(255, 186, 104, 76));
	}
	drawFrameTexture(m_SelectedFrameIndex, ImVec2(0.0f, 0.0f), IM_COL32(255, 255, 255, 255));
	ImGui::TextDisabled("%ux%u", texture->GetWidth(), texture->GetHeight());
}

void AnimationEditorPanel::DrawFrameEditor(float width)
{
	if (!m_CurrentAnimation)
		return;

	DrawFrameBatchTools(width);

	if (m_SelectedFrameIndex < 0 || m_SelectedFrameIndex >= (int)m_CurrentAnimation->GetFrames().size())
	{
		ImGui::TextDisabled("Select a frame to edit texture and duration.");
		return;
	}

	if (ImGui::Button("Duplicate Frame"))
	{
		auto& frames = m_CurrentAnimation->GetFrames();
		frames.insert(frames.begin() + m_SelectedFrameIndex + 1, frames[m_SelectedFrameIndex]);
		m_SelectedFrameIndex++;
		StopPreview(false);
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(m_SelectedFrameIndex <= 0);
	if (ImGui::Button("Move Left"))
	{
		auto& frames = m_CurrentAnimation->GetFrames();
		std::swap(frames[m_SelectedFrameIndex], frames[m_SelectedFrameIndex - 1]);
		m_SelectedFrameIndex--;
		StopPreview(false);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(m_SelectedFrameIndex >= (int)m_CurrentAnimation->GetFrames().size() - 1);
	if (ImGui::Button("Move Right"))
	{
		auto& frames = m_CurrentAnimation->GetFrames();
		std::swap(frames[m_SelectedFrameIndex], frames[m_SelectedFrameIndex + 1]);
		m_SelectedFrameIndex++;
		StopPreview(false);
	}
	ImGui::EndDisabled();

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
		std::string textureLabel = "Drop texture";
		if (frame.m_Texture != 0)
		{
			if (AssetManager::IsAssetHandleValid(frame.m_Texture) && AssetManager::GetAssetType(frame.m_Texture) == AssetType::Texture2D)
				textureLabel = AssetManager::GetAssetMetadata(frame.m_Texture).m_Filepath.generic_string();
			else
				textureLabel = "Invalid texture handle";
		}
		UI::DragDropTarget(AssetType::Texture2D, dragDropCallback, textureLabel.c_str(), true, std::max(160.0f, width - 140.0f), 0.0f);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Duration");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1.0f);
		static constexpr float minValue = 0.0f;
		ImGui::DragScalar("##DurationSeconds", ImGuiDataType_Float, &frame.m_Duration, 0.01f, &minValue, nullptr, "%.3f s");
		if (ImGui::Button("Apply To All Frames", ImVec2(-1.0f, 0.0f)))
		{
			for (AnimationFrame& targetFrame : m_CurrentAnimation->GetFrames())
				targetFrame.m_Duration = frame.m_Duration;
		}

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

	if (ImGui::Button("Sort Keys By Time", ImVec2(-1.0f, 0.0f)))
	{
		SortKeysByTime(m_CurrentAnimation->GetEvents());
		SortKeysByTime(m_CurrentAnimation->GetTranslationKeys());
		SortKeysByTime(m_CurrentAnimation->GetRotationKeys());
		SortKeysByTime(m_CurrentAnimation->GetScaleKeys());
		SortKeysByTime(m_CurrentAnimation->GetColorKeys());
	}

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

void AnimationEditorPanel::DrawFrameBatchTools(float width)
{
	if (!m_CurrentAnimation)
		return;

	auto& frames = m_CurrentAnimation->GetFrames();
	int emptyFrameCount = 0;
	int invalidTextureCount = 0;
	int zeroDurationCount = 0;
	for (const AnimationFrame& frame : frames)
	{
		if (frame.m_Texture == 0)
			++emptyFrameCount;
		else if (!AssetManager::IsAssetHandleValid(frame.m_Texture) || AssetManager::GetAssetType(frame.m_Texture) != AssetType::Texture2D)
			++invalidTextureCount;
		if (frame.m_Duration <= 0.0f)
			++zeroDurationCount;
	}

	ImGui::TextDisabled("Clip Tools");
	ImGui::SetNextItemWidth(std::min(140.0f, std::max(80.0f, width * 0.34f)));
	ImGui::DragFloat("Default Duration", &m_DefaultFrameDuration, 0.005f, 0.001f, 10.0f, "%.3f s");

	if (emptyFrameCount > 0 || invalidTextureCount > 0 || zeroDurationCount > 0)
	{
		ImGui::TextColored(
			ImVec4(1.0f, 0.42f, 0.28f, 1.0f),
			"%d empty, %d invalid texture, %d zero duration",
			emptyFrameCount,
			invalidTextureCount,
			zeroDurationCount);
	}

	ImGui::BeginDisabled(frames.empty());
	if (ImGui::Button("Reverse Frames"))
	{
		const int oldSelectedFrame = m_SelectedFrameIndex;
		std::reverse(frames.begin(), frames.end());
		if (oldSelectedFrame >= 0 && oldSelectedFrame < (int)frames.size())
			m_SelectedFrameIndex = (int)frames.size() - 1 - oldSelectedFrame;
		StopPreview(false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove Empty Frames"))
	{
		const AssetHandle selectedTexture =
			m_SelectedFrameIndex >= 0 && m_SelectedFrameIndex < (int)frames.size() ? frames[m_SelectedFrameIndex].m_Texture : AssetHandle(0);
		std::erase_if(frames, [](const AnimationFrame& frame)
			{
				return frame.m_Texture == 0 || !AssetManager::IsAssetHandleValid(frame.m_Texture) || AssetManager::GetAssetType(frame.m_Texture) != AssetType::Texture2D;
			});

		m_SelectedFrameIndex = -1;
		if (!frames.empty())
		{
			if (selectedTexture != 0)
			{
				for (size_t frameIndex = 0; frameIndex < frames.size(); ++frameIndex)
				{
					if (frames[frameIndex].m_Texture == selectedTexture)
					{
						m_SelectedFrameIndex = (int)frameIndex;
						break;
					}
				}
			}
			if (m_SelectedFrameIndex < 0)
				m_SelectedFrameIndex = 0;
		}
		StopPreview(false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Fix Zero Durations"))
	{
		for (AnimationFrame& frame : frames)
		{
			if (frame.m_Duration <= 0.0f)
				frame.m_Duration = std::max(0.001f, m_DefaultFrameDuration);
		}
		StopPreview(false);
	}

	if (ImGui::Button("12 FPS"))
		NormalizeFrameDurations(1.0f / 12.0f);
	ImGui::SameLine();
	if (ImGui::Button("24 FPS"))
		NormalizeFrameDurations(1.0f / 24.0f);
	ImGui::SameLine();
	if (ImGui::Button("60 FPS"))
		NormalizeFrameDurations(1.0f / 60.0f);
	ImGui::SameLine();
	if (ImGui::Button("Use Default Duration"))
		NormalizeFrameDurations(m_DefaultFrameDuration);
	ImGui::EndDisabled();

	ImGui::Separator();
}

void AnimationEditorPanel::ImportTextureFolderFrames()
{
	if (!m_CurrentAnimation)
		return;

	const std::filesystem::path assetDirectory = Project::GetActiveAssetDirectory();
	const std::string folder = FileDialogs::OpenFolderUnderASpesificDirectory(assetDirectory);
	if (folder.empty())
		return;

	std::error_code error;
	if (!std::filesystem::is_directory(folder, error) || error)
		return;

	std::vector<std::filesystem::path> textureFiles;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder, error))
	{
		if (error)
			break;

		if (!entry.is_regular_file(error) || error)
		{
			error.clear();
			continue;
		}

		const std::filesystem::path& filepath = entry.path();
		if (IsTextureSourceFile(filepath))
			textureFiles.push_back(filepath);
	}

	std::sort(textureFiles.begin(), textureFiles.end(), [](const std::filesystem::path& left, const std::filesystem::path& right)
		{
			return left.filename().string() < right.filename().string();
		});

	auto editorAssetManager = Project::GetActive()->GetEditorAssetManager();
	auto& frames = m_CurrentAnimation->GetFrames();
	const int firstImportedFrame = (int)frames.size();
	int importedFrameCount = 0;

	for (const std::filesystem::path& textureFile : textureFiles)
	{
		std::error_code relativeError;
		std::filesystem::path relativePath = std::filesystem::relative(textureFile, assetDirectory, relativeError);
		if (relativeError || !IsRelativePathInsideRoot(relativePath))
			continue;

		AssetHandle textureHandle = editorAssetManager->ImportAsset(relativePath.lexically_normal());
		if (textureHandle == 0)
			continue;

		AnimationFrame frame;
		frame.m_Texture = textureHandle;
		frame.m_Duration = std::max(0.001f, m_DefaultFrameDuration);
		m_CurrentAnimation->AddFrame(frame);
		++importedFrameCount;
	}

	if (importedFrameCount > 0)
	{
		m_SelectedFrameIndex = firstImportedFrame;
		StopPreview(false);
		if (m_RefreshAssetTreeCallback)
			m_RefreshAssetTreeCallback();
	}
}

void AnimationEditorPanel::NormalizeFrameDurations(float frameDuration)
{
	if (!m_CurrentAnimation)
		return;

	const float safeDuration = std::clamp(frameDuration, 0.001f, 10.0f);
	for (AnimationFrame& frame : m_CurrentAnimation->GetFrames())
		frame.m_Duration = safeDuration;
	m_DefaultFrameDuration = safeDuration;
	StopPreview(false);
}

AnimationEditorPanel::AnimationEditorSnapshot AnimationEditorPanel::CaptureSnapshot() const
{
	AnimationEditorSnapshot snapshot;
	snapshot.m_Mode = m_EditorMode;

	if (m_CurrentAnimation)
	{
		AnimationClipSnapshot& clip = snapshot.m_Clip;
		clip.m_Valid = true;
		clip.m_Handle = m_CurrentAnimation->m_Handle;
		clip.m_Name = m_CurrentAnimation->GetName();
		clip.m_Loop = m_CurrentAnimation->IsLooping();
		clip.m_Frames = m_CurrentAnimation->GetFrames();
		clip.m_Events = m_CurrentAnimation->GetEvents();
		clip.m_TranslationKeys = m_CurrentAnimation->GetTranslationKeys();
		clip.m_RotationKeys = m_CurrentAnimation->GetRotationKeys();
		clip.m_ScaleKeys = m_CurrentAnimation->GetScaleKeys();
		clip.m_ColorKeys = m_CurrentAnimation->GetColorKeys();
		clip.m_SelectedFrameIndex = m_SelectedFrameIndex;
	}

	if (m_CurrentController)
	{
		AnimationControllerSnapshot& controller = snapshot.m_Controller;
		controller.m_Valid = true;
		controller.m_Handle = m_CurrentController->m_Handle;
		controller.m_DefaultState = m_CurrentController->GetDefaultState();
		controller.m_States = m_CurrentController->GetStates();
		controller.m_Parameters = m_CurrentController->GetParameters();
		controller.m_AnyStateTransitions = m_CurrentController->GetAnyStateTransitions();
		controller.m_EntryGraphPosition = m_CurrentController->GetEntryGraphPosition();
		controller.m_AnyStateGraphPosition = m_CurrentController->GetAnyStateGraphPosition();
		controller.m_ExitGraphPosition = m_CurrentController->GetExitGraphPosition();
		controller.m_SelectedStateIndex = m_SelectedControllerStateIndex;
		controller.m_SelectedParameterIndex = m_SelectedControllerParameterIndex;
		controller.m_SelectedTransitionSourceIndex = m_SelectedTransitionSourceStateIndex;
		controller.m_SelectedTransitionIndex = m_SelectedTransitionIndex;
	}

	return snapshot;
}

void AnimationEditorPanel::RestoreSnapshot(const AnimationEditorSnapshot& snapshot)
{
	m_EditorMode = snapshot.m_Mode;

	if (snapshot.m_Clip.m_Valid)
	{
		Ref<Animation2D> clip = nullptr;
		if (m_CurrentAnimation && m_CurrentAnimation->m_Handle == snapshot.m_Clip.m_Handle)
			clip = m_CurrentAnimation;
		else if (snapshot.m_Clip.m_Handle != 0 && AssetManager::IsAssetHandleValid(snapshot.m_Clip.m_Handle) && AssetManager::GetAssetType(snapshot.m_Clip.m_Handle) == AssetType::Animation)
			clip = AssetManager::GetAsset<Animation2D>(snapshot.m_Clip.m_Handle);

		if (clip)
		{
			clip->SetName(snapshot.m_Clip.m_Name);
			clip->SetLoop(snapshot.m_Clip.m_Loop);
			clip->GetFrames() = snapshot.m_Clip.m_Frames;
			clip->GetEvents() = snapshot.m_Clip.m_Events;
			clip->GetTranslationKeys() = snapshot.m_Clip.m_TranslationKeys;
			clip->GetRotationKeys() = snapshot.m_Clip.m_RotationKeys;
			clip->GetScaleKeys() = snapshot.m_Clip.m_ScaleKeys;
			clip->GetColorKeys() = snapshot.m_Clip.m_ColorKeys;
			m_CurrentAnimation = clip;
			if (clip->GetFrames().empty())
				m_SelectedFrameIndex = -1;
			else
				m_SelectedFrameIndex = std::clamp(snapshot.m_Clip.m_SelectedFrameIndex, 0, (int)clip->GetFrames().size() - 1);
			StopPreview(false);
		}
	}
	else if (snapshot.m_Mode == AnimationEditorMode::Clip)
	{
		m_CurrentAnimation = nullptr;
		m_SelectedFrameIndex = -1;
		StopPreview(false);
	}

	if (snapshot.m_Controller.m_Valid)
	{
		Ref<AnimationController> controller = nullptr;
		if (m_CurrentController && m_CurrentController->m_Handle == snapshot.m_Controller.m_Handle)
			controller = m_CurrentController;
		else if (snapshot.m_Controller.m_Handle != 0 && AssetManager::IsAssetHandleValid(snapshot.m_Controller.m_Handle) && AssetManager::GetAssetType(snapshot.m_Controller.m_Handle) == AssetType::AnimationController)
			controller = AssetManager::GetAsset<AnimationController>(snapshot.m_Controller.m_Handle);

		if (controller)
		{
			controller->GetStates() = snapshot.m_Controller.m_States;
			controller->GetParameters() = snapshot.m_Controller.m_Parameters;
			controller->GetAnyStateTransitions() = snapshot.m_Controller.m_AnyStateTransitions;
			controller->GetEntryGraphPosition() = snapshot.m_Controller.m_EntryGraphPosition;
			controller->GetAnyStateGraphPosition() = snapshot.m_Controller.m_AnyStateGraphPosition;
			controller->GetExitGraphPosition() = snapshot.m_Controller.m_ExitGraphPosition;
			if (!snapshot.m_Controller.m_DefaultState.empty())
				controller->SetDefaultState(snapshot.m_Controller.m_DefaultState);
			else if (!controller->GetStates().empty())
				controller->SetDefaultState(controller->GetStates().front().m_Name);

			m_CurrentController = controller;
			if (controller->GetStates().empty())
				m_SelectedControllerStateIndex = 0;
			else
				m_SelectedControllerStateIndex = std::clamp(snapshot.m_Controller.m_SelectedStateIndex, 0, (int)controller->GetStates().size() - 1);
			m_SelectedControllerParameterIndex = snapshot.m_Controller.m_SelectedParameterIndex;
			if (m_SelectedControllerParameterIndex >= (int)controller->GetParameters().size())
				m_SelectedControllerParameterIndex = -1;
			m_SelectedTransitionSourceStateIndex = snapshot.m_Controller.m_SelectedTransitionSourceIndex;
			m_SelectedTransitionIndex = snapshot.m_Controller.m_SelectedTransitionIndex;
			if (!GetSelectedControllerTransition())
				ClearSelectedControllerTransition();
		}
	}
	else if (snapshot.m_Mode == AnimationEditorMode::Controller)
	{
		m_CurrentController = nullptr;
		m_SelectedControllerStateIndex = 0;
		m_SelectedControllerParameterIndex = -1;
		ClearSelectedControllerTransition();
	}
}

void AnimationEditorPanel::PushHistory()
{
	m_UndoStack.push_back(CaptureSnapshot());
	m_RedoStack.clear();
	static constexpr size_t MaxHistoryEntries = 80;
	if (m_UndoStack.size() > MaxHistoryEntries)
		m_UndoStack.erase(m_UndoStack.begin());
}

bool AnimationEditorPanel::Undo()
{
	if (m_UndoStack.empty())
		return false;

	m_RedoStack.push_back(CaptureSnapshot());
	const AnimationEditorSnapshot snapshot = m_UndoStack.back();
	m_UndoStack.pop_back();
	RestoreSnapshot(snapshot);
	return true;
}

bool AnimationEditorPanel::Redo()
{
	if (m_RedoStack.empty())
		return false;

	m_UndoStack.push_back(CaptureSnapshot());
	const AnimationEditorSnapshot snapshot = m_RedoStack.back();
	m_RedoStack.pop_back();
	RestoreSnapshot(snapshot);
	return true;
}

bool AnimationEditorPanel::ShouldConsumeShortcutAction(UI::EditorShortcutAction action) const
{
	switch (action)
	{
	case UI::EditorShortcutAction::Undo:
	case UI::EditorShortcutAction::Redo:
	case UI::EditorShortcutAction::Copy:
	case UI::EditorShortcutAction::Paste:
	case UI::EditorShortcutAction::Cut:
	case UI::EditorShortcutAction::DuplicateEntity:
	case UI::EditorShortcutAction::DeleteEntity:
		return true;
	default:
		return false;
	}
}

bool AnimationEditorPanel::ExecuteShortcutAction(UI::EditorShortcutAction action)
{
	if (ImGui::GetIO().WantTextInput)
		return false;

	switch (action)
	{
	case UI::EditorShortcutAction::Undo:
		return Undo();
	case UI::EditorShortcutAction::Redo:
		return Redo();
	case UI::EditorShortcutAction::Copy:
		return CopySelection();
	case UI::EditorShortcutAction::Cut:
		return CutSelection();
	case UI::EditorShortcutAction::Paste:
		return PasteSelection();
	case UI::EditorShortcutAction::DuplicateEntity:
		return DuplicateSelection();
	case UI::EditorShortcutAction::DeleteEntity:
		return DeleteSelection();
	default:
		return false;
	}
}

void AnimationEditorPanel::HandleShortcutInput(const UI::UISettings& settings)
{
	if (!WantsShortcutCapture() || ImGui::GetIO().WantTextInput)
		return;

	static constexpr UI::EditorShortcutAction Actions[] =
	{
		UI::EditorShortcutAction::Undo,
		UI::EditorShortcutAction::Redo,
		UI::EditorShortcutAction::Copy,
		UI::EditorShortcutAction::Paste,
		UI::EditorShortcutAction::Cut,
		UI::EditorShortcutAction::DuplicateEntity,
		UI::EditorShortcutAction::DeleteEntity
	};

	for (UI::EditorShortcutAction action : Actions)
	{
		if (ShortcutPressed(settings, action))
		{
			ExecuteShortcutAction(action);
			return;
		}
	}
}

bool AnimationEditorPanel::CopySelection()
{
	if (m_EditorMode == AnimationEditorMode::Clip)
	{
		if (!m_CurrentAnimation || m_SelectedFrameIndex < 0 || m_SelectedFrameIndex >= (int)m_CurrentAnimation->GetFrames().size())
			return false;

		m_Clipboard = {};
		m_Clipboard.m_Type = AnimationEditorClipboardType::Frame;
		m_Clipboard.m_Frame = m_CurrentAnimation->GetFrames()[m_SelectedFrameIndex];
		return true;
	}

	if (!m_CurrentController)
		return false;

	if (AnimationControllerTransition* transition = GetSelectedControllerTransition())
	{
		m_Clipboard = {};
		m_Clipboard.m_Type = AnimationEditorClipboardType::ControllerTransition;
		m_Clipboard.m_Transition = *transition;
		return true;
	}

	auto& states = m_CurrentController->GetStates();
	if (m_SelectedControllerStateIndex < 0 || m_SelectedControllerStateIndex >= (int)states.size())
		return false;

	m_Clipboard = {};
	m_Clipboard.m_Type = AnimationEditorClipboardType::ControllerState;
	m_Clipboard.m_State = states[m_SelectedControllerStateIndex];
	return true;
}

bool AnimationEditorPanel::CutSelection()
{
	if (!CopySelection())
		return false;
	return DeleteSelection();
}

bool AnimationEditorPanel::PasteSelection()
{
	if (m_EditorMode == AnimationEditorMode::Clip && m_Clipboard.m_Type == AnimationEditorClipboardType::Frame)
	{
		if (!m_CurrentAnimation)
			return false;
		PushHistory();
		PasteFrame(m_Clipboard.m_Frame);
		return true;
	}

	if (m_EditorMode != AnimationEditorMode::Controller || !m_CurrentController)
		return false;

	if (m_Clipboard.m_Type == AnimationEditorClipboardType::ControllerState)
	{
		PushHistory();
		PasteControllerState(m_Clipboard.m_State);
		return true;
	}

	if (m_Clipboard.m_Type == AnimationEditorClipboardType::ControllerTransition)
	{
		PushHistory();
		if (PasteControllerTransition(m_Clipboard.m_Transition))
			return true;
		Undo();
		return false;
	}

	return false;
}

bool AnimationEditorPanel::DuplicateSelection()
{
	if (m_EditorMode == AnimationEditorMode::Clip)
	{
		if (!m_CurrentAnimation || m_SelectedFrameIndex < 0 || m_SelectedFrameIndex >= (int)m_CurrentAnimation->GetFrames().size())
			return false;
		PushHistory();
		return DuplicateSelectedFrame();
	}

	if (!m_CurrentController)
		return false;

	if (AnimationControllerTransition* selectedTransition = GetSelectedControllerTransition())
	{
		PushHistory();
		const AnimationControllerTransition duplicate = *selectedTransition;
		if (m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource)
		{
			auto& transitions = m_CurrentController->GetAnyStateTransitions();
			transitions.push_back(duplicate);
			m_SelectedTransitionIndex = (int)transitions.size() - 1;
			return true;
		}

		auto& states = m_CurrentController->GetStates();
		if (m_SelectedTransitionSourceStateIndex >= 0 && m_SelectedTransitionSourceStateIndex < (int)states.size())
		{
			auto& transitions = states[m_SelectedTransitionSourceStateIndex].m_Transitions;
			transitions.push_back(duplicate);
			m_SelectedTransitionIndex = (int)transitions.size() - 1;
			return true;
		}
		return false;
	}

	if (m_SelectedControllerStateIndex < 0 || m_SelectedControllerStateIndex >= (int)m_CurrentController->GetStates().size())
		return false;
	PushHistory();
	return DuplicateSelectedControllerState();
}

bool AnimationEditorPanel::DeleteSelection()
{
	if (m_EditorMode == AnimationEditorMode::Clip)
	{
		if (!m_CurrentAnimation || m_SelectedFrameIndex < 0 || m_SelectedFrameIndex >= (int)m_CurrentAnimation->GetFrames().size())
			return false;
		PushHistory();
		return DeleteSelectedFrame();
	}

	if (!m_CurrentController)
		return false;

	if (GetSelectedControllerTransition())
	{
		PushHistory();
		RemoveSelectedControllerTransition();
		return true;
	}

	if (m_SelectedControllerStateIndex >= 0 && m_SelectedControllerStateIndex < (int)m_CurrentController->GetStates().size() && m_CurrentController->GetStates().size() > 1)
	{
		PushHistory();
		return DeleteSelectedControllerState();
	}

	return false;
}

bool AnimationEditorPanel::DuplicateSelectedFrame()
{
	if (!m_CurrentAnimation || m_SelectedFrameIndex < 0 || m_SelectedFrameIndex >= (int)m_CurrentAnimation->GetFrames().size())
		return false;

	auto& frames = m_CurrentAnimation->GetFrames();
	frames.insert(frames.begin() + m_SelectedFrameIndex + 1, frames[m_SelectedFrameIndex]);
	++m_SelectedFrameIndex;
	StopPreview(false);
	return true;
}

bool AnimationEditorPanel::DeleteSelectedFrame()
{
	if (!m_CurrentAnimation || m_SelectedFrameIndex < 0 || m_SelectedFrameIndex >= (int)m_CurrentAnimation->GetFrames().size())
		return false;

	m_CurrentAnimation->RemoveFrame(m_SelectedFrameIndex);
	if (m_CurrentAnimation->GetFrames().empty())
		m_SelectedFrameIndex = -1;
	else
		m_SelectedFrameIndex = std::min(m_SelectedFrameIndex, (int)m_CurrentAnimation->GetFrames().size() - 1);
	StopPreview(false);
	return true;
}

bool AnimationEditorPanel::DuplicateSelectedControllerState()
{
	if (!m_CurrentController)
		return false;

	auto& states = m_CurrentController->GetStates();
	if (m_SelectedControllerStateIndex < 0 || m_SelectedControllerStateIndex >= (int)states.size())
		return false;

	PasteControllerState(states[m_SelectedControllerStateIndex]);
	return true;
}

bool AnimationEditorPanel::DeleteSelectedControllerState()
{
	if (!m_CurrentController || m_CurrentController->GetStates().size() <= 1)
		return false;

	auto& states = m_CurrentController->GetStates();
	if (m_SelectedControllerStateIndex < 0 || m_SelectedControllerStateIndex >= (int)states.size())
		return false;

	const std::string removedName = states[m_SelectedControllerStateIndex].m_Name;
	m_CurrentController->RemoveState(removedName);
	m_SelectedControllerStateIndex = std::clamp(m_SelectedControllerStateIndex, 0, (int)m_CurrentController->GetStates().size() - 1);
	ClearSelectedControllerTransition();
	return true;
}

void AnimationEditorPanel::PasteFrame(const AnimationFrame& frame)
{
	auto& frames = m_CurrentAnimation->GetFrames();
	const int insertIndex = m_SelectedFrameIndex >= 0 && m_SelectedFrameIndex < (int)frames.size() ? m_SelectedFrameIndex + 1 : (int)frames.size();
	frames.insert(frames.begin() + insertIndex, frame);
	m_SelectedFrameIndex = insertIndex;
	StopPreview(false);
}

void AnimationEditorPanel::PasteControllerState(const AnimationControllerState& state)
{
	AnimationControllerState& duplicate = m_CurrentController->AddState(state.m_Name + " Copy", state.m_Clip);
	const std::string uniqueName = duplicate.m_Name;
	duplicate = state;
	duplicate.m_Name = uniqueName;
	duplicate.m_GraphPosition += glm::vec2{ 44.0f, 44.0f };
	duplicate.m_Transitions.clear();
	m_SelectedControllerStateIndex = (int)m_CurrentController->GetStates().size() - 1;
	ClearSelectedControllerTransition();
}

bool AnimationEditorPanel::PasteControllerTransition(const AnimationControllerTransition& transition)
{
	if (m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource)
	{
		auto& transitions = m_CurrentController->GetAnyStateTransitions();
		transitions.push_back(transition);
		m_SelectedTransitionIndex = (int)transitions.size() - 1;
		return true;
	}

	auto& states = m_CurrentController->GetStates();
	int targetSourceIndex = m_SelectedControllerStateIndex;
	if (m_SelectedTransitionSourceStateIndex >= 0 && m_SelectedTransitionSourceStateIndex < (int)states.size())
		targetSourceIndex = m_SelectedTransitionSourceStateIndex;

	if (targetSourceIndex < 0 || targetSourceIndex >= (int)states.size())
		return false;

	auto& transitions = states[targetSourceIndex].m_Transitions;
	transitions.push_back(transition);
	m_SelectedControllerStateIndex = targetSourceIndex;
	m_SelectedTransitionSourceStateIndex = targetSourceIndex;
	m_SelectedTransitionIndex = (int)transitions.size() - 1;
	return true;
}

std::string AnimationEditorPanel::GetWindowTitle() const
{
	std::string title = "Animation Editor";
	if (m_EditorMode == AnimationEditorMode::Clip && m_CurrentAnimation)
	{
		const auto& metadata = AssetManager::GetAssetMetadata(m_CurrentAnimation->m_Handle);
		title = std::string("Animation - ") + (metadata ? metadata.m_Filepath.filename().string() : m_CurrentAnimation->GetName());
	}
	else if (m_EditorMode == AnimationEditorMode::Controller && m_CurrentController)
	{
		const auto& metadata = AssetManager::GetAssetMetadata(m_CurrentController->m_Handle);
		title = std::string("Animation Controller - ") + (metadata ? metadata.m_Filepath.filename().string() : "Controller");
	}

	title += "###AnimationEditor";
	return title;
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
