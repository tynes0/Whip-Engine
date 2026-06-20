#include <Whip-Editor/Panels/AnimationEditorPanel.h>

#include <Whip-Editor/Managers/EditorShortcutManager.h>
#include <Whip-Editor/UI/UIHelpers.h>
#include <Whip-Editor/Helpers/IconManager.h>

#include <Whip/Core/Application.h>
#include <Whip/Utils/FileExtensions.h>
#include <Whip/Utils/PlatformUtils.h>
#include <Whip/Asset/AssetManager.h>
#include <Whip/Asset/AssetUtils.h>
#include <Whip/Asset/AnimationImporter.h>
#include <Whip/Project/Project.h>
#include <Whip/Animation/AnimationController.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Whip/Math/Math.h"

_WHIP_START
	namespace
{
	constexpr int NoTransitionSource = -1;
	constexpr int EntryTransitionSource = -2;
	constexpr int AnyStateTransitionSource = -3;

	enum class WindowControlType : uint8_t
	{
		Minimize,
		Maximize,
		Restore
	};

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
		int result = std::snprintf(buffer, sizeof(buffer), "%.2f", value);
		if (result < 0 || std::cmp_greater_equal(result, sizeof(buffer)))
			WHP_EDITOR_WARN("[Animation Editor] Float formatting failed!");
		return buffer;
	}

	bool IsTextureSourceFile(const std::filesystem::path& filepath)
	{
		return Utils::TryGetAssetTypeFromFileExtension(filepath.extension()) == AssetType::Texture2D;
	}

	const TextureSpriteRect* GetAnimationFrameSpriteRect(const AnimationFrame& frame)
	{
		if (!frame.m_Texture || frame.m_TextureSpriteIndex < 0 || !AssetManager::IsAssetHandleValid(frame.m_Texture) || AssetManager::GetAssetType(frame.m_Texture) != AssetType::Texture2D)
			return nullptr;

		const AssetMetadata& metadata = AssetManager::GetAssetMetadata(frame.m_Texture);
		const auto& sprites = metadata.m_TextureSettings.m_Sprites;
		if (std::cmp_greater_equal(frame.m_TextureSpriteIndex, sprites.size()))
			return nullptr;
		return &sprites[static_cast<size_t>(frame.m_TextureSpriteIndex)];
	}

	ImVec2 GetAnimationFramePixelSize(const AnimationFrame& frame, const Ref<Texture2D>& texture)
	{
		if (const TextureSpriteRect* sprite = GetAnimationFrameSpriteRect(frame))
			return { static_cast<float>(sprite->m_Width), static_cast<float>(sprite->m_Height) };
		if (!texture)
			return { 1.0f, 1.0f };
		return { static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight()) };
	}

	void GetAnimationFrameImageUvs(const AnimationFrame& frame, const Ref<Texture2D>& texture, ImVec2& uv0, ImVec2& uv1)
	{
		uv0 = ImVec2(0.0f, 1.0f);
		uv1 = ImVec2(1.0f, 0.0f);
		if (!texture)
			return;

		const TextureSpriteRect* sprite = GetAnimationFrameSpriteRect(frame);
		if (!sprite || texture->GetWidth() == 0 || texture->GetHeight() == 0)
			return;

		const float width = static_cast<float>(texture->GetWidth());
		const float height = static_cast<float>(texture->GetHeight());
		uv0 = ImVec2(
			std::clamp(static_cast<float>(sprite->m_X) / width, 0.0f, 1.0f),
			std::clamp(1.0f - static_cast<float>(sprite->m_Y) / height, 0.0f, 1.0f));
		uv1 = ImVec2(
			std::clamp(static_cast<float>(sprite->m_X + sprite->m_Width) / width, 0.0f, 1.0f),
			std::clamp(1.0f - static_cast<float>(sprite->m_Y + sprite->m_Height) / height, 0.0f, 1.0f));
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

	ImVec2 DefaultAnimationWindowSize()
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		const ImVec2 maxSize(
			std::min(1280.0f, viewport->WorkSize.x - 40.0f),
			std::min(720.0f, viewport->WorkSize.y - 40.0f));
		return {
			std::clamp(1120.0f, 520.0f, std::max(520.0f, maxSize.x)),
			std::clamp(680.0f, 320.0f, std::max(320.0f, maxSize.y))
		};
	}

	bool DrawWindowControl(const char* id, WindowControlType type)
	{
		constexpr ImVec2 size(28.0f, 22.0f);
		ImGui::InvisibleButton(id, size);
		const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		const bool hovered = ImGui::IsItemHovered();
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(min, max, IM_COL32(255, 255, 255, hovered ? 24 : 0), 3.0f);
		constexpr ImU32 color = IM_COL32(226, 226, 226, 230);
		const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);

		if (type == WindowControlType::Minimize)
		{
			drawList->AddLine(ImVec2(center.x - 5.0f, center.y + 4.0f), ImVec2(center.x + 5.0f, center.y + 4.0f), color, 1.4f);
		}
		else if (type == WindowControlType::Maximize)
		{
			drawList->AddRect(ImVec2(center.x - 5.0f, center.y - 5.0f), ImVec2(center.x + 5.0f, center.y + 5.0f), color, 0.0f, 0, 1.2f);
		}
		else
		{
			drawList->AddRect(ImVec2(center.x - 3.0f, center.y - 6.0f), ImVec2(center.x + 7.0f, center.y + 4.0f), color, 0.0f, 0, 1.1f);
			drawList->AddRect(ImVec2(center.x - 7.0f, center.y - 2.0f), ImVec2(center.x + 3.0f, center.y + 8.0f), color, 0.0f, 0, 1.1f);
		}

		return clicked;
	}

	bool TitlebarDragStarted()
	{
		const ImGuiWindow* window = ImGui::GetCurrentWindowRead();
		if (!window || !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
			return false;

		const ImVec2 click = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left];
		const float titlebarBottom = window->Pos.y + ImGui::GetFrameHeight();
		return click.y >= window->Pos.y && click.y <= titlebarBottom;
	}
}

AnimationEditorPanel::AnimationEditorPanel()
	: EditorPanel("Animation Editor", false)
{
}

AnimationEditorPanel::~AnimationEditorPanel() = default;

void AnimationEditorPanel::OnImGuiRender()
{
	if (!m_Open)
	{
		m_ShortcutContextActive = false;
		return;
	}
	if (m_Minimized)
	{
		m_ShortcutContextActive = false;
		DrawMinimizedStrip();
		return;
	}

	bool open = m_Open;
	if (m_FullscreenRequested)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
		ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
		m_FullscreenRequested = false;
	}
	else if (m_Fullscreen)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
	}
	if (m_FocusRequested)
	{
		ImGui::SetNextWindowFocus();
		m_FocusRequested = false;
	}
	if (!m_Fullscreen)
		ImGui::SetNextWindowSize(DefaultAnimationWindowSize(), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 260.0f), ImVec2(FLT_MAX, FLT_MAX));
	const std::string windowTitle = GetWindowTitle();
	ImGui::Begin(windowTitle.c_str(), &open);
	m_ShortcutContextActive = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_DockHierarchy);
	if (open != m_Open)
		SetOpen(open);
	if (m_Fullscreen && TitlebarDragStarted())
		RestoreWindowRect();
	else if (!m_Fullscreen && !ImGui::IsWindowDocked())
		CaptureWindowRect();
	UpdatePreview();
	DrawEditorContent(true);
	ImGui::End();
}

void AnimationEditorPanel::OnImGuiRenderEmbedded()
{
	m_Minimized = false;
	m_Fullscreen = false;
	m_FullscreenRequested = false;
	m_ShortcutContextActive = m_ShortcutContextActive || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_DockHierarchy);
	UpdatePreview();
	DrawEditorContent(false);
}

void AnimationEditorPanel::DrawEditorContent(bool showWindowControls)
{
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
	if (showWindowControls)
		DrawWindowControls();
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
			if (const auto& metadata = AssetManager::GetAssetMetadata(m_CurrentAnimation->m_Handle); metadata)
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
			char buffer[256]{};
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
}

void AnimationEditorPanel::SetOpen(bool open)
{
	if (m_Open == open)
		return;
	m_Open = open;
	m_OpenDirty = true;
	MarkLayoutDirty();
}

bool AnimationEditorPanel::OpenAsset(AssetHandle handle, bool openWindow)
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
		if (openWindow)
		{
			SetOpen(true);
			m_FocusRequested = true;
		}
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
		if (openWindow)
		{
			SetOpen(true);
			m_FocusRequested = true;
		}
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

bool AnimationEditorPanel::ConsumeLayoutDirty()
{
	const bool dirty = m_LayoutDirty;
	m_LayoutDirty = false;
	return dirty;
}

AnimationEditorPanel::WorkspacePreferences AnimationEditorPanel::GetWorkspacePreferences() const
{
	WorkspacePreferences preferences;
	preferences.m_Open = m_Open;
	preferences.m_Minimized = m_Minimized;
	preferences.m_Fullscreen = m_Fullscreen;
	preferences.m_HasRestoreRect = m_HasRestoreRect;
	preferences.m_RestorePosition = m_RestorePosition;
	preferences.m_RestoreSize = m_RestoreSize;
	return preferences;
}

void AnimationEditorPanel::ApplyWorkspacePreferences(const WorkspacePreferences& preferences)
{
	m_Open = preferences.m_Open;
	m_Minimized = preferences.m_Minimized;
	m_Fullscreen = preferences.m_Fullscreen;
	m_FullscreenRequested = preferences.m_Fullscreen;
	m_HasRestoreRect = preferences.m_HasRestoreRect;
	m_RestorePosition = preferences.m_RestorePosition;
	m_RestoreSize = preferences.m_RestoreSize;
	m_LayoutDirty = false;
}

void AnimationEditorPanel::DrawWindowControls()
{
	const float controlsWidth = 28.0f * 2.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
	if (ImGui::GetContentRegionAvail().x > controlsWidth)
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - controlsWidth);
	else
		ImGui::SameLine();

	if (DrawWindowControl("##AnimationEditorMinimize", WindowControlType::Minimize))
	{
		m_Minimized = true;
		m_Fullscreen = false;
		m_OpenDirty = true;
		MarkLayoutDirty();
	}
	ImGui::SameLine();
	if (DrawWindowControl("##AnimationEditorMaximize", m_Fullscreen ? WindowControlType::Restore : WindowControlType::Maximize))
	{
		if (m_Fullscreen)
			RestoreWindowRect();
		else
			RequestFullscreen();
	}
}

void AnimationEditorPanel::DrawMinimizedStrip()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	constexpr float stripHeight = 42.0f;
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 10.0f, viewport->WorkPos.y + viewport->WorkSize.y - stripHeight - 58.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(240.0f, stripHeight), ImGuiCond_Always);
	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;

	if (!ImGui::Begin("##MinimizedAnimationEditor", nullptr, flags))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Animation Editor###RestoreAnimationEditor", ImVec2(220.0f, 24.0f)))
	{
		m_Minimized = false;
		m_FocusRequested = true;
		MarkLayoutDirty();
	}

	ImGui::End();
}

void AnimationEditorPanel::CaptureWindowRect()
{
	const ImVec2 pos = ImGui::GetWindowPos();
	const ImVec2 size = ImGui::GetWindowSize();
	if (size.x <= 0.0f || size.y <= 0.0f)
		return;

	const glm::vec2 newPosition{ pos.x, pos.y };
	const glm::vec2 newSize{ size.x, size.y };
	if (!m_HasRestoreRect ||
		std::abs(m_RestorePosition.x - newPosition.x) > 0.5f ||
		std::abs(m_RestorePosition.y - newPosition.y) > 0.5f ||
		std::abs(m_RestoreSize.x - newSize.x) > 0.5f ||
		std::abs(m_RestoreSize.y - newSize.y) > 0.5f)
	{
		m_RestorePosition = newPosition;
		m_RestoreSize = newSize;
		m_HasRestoreRect = true;
		MarkLayoutDirty();
	}
}

void AnimationEditorPanel::RequestFullscreen()
{
	CaptureWindowRect();
	m_Minimized = false;
	m_Fullscreen = true;
	m_FullscreenRequested = true;
	m_FocusRequested = true;
	MarkLayoutDirty();
}

void AnimationEditorPanel::RestoreWindowRect()
{
	m_Fullscreen = false;
	m_FullscreenRequested = false;
	const ImVec2 size(
		m_HasRestoreRect ? m_RestoreSize.x : 1120.0f,
		m_HasRestoreRect ? m_RestoreSize.y : 680.0f);
	const ImVec2 pos(
		m_HasRestoreRect ? m_RestorePosition.x : ImGui::GetMainViewport()->WorkPos.x + 80.0f,
		m_HasRestoreRect ? m_RestorePosition.y : ImGui::GetMainViewport()->WorkPos.y + 80.0f);
	ImGui::SetWindowPos(pos, ImGuiCond_Always);
	ImGui::SetWindowSize(size, ImGuiCond_Always);
	MarkLayoutDirty();
}

void AnimationEditorPanel::MarkLayoutDirty()
{
	m_LayoutDirty = true;
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
				constexpr float iconSize = 17.0f;
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
			if (m_SelectedFrameIndex < 0 || std::cmp_greater_equal(m_SelectedFrameIndex, m_CurrentAnimation->GetFrames().size()))
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
		if (const auto& metadata = AssetManager::GetAssetMetadata(m_CurrentAnimation->m_Handle); metadata)
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
	char buffer[256]{};
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

	m_SelectedControllerStateIndex = std::clamp(m_SelectedControllerStateIndex, 0, static_cast<int>(m_CurrentController->GetStates().size()) - 1);

	const float availableWidth = ImGui::GetContentRegionAvail().x;
	const float parameterWidth = std::min(270.0f, availableWidth * 0.28f);
	const float inspectorWidth = std::min(360.0f, availableWidth * 0.34f);
	const float graphWidth = std::max(260.0f, availableWidth - parameterWidth - inspectorWidth - ImGui::GetStyle().ItemSpacing.x * 2.0f);

	DrawControllerParameters(parameterWidth, height);
	ImGui::SameLine();
	ImGui::BeginChild("##ControllerWorkspace", ImVec2(graphWidth, height), false);
	if (ImGui::BeginTabBar("##ControllerWorkspaceTabs"))
	{
		const ImGuiTabItemFlags stateMachineFlags = m_ControllerWorkspaceTabSelectionRequested && m_ControllerWorkspaceTab == AnimationControllerWorkspaceTab::StateMachine ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
		if (ImGui::BeginTabItem("State Machine", nullptr, stateMachineFlags))
		{
			m_ControllerWorkspaceTab = AnimationControllerWorkspaceTab::StateMachine;
			DrawControllerGraph(ImGui::GetContentRegionAvail().x, std::max(220.0f, ImGui::GetContentRegionAvail().y));
			ImGui::EndTabItem();
		}

		const ImGuiTabItemFlags transitionBlueprintFlags = m_ControllerWorkspaceTabSelectionRequested && m_ControllerWorkspaceTab == AnimationControllerWorkspaceTab::TransitionBlueprint ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
		if (ImGui::BeginTabItem("Transition Blueprint", nullptr, transitionBlueprintFlags))
		{
			m_ControllerWorkspaceTab = AnimationControllerWorkspaceTab::TransitionBlueprint;
			if (AnimationControllerTransition* selectedTransition = GetSelectedControllerTransition())
				DrawTransitionConditionGraph(*selectedTransition, std::max(220.0f, ImGui::GetContentRegionAvail().y));
			else
			{
				ImGui::BeginChild("##TransitionBlueprintEmpty", ImVec2(0.0f, std::max(220.0f, ImGui::GetContentRegionAvail().y)), true);
				ImGui::TextDisabled("Select a transition in the State Machine tab to edit its blueprint.");
				ImGui::EndChild();
			}
			ImGui::EndTabItem();
		}
		m_ControllerWorkspaceTabSelectionRequested = false;
		ImGui::EndTabBar();
	}
	ImGui::EndChild();
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
			m_SelectedControllerParameterIndex = static_cast<int>(m_CurrentController->GetParameters().size()) - 1;
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
		ImGui::PushID(static_cast<int>(i));
		const bool selected = std::cmp_equal(m_SelectedControllerParameterIndex, i);
		if (ImGui::Selectable(parameters[i].m_Name.c_str(), selected))
			m_SelectedControllerParameterIndex = static_cast<int>(i);
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			const int parameterIndex = static_cast<int>(i);
			ImGui::SetDragDropPayload("ANIMATION_PARAMETER", &parameterIndex, sizeof(parameterIndex));
			ImGui::Text("%s", parameters[i].m_Name.c_str());
			ImGui::TextDisabled("%s", frenum::to_string(parameters[i].m_Type).data());
			ImGui::EndDragDropSource();
		}
		ImGui::PopID();
	}

	if (m_SelectedControllerParameterIndex >= 0 && std::cmp_less(m_SelectedControllerParameterIndex, parameters.size()))
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
						for (AnimationControllerBlueprintNode& node : transition.m_BlueprintNodes)
						{
							if (node.m_Parameter == oldName)
								node.m_Parameter = parameter.m_Name;
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
					for (AnimationControllerBlueprintNode& node : transition.m_BlueprintNodes)
					{
						if (node.m_Parameter == oldName)
							node.m_Parameter = parameter.m_Name;
					}
				}
			}
		}

		constexpr std::array<AnimationParameterType, 4> parameterTypes =
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
			auto removeBlueprintReferences = [&removedName](AnimationControllerTransition& transition)
				{
					std::unordered_set<uint32_t> removedNodes;
					std::erase_if(transition.m_BlueprintNodes, [&removedName, &removedNodes](const AnimationControllerBlueprintNode& node)
						{
							if (node.m_Parameter != removedName)
								return false;
							removedNodes.insert(node.m_Id);
							return true;
						});
					std::erase_if(transition.m_BlueprintLinks, [&removedNodes](const AnimationControllerBlueprintLink& link)
						{
							return removedNodes.contains(link.m_OutputNode) || removedNodes.contains(link.m_InputNode);
						});
				};

			for (AnimationControllerState& state : m_CurrentController->GetStates())
			{
				for (AnimationControllerTransition& transition : state.m_Transitions)
				{
					std::erase_if(transition.m_Conditions, [&removedName](const AnimationControllerCondition& condition)
						{
							return condition.m_Parameter == removedName;
						});
					removeBlueprintReferences(transition);
				}
			}
			for (AnimationControllerTransition& transition : m_CurrentController->GetAnyStateTransitions())
			{
				std::erase_if(transition.m_Conditions, [&removedName](const AnimationControllerCondition& condition)
					{
						return condition.m_Parameter == removedName;
					});
				removeBlueprintReferences(transition);
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
	auto& states = m_CurrentController->GetStates();
	if (states.empty())
		m_SelectedControllerStateIndex = -1;
	else
		m_SelectedControllerStateIndex = std::clamp(m_SelectedControllerStateIndex, 0, (int)static_cast<int>(states.size()) - 1);
	const bool hasSelectedState = m_SelectedControllerStateIndex >= 0 && std::cmp_less(m_SelectedControllerStateIndex, states.size());

	ImGui::TextDisabled("Controller Graph");
	ImGui::SameLine();
	if (ImGui::Button("+ State"))
	{
		AnimationControllerState& state = m_CurrentController->AddState("State");
		m_SelectedControllerStateIndex = static_cast<int>(states.size()) - 1;
		if (m_CurrentController->GetDefaultState().empty())
			m_CurrentController->SetDefaultState(state.m_Name);
		ClearSelectedControllerTransition();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(states.size() <= 1 || !hasSelectedState);
	if (ImGui::Button("Remove"))
	{
		const std::string removedName = states[m_SelectedControllerStateIndex].m_Name;
		m_CurrentController->RemoveState(removedName);
		m_SelectedControllerStateIndex = m_CurrentController->GetStates().empty() ? -1 : std::clamp(m_SelectedControllerStateIndex, 0, static_cast<int>(m_CurrentController->GetStates().size()) - 1);
		ClearSelectedControllerTransition();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!hasSelectedState);
	if (ImGui::Button("Duplicate"))
	{
		const AnimationControllerState sourceState = states[m_SelectedControllerStateIndex];
		AnimationControllerState& duplicate = m_CurrentController->AddState(sourceState.m_Name + " Copy", sourceState.m_Clip);
		const std::string uniqueName = duplicate.m_Name;
		duplicate = sourceState;
		duplicate.m_Name = uniqueName;
		duplicate.m_GraphPosition += glm::vec2{ 44.0f, 44.0f };
		duplicate.m_Transitions.clear();
		m_SelectedControllerStateIndex = static_cast<int>(m_CurrentController->GetStates().size()) - 1;
		ClearSelectedControllerTransition();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!hasSelectedState);
	if (ImGui::Button("Set Default"))
	{
		m_CurrentController->SetDefaultState(states[m_SelectedControllerStateIndex].m_Name);
		ClearSelectedControllerTransition();
	}
	ImGui::EndDisabled();
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

	const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(8, 12, 16, 255), 4.0f);
	drawList->AddRect(canvasMin, canvasMax, IM_COL32(52, 66, 80, 170), 4.0f);

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
	if (canvasHovered && ImGui::GetIO().MouseWheel != 0.0f)
	{
		const float oldZoom = m_ControllerGraphZoom;
		const float newZoom = std::clamp(oldZoom + ImGui::GetIO().MouseWheel * 0.08f, 0.45f, 2.0f);
		if (!Math::EqualF(newZoom, oldZoom))
		{
			const glm::vec2 focusWorld
			{
				(mousePos.x - canvasMin.x - m_ControllerGraphPan.x) / oldZoom,
				(mousePos.y - canvasMin.y - m_ControllerGraphPan.y) / oldZoom
			};
			m_ControllerGraphZoom = newZoom;
			m_ControllerGraphPan =
			{
				mousePos.x - canvasMin.x - focusWorld.x * newZoom,
				mousePos.y - canvasMin.y - focusWorld.y * newZoom
			};
		}
	}

	constexpr float nodeWidth = 174.0f;
	constexpr float nodeHeight = 82.0f;
	constexpr float specialWidth = 136.0f;
	constexpr float specialHeight = 58.0f;
	float zoom = m_ControllerGraphZoom;
	auto snapPosition = [&](glm::vec2& position)
		{
			if (!m_ControllerGraphSnapToGrid)
				return;
			constexpr float grid = 16.0f;
			position.x = std::round(position.x / grid) * grid;
			position.y = std::round(position.y / grid) * grid;
		};

	const int columns = std::max(1, static_cast<int>((canvasSize.x / std::max(zoom, 0.1f) - 280.0f) / 230.0f));
	for (size_t i = 0; i < states.size(); ++i)
	{
		const int row = static_cast<int>(i) / columns;
		const int column = static_cast<int>(i) % columns;
		if (states[i].m_GraphPosition.x == 0.0f && states[i].m_GraphPosition.y == 0.0f)
			states[i].m_GraphPosition = { 240.0f + static_cast<float>(column) * 230.0f, 42.0f + static_cast<float>(row) * 126.0f };
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
	constexpr ImU32 gridColor = IM_COL32(46, 57, 68, 82);

	const float startX = std::fmod(m_ControllerGraphPan.x, gridStep);
	for (int i = 0; startX + (static_cast<float>(i) * gridStep) < canvasSize.x; ++i)
	{
		float x = startX + (static_cast<float>(i) * gridStep);
		drawList->AddLine(ImVec2(canvasMin.x + x, canvasMin.y), ImVec2(canvasMin.x + x, canvasMax.y), gridColor);
	}

	const float startY = std::fmod(m_ControllerGraphPan.y, gridStep);
	for (int i = 0; startY + (static_cast<float>(i) * gridStep) < canvasSize.y; ++i)
	{
		float y = startY + (static_cast<float>(i) * gridStep);
		drawList->AddLine(ImVec2(canvasMin.x, canvasMin.y + y), ImVec2(canvasMax.x, canvasMin.y + y), gridColor);
	}
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
					return static_cast<int>(i);
			}
			return -1;
		};

	auto selectTransition = [&](int sourceStateIndex, int transitionIndex)
		{
			m_SelectedTransitionSourceStateIndex = sourceStateIndex;
			m_SelectedTransitionIndex = transitionIndex;
			m_SelectedBlueprintNodeId = 0;
			m_PendingBlueprintLinkNodeId = 0;
			m_PendingBlueprintLinkFromInput = false;
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
			if (!targetExit && (targetStateIndex < 0 || std::cmp_greater_equal(targetStateIndex, states.size())))
			{
				clearPendingConnection();
				return;
			}

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
				const bool alreadyExists = std::ranges::any_of(transitions, [&target](const AnimationControllerTransition& existingTransition)
				{
					return existingTransition.m_TargetState == target;
				});
				if (alreadyExists)
				{
					clearPendingConnection();
					return;
				}
				transitions.push_back(transition);
				selectTransition(AnyStateTransitionSource, static_cast<int>(transitions.size()) - 1);
				clearPendingConnection();
				return;
			}

			if (m_PendingTransitionSourceStateIndex >= 0 && std::cmp_less(m_PendingTransitionSourceStateIndex, states.size()))
			{
				if (!targetExit && m_PendingTransitionSourceStateIndex == targetStateIndex)
				{
					clearPendingConnection();
					return;
				}
				auto& transitions = states[m_PendingTransitionSourceStateIndex].m_Transitions;
				const bool alreadyExists = std::ranges::any_of(transitions, [&target](const AnimationControllerTransition& existingTransition)
				{
					return existingTransition.m_TargetState == target;
				});
				if (alreadyExists)
				{
					clearPendingConnection();
					return;
				}
				transitions.push_back(transition);
				selectTransition(m_PendingTransitionSourceStateIndex, static_cast<int>(transitions.size()) - 1);
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
			targetPin = stateInputPin(static_cast<size_t>(targetIndex));
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
			const ImU32 color = selected ? IM_COL32(122, 196, 255, 255) : hovered ? IM_COL32(188, 210, 232, 255) : IM_COL32(92, 126, 160, 220);
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
			if (!transition.m_BlueprintNodes.empty())
			{
				const size_t logicNodeCount = std::ranges::count_if(transition.m_BlueprintNodes, [](const AnimationControllerBlueprintNode& node)
				{
					return node.m_Type != AnimationBlueprintNodeType::Parameter && node.m_Type != AnimationBlueprintNodeType::Reroute && node.m_Type != AnimationBlueprintNodeType::Result;
				});
				if (logicNodeCount > 0)
					badge += std::to_string(logicNodeCount) + " logic";
			}
			else if (!transition.m_Conditions.empty())
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
				drawList->AddRectFilled(badgeMin, badgeMax, IM_COL32(14, 20, 27, 235), 4.0f);
				drawList->AddRect(badgeMin, badgeMax, color, 4.0f);
				drawList->AddText(ImVec2(badgeMin.x + 6.0f, badgeMin.y + 3.0f), IM_COL32(225, 233, 240, 255), badge.c_str());
			}

			if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				selectTransition(sourceStateIndex, transitionIndex);
				m_ControllerWorkspaceTab = AnimationControllerWorkspaceTab::TransitionBlueprint;
				m_ControllerWorkspaceTabSelectionRequested = true;
			}
			else if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				selectTransition(sourceStateIndex, transitionIndex);
		};

	drawList->PushClipRect(canvasMin, canvasMax, true);
	const int defaultStateIndex = findStateIndex(m_CurrentController->GetDefaultState());
	if (defaultStateIndex >= 0)
	{
		const ImVec2 targetPin = stateInputPin(static_cast<size_t>(defaultStateIndex));
		const float tangent = std::max(54.0f * zoom, std::abs(targetPin.x - entryOutput.x) * 0.36f);
		drawList->AddBezierCubic(entryOutput, ImVec2(entryOutput.x + tangent, entryOutput.y), ImVec2(targetPin.x - tangent, targetPin.y), targetPin, IM_COL32(92, 168, 236, 235), 2.4f);
		drawList->AddCircleFilled(targetPin, pinRadius, IM_COL32(92, 168, 236, 235));
	}

	const auto& anyStateTransitions = m_CurrentController->GetAnyStateTransitions();
	for (size_t transitionIndex = 0; transitionIndex < anyStateTransitions.size(); ++transitionIndex)
		drawTransition(AnyStateTransitionSource, static_cast<int>(transitionIndex), anyStateOutput, anyStateTransitions[transitionIndex]);

	for (size_t stateIndex = 0; stateIndex < states.size(); ++stateIndex)
	{
		const ImVec2 sourcePin = stateOutputPin(stateIndex);
		for (size_t transitionIndex = 0; transitionIndex < states[stateIndex].m_Transitions.size(); ++transitionIndex)
			drawTransition(static_cast<int>(stateIndex), static_cast<int>(transitionIndex), sourcePin, states[stateIndex].m_Transitions[transitionIndex]);
	}

	const float pinHitSize = std::max(26.0f, 26.0f * zoom);
	int hoveredConnectionTargetStateIndex = -1;
	bool hoveredConnectionTargetExit = false;
	auto isConnectionPending = [&]()
		{
			return m_PendingTransitionSourceStateIndex != NoTransitionSource;
		};

	auto drawSpecialNode = [&](const char* label, glm::vec2& worldPosition, ImU32 fill, ImU32 border, int sourceIndex, bool hasInput)
		{
			const ImVec2 nodeMin = worldToScreen(worldPosition);
			const ImVec2 nodeMax(nodeMin.x + specialSize.x, nodeMin.y + specialSize.y);
			drawList->AddRectFilled(nodeMin, nodeMax, fill, 6.0f);
			drawList->AddRect(nodeMin, nodeMax, border, 6.0f, 0, 1.6f);
			drawList->AddText(ImVec2(nodeMin.x + 12.0f * zoom, nodeMin.y + 18.0f * zoom), IM_COL32(225, 233, 240, 255), label);

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
				drawList->AddCircleFilled(outPin, pinRadius, IM_COL32(150, 170, 190, 255));
				ImGui::SetCursorScreenPos(ImVec2(outPin.x - pinHitSize * 0.5f, outPin.y - pinHitSize * 0.5f));
				ImGui::PushID(sourceIndex);
				ImGui::InvisibleButton("##SpecialOutPin", ImVec2(pinHitSize, pinHitSize), ImGuiButtonFlags_AllowOverlap);
				if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
					m_PendingTransitionSourceStateIndex = sourceIndex;
				ImGui::PopID();
			}

			if (hasInput)
			{
				const ImVec2 inPin(nodeMin.x, nodeMin.y + specialSize.y * 0.5f);
				const ImRect inputHitRect(ImVec2(nodeMin.x - pinHitSize * 0.5f, nodeMin.y), ImVec2(nodeMin.x + std::min(48.0f * zoom, specialSize.x), nodeMax.y));
				const bool inputHovered = isConnectionPending() && inputHitRect.Contains(mousePos);
				if (inputHovered)
					hoveredConnectionTargetExit = true;
				drawList->AddCircleFilled(inPin, inputHovered ? pinRadius + 2.0f : pinRadius, inputHovered ? IM_COL32(122, 196, 255, 255) : IM_COL32(94, 184, 178, 255));
				ImGui::SetCursorScreenPos(ImVec2(inPin.x - pinHitSize * 0.5f, inPin.y - pinHitSize * 0.5f));
				ImGui::PushID("ExitInput");
				ImGui::InvisibleButton("##SpecialInPin", ImVec2(pinHitSize, pinHitSize), ImGuiButtonFlags_AllowOverlap);
				if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
					createConnection(-1, true);
				ImGui::PopID();
			}
		};

	drawSpecialNode("Entry", entryPosition, IM_COL32(24, 45, 62, 255), IM_COL32(92, 168, 236, 235), EntryTransitionSource, false);
	drawSpecialNode("Any State", anyStatePosition, IM_COL32(24, 47, 45, 255), IM_COL32(94, 184, 178, 235), AnyStateTransitionSource, false);
	drawSpecialNode("Exit", exitPosition, IM_COL32(48, 38, 52, 255), IM_COL32(178, 126, 170, 235), NoTransitionSource, true);

	int stateToRemove = -1;
	int stateToDuplicate = -1;
	for (size_t i = 0; i < states.size(); ++i)
	{
		AnimationControllerState& state = states[i];
		const ImVec2 nodeMin = worldToScreen(state.m_GraphPosition);
		const ImVec2 nodeMax(nodeMin.x + nodeSize.x, nodeMin.y + nodeSize.y);
		const bool selected = std::cmp_equal(m_SelectedControllerStateIndex, i) && m_SelectedTransitionIndex < 0;
		const bool isDefault = state.m_Name == m_CurrentController->GetDefaultState();
		const ImU32 fill = selected ? IM_COL32(38, 55, 72, 255) : isDefault ? IM_COL32(28, 42, 56, 255) : IM_COL32(24, 32, 41, 255);
		const ImU32 border = isDefault ? IM_COL32(92, 168, 236, 245) : selected ? IM_COL32(188, 210, 232, 230) : IM_COL32(68, 84, 100, 190);
		drawList->AddRectFilled(nodeMin, nodeMax, fill, 6.0f);
		drawList->AddRect(nodeMin, nodeMax, border, 6.0f, 0, isDefault ? 2.2f : 1.2f);
		drawList->AddText(ImVec2(nodeMin.x + 12.0f * zoom, nodeMin.y + 10.0f * zoom), IM_COL32(225, 233, 240, 255), state.m_Name.c_str());

		std::string clipLabel = state.m_MotionType == AnimationMotionType::BlendTree1D ? "Blend Tree 1D" : "No Clip";
		if (state.m_MotionType == AnimationMotionType::Clip && state.m_Clip != 0 && AssetManager::IsAssetHandleValid(state.m_Clip) && AssetManager::GetAssetType(state.m_Clip) == AssetType::Animation)
			clipLabel = AssetManager::GetAssetMetadata(state.m_Clip).m_Filepath.filename().string();
		drawList->AddText(ImVec2(nodeMin.x + 12.0f * zoom, nodeMin.y + 36.0f * zoom), IM_COL32(145, 158, 172, 255), clipLabel.c_str());

		if (isDefault)
			drawList->AddText(ImVec2(nodeMin.x + 12.0f * zoom, nodeMin.y + 58.0f * zoom), IM_COL32(106, 182, 248, 255), "Default");

		const float pinGutter = 16.0f * zoom;
		ImGui::SetCursorScreenPos(ImVec2(nodeMin.x + pinGutter, nodeMin.y));
		ImGui::PushID(static_cast<int>(i));
		ImGui::InvisibleButton("##ControllerStateNode", ImVec2(std::max(24.0f, nodeSize.x - pinGutter * 2.0f), nodeSize.y));
		if (ImGui::IsItemClicked())
		{
			m_SelectedControllerStateIndex = static_cast<int>(i);
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
				stateToDuplicate = static_cast<int>(i);
			ImGui::BeginDisabled(states.size() <= 1);
			if (ImGui::MenuItem("Remove"))
				stateToRemove = static_cast<int>(i);
			ImGui::EndDisabled();
			ImGui::EndPopup();
		}

		const ImVec2 inputPin = stateInputPin(i);
		const ImRect inputHitRect(ImVec2(nodeMin.x - pinHitSize * 0.5f, nodeMin.y), ImVec2(nodeMin.x + std::min(54.0f * zoom, nodeSize.x), nodeMax.y));
		const bool inputHovered = isConnectionPending() && inputHitRect.Contains(mousePos);
		if (inputHovered)
			hoveredConnectionTargetStateIndex = static_cast<int>(i);
		drawList->AddCircleFilled(inputPin, inputHovered ? pinRadius + 2.0f : pinRadius, inputHovered ? IM_COL32(122, 196, 255, 255) : IM_COL32(94, 184, 178, 255));
		ImGui::SetCursorScreenPos(ImVec2(inputPin.x - pinHitSize * 0.5f, inputPin.y - pinHitSize * 0.5f));
		ImGui::InvisibleButton("##StateInPin", ImVec2(pinHitSize, pinHitSize), ImGuiButtonFlags_AllowOverlap);
		if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			createConnection(static_cast<int>(i), false);

		const ImVec2 outputPin = stateOutputPin(i);
		drawList->AddCircleFilled(outputPin, pinRadius, IM_COL32(150, 170, 190, 255));
		ImGui::SetCursorScreenPos(ImVec2(outputPin.x - pinHitSize * 0.5f, outputPin.y - pinHitSize * 0.5f));
		ImGui::InvisibleButton("##StateOutPin", ImVec2(pinHitSize, pinHitSize), ImGuiButtonFlags_AllowOverlap);
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			m_PendingTransitionSourceStateIndex = static_cast<int>(i);
		ImGui::PopID();
	}

	if (stateToDuplicate >= 0 && std::cmp_less(stateToDuplicate, states.size()))
	{
		const AnimationControllerState sourceState = states[stateToDuplicate];
		AnimationControllerState& duplicate = m_CurrentController->AddState(sourceState.m_Name + " Copy", sourceState.m_Clip);
		const std::string uniqueName = duplicate.m_Name;
		duplicate = sourceState;
		duplicate.m_Name = uniqueName;
		duplicate.m_GraphPosition += glm::vec2{ 44.0f, 44.0f };
		duplicate.m_Transitions.clear();
		m_SelectedControllerStateIndex = static_cast<int>(m_CurrentController->GetStates().size()) - 1;
		ClearSelectedControllerTransition();
	}
	if (stateToRemove >= 0 && std::cmp_less(stateToRemove, m_CurrentController->GetStates().size()) && m_CurrentController->GetStates().size() > 1)
	{
		const std::string removedName = m_CurrentController->GetStates()[stateToRemove].m_Name;
		m_CurrentController->RemoveState(removedName);
		m_SelectedControllerStateIndex = std::clamp(m_SelectedControllerStateIndex, 0, static_cast<int>(m_CurrentController->GetStates().size()) - 1);
		ClearSelectedControllerTransition();
	}

	if (m_PendingTransitionSourceStateIndex != NoTransitionSource)
	{
		ImVec2 sourcePin = entryOutput;
		if (m_PendingTransitionSourceStateIndex == AnyStateTransitionSource)
			sourcePin = anyStateOutput;
		else if (m_PendingTransitionSourceStateIndex >= 0 && std::cmp_less(m_PendingTransitionSourceStateIndex, states.size()))
			sourcePin = stateOutputPin(static_cast<size_t>(m_PendingTransitionSourceStateIndex));

		const float tangent = std::max(48.0f * zoom, std::abs(mousePos.x - sourcePin.x) * 0.35f);
		drawList->AddBezierCubic(sourcePin, ImVec2(sourcePin.x + tangent, sourcePin.y), ImVec2(mousePos.x - tangent, mousePos.y), mousePos, IM_COL32(122, 196, 255, 230), 2.4f);
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			if (hoveredConnectionTargetExit)
				createConnection(-1, true);
			else if (hoveredConnectionTargetStateIndex >= 0)
				createConnection(hoveredConnectionTargetStateIndex, false);
			else
				clearPendingConnection();
		}
		else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
			clearPendingConnection();
	}

	const bool controllerGraphItemHovered = edgeHovered || ImGui::IsAnyItemHovered();
	if (canvasHovered && !controllerGraphItemHovered)
	{
		ImGui::SetCursorScreenPos(canvasMin);
		ImGui::InvisibleButton("##ControllerGraphCanvasCapture", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	}

	if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !controllerGraphItemHovered)
		ClearSelectedControllerTransition();
	if (canvasHovered && ImGui::IsKeyPressed(ImGuiKey_Delete) && GetSelectedControllerTransition())
		RemoveSelectedControllerTransition();
	if (canvasHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !controllerGraphItemHovered && Length(ImGui::GetMouseDragDelta(ImGuiMouseButton_Right)) < 2.0f)
		ImGui::OpenPopup("##ControllerGraphCanvasContext");
	if (ImGui::BeginPopup("##ControllerGraphCanvasContext"))
	{
		if (ImGui::MenuItem("Add State Here"))
		{
			AnimationControllerState& state = m_CurrentController->AddState("State");
			state.m_GraphPosition = screenToWorld(mousePos);
			snapPosition(state.m_GraphPosition);
			m_SelectedControllerStateIndex = static_cast<int>(m_CurrentController->GetStates().size()) - 1;
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
		if (std::cmp_greater_equal(m_SelectedTransitionIndex, transitions.size()))
			return nullptr;
		return &transitions[m_SelectedTransitionIndex];
	}

	auto& states = m_CurrentController->GetStates();
	if (m_SelectedTransitionSourceStateIndex < 0 || std::cmp_greater_equal(m_SelectedTransitionSourceStateIndex, states.size()))
		return nullptr;

	auto& transitions = states[m_SelectedTransitionSourceStateIndex].m_Transitions;
	if (std::cmp_greater_equal(m_SelectedTransitionIndex, transitions.size()))
		return nullptr;
	return &transitions[m_SelectedTransitionIndex];
}

void AnimationEditorPanel::ClearSelectedControllerTransition()
{
	m_SelectedTransitionSourceStateIndex = NoTransitionSource;
	m_SelectedTransitionIndex = -1;
	m_SelectedBlueprintNodeId = 0;
	m_PendingBlueprintLinkNodeId = 0;
	m_PendingBlueprintLinkFromInput = false;
}

void AnimationEditorPanel::RemoveSelectedControllerTransition()
{
	if (!m_CurrentController || m_SelectedTransitionIndex < 0)
		return;

	if (m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource)
	{
		auto& transitions = m_CurrentController->GetAnyStateTransitions();
		if (m_SelectedTransitionIndex >= 0 && std::cmp_less(m_SelectedTransitionIndex, transitions.size()))
			transitions.erase(transitions.begin() + m_SelectedTransitionIndex);
		ClearSelectedControllerTransition();
		return;
	}

	auto& states = m_CurrentController->GetStates();
	if (m_SelectedTransitionSourceStateIndex < 0 || std::cmp_greater_equal(m_SelectedTransitionSourceStateIndex, states.size()))
	{
		ClearSelectedControllerTransition();
		return;
	}

	auto& transitions = states[m_SelectedTransitionSourceStateIndex].m_Transitions;
	if (m_SelectedTransitionIndex >= 0 && std::cmp_less(m_SelectedTransitionIndex, transitions.size()))
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
	const int columns = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(states.size())))));
	for (size_t i = 0; i < states.size(); ++i)
	{
		constexpr float rowGap = 50.0f;
		constexpr float columnGap = 96.0f;
		const int row = static_cast<int>(i) / columns;
		const int column = static_cast<int>(i) % columns;
		states[i].m_GraphPosition = { 250.0f + static_cast<float>(column) * (nodeWidth + columnGap), 42.0f + static_cast<float>(row) * (82.0f + rowGap) };
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
	ImGui::BeginDisabled(transition.m_Conditions.empty() && transition.m_BlueprintNodes.empty());
	if (ImGui::Button("Clear Conditions", ImVec2(-1.0f, 0.0f)))
	{
		transition.m_Conditions.clear();
		transition.m_BlueprintNodes.clear();
		transition.m_BlueprintLinks.clear();
		transition.m_NextBlueprintNodeId = 1;
		transition.m_NextBlueprintLinkId = 1;
		m_SelectedBlueprintNodeId = 0;
	}
	ImGui::EndDisabled();

	int conditionToDuplicate = -1;
	int conditionToMoveUp = -1;
	int conditionToMoveDown = -1;
	for (size_t conditionIndex = 0; conditionIndex < transition.m_Conditions.size(); ++conditionIndex)
	{
		ImGui::PushID(static_cast<int>(conditionIndex));
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

		constexpr std::array<AnimationConditionMode, 6> conditionModes =
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

		const auto parameterIt = std::ranges::find_if(m_CurrentController->GetParameters(), [&condition](const AnimationControllerParameter& parameter)
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
			conditionToMoveUp = static_cast<int>(conditionIndex);
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(conditionIndex + 1 >= transition.m_Conditions.size());
		if (ImGui::Button("Move Down"))
			conditionToMoveDown = static_cast<int>(conditionIndex);
		ImGui::EndDisabled();
		if (ImGui::Button("Duplicate Condition", ImVec2(-1.0f, 0.0f)))
			conditionToDuplicate = static_cast<int>(conditionIndex);
		if (ImGui::Button("Remove Condition", ImVec2(-1.0f, 0.0f)))
		{
			transition.m_Conditions.erase(transition.m_Conditions.begin() + static_cast<std::vector<AnimationControllerCondition>::difference_type>(conditionIndex));
			ImGui::PopID();
			break;
		}
		ImGui::Separator();
		ImGui::PopID();
	}
	if (conditionToMoveUp > 0 && std::cmp_less(conditionToMoveUp, transition.m_Conditions.size()))
		std::swap(transition.m_Conditions[conditionToMoveUp], transition.m_Conditions[conditionToMoveUp - 1]);
	if (conditionToMoveDown >= 0 && conditionToMoveDown + 1 < static_cast<int>(transition.m_Conditions.size()))
		std::swap(transition.m_Conditions[conditionToMoveDown], transition.m_Conditions[conditionToMoveDown + 1]);
	if (conditionToDuplicate >= 0 && std::cmp_less(conditionToDuplicate, transition.m_Conditions.size()))
		transition.m_Conditions.insert(transition.m_Conditions.begin() + conditionToDuplicate + 1, transition.m_Conditions[conditionToDuplicate]);
}

void AnimationEditorPanel::DrawTransitionConditionGraph(AnimationControllerTransition& transition, float height)
{
	if (!m_CurrentController || height <= 0.0f)
		return;

	auto& parameters = m_CurrentController->GetParameters();

	struct NodePalette
	{
		ImU32 m_Fill;
		ImU32 m_Border;
		ImU32 m_Accent;
	};

	auto findParameter = [&](std::string_view name) -> const AnimationControllerParameter*
		{
			const auto it = std::ranges::find_if(parameters, [name](const AnimationControllerParameter& parameter)
			{
				return parameter.m_Name == name;
			});
			return it == parameters.end() ? nullptr : &*it;
		};

	auto findNode = [&](uint32_t nodeId) -> AnimationControllerBlueprintNode*
		{
			const auto it = std::ranges::find_if(transition.m_BlueprintNodes, [nodeId](const AnimationControllerBlueprintNode& node)
			{
				return node.m_Id == nodeId;
			});
			return it == transition.m_BlueprintNodes.end() ? nullptr : &*it;
		};

	auto nodeLabel = [](AnimationBlueprintNodeType type) -> const char*
		{
			switch (type)
			{
			case AnimationBlueprintNodeType::Start: return "Start";
			case AnimationBlueprintNodeType::Parameter: return "Parameter";
			case AnimationBlueprintNodeType::If: return "If";
			case AnimationBlueprintNodeType::IfNot: return "If Not";
			case AnimationBlueprintNodeType::Greater: return "Greater";
			case AnimationBlueprintNodeType::Less: return "Less";
			case AnimationBlueprintNodeType::Equals: return "Equals";
			case AnimationBlueprintNodeType::NotEquals: return "Not Equals";
			case AnimationBlueprintNodeType::Not: return "Not";
			case AnimationBlueprintNodeType::And: return "And";
			case AnimationBlueprintNodeType::Or: return "Or";
			case AnimationBlueprintNodeType::Reroute: return "Reroute";
			case AnimationBlueprintNodeType::Result: return "Result";
			}
			return "Node";
		};

	auto nodePalette = [&](const AnimationControllerBlueprintNode& node)
		{
			switch (node.m_Type)
			{
			case AnimationBlueprintNodeType::Start:
				return NodePalette{
					.m_Fill = IM_COL32(25, 42, 58, 255),
					.m_Border = IM_COL32(102, 160, 214, 225),
					.m_Accent = IM_COL32(122, 196, 255, 245)
				};
			case AnimationBlueprintNodeType::Parameter:
				if (const AnimationControllerParameter* parameter = findParameter(node.m_Parameter))
				{
					switch (parameter->m_Type)
					{
					case AnimationParameterType::Bool: return NodePalette{
						.m_Fill = IM_COL32(21, 48, 46, 255),
						.m_Border = IM_COL32(84, 184, 174, 225),
						.m_Accent = IM_COL32(105, 214, 204, 245)
					};
					case AnimationParameterType::Int: return NodePalette{
						.m_Fill = IM_COL32(40, 36, 56, 255),
						.m_Border = IM_COL32(148, 128, 214, 225),
						.m_Accent = IM_COL32(178, 156, 238, 245)
					};
					case AnimationParameterType::Float: return NodePalette{
						.m_Fill = IM_COL32(24, 42, 61, 255),
						.m_Border = IM_COL32(86, 158, 232, 230),
						.m_Accent = IM_COL32(118, 190, 255, 245)
					};
					case AnimationParameterType::Trigger: return NodePalette{
						.m_Fill=IM_COL32(32, 50, 35, 255),
						.m_Border=IM_COL32(116, 188, 126, 225),
						.m_Accent=IM_COL32(145, 226, 152, 245)
					};
					}
				}
				return NodePalette{
					.m_Fill = IM_COL32(30, 38, 48, 255),
					.m_Border = IM_COL32(108, 126, 146, 220),
					.m_Accent = IM_COL32(154, 170, 188, 235)
				};
			case AnimationBlueprintNodeType::If:
			case AnimationBlueprintNodeType::IfNot:
				return NodePalette{
					.m_Fill = IM_COL32(27, 44, 48, 255),
					.m_Border = IM_COL32(96, 188, 178, 225),
					.m_Accent = IM_COL32(124, 220, 208, 245)
				};
			case AnimationBlueprintNodeType::Greater:
			case AnimationBlueprintNodeType::Less:
			case AnimationBlueprintNodeType::Equals:
			case AnimationBlueprintNodeType::NotEquals:
				return NodePalette{
					.m_Fill = IM_COL32(27, 42, 60, 255),
					.m_Border = IM_COL32(98, 166, 230, 225),
					.m_Accent = IM_COL32(124, 196, 255, 245)
				};
			case AnimationBlueprintNodeType::Not:
				return NodePalette{
					.m_Fill = IM_COL32(40, 40, 58, 255),
					.m_Border = IM_COL32(138, 148, 214, 225),
					.m_Accent = IM_COL32(168, 180, 236, 245)
				};
			case AnimationBlueprintNodeType::And:
			case AnimationBlueprintNodeType::Or:
				return NodePalette{
					.m_Fill = IM_COL32(42, 38, 50, 255),
					.m_Border = IM_COL32(176, 130, 186, 225),
					.m_Accent = IM_COL32(210, 160, 220, 245)
				};
			case AnimationBlueprintNodeType::Reroute:
				return NodePalette{
					.m_Fill = IM_COL32(24, 34, 42, 255),
					.m_Border = IM_COL32(118, 150, 176, 220),
					.m_Accent = IM_COL32(170, 198, 222, 245)
				};
			case AnimationBlueprintNodeType::Result:
				return NodePalette{
					.m_Fill = IM_COL32(27, 48, 38, 255),
					.m_Border = IM_COL32(110, 190, 132, 225),
					.m_Accent = IM_COL32(142, 226, 156, 245)
				};
			}
			return NodePalette{
				.m_Fill = IM_COL32(30, 38, 48, 255),
				.m_Border = IM_COL32(108, 126, 146, 220),
				.m_Accent = IM_COL32(154, 170, 188, 235)
			};
		};

	auto inputPinCount = [](AnimationBlueprintNodeType type) -> uint32_t
		{
			switch (type)
			{
			case AnimationBlueprintNodeType::Start:
			case AnimationBlueprintNodeType::Parameter: return 0;
			case AnimationBlueprintNodeType::Greater:
			case AnimationBlueprintNodeType::Less:
			case AnimationBlueprintNodeType::Equals:
			case AnimationBlueprintNodeType::NotEquals:
			case AnimationBlueprintNodeType::And:
			case AnimationBlueprintNodeType::Or: return 2;
			case AnimationBlueprintNodeType::Reroute:
			case AnimationBlueprintNodeType::Result:
			case AnimationBlueprintNodeType::If:
			case AnimationBlueprintNodeType::IfNot:
			case AnimationBlueprintNodeType::Not: return 1;
			}
			return 1;
		};

	auto outputPinCount = [](AnimationBlueprintNodeType type) -> uint32_t
		{
			switch (type)
			{
			case AnimationBlueprintNodeType::Start:
			case AnimationBlueprintNodeType::Result: return 0;
			case AnimationBlueprintNodeType::Parameter:
			case AnimationBlueprintNodeType::If:
			case AnimationBlueprintNodeType::IfNot:
			case AnimationBlueprintNodeType::Greater:
			case AnimationBlueprintNodeType::Less:
			case AnimationBlueprintNodeType::Equals:
			case AnimationBlueprintNodeType::NotEquals:
			case AnimationBlueprintNodeType::Reroute:
			case AnimationBlueprintNodeType::Not:
			case AnimationBlueprintNodeType::And:
			case AnimationBlueprintNodeType::Or: return 1;
			}
			return 0;
		};

	auto inputPinLabel = [](AnimationBlueprintNodeType type, uint32_t pin) -> const char*
		{
			switch (type)
			{
			case AnimationBlueprintNodeType::And:
			case AnimationBlueprintNodeType::Or: return pin == 0 ? "A" : "B";
			case AnimationBlueprintNodeType::Not: return "In";
			case AnimationBlueprintNodeType::Reroute: return "";
			case AnimationBlueprintNodeType::Result: return "Pass";
			case AnimationBlueprintNodeType::If:
			case AnimationBlueprintNodeType::IfNot: return "Bool";
			case AnimationBlueprintNodeType::Greater:
			case AnimationBlueprintNodeType::Less:
			case AnimationBlueprintNodeType::Equals:
			case AnimationBlueprintNodeType::NotEquals: return pin == 0 ? "A" : "B";
			case AnimationBlueprintNodeType::Start:
			case AnimationBlueprintNodeType::Parameter: return "";
			}
			return "";
		};

	auto outputPinLabel = [](AnimationBlueprintNodeType type, uint32_t) -> const char*
		{
			switch (type)
			{
			case AnimationBlueprintNodeType::Parameter: return "Value";
			case AnimationBlueprintNodeType::If:
			case AnimationBlueprintNodeType::IfNot:
			case AnimationBlueprintNodeType::Greater:
			case AnimationBlueprintNodeType::Less:
			case AnimationBlueprintNodeType::Equals:
			case AnimationBlueprintNodeType::NotEquals: return "Bool";
			case AnimationBlueprintNodeType::Not:
			case AnimationBlueprintNodeType::And:
			case AnimationBlueprintNodeType::Or: return "Out";
			case AnimationBlueprintNodeType::Reroute:
			case AnimationBlueprintNodeType::Start:
			case AnimationBlueprintNodeType::Result: return "";
			}
			return "";
		};

	auto execInputPinCount = [](AnimationBlueprintNodeType type) -> uint32_t
		{
			switch (type)
			{
			case AnimationBlueprintNodeType::If:
			case AnimationBlueprintNodeType::IfNot:
			case AnimationBlueprintNodeType::Greater:
			case AnimationBlueprintNodeType::Less:
			case AnimationBlueprintNodeType::Equals:
			case AnimationBlueprintNodeType::NotEquals:
			case AnimationBlueprintNodeType::Reroute:
			case AnimationBlueprintNodeType::Result:
				return 1;
			case AnimationBlueprintNodeType::Start:
			case AnimationBlueprintNodeType::Parameter:
			case AnimationBlueprintNodeType::Not:
			case AnimationBlueprintNodeType::And:
			case AnimationBlueprintNodeType::Or:
				return 0;
			}
			return 0;
		};

	auto execOutputPinCount = [](AnimationBlueprintNodeType type) -> uint32_t
		{
			switch (type)
			{
			case AnimationBlueprintNodeType::Start:
			case AnimationBlueprintNodeType::Reroute:
				return 1;
			case AnimationBlueprintNodeType::If:
			case AnimationBlueprintNodeType::IfNot:
			case AnimationBlueprintNodeType::Greater:
			case AnimationBlueprintNodeType::Less:
			case AnimationBlueprintNodeType::Equals:
			case AnimationBlueprintNodeType::NotEquals:
				return 2;
			case AnimationBlueprintNodeType::Parameter:
			case AnimationBlueprintNodeType::Not:
			case AnimationBlueprintNodeType::And:
			case AnimationBlueprintNodeType::Or:
			case AnimationBlueprintNodeType::Result:
				return 0;
			}
			return 0;
		};

	auto execOutputPinId = [](AnimationBlueprintNodeType type, uint32_t pin) -> uint32_t
		{
			if (type == AnimationBlueprintNodeType::Start || type == AnimationBlueprintNodeType::Reroute)
				return AnimationBlueprintThenPin;
			return pin == 0 ? AnimationBlueprintTruePin : AnimationBlueprintFalsePin;
		};

	auto execInputPinLabel = [](AnimationBlueprintNodeType type) -> const char*
		{
			if (type == AnimationBlueprintNodeType::Reroute)
				return "";
			return "Exec";
		};

	auto execOutputPinLabel = [](AnimationBlueprintNodeType type, uint32_t pin) -> const char*
		{
			if (type == AnimationBlueprintNodeType::Start)
				return "Then";
			if (type == AnimationBlueprintNodeType::Reroute)
				return "";
			return pin == 0 ? "True" : "False";
		};

	auto nodeSize = [](AnimationBlueprintNodeType type) -> ImVec2
		{
			switch (type)
			{
			case AnimationBlueprintNodeType::Start: return { 176.0f, 86.0f };
			case AnimationBlueprintNodeType::Parameter: return { 196.0f, 84.0f };
			case AnimationBlueprintNodeType::Reroute: return { 96.0f, 56.0f };
			case AnimationBlueprintNodeType::Not: return { 184.0f, 94.0f };
			case AnimationBlueprintNodeType::And:
			case AnimationBlueprintNodeType::Or: return { 196.0f, 108.0f };
			case AnimationBlueprintNodeType::Greater:
			case AnimationBlueprintNodeType::Less:
			case AnimationBlueprintNodeType::Equals:
			case AnimationBlueprintNodeType::NotEquals: return { 252.0f, 150.0f };
			case AnimationBlueprintNodeType::Result: return { 176.0f, 92.0f };
			case AnimationBlueprintNodeType::If:
			case AnimationBlueprintNodeType::IfNot:
				return { 224.0f, 118.0f };
			}
			return { 224.0f, 118.0f };
		};

	auto conditionModeFromNodeType = [](AnimationBlueprintNodeType type) -> AnimationConditionMode
		{
			switch (type)
			{
			case AnimationBlueprintNodeType::IfNot: return AnimationConditionMode::IfNot;
			case AnimationBlueprintNodeType::Greater: return AnimationConditionMode::Greater;
			case AnimationBlueprintNodeType::Less: return AnimationConditionMode::Less;
			case AnimationBlueprintNodeType::Equals: return AnimationConditionMode::Equals;
			case AnimationBlueprintNodeType::NotEquals: return AnimationConditionMode::NotEquals;
			case AnimationBlueprintNodeType::Start:
			case AnimationBlueprintNodeType::Parameter:
			case AnimationBlueprintNodeType::If:
			case AnimationBlueprintNodeType::Not:
			case AnimationBlueprintNodeType::And:
			case AnimationBlueprintNodeType::Or:
			case AnimationBlueprintNodeType::Reroute:
			case AnimationBlueprintNodeType::Result:
				return AnimationConditionMode::If;
			}
			return AnimationConditionMode::If;
		};

	auto nodeTypeFromConditionMode = [](AnimationConditionMode mode) -> AnimationBlueprintNodeType
		{
			switch (mode)
			{
			case AnimationConditionMode::IfNot: return AnimationBlueprintNodeType::IfNot;
			case AnimationConditionMode::Greater: return AnimationBlueprintNodeType::Greater;
			case AnimationConditionMode::Less: return AnimationBlueprintNodeType::Less;
			case AnimationConditionMode::Equals: return AnimationBlueprintNodeType::Equals;
			case AnimationConditionMode::NotEquals: return AnimationBlueprintNodeType::NotEquals;
			case AnimationConditionMode::If: return AnimationBlueprintNodeType::If;
			}
			return AnimationBlueprintNodeType::If;
		};

	auto addNode = [&](AnimationBlueprintNodeType type, glm::vec2 graphPosition, std::string parameterName = {}) -> AnimationControllerBlueprintNode&
		{
			AnimationControllerBlueprintNode node;
			node.m_Id = transition.m_NextBlueprintNodeId++;
			node.m_Type = type;
			node.m_Parameter = std::move(parameterName);
			node.m_GraphPosition = graphPosition;
			transition.m_BlueprintNodes.push_back(node);
			m_SelectedBlueprintNodeId = node.m_Id;
			return transition.m_BlueprintNodes.back();
		};

	auto removeLinksForNode = [&](uint32_t nodeId)
		{
			std::erase_if(transition.m_BlueprintLinks, [nodeId](const AnimationControllerBlueprintLink& link)
				{
					return link.m_OutputNode == nodeId || link.m_InputNode == nodeId;
				});
		};

	auto addLink = [&](uint32_t outputNode, uint32_t outputPin, uint32_t inputNode, uint32_t inputPin)
		{
			if (outputNode == 0 || inputNode == 0 || outputNode == inputNode)
				return;
			AnimationControllerBlueprintNode* input = findNode(inputNode);
			AnimationControllerBlueprintNode* output = findNode(outputNode);
			if (!input || !output)
				return;
			if (const bool execLink = IsAnimationBlueprintExecPin(outputPin) || IsAnimationBlueprintExecPin(inputPin); execLink)
			{
				if (!IsAnimationBlueprintExecPin(outputPin) || inputPin != AnimationBlueprintExecInputPin || execInputPinCount(input->m_Type) == 0)
					return;

				bool validExecOutput = false;
				for (uint32_t pin = 0; pin < execOutputPinCount(output->m_Type); ++pin)
				{
					if (execOutputPinId(output->m_Type, pin) == outputPin)
					{
						validExecOutput = true;
						break;
					}
				}
				if (!validExecOutput)
					return;
			}
			else
			{
				if (inputPin >= inputPinCount(input->m_Type) || outputPin >= outputPinCount(output->m_Type))
					return;
			}

			if (input->m_Type != AnimationBlueprintNodeType::Result)
			{
				std::erase_if(transition.m_BlueprintLinks, [inputNode, inputPin](const AnimationControllerBlueprintLink& link)
					{
						return link.m_InputNode == inputNode && link.m_InputPin == inputPin;
					});
			}
			std::erase_if(transition.m_BlueprintLinks, [outputNode, outputPin, inputNode, inputPin](const AnimationControllerBlueprintLink& link)
				{
					return link.m_OutputNode == outputNode && link.m_OutputPin == outputPin && link.m_InputNode == inputNode && link.m_InputPin == inputPin;
				});

			AnimationControllerBlueprintLink link;
			link.m_Id = transition.m_NextBlueprintLinkId++;
			link.m_OutputNode = outputNode;
			link.m_OutputPin = outputPin;
			link.m_InputNode = inputNode;
			link.m_InputPin = inputPin;
			transition.m_BlueprintLinks.push_back(link);

			if (input->m_Type != AnimationBlueprintNodeType::Result && output->m_Type == AnimationBlueprintNodeType::Parameter)
				input->m_Parameter = output->m_Parameter;
		};

	auto resultNodeId = [&]() -> uint32_t
		{
			const auto resultIt = std::ranges::find_if(transition.m_BlueprintNodes, [](const AnimationControllerBlueprintNode& node)
			{
				return node.m_Type == AnimationBlueprintNodeType::Result;
			});
			if (resultIt != transition.m_BlueprintNodes.end())
				return resultIt->m_Id;
			AnimationControllerBlueprintNode& result = addNode(AnimationBlueprintNodeType::Result, { 620.0f, 96.0f });
			return result.m_Id;
		};

	auto startNodeId = [&]() -> uint32_t
		{
			const auto startIt = std::ranges::find_if(transition.m_BlueprintNodes, [](const AnimationControllerBlueprintNode& node)
			{
				return node.m_Type == AnimationBlueprintNodeType::Start;
			});
			if (startIt != transition.m_BlueprintNodes.end())
				return startIt->m_Id;
			AnimationControllerBlueprintNode& start = addNode(AnimationBlueprintNodeType::Start, { 28.0f, 96.0f });
			return start.m_Id;
		};

	auto pickParameterForNode = [&](AnimationBlueprintNodeType type) -> std::string
		{
			if (parameters.empty())
				return {};
			if (type == AnimationBlueprintNodeType::If || type == AnimationBlueprintNodeType::IfNot)
			{
				const auto boolIt = std::ranges::find_if(parameters, [](const AnimationControllerParameter& parameter)
				{
					return parameter.m_Type == AnimationParameterType::Bool || parameter.m_Type == AnimationParameterType::Trigger;
				});
				if (boolIt != parameters.end())
					return boolIt->m_Name;
				return {};
			}
			if (type == AnimationBlueprintNodeType::Greater || type == AnimationBlueprintNodeType::Less || type == AnimationBlueprintNodeType::Equals || type == AnimationBlueprintNodeType::NotEquals)
			{
				const auto numericIt = std::ranges::find_if(parameters, [](const AnimationControllerParameter& parameter)
				{
					return parameter.m_Type == AnimationParameterType::Float || parameter.m_Type == AnimationParameterType::Int;
				});
				if (numericIt != parameters.end())
					return numericIt->m_Name;
				if (type == AnimationBlueprintNodeType::Greater || type == AnimationBlueprintNodeType::Less)
					return {};
			}
			return parameters.front().m_Name;
		};

	auto nodeUsesParameter = [](AnimationBlueprintNodeType type)
		{
			return type == AnimationBlueprintNodeType::Parameter ||
				type == AnimationBlueprintNodeType::If ||
				type == AnimationBlueprintNodeType::IfNot ||
				type == AnimationBlueprintNodeType::Greater ||
				type == AnimationBlueprintNodeType::Less ||
				type == AnimationBlueprintNodeType::Equals ||
				type == AnimationBlueprintNodeType::NotEquals;
		};

	auto isSystemBlueprintNode = [](AnimationBlueprintNodeType type)
		{
			return type == AnimationBlueprintNodeType::Start || type == AnimationBlueprintNodeType::Result;
		};

	auto isConditionBlueprintNode = [](AnimationBlueprintNodeType type)
		{
			return type == AnimationBlueprintNodeType::If ||
				type == AnimationBlueprintNodeType::IfNot ||
				type == AnimationBlueprintNodeType::Greater ||
				type == AnimationBlueprintNodeType::Less ||
				type == AnimationBlueprintNodeType::Equals ||
				type == AnimationBlueprintNodeType::NotEquals;
		};

	auto parameterCompatibleWithNode = [](AnimationBlueprintNodeType type, const AnimationControllerParameter& parameter)
		{
			switch (type)
			{
			case AnimationBlueprintNodeType::Parameter:
				return true;
			case AnimationBlueprintNodeType::If:
			case AnimationBlueprintNodeType::IfNot:
				return parameter.m_Type == AnimationParameterType::Bool || parameter.m_Type == AnimationParameterType::Trigger;
			case AnimationBlueprintNodeType::Greater:
			case AnimationBlueprintNodeType::Less:
				return parameter.m_Type == AnimationParameterType::Float || parameter.m_Type == AnimationParameterType::Int;
			case AnimationBlueprintNodeType::Equals:
			case AnimationBlueprintNodeType::NotEquals:
				return true;
			case AnimationBlueprintNodeType::Start:
			case AnimationBlueprintNodeType::Not:
			case AnimationBlueprintNodeType::And:
			case AnimationBlueprintNodeType::Or:
			case AnimationBlueprintNodeType::Reroute:
			case AnimationBlueprintNodeType::Result:
				return false;
			}
			return false;
		};

	auto ensureCompatibleParameter = [&](AnimationControllerBlueprintNode& node)
		{
			if (!nodeUsesParameter(node.m_Type))
			{
				node.m_Parameter.clear();
				return;
			}

			if (const AnimationControllerParameter* parameter = findParameter(node.m_Parameter))
			{
				if (parameterCompatibleWithNode(node.m_Type, *parameter))
					return;
			}

			for (const AnimationControllerParameter& parameter : parameters)
			{
				if (parameterCompatibleWithNode(node.m_Type, parameter))
				{
					node.m_Parameter = parameter.m_Name;
					return;
				}
			}

			node.m_Parameter.clear();
		};

	auto resetNodeDefaults = [](AnimationControllerBlueprintNode& node)
		{
			node.m_Threshold = 0.0f;
			node.m_IntValue = 0;
			node.m_BoolValue = true;
			node.m_InputFloatValues[0] = 0.0f;
			node.m_InputFloatValues[1] = 0.0f;
			node.m_InputIntValues[0] = 0;
			node.m_InputIntValues[1] = 0;
			node.m_InputBoolValues[0] = false;
			node.m_InputBoolValues[1] = true;
		};

	auto pruneInvalidBlueprintLinks = [&]()
		{
			std::erase_if(transition.m_BlueprintLinks, [&](const AnimationControllerBlueprintLink& link)
				{
					const AnimationControllerBlueprintNode* output = findNode(link.m_OutputNode);
					const AnimationControllerBlueprintNode* input = findNode(link.m_InputNode);
					if (!output || !input || output->m_Id == input->m_Id)
						return true;

					if (const bool execLink = IsAnimationBlueprintExecPin(link.m_OutputPin) || IsAnimationBlueprintExecPin(link.m_InputPin); execLink)
					{
						if (!IsAnimationBlueprintExecPin(link.m_OutputPin) || link.m_InputPin != AnimationBlueprintExecInputPin || execInputPinCount(input->m_Type) == 0)
							return true;

						for (uint32_t pin = 0; pin < execOutputPinCount(output->m_Type); ++pin)
						{
							if (execOutputPinId(output->m_Type, pin) == link.m_OutputPin)
								return false;
						}
						return true;
					}

					return link.m_OutputPin >= outputPinCount(output->m_Type) || link.m_InputPin >= inputPinCount(input->m_Type);
				});
		};

	auto createConditionChain = [&](AnimationBlueprintNodeType type, glm::vec2 graphPosition, std::string parameterName = {})
		{
			if (parameterName.empty())
				parameterName = pickParameterForNode(type);
			const uint32_t resultId = resultNodeId();
			const uint32_t parameterId = addNode(AnimationBlueprintNodeType::Parameter, graphPosition, parameterName).m_Id;
			const uint32_t logicId = addNode(type, graphPosition + glm::vec2{ 228.0f, -12.0f }, parameterName).m_Id;
			addLink(parameterId, 0, logicId, 0);
			addLink(logicId, 0, resultId, 0);
			m_SelectedBlueprintNodeId = logicId;
		};

	if (transition.m_BlueprintNodes.empty() && !transition.m_Conditions.empty())
	{
		startNodeId();
		const uint32_t resultId = resultNodeId();
		for (size_t index = 0; index < transition.m_Conditions.size(); ++index)
		{
			const AnimationControllerCondition& condition = transition.m_Conditions[index];
			const glm::vec2 basePosition = condition.m_GraphPosition.x == 0.0f && condition.m_GraphPosition.y == 0.0f ? glm::vec2{ 32.0f, 54.0f + static_cast<float>(index) * 94.0f } : condition.m_GraphPosition;
			const uint32_t parameterId = addNode(AnimationBlueprintNodeType::Parameter, basePosition, condition.m_Parameter).m_Id;
			const uint32_t logicId = addNode(nodeTypeFromConditionMode(condition.m_Mode), basePosition + glm::vec2{ 228.0f, -12.0f }, condition.m_Parameter).m_Id;
			if (AnimationControllerBlueprintNode* logic = findNode(logicId))
			{
				logic->m_Threshold = condition.m_Threshold;
				logic->m_IntValue = condition.m_IntValue;
				logic->m_BoolValue = condition.m_BoolValue;
				logic->m_InputFloatValues[1] = condition.m_Threshold;
				logic->m_InputIntValues[1] = condition.m_IntValue;
				logic->m_InputBoolValues[1] = condition.m_BoolValue;
			}
			addLink(parameterId, 0, logicId, 0);
			addLink(logicId, 0, resultId, 0);
		}
		m_SelectedBlueprintNodeId = 0;
	}
	else
	{
		startNodeId();
		resultNodeId();
	}

	for (AnimationControllerBlueprintNode& node : transition.m_BlueprintNodes)
		ensureCompatibleParameter(node);
	pruneInvalidBlueprintLinks();

	if (m_SelectedBlueprintNodeId != 0 && !findNode(m_SelectedBlueprintNodeId))
		m_SelectedBlueprintNodeId = 0;

	auto syncLegacyConditions = [&]()
		{
			transition.m_Conditions.clear();
			for (const AnimationControllerBlueprintNode& node : transition.m_BlueprintNodes)
			{
				if (node.m_Type != AnimationBlueprintNodeType::If && node.m_Type != AnimationBlueprintNodeType::IfNot && node.m_Type != AnimationBlueprintNodeType::Greater && node.m_Type != AnimationBlueprintNodeType::Less && node.m_Type != AnimationBlueprintNodeType::Equals && node.m_Type != AnimationBlueprintNodeType::NotEquals)
					continue;
				if (node.m_Parameter.empty())
					continue;

				AnimationControllerCondition condition;
				condition.m_Parameter = node.m_Parameter;
				condition.m_Mode = conditionModeFromNodeType(node.m_Type);
				condition.m_Threshold = node.m_InputFloatValues[1];
				condition.m_IntValue = node.m_InputIntValues[1];
				condition.m_BoolValue = node.m_InputBoolValues[1];
				condition.m_GraphPosition = node.m_GraphPosition;
				transition.m_Conditions.push_back(condition);
			}
		};

	ImGui::BeginChild("##TransitionBlueprintGraph", ImVec2(0.0f, height), true);
	ImGui::TextDisabled("Transition Blueprint");
	ImGui::SameLine();
	if (ImGui::SmallButton("Auto Layout"))
	{
		float y = 52.0f;
		uint32_t resultId = resultNodeId();
		uint32_t startId = startNodeId();
		for (AnimationControllerBlueprintNode& node : transition.m_BlueprintNodes)
		{
			if (node.m_Type == AnimationBlueprintNodeType::Start)
			{
				node.m_GraphPosition = { 28.0f, 96.0f };
				continue;
			}
			if (node.m_Type == AnimationBlueprintNodeType::Result)
				continue;
			if (node.m_Type == AnimationBlueprintNodeType::Parameter)
				node.m_GraphPosition = { 36.0f, y };
			else if (node.m_Type == AnimationBlueprintNodeType::Reroute)
				node.m_GraphPosition = { 638.0f, y + 10.0f };
			else if (node.m_Type == AnimationBlueprintNodeType::And || node.m_Type == AnimationBlueprintNodeType::Or || node.m_Type == AnimationBlueprintNodeType::Not)
				node.m_GraphPosition = { 518.0f, y };
			else
				node.m_GraphPosition = { 276.0f, y - 12.0f };
			y += 128.0f;
		}
		if (AnimationControllerBlueprintNode* start = findNode(startId))
			start->m_GraphPosition = { 28.0f, 96.0f };
		if (AnimationControllerBlueprintNode* result = findNode(resultId))
			result->m_GraphPosition = { 760.0f, 94.0f };
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Frame"))
	{
		glm::vec2 minBounds{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
		glm::vec2 maxBounds{ -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
		for (const AnimationControllerBlueprintNode& node : transition.m_BlueprintNodes)
		{
			const ImVec2 size = nodeSize(node.m_Type);
			minBounds.x = std::min(minBounds.x, node.m_GraphPosition.x);
			minBounds.y = std::min(minBounds.y, node.m_GraphPosition.y);
			maxBounds.x = std::max(maxBounds.x, node.m_GraphPosition.x + size.x);
			maxBounds.y = std::max(maxBounds.y, node.m_GraphPosition.y + size.y);
		}
		if (minBounds.x < std::numeric_limits<float>::max())
			m_BlueprintGraphPan = { 34.0f - minBounds.x * m_BlueprintGraphZoom, 34.0f - minBounds.y * m_BlueprintGraphZoom };
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Zoom -"))
		m_BlueprintGraphZoom = std::max(0.55f, m_BlueprintGraphZoom - 0.1f);
	ImGui::SameLine();
	if (ImGui::SmallButton("Zoom +"))
		m_BlueprintGraphZoom = std::min(2.0f, m_BlueprintGraphZoom + 0.1f);
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset View"))
	{
		m_BlueprintGraphPan = { 18.0f, 18.0f };
		m_BlueprintGraphZoom = 1.0f;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Clear Graph"))
	{
		transition.m_BlueprintNodes.clear();
		transition.m_BlueprintLinks.clear();
		transition.m_Conditions.clear();
		transition.m_NextBlueprintNodeId = 1;
		transition.m_NextBlueprintLinkId = 1;
		m_SelectedBlueprintNodeId = 0;
		m_PendingBlueprintLinkNodeId = 0;
		m_PendingBlueprintLinkFromInput = false;
		startNodeId();
		resultNodeId();
		m_SelectedBlueprintNodeId = 0;
	}

	constexpr float selectedPanelHeight = 0.0f;
	const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 available = ImGui::GetContentRegionAvail();
	const ImVec2 canvasSize(std::max(420.0f, available.x), std::max(180.0f, available.y - selectedPanelHeight - ImGui::GetStyle().ItemSpacing.y));
	ImGui::Dummy(canvasSize);
	const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
	const bool canvasHovered =
		ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
		ImGui::IsMouseHoveringRect(canvasMin, canvasMax, true);
	const ImVec2 mousePos = ImGui::GetIO().MousePos;
	if (canvasHovered && ImGui::GetIO().MouseWheel != 0.0f)
	{
		const float oldZoom = m_BlueprintGraphZoom;
		const float newZoom = std::clamp(oldZoom + ImGui::GetIO().MouseWheel * 0.08f, 0.55f, 2.0f);
		if (!Math::EqualF(newZoom, oldZoom))
		{
			const glm::vec2 focusGraph {
				(mousePos.x - canvasMin.x - m_BlueprintGraphPan.x) / oldZoom,
				(mousePos.y - canvasMin.y - m_BlueprintGraphPan.y) / oldZoom
			};
			m_BlueprintGraphZoom = newZoom;
			m_BlueprintGraphPan = {
				mousePos.x - canvasMin.x - focusGraph.x * newZoom,
				mousePos.y - canvasMin.y - focusGraph.y * newZoom
			};
		}
	}
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(8, 12, 16, 255), 5.0f);
	drawList->AddRect(canvasMin, canvasMax, IM_COL32(54, 68, 82, 200), 5.0f);

	constexpr ImU32 gridColor = IM_COL32(46, 57, 68, 72);
	const float blueprintZoom = m_BlueprintGraphZoom;
	const float gridStep = 24.0f * blueprintZoom;

	const float startX = std::fmod(m_BlueprintGraphPan.x, gridStep);
	for (int i = 0; startX + (static_cast<float>(i) * gridStep) < canvasSize.x; ++i)
	{
		float x = startX + (static_cast<float>(i) * gridStep);
		drawList->AddLine(ImVec2(canvasMin.x + x, canvasMin.y), ImVec2(canvasMin.x + x, canvasMax.y), gridColor);
	}

	const float startY = std::fmod(m_BlueprintGraphPan.y, gridStep);
	for (int i = 0; startY + (static_cast<float>(i) * gridStep) < canvasSize.y; ++i)
	{
		float y = startY + (static_cast<float>(i) * gridStep);
		drawList->AddLine(ImVec2(canvasMin.x, canvasMin.y + y), ImVec2(canvasMax.x, canvasMin.y + y), gridColor);
	}

	auto graphToScreen = [&](const glm::vec2& graph) -> ImVec2
		{
			return { canvasMin.x + m_BlueprintGraphPan.x + graph.x * blueprintZoom, canvasMin.y + m_BlueprintGraphPan.y + graph.y * blueprintZoom };
		};

	auto screenToGraph = [&](const ImVec2& screen) -> glm::vec2
		{
			return
			{
				(screen.x - canvasMin.x - m_BlueprintGraphPan.x) / blueprintZoom,
				(screen.y - canvasMin.y - m_BlueprintGraphPan.y) / blueprintZoom
			};
		};

	auto nodeScreenSize = [&](AnimationBlueprintNodeType type) -> ImVec2
		{
			const ImVec2 size = nodeSize(type);
			return { size.x * blueprintZoom, size.y * blueprintZoom };
		};

	auto openBlueprintNodeMenu = [&](const glm::vec2& spawnPosition, uint32_t linkNodeId = 0, uint32_t linkPin = 0, bool fromInput = false)
		{
			m_BlueprintContextSpawnPosition = spawnPosition;
			m_ContextBlueprintLinkNodeId = linkNodeId;
			m_ContextBlueprintLinkPin = linkPin;
			m_ContextBlueprintLinkFromInput = fromInput;
			m_BlueprintNodeSearch.clear();
			m_BlueprintNodeSearchFocusRequested = true;
			ImGui::OpenPopup("##TransitionBlueprintContext");
		};

	auto spawnBlueprintNode = [&](AnimationBlueprintNodeType type, std::string parameterName = {})
		{
			AnimationControllerBlueprintNode& node = addNode(type, m_BlueprintContextSpawnPosition, std::move(parameterName));
			const uint32_t nodeId = node.m_Id;
			if (m_ContextBlueprintLinkNodeId != 0)
			{
				if (m_ContextBlueprintLinkFromInput)
				{
					const uint32_t outputPin = IsAnimationBlueprintExecPin(m_ContextBlueprintLinkPin) ? execOutputPinId(type, 0) : 0;
					addLink(nodeId, outputPin, m_ContextBlueprintLinkNodeId, m_ContextBlueprintLinkPin);
				}
				else
				{
					const uint32_t inputPin = IsAnimationBlueprintExecPin(m_ContextBlueprintLinkPin) ? AnimationBlueprintExecInputPin : 0;
					addLink(m_ContextBlueprintLinkNodeId, m_ContextBlueprintLinkPin, nodeId, inputPin);
				}
			}
		};

	auto drawBlueprintNodeMenu = [&]()
		{
			enum class PaletteAction : uint8_t
			{
				SpawnNode,
				SpawnCondition,
				ConnectResult
			};

			struct PaletteEntry
			{
				PaletteAction m_Action = PaletteAction::SpawnNode;
				AnimationBlueprintNodeType m_Type = AnimationBlueprintNodeType::Parameter;
				std::string m_Category;
				std::string m_Name;
				std::string m_Detail;
				std::string m_Parameter;
			};

			const bool fromPin = m_ContextBlueprintLinkNodeId != 0;
			const bool fromInput = fromPin && m_ContextBlueprintLinkFromInput;
			const bool fromExecPin = fromPin && IsAnimationBlueprintExecPin(m_ContextBlueprintLinkPin);
			std::vector<PaletteEntry> entries;
			auto addEntry = [&](PaletteAction action, AnimationBlueprintNodeType type, std::string category, std::string name, std::string detail = {}, std::string parameterName = {})
				{
					entries.push_back({
						.m_Action = action,
						.m_Type = type,
						.m_Category = std::move(category),
						.m_Name = std::move(name),
						.m_Detail = std::move(detail),
						.m_Parameter = std::move(parameterName)
					});
				};

			if (fromPin && !fromInput)
				addEntry(PaletteAction::ConnectResult, AnimationBlueprintNodeType::Result, fromExecPin ? "Flow" : "Result", "Connect To Result", fromExecPin ? "Execute transition result" : "Feed result condition");
			if (fromPin)
				addEntry(PaletteAction::SpawnNode, AnimationBlueprintNodeType::Reroute, "Routing", "Reroute", fromExecPin ? "Route execution flow" : "Route value wire");

			if (!fromPin)
			{
				for (const AnimationControllerParameter& parameter : parameters)
					addEntry(PaletteAction::SpawnNode, AnimationBlueprintNodeType::Parameter, "Parameters", parameter.m_Name, std::string("Get ") + frenum::to_string(parameter.m_Type).data(), parameter.m_Name);
				addEntry(PaletteAction::SpawnNode, AnimationBlueprintNodeType::Reroute, "Routing", "Reroute", "Clean up long wires");
			}

			auto addConditionEntriesForParameter = [&](const AnimationControllerParameter& parameter)
				{
					if (parameter.m_Type == AnimationParameterType::Bool || parameter.m_Type == AnimationParameterType::Trigger)
					{
						addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::If, "Parameter Conditions", parameter.m_Name + " If", "True branch", parameter.m_Name);
						addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::IfNot, "Parameter Conditions", parameter.m_Name + " If Not", "False branch", parameter.m_Name);
						addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::Equals, "Parameter Conditions", parameter.m_Name + " Equals", "Compare bool", parameter.m_Name);
						addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::NotEquals, "Parameter Conditions", parameter.m_Name + " Not Equals", "Compare bool", parameter.m_Name);
						return;
					}

					addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::Greater, "Parameter Conditions", parameter.m_Name + " Greater", "Compare number", parameter.m_Name);
					addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::Less, "Parameter Conditions", parameter.m_Name + " Less", "Compare number", parameter.m_Name);
					addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::Equals, "Parameter Conditions", parameter.m_Name + " Equals", "Compare number", parameter.m_Name);
					addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::NotEquals, "Parameter Conditions", parameter.m_Name + " Not Equals", "Compare number", parameter.m_Name);
				};
			for (const AnimationControllerParameter& parameter : parameters)
				addConditionEntriesForParameter(parameter);

			addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::If, "Conditions", "If", "Boolean condition", pickParameterForNode(AnimationBlueprintNodeType::If));
			addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::IfNot, "Conditions", "If Not", "Inverted boolean condition", pickParameterForNode(AnimationBlueprintNodeType::IfNot));
			addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::Greater, "Conditions", "Greater", "Number > threshold", pickParameterForNode(AnimationBlueprintNodeType::Greater));
			addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::Less, "Conditions", "Less", "Number < threshold", pickParameterForNode(AnimationBlueprintNodeType::Less));
			addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::Equals, "Conditions", "Equals", "Value equals target", pickParameterForNode(AnimationBlueprintNodeType::Equals));
			addEntry(PaletteAction::SpawnCondition, AnimationBlueprintNodeType::NotEquals, "Conditions", "Not Equals", "Value differs from target", pickParameterForNode(AnimationBlueprintNodeType::NotEquals));
			if (!fromExecPin)
			{
				addEntry(PaletteAction::SpawnNode, AnimationBlueprintNodeType::Not, "Logic", "Not", "Invert bool");
				addEntry(PaletteAction::SpawnNode, AnimationBlueprintNodeType::And, "Logic", "And", "A and B");
				addEntry(PaletteAction::SpawnNode, AnimationBlueprintNodeType::Or, "Logic", "Or", "A or B");
			}

			auto lower = [](std::string value)
				{
					std::ranges::transform(value, value.begin(), [](unsigned char c)
					{
						return static_cast<char>(std::tolower(c));
					});
					return value;
				};
			const std::string query = lower(m_BlueprintNodeSearch);
			auto matchesQuery = [&](const PaletteEntry& entry)
				{
					if (query.empty())
						return true;
					std::string searchable = entry.m_Category + " " + entry.m_Name + " " + entry.m_Detail + " " + entry.m_Parameter;
					return lower(std::move(searchable)).find(query) != std::string::npos;
				};

			constexpr float paletteWidth = 520.0f;
			ImGui::SetNextItemWidth(paletteWidth);
			if (m_BlueprintNodeSearchFocusRequested)
			{
				ImGui::SetKeyboardFocusHere();
				m_BlueprintNodeSearchFocusRequested = false;
			}
			ImGui::InputTextWithHint("##BlueprintNodeSearch", "Search nodes or parameters...", &m_BlueprintNodeSearch);
			if (fromPin)
				ImGui::TextDisabled(fromInput ? "Pick a node to auto-connect into the dragged input." : "Pick a node to auto-connect the dragged output.");
			ImGui::Separator();

			std::vector<size_t> filteredEntries;
			for (size_t index = 0; index < entries.size(); ++index)
			{
				if (matchesQuery(entries[index]))
					filteredEntries.push_back(index);
			}

			auto executeEntry = [&](const PaletteEntry& entry)
				{
					switch (entry.m_Action)
					{
					case PaletteAction::ConnectResult:
						addLink(m_ContextBlueprintLinkNodeId, m_ContextBlueprintLinkPin, resultNodeId(), fromExecPin ? AnimationBlueprintExecInputPin : 0);
						break;
					case PaletteAction::SpawnCondition:
						if (fromPin)
							spawnBlueprintNode(entry.m_Type, entry.m_Parameter);
						else
							createConditionChain(entry.m_Type, m_BlueprintContextSpawnPosition, entry.m_Parameter);
						break;
					case PaletteAction::SpawnNode:
						spawnBlueprintNode(entry.m_Type, entry.m_Parameter);
						break;
					}
					m_ContextBlueprintLinkNodeId = 0;
					m_ContextBlueprintLinkPin = 0;
					m_ContextBlueprintLinkFromInput = false;
					ImGui::CloseCurrentPopup();
				};

			if (!filteredEntries.empty() && ImGui::IsKeyPressed(ImGuiKey_Enter))
			{
				executeEntry(entries[filteredEntries.front()]);
				return;
			}

			ImGui::TextDisabled("%zu result%s", filteredEntries.size(), filteredEntries.size() == 1 ? "" : "s");
			ImGui::BeginChild("##BlueprintNodeSearchResults", ImVec2(paletteWidth, 340.0f), false);
			if (filteredEntries.empty())
			{
				ImGui::TextDisabled("No nodes found.");
				ImGui::EndChild();
				return;
			}

			std::string lastCategory;
			constexpr float nameColumnWidth = 168.0f;
			constexpr float rowHeight = 24.0f;
			for (size_t filteredIndex : filteredEntries)
			{
				const PaletteEntry& entry = entries[filteredIndex];
				if (entry.m_Category != lastCategory)
				{
					if (!lastCategory.empty())
						ImGui::Spacing();
					ImGui::TextDisabled("%s", entry.m_Category.c_str());
					lastCategory = entry.m_Category;
				}

				ImGui::PushID(static_cast<int>(filteredIndex));
				if (ImGui::Selectable(entry.m_Name.c_str(), false, 0, ImVec2(nameColumnWidth, rowHeight)))
					executeEntry(entry);
				ImGui::SameLine();
				ImGui::TextDisabled("%s", entry.m_Detail.c_str());
				ImGui::PopID();
			}
			ImGui::EndChild();
		};

	if (ImGui::BeginDragDropTargetCustom(ImRect(canvasMin, canvasMax), ImGui::GetID("##TransitionBlueprintCanvasTarget")))
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ANIMATION_PARAMETER"))
		{
			const int parameterIndex = *static_cast<const int*>(payload->Data);
			if (parameterIndex >= 0 && std::cmp_less(parameterIndex, parameters.size()))
			{
				const AnimationControllerParameter& parameter = parameters[parameterIndex];
				const AnimationBlueprintNodeType conditionType = (parameter.m_Type == AnimationParameterType::Float || parameter.m_Type == AnimationParameterType::Int) ? AnimationBlueprintNodeType::Greater : AnimationBlueprintNodeType::If;
				createConditionChain(conditionType, screenToGraph(ImGui::GetIO().MousePos), parameter.m_Name);
			}
		}
		ImGui::EndDragDropTarget();
	}

	auto nodeMin = [&](const AnimationControllerBlueprintNode& node) -> ImVec2
		{
			return graphToScreen(node.m_GraphPosition);
		};

	auto inputPinPosition = [&](const AnimationControllerBlueprintNode& node, uint32_t inputPin) -> ImVec2
		{
			const ImVec2 min = nodeMin(node);
			const ImVec2 size = nodeScreenSize(node.m_Type);
			if (node.m_Type == AnimationBlueprintNodeType::Result)
				return { min.x, min.y + size.y * 0.72f };
			if (node.m_Type == AnimationBlueprintNodeType::Reroute)
				return { min.x, min.y + size.y * 0.70f };
			if (node.m_Type == AnimationBlueprintNodeType::Greater || node.m_Type == AnimationBlueprintNodeType::Less || node.m_Type == AnimationBlueprintNodeType::Equals || node.m_Type == AnimationBlueprintNodeType::NotEquals)
				return { min.x, min.y + (inputPin == 0 ? size.y * 0.50f : size.y * 0.74f) };
			if (node.m_Type == AnimationBlueprintNodeType::If || node.m_Type == AnimationBlueprintNodeType::IfNot || node.m_Type == AnimationBlueprintNodeType::Greater || node.m_Type == AnimationBlueprintNodeType::Less || node.m_Type == AnimationBlueprintNodeType::Equals || node.m_Type == AnimationBlueprintNodeType::NotEquals)
				return { min.x, min.y + size.y * 0.62f };
			if (inputPinCount(node.m_Type) == 2)
				return { min.x, min.y + (inputPin == 0 ? size.y * 0.38f : size.y * 0.70f) };
			return { min.x, min.y + size.y * 0.5f };
		};

	auto outputPinPosition = [&](const AnimationControllerBlueprintNode& node, uint32_t outputPin) -> ImVec2
		{
			const ImVec2 min = nodeMin(node);
			const ImVec2 size = nodeScreenSize(node.m_Type);
			if (node.m_Type == AnimationBlueprintNodeType::Reroute)
				return { min.x + size.x, min.y + size.y * 0.70f };
			if (node.m_Type == AnimationBlueprintNodeType::If || node.m_Type == AnimationBlueprintNodeType::IfNot || node.m_Type == AnimationBlueprintNodeType::Greater || node.m_Type == AnimationBlueprintNodeType::Less || node.m_Type == AnimationBlueprintNodeType::Equals || node.m_Type == AnimationBlueprintNodeType::NotEquals)
				return { min.x + size.x, min.y + size.y * 0.62f };
			if (outputPinCount(node.m_Type) == 2)
				return { min.x + size.x, min.y + (outputPin == 0 ? size.y * 0.38f : size.y * 0.70f) };
			return { min.x + size.x, min.y + size.y * 0.5f };
		};

	auto execInputPinPosition = [&](const AnimationControllerBlueprintNode& node) -> ImVec2
		{
			const ImVec2 min = nodeMin(node);
			const ImVec2 size = nodeScreenSize(node.m_Type);
			if (node.m_Type == AnimationBlueprintNodeType::Reroute)
				return { min.x, min.y + size.y * 0.32f };
			return { min.x, min.y + 38.0f * blueprintZoom };
		};

	auto execOutputPinPosition = [&](const AnimationControllerBlueprintNode& node, uint32_t outputPin) -> ImVec2
		{
			const ImVec2 min = nodeMin(node);
			const ImVec2 size = nodeScreenSize(node.m_Type);
			if (node.m_Type == AnimationBlueprintNodeType::Reroute)
				return { min.x + size.x, min.y + size.y * 0.32f };
			if (execOutputPinCount(node.m_Type) == 2)
				return { min.x + size.x, min.y + (outputPin == 0 ? 38.0f * blueprintZoom : size.y - 28.0f * blueprintZoom) };
			return { min.x + size.x, min.y + 38.0f * blueprintZoom };
		};

	auto nodeDetail = [&](const AnimationControllerBlueprintNode& node) -> std::string
		{
			if (node.m_Type == AnimationBlueprintNodeType::Start)
				return "Exec Entry";
			if (node.m_Type == AnimationBlueprintNodeType::Result)
				return "";
			if (node.m_Type == AnimationBlueprintNodeType::And || node.m_Type == AnimationBlueprintNodeType::Or)
				return "A / B";
			if (node.m_Type == AnimationBlueprintNodeType::Not)
				return "Invert";
			if (node.m_Type == AnimationBlueprintNodeType::Reroute)
				return "";
			if (node.m_Type == AnimationBlueprintNodeType::Parameter)
				return node.m_Parameter.empty() ? "No Parameter" : node.m_Parameter;
			if (const AnimationControllerParameter* parameter = findParameter(node.m_Parameter))
			{
				if (parameter->m_Type == AnimationParameterType::Float)
					return node.m_Parameter + " " + std::string(nodeLabel(node.m_Type)) + " " + FormatCompactFloat(node.m_Threshold);
				if (parameter->m_Type == AnimationParameterType::Int)
					return node.m_Parameter + " " + std::string(nodeLabel(node.m_Type)) + " " + std::to_string(node.m_IntValue);
				if (node.m_Type == AnimationBlueprintNodeType::Equals || node.m_Type == AnimationBlueprintNodeType::NotEquals)
					return node.m_Parameter + (node.m_BoolValue ? " == true" : " == false");
				return node.m_Parameter;
			}
			return "Missing Parameter";
		};

	drawList->PushClipRect(canvasMin, canvasMax, true);
	uint32_t blueprintLinkToRemove = 0;
	bool blueprintItemHovered = false;
	for (const AnimationControllerBlueprintLink& link : transition.m_BlueprintLinks)
	{
		const AnimationControllerBlueprintNode* output = findNode(link.m_OutputNode);
		const AnimationControllerBlueprintNode* input = findNode(link.m_InputNode);
		if (!output || !input)
			continue;
		const bool execLink = IsAnimationBlueprintExecPin(link.m_OutputPin) || IsAnimationBlueprintExecPin(link.m_InputPin);
		if (execLink && (!IsAnimationBlueprintExecPin(link.m_OutputPin) || link.m_InputPin != AnimationBlueprintExecInputPin || execInputPinCount(input->m_Type) == 0))
			continue;
		if (!execLink && (link.m_OutputPin >= outputPinCount(output->m_Type) || link.m_InputPin >= inputPinCount(input->m_Type)))
			continue;

		ImVec2 start;
		ImVec2 end;
		if (execLink)
		{
			uint32_t execIndex = 0;
			if (link.m_OutputPin == AnimationBlueprintFalsePin)
				execIndex = 1;
			start = execOutputPinPosition(*output, execIndex);
			end = execInputPinPosition(*input);
		}
		else
		{
			start = outputPinPosition(*output, link.m_OutputPin);
			end = inputPinPosition(*input, link.m_InputPin);
		}
		const NodePalette palette = nodePalette(*output);
		const bool hovered = canvasHovered && DistanceToSegment(mousePos, start, end) <= (execLink ? 10.0f : 8.0f);
		blueprintItemHovered |= hovered;
		if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)))
			blueprintLinkToRemove = link.m_Id;
		const ImU32 color = hovered ? IM_COL32(255, 246, 210, 255) : execLink ? IM_COL32(222, 228, 232, 235) : palette.m_Accent;
		const float tangent = std::max(48.0f, std::abs(end.x - start.x) * 0.42f);
		drawList->AddBezierCubic(start, ImVec2(start.x + tangent, start.y), ImVec2(end.x - tangent, end.y), end, color, hovered ? 3.0f : execLink ? 2.4f : 2.0f);
	}
	if (blueprintLinkToRemove != 0)
	{
		std::erase_if(transition.m_BlueprintLinks, [blueprintLinkToRemove](const AnimationControllerBlueprintLink& link)
			{
				return link.m_Id == blueprintLinkToRemove;
			});
	}

	uint32_t hoveredInputNode = 0;
	uint32_t hoveredInputPin = 0;
	uint32_t hoveredOutputNode = 0;
	uint32_t hoveredOutputPin = 0;
	uint32_t nodeToDuplicate = 0;
	uint32_t nodeToRemove = 0;
	uint32_t nodeToReset = 0;
	auto removeInputLinks = [&](uint32_t nodeId, uint32_t inputPin)
		{
			std::erase_if(transition.m_BlueprintLinks, [nodeId, inputPin](const AnimationControllerBlueprintLink& link)
				{
					return link.m_InputNode == nodeId && link.m_InputPin == inputPin;
				});
		};
	auto removeOutputLinks = [&](uint32_t nodeId, uint32_t outputPin)
		{
			std::erase_if(transition.m_BlueprintLinks, [nodeId, outputPin](const AnimationControllerBlueprintLink& link)
				{
					return link.m_OutputNode == nodeId && link.m_OutputPin == outputPin;
				});
		};
	auto removeIncomingLinks = [&](uint32_t nodeId)
		{
			std::erase_if(transition.m_BlueprintLinks, [nodeId](const AnimationControllerBlueprintLink& link)
				{
					return link.m_InputNode == nodeId;
				});
		};
	auto removeOutgoingLinks = [&](uint32_t nodeId)
		{
			std::erase_if(transition.m_BlueprintLinks, [nodeId](const AnimationControllerBlueprintLink& link)
				{
					return link.m_OutputNode == nodeId;
				});
		};
	auto drawExecPin = [&](const ImVec2& pinPosition, bool hovered, bool output)
		{
			const ImU32 fill = hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(226, 232, 238, 255);
			constexpr ImU32 outline = IM_COL32(10, 16, 22, 210);
			const float radius = hovered ? 7.0f : 6.0f;
			const ImVec2 tip = output ? ImVec2(pinPosition.x + radius + 1.0f, pinPosition.y) : ImVec2(pinPosition.x + radius - 1.0f, pinPosition.y);
			const ImVec2 top = ImVec2(pinPosition.x - radius + 1.0f, pinPosition.y - radius);
			const ImVec2 bottom = ImVec2(pinPosition.x - radius + 1.0f, pinPosition.y + radius);
			drawList->AddTriangleFilled(tip, top, bottom, fill);
			drawList->AddTriangle(tip, top, bottom, outline, 1.2f);
		};
	auto hasInputLink = [&](uint32_t nodeId, uint32_t inputPin)
		{
			return std::ranges::any_of(transition.m_BlueprintLinks, [nodeId, inputPin](const AnimationControllerBlueprintLink& link)
			{
				return link.m_InputNode == nodeId && link.m_InputPin == inputPin;
			});
		};
	auto inputValueType = [&](const AnimationControllerBlueprintNode& node) -> AnimationParameterType
		{
			if (node.m_Type == AnimationBlueprintNodeType::If || node.m_Type == AnimationBlueprintNodeType::IfNot || node.m_Type == AnimationBlueprintNodeType::Not || node.m_Type == AnimationBlueprintNodeType::And || node.m_Type == AnimationBlueprintNodeType::Or || node.m_Type == AnimationBlueprintNodeType::Result)
				return AnimationParameterType::Bool;
			if (node.m_Type == AnimationBlueprintNodeType::Equals || node.m_Type == AnimationBlueprintNodeType::NotEquals)
			{
				if (const AnimationControllerParameter* parameter = findParameter(node.m_Parameter))
					return parameter->m_Type;
			}
			if (node.m_Type == AnimationBlueprintNodeType::Greater || node.m_Type == AnimationBlueprintNodeType::Less)
			{
				if (const AnimationControllerParameter* parameter = findParameter(node.m_Parameter); parameter && parameter->m_Type == AnimationParameterType::Int)
					return AnimationParameterType::Int;
			}
			return AnimationParameterType::Float;
		};
	auto drawInputDefaultEditor = [&](AnimationControllerBlueprintNode& node, uint32_t pin)
		{
			if (pin >= 2 || node.m_Type == AnimationBlueprintNodeType::Reroute || hasInputLink(node.m_Id, pin))
				return false;

			const ImVec2 pinPosition = inputPinPosition(node, pin);
			const AnimationParameterType valueType = inputValueType(node);
			ImGui::PushID(static_cast<int>(node.m_Id));
			ImGui::PushID(static_cast<int>(pin) + 7000);
			bool active;
			if (valueType == AnimationParameterType::Bool || valueType == AnimationParameterType::Trigger)
			{
				ImGui::SetCursorScreenPos(ImVec2(pinPosition.x + 58.0f, pinPosition.y - 10.0f));
				if (ImGui::Checkbox("##PinDefaultBool", &node.m_InputBoolValues[pin]))
					node.m_BoolValue = node.m_InputBoolValues[1];
				active = ImGui::IsItemHovered() || ImGui::IsItemActive();
			}
			else if (valueType == AnimationParameterType::Int)
			{
				ImGui::SetCursorScreenPos(ImVec2(pinPosition.x + 46.0f, pinPosition.y - 11.0f));
				ImGui::SetNextItemWidth(76.0f);
				if (ImGui::DragInt("##PinDefaultInt", &node.m_InputIntValues[pin], 1.0f))
				{
					node.m_IntValue = node.m_InputIntValues[1];
					node.m_InputFloatValues[pin] = static_cast<float>(node.m_InputIntValues[pin]);
				}
				active = ImGui::IsItemHovered() || ImGui::IsItemActive();
			}
			else
			{
				ImGui::SetCursorScreenPos(ImVec2(pinPosition.x + 46.0f, pinPosition.y - 11.0f));
				ImGui::SetNextItemWidth(82.0f);
				if (ImGui::DragFloat("##PinDefaultFloat", &node.m_InputFloatValues[pin], 0.01f, 0.0f, 0.0f, "%.3g"))
					node.m_Threshold = node.m_InputFloatValues[1];
				active = ImGui::IsItemHovered() || ImGui::IsItemActive();
			}
			ImGui::PopID();
			ImGui::PopID();
			return active;
		};
	for (AnimationControllerBlueprintNode& node : transition.m_BlueprintNodes)
	{
		constexpr float pinHitSize = 22.0f;
		const ImVec2 size = nodeScreenSize(node.m_Type);
		const ImVec2 min = nodeMin(node);
		const ImVec2 max(min.x + size.x, min.y + size.y);
		const NodePalette palette = nodePalette(node);
		const bool selected = m_SelectedBlueprintNodeId == node.m_Id;

		drawList->AddRectFilled(ImVec2(min.x + 2.0f, min.y + 3.0f), ImVec2(max.x + 2.0f, max.y + 3.0f), IM_COL32(0, 0, 0, 80), 4.0f);
		drawList->AddRectFilled(min, max, palette.m_Fill, 4.0f);
		drawList->AddRect(min, max, selected ? IM_COL32(238, 244, 250, 245) : palette.m_Border, 4.0f, 0, selected ? 2.2f : 1.35f);
		drawList->AddRectFilled(min, ImVec2(max.x, min.y + 24.0f), IM_COL32(12, 18, 25, 122), 4.0f, ImDrawFlags_RoundCornersTop);
		drawList->AddRectFilled(min, ImVec2(min.x + 4.0f, max.y), palette.m_Accent, 4.0f, ImDrawFlags_RoundCornersLeft);
		drawList->AddText(ImVec2(min.x + 12.0f, min.y + 6.0f), IM_COL32(238, 244, 250, 255), nodeLabel(node.m_Type));
		const std::string detail = node.m_Type == AnimationBlueprintNodeType::Parameter ? nodeDetail(node) : std::string{};
		if (!detail.empty())
		{
			const ImVec2 detailMin(min.x + 12.0f, min.y + size.y - 24.0f);
			const ImVec2 detailMax(max.x - 42.0f, max.y - 6.0f);
			drawList->PushClipRect(detailMin, detailMax, true);
			drawList->AddText(detailMin, IM_COL32(154, 166, 178, 255), detail.c_str());
			drawList->PopClipRect();
		}

		if (execInputPinCount(node.m_Type) > 0)
		{
			const ImVec2 pinPosition = execInputPinPosition(node);
			const bool pending = m_PendingBlueprintLinkNodeId != 0 && !m_PendingBlueprintLinkFromInput && IsAnimationBlueprintExecPin(m_PendingBlueprintLinkPin);
			const bool hovered = pending && ImGui::IsMouseHoveringRect(ImVec2(pinPosition.x - pinHitSize * 0.5f, pinPosition.y - pinHitSize * 0.5f), ImVec2(pinPosition.x + pinHitSize * 0.5f, pinPosition.y + pinHitSize * 0.5f));
			if (hovered)
			{
				hoveredInputNode = node.m_Id;
				hoveredInputPin = AnimationBlueprintExecInputPin;
			}
			drawExecPin(pinPosition, hovered, false);
			const char* label = execInputPinLabel(node.m_Type);
			if (label[0] != '\0')
				drawList->AddText(ImVec2(pinPosition.x + 12.0f, pinPosition.y - 7.0f), IM_COL32(210, 218, 224, 240), label);
			ImGui::SetCursorScreenPos(ImVec2(pinPosition.x - pinHitSize * 0.5f, pinPosition.y - pinHitSize * 0.5f));
			ImGui::PushID(static_cast<int>(node.m_Id));
			ImGui::PushID("ExecInput");
			ImGui::InvisibleButton("##BlueprintExecInputPin", ImVec2(pinHitSize, pinHitSize), ImGuiButtonFlags_AllowOverlap);
			const bool execInputHovered = ImGui::IsItemHovered();
			blueprintItemHovered |= execInputHovered;
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				m_PendingBlueprintLinkNodeId = node.m_Id;
				m_PendingBlueprintLinkPin = AnimationBlueprintExecInputPin;
				m_PendingBlueprintLinkFromInput = true;
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				removeInputLinks(node.m_Id, AnimationBlueprintExecInputPin);
			if (execInputHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && m_PendingBlueprintLinkNodeId != 0 && !m_PendingBlueprintLinkFromInput)
			{
				addLink(m_PendingBlueprintLinkNodeId, m_PendingBlueprintLinkPin, node.m_Id, AnimationBlueprintExecInputPin);
				m_PendingBlueprintLinkNodeId = 0;
				m_PendingBlueprintLinkFromInput = false;
			}
			ImGui::PopID();
			ImGui::PopID();
		}

		for (uint32_t pin = 0; pin < execOutputPinCount(node.m_Type); ++pin)
		{
			const uint32_t pinId = execOutputPinId(node.m_Type, pin);
			const ImVec2 pinPosition = execOutputPinPosition(node, pin);
			const char* label = execOutputPinLabel(node.m_Type, pin);
			const bool pending = m_PendingBlueprintLinkNodeId != 0 && m_PendingBlueprintLinkFromInput && IsAnimationBlueprintExecPin(m_PendingBlueprintLinkPin);
			const bool hovered = pending && ImGui::IsMouseHoveringRect(ImVec2(pinPosition.x - pinHitSize * 0.5f, pinPosition.y - pinHitSize * 0.5f), ImVec2(pinPosition.x + pinHitSize * 0.5f, pinPosition.y + pinHitSize * 0.5f));
			if (hovered)
			{
				hoveredOutputNode = node.m_Id;
				hoveredOutputPin = pinId;
			}
			if (label[0] != '\0')
			{
				const ImVec2 textSize = ImGui::CalcTextSize(label);
				drawList->AddText(ImVec2(pinPosition.x - textSize.x - 14.0f, pinPosition.y - 7.0f), IM_COL32(210, 218, 224, 240), label);
			}
			drawExecPin(pinPosition, hovered, true);
			ImGui::SetCursorScreenPos(ImVec2(pinPosition.x - pinHitSize * 0.5f, pinPosition.y - pinHitSize * 0.5f));
			ImGui::PushID(static_cast<int>(node.m_Id));
			ImGui::PushID(static_cast<int>(pin) + 2000);
			ImGui::InvisibleButton("##BlueprintExecOutputPin", ImVec2(pinHitSize, pinHitSize), ImGuiButtonFlags_AllowOverlap);
			const bool execOutputHovered = ImGui::IsItemHovered();
			blueprintItemHovered |= execOutputHovered;
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				m_PendingBlueprintLinkNodeId = node.m_Id;
				m_PendingBlueprintLinkPin = pinId;
				m_PendingBlueprintLinkFromInput = false;
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				removeOutputLinks(node.m_Id, pinId);
			if (execOutputHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && m_PendingBlueprintLinkNodeId != 0 && m_PendingBlueprintLinkFromInput)
			{
				addLink(node.m_Id, pinId, m_PendingBlueprintLinkNodeId, m_PendingBlueprintLinkPin);
				m_PendingBlueprintLinkNodeId = 0;
				m_PendingBlueprintLinkFromInput = false;
			}
			ImGui::PopID();
			ImGui::PopID();
		}

		for (uint32_t pin = 0; pin < inputPinCount(node.m_Type); ++pin)
		{
			const ImVec2 pinPosition = inputPinPosition(node, pin);
			const char* label = inputPinLabel(node.m_Type, pin);
			if (label[0] != '\0')
				drawList->AddText(ImVec2(pinPosition.x + 10.0f, pinPosition.y - 7.0f), IM_COL32(182, 194, 206, 235), label);
			const bool pending = m_PendingBlueprintLinkNodeId != 0 && !m_PendingBlueprintLinkFromInput && !IsAnimationBlueprintExecPin(m_PendingBlueprintLinkPin);
			const bool hovered = pending && ImGui::IsMouseHoveringRect(ImVec2(pinPosition.x - pinHitSize * 0.5f, pinPosition.y - pinHitSize * 0.5f), ImVec2(pinPosition.x + pinHitSize * 0.5f, pinPosition.y + pinHitSize * 0.5f));
			if (hovered)
			{
				hoveredInputNode = node.m_Id;
				hoveredInputPin = pin;
			}
			drawList->AddCircleFilled(pinPosition, hovered ? 6.2f : 4.8f, hovered ? IM_COL32(122, 196, 255, 255) : IM_COL32(94, 184, 178, 255));
			ImGui::SetCursorScreenPos(ImVec2(pinPosition.x - pinHitSize * 0.5f, pinPosition.y - pinHitSize * 0.5f));
			ImGui::PushID(static_cast<int>(node.m_Id));
			ImGui::PushID(static_cast<int>(pin));
			ImGui::InvisibleButton("##BlueprintInputPin", ImVec2(pinHitSize, pinHitSize), ImGuiButtonFlags_AllowOverlap);
			const bool inputPinItemHovered = ImGui::IsItemHovered();
			blueprintItemHovered |= inputPinItemHovered;
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				m_PendingBlueprintLinkNodeId = node.m_Id;
				m_PendingBlueprintLinkPin = pin;
				m_PendingBlueprintLinkFromInput = true;
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				removeInputLinks(node.m_Id, pin);
			if (inputPinItemHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && m_PendingBlueprintLinkNodeId != 0 && !m_PendingBlueprintLinkFromInput)
			{
				addLink(m_PendingBlueprintLinkNodeId, m_PendingBlueprintLinkPin, node.m_Id, pin);
				m_PendingBlueprintLinkNodeId = 0;
				m_PendingBlueprintLinkFromInput = false;
			}
			ImGui::PopID();
			ImGui::PopID();
		}

		for (uint32_t pin = 0; pin < outputPinCount(node.m_Type); ++pin)
		{
			const ImVec2 pinPosition = outputPinPosition(node, pin);
			const char* label = outputPinLabel(node.m_Type, pin);
			const bool pending = m_PendingBlueprintLinkNodeId != 0 && m_PendingBlueprintLinkFromInput && !IsAnimationBlueprintExecPin(m_PendingBlueprintLinkPin);
			const bool hovered = pending && ImGui::IsMouseHoveringRect(ImVec2(pinPosition.x - pinHitSize * 0.5f, pinPosition.y - pinHitSize * 0.5f), ImVec2(pinPosition.x + pinHitSize * 0.5f, pinPosition.y + pinHitSize * 0.5f));
			if (hovered)
			{
				hoveredOutputNode = node.m_Id;
				hoveredOutputPin = pin;
			}
			if (label[0] != '\0')
			{
				const ImVec2 textSize = ImGui::CalcTextSize(label);
				drawList->AddText(ImVec2(pinPosition.x - textSize.x - 12.0f, pinPosition.y - 7.0f), IM_COL32(182, 194, 206, 235), label);
			}
			drawList->AddCircleFilled(pinPosition, hovered ? 6.2f : 4.8f, hovered ? IM_COL32(255, 255, 255, 255) : pin == 1 ? IM_COL32(214, 126, 140, 255) : palette.m_Accent);
			ImGui::SetCursorScreenPos(ImVec2(pinPosition.x - pinHitSize * 0.5f, pinPosition.y - pinHitSize * 0.5f));
			ImGui::PushID(static_cast<int>(node.m_Id));
			ImGui::PushID(static_cast<int>(pin) + 1000);
			ImGui::InvisibleButton("##BlueprintOutputPin", ImVec2(pinHitSize, pinHitSize), ImGuiButtonFlags_AllowOverlap);
			const bool outputPinItemHovered = ImGui::IsItemHovered();
			blueprintItemHovered |= outputPinItemHovered;
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				m_PendingBlueprintLinkNodeId = node.m_Id;
				m_PendingBlueprintLinkPin = pin;
				m_PendingBlueprintLinkFromInput = false;
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				removeOutputLinks(node.m_Id, pin);
			if (outputPinItemHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && m_PendingBlueprintLinkNodeId != 0 && m_PendingBlueprintLinkFromInput)
			{
				addLink(node.m_Id, pin, m_PendingBlueprintLinkNodeId, m_PendingBlueprintLinkPin);
				m_PendingBlueprintLinkNodeId = 0;
				m_PendingBlueprintLinkFromInput = false;
			}
			ImGui::PopID();
			ImGui::PopID();
		}

		bool hasDefaultValueEditor = false;
		for (uint32_t pin = 0; pin < inputPinCount(node.m_Type); ++pin)
		{
			if (pin < 2 && node.m_Type != AnimationBlueprintNodeType::Reroute && !hasInputLink(node.m_Id, pin))
			{
				hasDefaultValueEditor = true;
				break;
			}
		}
		const float headerHeight = std::min(size.y, 24.0f * blueprintZoom);
		auto drawNodeDragArea = [&](const char* id, const ImVec2& position, const ImVec2& areaSize)
			{
				if (areaSize.x <= 1.0f || areaSize.y <= 1.0f)
					return;
				ImGui::SetCursorScreenPos(position);
				ImGui::PushID(static_cast<int>(node.m_Id));
				ImGui::InvisibleButton(id, areaSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_AllowOverlap);
				blueprintItemHovered |= ImGui::IsItemHovered();
				if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
					m_SelectedBlueprintNodeId = node.m_Id;
				if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
				{
					const ImVec2 delta = ImGui::GetIO().MouseDelta;
					node.m_GraphPosition.x += delta.x / blueprintZoom;
					node.m_GraphPosition.y += delta.y / blueprintZoom;
				}
				if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && Length(ImGui::GetMouseDragDelta(ImGuiMouseButton_Right)) < 2.0f)
				{
					m_SelectedBlueprintNodeId = node.m_Id;
					ImGui::OpenPopup("##BlueprintNodeContext");
				}
				ImGui::PopID();
			};

		drawNodeDragArea("##BlueprintNodeHeader", min, ImVec2(size.x, headerHeight));

		const float leftGutter = hasDefaultValueEditor ? std::min(142.0f, std::max(14.0f, size.x * 0.62f)) : inputPinCount(node.m_Type) > 0 ? 14.0f : 0.0f;
		const float rightGutter = outputPinCount(node.m_Type) > 0 ? 14.0f : 0.0f;
		drawNodeDragArea("##BlueprintNodeBody", ImVec2(min.x + leftGutter, min.y + headerHeight), ImVec2(std::max(24.0f, size.x - leftGutter - rightGutter), std::max(1.0f, size.y - headerHeight)));

		for (uint32_t pin = 0; pin < inputPinCount(node.m_Type); ++pin)
			blueprintItemHovered |= drawInputDefaultEditor(node, pin);
	}

	if (ImGui::BeginPopup("##BlueprintNodeContext"))
	{
		if (AnimationControllerBlueprintNode* contextNode = findNode(m_SelectedBlueprintNodeId))
		{
			const bool systemNode = isSystemBlueprintNode(contextNode->m_Type);
			ImGui::TextDisabled("%s Node", nodeLabel(contextNode->m_Type));
			ImGui::Separator();

			ImGui::BeginDisabled(systemNode);
			if (ImGui::MenuItem("Duplicate Node", "Ctrl+D"))
				nodeToDuplicate = contextNode->m_Id;
			if (ImGui::MenuItem("Delete Node", "Del"))
				nodeToRemove = contextNode->m_Id;
			ImGui::EndDisabled();

			if (ImGui::MenuItem("Break Incoming Links"))
				removeIncomingLinks(contextNode->m_Id);
			if (ImGui::MenuItem("Break Outgoing Links"))
				removeOutgoingLinks(contextNode->m_Id);
			if (ImGui::MenuItem("Break All Links"))
				removeLinksForNode(contextNode->m_Id);

			ImGui::Separator();
			ImGui::BeginDisabled(systemNode);
			if (ImGui::BeginMenu("Convert To"))
			{
				constexpr std::array<AnimationBlueprintNodeType, 10> editableTypes =
				{
					AnimationBlueprintNodeType::Parameter,
					AnimationBlueprintNodeType::If,
					AnimationBlueprintNodeType::IfNot,
					AnimationBlueprintNodeType::Greater,
					AnimationBlueprintNodeType::Less,
					AnimationBlueprintNodeType::Equals,
					AnimationBlueprintNodeType::NotEquals,
					AnimationBlueprintNodeType::Not,
					AnimationBlueprintNodeType::And,
					AnimationBlueprintNodeType::Or
				};
				for (AnimationBlueprintNodeType type : editableTypes)
				{
					const bool selected = contextNode->m_Type == type;
					if (ImGui::MenuItem(nodeLabel(type), nullptr, selected))
					{
						contextNode->m_Type = type;
						ensureCompatibleParameter(*contextNode);
						pruneInvalidBlueprintLinks();
					}
				}
				if (ImGui::MenuItem("Reroute", nullptr, contextNode->m_Type == AnimationBlueprintNodeType::Reroute))
				{
					contextNode->m_Type = AnimationBlueprintNodeType::Reroute;
					ensureCompatibleParameter(*contextNode);
					pruneInvalidBlueprintLinks();
				}
				ImGui::EndMenu();
			}
			ImGui::EndDisabled();

			if (nodeUsesParameter(contextNode->m_Type))
			{
				if (ImGui::BeginMenu("Parameter"))
				{
					bool hasCompatibleParameter = false;
					for (const AnimationControllerParameter& parameter : parameters)
					{
						if (!parameterCompatibleWithNode(contextNode->m_Type, parameter))
							continue;
						hasCompatibleParameter = true;
						if (ImGui::MenuItem(parameter.m_Name.c_str(), frenum::to_string(parameter.m_Type).data(), contextNode->m_Parameter == parameter.m_Name))
							contextNode->m_Parameter = parameter.m_Name;
					}
					if (!hasCompatibleParameter)
						ImGui::TextDisabled("No compatible parameters.");
					ImGui::EndMenu();
				}
			}

			if (isConditionBlueprintNode(contextNode->m_Type) || contextNode->m_Type == AnimationBlueprintNodeType::Not || contextNode->m_Type == AnimationBlueprintNodeType::And || contextNode->m_Type == AnimationBlueprintNodeType::Or)
			{
				if (ImGui::MenuItem("Reset Input Defaults"))
					nodeToReset = contextNode->m_Id;
			}
		}
		else
			ImGui::TextDisabled("Node no longer exists.");
		ImGui::EndPopup();
	}

	const bool blueprintGraphItemHovered = blueprintItemHovered || ImGui::IsAnyItemHovered();
	if (canvasHovered && !blueprintGraphItemHovered)
	{
		ImGui::SetCursorScreenPos(canvasMin);
		ImGui::InvisibleButton("##TransitionBlueprintCanvasCapture", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	}

	if (canvasHovered && !blueprintGraphItemHovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
	{
		const ImVec2 delta = ImGui::GetIO().MouseDelta;
		m_BlueprintGraphPan.x += delta.x;
		m_BlueprintGraphPan.y += delta.y;
	}

	if (canvasHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !blueprintGraphItemHovered && Length(ImGui::GetMouseDragDelta(ImGuiMouseButton_Right)) < 2.0f)
		openBlueprintNodeMenu(screenToGraph(ImGui::GetIO().MousePos));

	if (m_PendingBlueprintLinkNodeId != 0)
	{
		if (const AnimationControllerBlueprintNode* pendingNode = findNode(m_PendingBlueprintLinkNodeId))
		{
			ImVec2 start;
			ImVec2 end = ImGui::GetIO().MousePos;
			if (m_PendingBlueprintLinkFromInput)
			{
				if (IsAnimationBlueprintExecPin(m_PendingBlueprintLinkPin))
					end = execInputPinPosition(*pendingNode);
				else
					end = inputPinPosition(*pendingNode, m_PendingBlueprintLinkPin);
				start = ImGui::GetIO().MousePos;
			}
			else
			{
				if (IsAnimationBlueprintExecPin(m_PendingBlueprintLinkPin))
				{
					uint32_t execIndex = m_PendingBlueprintLinkPin == AnimationBlueprintFalsePin ? 1 : 0;
					start = execOutputPinPosition(*pendingNode, execIndex);
				}
				else
					start = outputPinPosition(*pendingNode, m_PendingBlueprintLinkPin);
			}
			const float tangent = std::max(48.0f, std::abs(end.x - start.x) * 0.35f);
			drawList->AddBezierCubic(start, ImVec2(start.x + tangent, start.y), ImVec2(end.x - tangent, end.y), end, IsAnimationBlueprintExecPin(m_PendingBlueprintLinkPin) ? IM_COL32(222, 228, 232, 235) : IM_COL32(122, 196, 255, 230), IsAnimationBlueprintExecPin(m_PendingBlueprintLinkPin) ? 2.5f : 2.2f);
		}
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			if (m_PendingBlueprintLinkFromInput && hoveredOutputNode != 0)
				addLink(hoveredOutputNode, hoveredOutputPin, m_PendingBlueprintLinkNodeId, m_PendingBlueprintLinkPin);
			else if (!m_PendingBlueprintLinkFromInput && hoveredInputNode != 0)
				addLink(m_PendingBlueprintLinkNodeId, m_PendingBlueprintLinkPin, hoveredInputNode, hoveredInputPin);
			else if (canvasHovered)
				openBlueprintNodeMenu(screenToGraph(ImGui::GetIO().MousePos), m_PendingBlueprintLinkNodeId, m_PendingBlueprintLinkPin, m_PendingBlueprintLinkFromInput);
			m_PendingBlueprintLinkNodeId = 0;
			m_PendingBlueprintLinkFromInput = false;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) || !ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			m_PendingBlueprintLinkNodeId = 0;
			m_PendingBlueprintLinkFromInput = false;
		}
	}

	if (ImGui::BeginPopup("##TransitionBlueprintContext"))
	{
		drawBlueprintNodeMenu();
		ImGui::EndPopup();
	}
	drawList->PopClipRect();

	ImGui::SetCursorScreenPos(ImVec2(canvasMin.x, canvasMax.y));
	ImGui::Dummy(ImVec2(0.0f, 0.0f));

	if (nodeToReset != 0)
	{
		if (AnimationControllerBlueprintNode* node = findNode(nodeToReset))
			resetNodeDefaults(*node);
	}
	if (nodeToDuplicate != 0)
	{
		if (AnimationControllerBlueprintNode* source = findNode(nodeToDuplicate))
		{
			AnimationControllerBlueprintNode duplicate = *source;
			duplicate.m_Id = transition.m_NextBlueprintNodeId++;
			duplicate.m_GraphPosition += glm::vec2{ 30.0f, 30.0f };
			transition.m_BlueprintNodes.push_back(duplicate);
			m_SelectedBlueprintNodeId = duplicate.m_Id;
		}
	}
	if (nodeToRemove != 0)
	{
		removeLinksForNode(nodeToRemove);
		std::erase_if(transition.m_BlueprintNodes, [nodeToRemove](const AnimationControllerBlueprintNode& node)
			{
				return node.m_Id == nodeToRemove && node.m_Type != AnimationBlueprintNodeType::Start && node.m_Type != AnimationBlueprintNodeType::Result;
			});
		if (m_SelectedBlueprintNodeId == nodeToRemove)
			m_SelectedBlueprintNodeId = 0;
	}

	syncLegacyConditions();
	ImGui::EndChild();
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
			return std::ranges::any_of(states, [name](const AnimationControllerState& state)
			{
				return state.m_Name == name;
			});
		};

	auto parameterExists = [&](std::string_view name)
		{
			return std::ranges::any_of(parameters, [name](const AnimationControllerParameter& parameter)
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

	m_SelectedControllerStateIndex = std::clamp(m_SelectedControllerStateIndex, 0, static_cast<int>(states.size()) - 1);
	DrawControllerValidation();

	if (AnimationControllerTransition* selectedTransition = GetSelectedControllerTransition())
	{
		ImGui::TextDisabled("Transition Inspector");
		ImGui::Separator();
		if (m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource)
			ImGui::TextDisabled("Source: Any State");
		else if (m_SelectedTransitionSourceStateIndex >= 0 && std::cmp_less(m_SelectedTransitionSourceStateIndex, states.size()))
			ImGui::TextDisabled("Source: %s", states[m_SelectedTransitionSourceStateIndex].m_Name.c_str());

		if (ImGui::Button("Duplicate Transition", ImVec2(-1.0f, 0.0f)))
		{
			const AnimationControllerTransition duplicate = *selectedTransition;
			if (m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource)
			{
				auto& transitions = m_CurrentController->GetAnyStateTransitions();
				transitions.push_back(duplicate);
				m_SelectedTransitionIndex = static_cast<int>(transitions.size()) - 1;
			}
			else if (m_SelectedTransitionSourceStateIndex >= 0 && std::cmp_less(m_SelectedTransitionSourceStateIndex,
				states.size()))
			{
				auto& transitions = states[m_SelectedTransitionSourceStateIndex].m_Transitions;
				transitions.push_back(duplicate);
				m_SelectedTransitionIndex = static_cast<int>(transitions.size()) - 1;
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

	constexpr std::array<AnimationMotionType, 2> motionTypes =
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
			std::ranges::sort(state.m_BlendChildren, [](const AnimationBlendChild& left, const AnimationBlendChild& right)
			{
				return left.m_Threshold < right.m_Threshold;
			});
		}

		for (size_t childIndex = 0; childIndex < state.m_BlendChildren.size(); ++childIndex)
		{
			ImGui::PushID(static_cast<int>(childIndex));
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
				state.m_BlendChildren.erase(state.m_BlendChildren.begin() + static_cast<std::vector<AnimationBlendChild>::difference_type>(childIndex));
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
		m_SelectedTransitionIndex = static_cast<int>(state.m_Transitions.size()) - 1;
	}

	int transitionToMoveUp = -1;
	int transitionToMoveDown = -1;
	for (size_t transitionIndex = 0; transitionIndex < state.m_Transitions.size(); ++transitionIndex)
	{
		ImGui::PushID(static_cast<int>(transitionIndex));
		AnimationControllerTransition& transition = state.m_Transitions[transitionIndex];
		std::string header = "-> " + (transition.m_TargetState.empty() ? std::string("None") : transition.m_TargetState);
		if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button("Select In Graph", ImVec2(-1.0f, 0.0f)))
			{
				m_SelectedTransitionSourceStateIndex = m_SelectedControllerStateIndex;
				m_SelectedTransitionIndex = static_cast<int>(transitionIndex);
			}
			ImGui::BeginDisabled(transitionIndex == 0);
			if (ImGui::Button("Move Up"))
				transitionToMoveUp = static_cast<int>(transitionIndex);
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(transitionIndex + 1 >= state.m_Transitions.size());
			if (ImGui::Button("Move Down"))
				transitionToMoveDown = static_cast<int>(transitionIndex);
			ImGui::EndDisabled();
			DrawControllerTransitionInspector(transition, true);

			if (ImGui::Button("Remove Transition", ImVec2(-1.0f, 0.0f)))
			{
				state.m_Transitions.erase(state.m_Transitions.begin() + static_cast<std::vector<AnimationControllerTransition>::difference_type>(transitionIndex));
				ClearSelectedControllerTransition();
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (transitionToMoveUp > 0 && std::cmp_less(transitionToMoveUp, state.m_Transitions.size()))
	{
		std::swap(state.m_Transitions[transitionToMoveUp], state.m_Transitions[transitionToMoveUp - 1]);
		m_SelectedTransitionSourceStateIndex = m_SelectedControllerStateIndex;
		m_SelectedTransitionIndex = transitionToMoveUp - 1;
	}
	if (transitionToMoveDown >= 0 && transitionToMoveDown + 1 < static_cast<int>(state.m_Transitions.size()))
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
		m_SelectedTransitionIndex = static_cast<int>(transitions.size()) - 1;
	}
	auto& anyTransitions = m_CurrentController->GetAnyStateTransitions();
	int anyTransitionToMoveUp = -1;
	int anyTransitionToMoveDown = -1;
	for (size_t transitionIndex = 0; transitionIndex < anyTransitions.size(); ++transitionIndex)
	{
		ImGui::PushID(static_cast<int>(transitionIndex));
		const AnimationControllerTransition& transition = anyTransitions[transitionIndex];
		std::string label = "Any -> " + (transition.m_TargetState.empty() ? std::string("None") : transition.m_TargetState);
		if (ImGui::Selectable(label.c_str(), m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource &&
			std::cmp_equal(m_SelectedTransitionIndex, transitionIndex), 0, ImVec2(std::max(80.0f, width - 112.0f), 0.0f)))
		{
			m_SelectedTransitionSourceStateIndex = AnyStateTransitionSource;
			m_SelectedTransitionIndex = static_cast<int>(transitionIndex);
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(transitionIndex == 0);
		if (ImGui::SmallButton("Up"))
			anyTransitionToMoveUp = static_cast<int>(transitionIndex);
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(transitionIndex + 1 >= anyTransitions.size());
		if (ImGui::SmallButton("Down"))
			anyTransitionToMoveDown = static_cast<int>(transitionIndex);
		ImGui::EndDisabled();
		ImGui::PopID();
	}
	if (anyTransitionToMoveUp > 0 && std::cmp_less(anyTransitionToMoveUp, anyTransitions.size()))
	{
		std::swap(anyTransitions[anyTransitionToMoveUp], anyTransitions[anyTransitionToMoveUp - 1]);
		m_SelectedTransitionSourceStateIndex = AnyStateTransitionSource;
		m_SelectedTransitionIndex = anyTransitionToMoveUp - 1;
	}
	if (anyTransitionToMoveDown >= 0 && anyTransitionToMoveDown + 1 < static_cast<int>(anyTransitions.size()))
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

	if (const AssetMetadata& metadata = AssetManager::GetAssetMetadata(m_CurrentController->m_Handle); metadata)
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
			if (ImGui::Selectable(label.c_str(), std::cmp_equal(m_SelectedFrameIndex, i), 0, ImVec2(width - ImGui::GetStyle().WindowPadding.x, 0.0f)))
				m_SelectedFrameIndex = static_cast<int>(i);
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
		m_SelectedFrameIndex = static_cast<int>(m_CurrentAnimation->GetFrames().size() - 1);
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
			m_SelectedFrameIndex = std::min(m_SelectedFrameIndex, static_cast<int>(m_CurrentAnimation->GetFrames().size()) - 1);
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

	if (!m_CurrentAnimation || m_SelectedFrameIndex < 0 || std::cmp_greater_equal(m_SelectedFrameIndex,
		m_CurrentAnimation->GetFrames().size()))
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

	const ImVec2 previewPixelSize = GetAnimationFramePixelSize(frame, texture);
	const float maxPreview = std::max(48.0f, std::min(width, height - 42.0f));
	const float aspect = previewPixelSize.y > 0.0f ? previewPixelSize.x / previewPixelSize.y : 1.0f;
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
			if (frameIndex < 0 || std::cmp_greater_equal(frameIndex, m_CurrentAnimation->GetFrames().size()))
				return;

			const AnimationFrame& previewFrame = m_CurrentAnimation->GetFrames()[frameIndex];
			const AssetHandle textureHandle = previewFrame.m_Texture;
			if (!textureHandle || !AssetManager::IsAssetHandleValid(textureHandle) || AssetManager::GetAssetType(textureHandle) != AssetType::Texture2D)
				return;

			Ref<Texture2D> onionTexture = AssetManager::GetAsset<Texture2D>(textureHandle);
			if (!onionTexture || !onionTexture->IsLoaded())
				return;

			ImVec2 uv0;
			ImVec2 uv1;
			GetAnimationFrameImageUvs(previewFrame, onionTexture, uv0, uv1);
			ImGui::GetWindowDrawList()->AddImage(
				UI::ToImGuiTextureId(onionTexture->GetRendererId()),
				Add(imageMin, offset),
				Add(imageMax, offset),
				uv0,
				uv1,
				tint);
		};

	if (m_ShowOnionSkin)
	{
		drawFrameTexture(m_SelectedFrameIndex - 1, ImVec2(-7.0f, 0.0f), IM_COL32(120, 172, 255, 76));
		drawFrameTexture(m_SelectedFrameIndex + 1, ImVec2(7.0f, 0.0f), IM_COL32(255, 186, 104, 76));
	}
	drawFrameTexture(m_SelectedFrameIndex, ImVec2(0.0f, 0.0f), IM_COL32(255, 255, 255, 255));
	if (const TextureSpriteRect* sprite = GetAnimationFrameSpriteRect(frame))
		ImGui::TextDisabled("%ux%u  %s", sprite->m_Width, sprite->m_Height, sprite->m_Name.c_str());
	else
		ImGui::TextDisabled("%ux%u", texture->GetWidth(), texture->GetHeight());
}

void AnimationEditorPanel::DrawFrameEditor(float width)
{
	if (!m_CurrentAnimation)
		return;

	DrawFrameBatchTools(width);

	if (m_SelectedFrameIndex < 0 || std::cmp_greater_equal(m_SelectedFrameIndex, m_CurrentAnimation->GetFrames().size()))
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
	ImGui::BeginDisabled(m_SelectedFrameIndex >= static_cast<int>(m_CurrentAnimation->GetFrames().size()) - 1);
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
			frame.m_TextureSpriteIndex = -1;
		};
	const auto assetReferenceCallback = [&frame](AssetHandle handle, int32_t spriteIndex)
		{
			frame.m_Texture = handle;
			frame.m_TextureSpriteIndex = spriteIndex;
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
			{
				textureLabel = AssetManager::GetAssetMetadata(frame.m_Texture).m_Filepath.generic_string();
				const auto& sprites = AssetManager::GetAssetMetadata(frame.m_Texture).m_TextureSettings.m_Sprites;
				if (frame.m_TextureSpriteIndex >= 0 && std::cmp_less(frame.m_TextureSpriteIndex, sprites.size()))
					textureLabel += " / " + sprites[static_cast<size_t>(frame.m_TextureSpriteIndex)].m_Name;
			}
			else
				textureLabel = "Invalid texture handle";
		}
		UI::DragDropTarget(AssetType::Texture2D, dragDropCallback, textureLabel.c_str(), true, std::max(160.0f, width - 140.0f), 0.0f, true, nullptr, assetReferenceCallback);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Sprite");
		ImGui::TableNextColumn();
		if (frame.m_Texture != 0 && AssetManager::IsAssetHandleValid(frame.m_Texture) && AssetManager::GetAssetType(frame.m_Texture) == AssetType::Texture2D)
		{
			const AssetMetadata& metadata = AssetManager::GetAssetMetadata(frame.m_Texture);
			const auto& sprites = metadata.m_TextureSettings.m_Sprites;
			bool hasSlices = !sprites.empty();
			const bool validSpriteIndex = frame.m_TextureSpriteIndex >= 0 && std::cmp_less(frame.m_TextureSpriteIndex, sprites.size());
			const char* spritePreview = validSpriteIndex ? sprites[static_cast<size_t>(frame.m_TextureSpriteIndex)].m_Name.c_str() : "Full Texture";
			if (ImGui::BeginCombo("##FrameSprite", spritePreview))
			{
				if (ImGui::Selectable("Full Texture", frame.m_TextureSpriteIndex < 0))
					frame.m_TextureSpriteIndex = -1;
				for (int32_t spriteIndex = 0; std::cmp_less(spriteIndex, sprites.size()); ++spriteIndex)
				{
					const TextureSpriteRect& sprite = sprites[static_cast<size_t>(spriteIndex)];
					const bool selected = frame.m_TextureSpriteIndex == spriteIndex;
					std::string label = sprite.m_Name + "  (" + std::to_string(sprite.m_Width) + "x" + std::to_string(sprite.m_Height) + ")";
					if (ImGui::Selectable(label.c_str(), selected))
						frame.m_TextureSpriteIndex = spriteIndex;
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::BeginDisabled(!hasSlices);
			if (ImGui::Button("Add All Slices As Frames", ImVec2(-1.0f, 0.0f)))
			{
				auto& frames = m_CurrentAnimation->GetFrames();
				const int firstAddedIndex = static_cast<int>(frames.size());
				for (int32_t spriteIndex = 0; std::cmp_less(spriteIndex, sprites.size()); ++spriteIndex)
				{
					AnimationFrame sliceFrame;
					sliceFrame.m_Texture = frame.m_Texture;
					sliceFrame.m_TextureSpriteIndex = spriteIndex;
					sliceFrame.m_Duration = std::max(0.001f, m_DefaultFrameDuration);
					frames.push_back(sliceFrame);
				}
				if (!sprites.empty())
					m_SelectedFrameIndex = firstAddedIndex;
				StopPreview(false);
			}
			ImGui::EndDisabled();
		}
		else
		{
			frame.m_TextureSpriteIndex = -1;
			ImGui::TextDisabled("Assign a texture with sprite slices.");
		}

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

	const float keyTime = m_CurrentAnimation->GetFrameStartTime(static_cast<size_t>(m_SelectedFrameIndex));
	ImGui::Separator();
	ImGui::TextDisabled("Events");
	if (ImGui::Button("+ Event"))
		m_CurrentAnimation->GetEvents().push_back({
			.m_Time = keyTime,
			.m_Name = "Event"
		});

	auto& events = m_CurrentAnimation->GetEvents();
	for (size_t eventIndex = 0; eventIndex < events.size(); ++eventIndex)
	{
		ImGui::PushID(static_cast<int>(eventIndex));
		ImGui::DragFloat("Time", &events[eventIndex].m_Time, 0.01f, 0.0f);
		ImGui::InputText("Name", &events[eventIndex].m_Name);
		if (ImGui::Button("Remove Event", ImVec2(-1.0f, 0.0f)))
		{
			events.erase(events.begin() + static_cast<std::vector<AnimationEventKey>::difference_type>(eventIndex));
			ImGui::PopID();
			break;
		}
		ImGui::Separator();
		ImGui::PopID();
	}

	ImGui::TextDisabled("Property Tracks");
	if (ImGui::Button("+ Translation"))
		m_CurrentAnimation->GetTranslationKeys().push_back({
			.m_Time = keyTime,
			.m_Value = glm::vec3{ 0.0f }
		});
	ImGui::SameLine();
	if (ImGui::Button("+ Rotation"))
		m_CurrentAnimation->GetRotationKeys().push_back({
			.m_Time = keyTime,
			.m_Value = glm::vec3{ 0.0f }
		});
	if (ImGui::Button("+ Scale"))
		m_CurrentAnimation->GetScaleKeys().push_back({
			.m_Time = keyTime,
			.m_Value = glm::vec3{ 1.0f }
		});
	ImGui::SameLine();
	if (ImGui::Button("+ Color"))
		m_CurrentAnimation->GetColorKeys().push_back({
			.m_Time = keyTime,
			.m_Value = glm::vec4{ 1.0f }
		});

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
				ImGui::PushID(static_cast<int>(keyIndex));
				ImGui::DragFloat("Time", &keys[keyIndex].m_Time, 0.01f, 0.0f);
				ImGui::DragFloat3("Value", &keys[keyIndex].m_Value.x, 0.01f);
				if (ImGui::Button("Remove Key", ImVec2(-1.0f, 0.0f)))
				{
					keys.erase(keys.begin() + static_cast<std::vector<AnimationVec3Key>::difference_type>(keyIndex));
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
				ImGui::PushID(static_cast<int>(keyIndex));
				ImGui::DragFloat("Time", &keys[keyIndex].m_Time, 0.01f, 0.0f);
				ImGui::ColorEdit4("Value", &keys[keyIndex].m_Value.x);
				if (ImGui::Button("Remove Key", ImVec2(-1.0f, 0.0f)))
				{
					keys.erase(keys.begin() + static_cast<std::vector<AnimationVec4Key>::difference_type>(keyIndex));
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
		std::ranges::reverse(frames);
		if (oldSelectedFrame >= 0 && std::cmp_less(oldSelectedFrame, frames.size()))
			m_SelectedFrameIndex = static_cast<int>(frames.size()) - 1 - oldSelectedFrame;
		StopPreview(false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove Empty Frames"))
	{
		const AssetHandle selectedTexture =
			m_SelectedFrameIndex >= 0 && std::cmp_less(m_SelectedFrameIndex, frames.size()) ? frames[m_SelectedFrameIndex].m_Texture : AssetHandle(0);
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
						m_SelectedFrameIndex = static_cast<int>(frameIndex);
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

	std::ranges::sort(textureFiles, [](const std::filesystem::path& left, const std::filesystem::path& right)
	{
		return left.filename().string() < right.filename().string();
	});

	auto editorAssetManager = Project::GetActive()->GetEditorAssetManager();
	auto& frames = m_CurrentAnimation->GetFrames();
	const int firstImportedFrame = static_cast<int>(frames.size());
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
				m_SelectedFrameIndex = std::clamp(snapshot.m_Clip.m_SelectedFrameIndex, 0, static_cast<int>(clip->GetFrames().size()) - 1);
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
				m_SelectedControllerStateIndex = -1;
			else
				m_SelectedControllerStateIndex = std::clamp(snapshot.m_Controller.m_SelectedStateIndex, 0, static_cast<int>(controller->GetStates().size()) - 1);
			m_SelectedControllerParameterIndex = snapshot.m_Controller.m_SelectedParameterIndex;
			if (std::cmp_greater_equal(m_SelectedControllerParameterIndex, controller->GetParameters().size()))
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
		m_SelectedControllerStateIndex = -1;
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

void AnimationEditorPanel::RegisterShortcuts(EditorShortcutManager& shortcuts)
{
	auto addShortcut = [this, &shortcuts](const char* id, const char* displayName, UI::EditorShortcutAction action, const UI::ShortcutBinding& binding)
	{
		shortcuts.Add(
			EditorShortcutScope::AnimationEditor,
			std::string("animation.") + id,
			displayName,
			"Edit",
			binding,
			[this, action]()
			{
				ExecuteShortcutAction(action);
				return true;
			},
			[]() { return true; },
			[this]() { return WantsShortcutCapture(); });
	};

	addShortcut("undo", "Undo Animation Edit", UI::EditorShortcutAction::Undo, { Key::Z, true, false, false });
	addShortcut("redo", "Redo Animation Edit", UI::EditorShortcutAction::Redo, { Key::Y, true, false, false });
	addShortcut("copy", "Copy Animation Selection", UI::EditorShortcutAction::Copy, { Key::C, true, false, false });
	addShortcut("cut", "Cut Animation Selection", UI::EditorShortcutAction::Cut, { Key::X, true, false, false });
	addShortcut("paste", "Paste Animation Selection", UI::EditorShortcutAction::Paste, { Key::V, true, false, false });
	addShortcut("duplicate", "Duplicate Animation Selection", UI::EditorShortcutAction::DuplicateEntity, { Key::D, true, false, false });
	addShortcut("delete", "Delete Animation Selection", UI::EditorShortcutAction::DeleteEntity, { Key::Delete, false, false, false });
}

bool AnimationEditorPanel::ShouldConsumeShortcutAction(UI::EditorShortcutAction action) const
{
	switch (action) // NOLINT(clang-diagnostic-switch-enum)
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

	switch (action) // NOLINT(clang-diagnostic-switch-enum)
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
		if (!m_CurrentAnimation || m_SelectedFrameIndex < 0 || std::cmp_greater_equal(m_SelectedFrameIndex,
			m_CurrentAnimation->GetFrames().size()))
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
		if (m_SelectedBlueprintNodeId != 0)
		{
			const auto nodeIt = std::ranges::find_if(transition->m_BlueprintNodes, [this](const AnimationControllerBlueprintNode& node)
			{
				return node.m_Id == m_SelectedBlueprintNodeId && node.m_Type != AnimationBlueprintNodeType::Start && node.m_Type != AnimationBlueprintNodeType::Result;
			});
			if (nodeIt != transition->m_BlueprintNodes.end())
			{
				m_Clipboard = {};
				m_Clipboard.m_Type = AnimationEditorClipboardType::ControllerBlueprintNode;
				m_Clipboard.m_BlueprintNode = *nodeIt;
				return true;
			}
		}

		m_Clipboard = {};
		m_Clipboard.m_Type = AnimationEditorClipboardType::ControllerTransition;
		m_Clipboard.m_Transition = *transition;
		return true;
	}

	auto& states = m_CurrentController->GetStates();
	if (m_SelectedControllerStateIndex < 0 || std::cmp_greater_equal(m_SelectedControllerStateIndex, states.size()))
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

	if (m_Clipboard.m_Type == AnimationEditorClipboardType::ControllerBlueprintNode)
	{
		AnimationControllerTransition* transition = GetSelectedControllerTransition();
		if (!transition || m_Clipboard.m_BlueprintNode.m_Type == AnimationBlueprintNodeType::Start || m_Clipboard.m_BlueprintNode.m_Type == AnimationBlueprintNodeType::Result)
			return false;
		PushHistory();
		AnimationControllerBlueprintNode node = m_Clipboard.m_BlueprintNode;
		node.m_Id = transition->m_NextBlueprintNodeId++;
		node.m_GraphPosition += glm::vec2{ 30.0f, 30.0f };
		transition->m_BlueprintNodes.push_back(node);
		m_SelectedBlueprintNodeId = node.m_Id;
		return true;
	}

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
		if (!m_CurrentAnimation || m_SelectedFrameIndex < 0 || std::cmp_greater_equal(m_SelectedFrameIndex,
			m_CurrentAnimation->GetFrames().size()))
			return false;
		PushHistory();
		return DuplicateSelectedFrame();
	}

	if (!m_CurrentController)
		return false;

	if (AnimationControllerTransition* selectedTransition = GetSelectedControllerTransition())
	{
		if (m_SelectedBlueprintNodeId != 0)
		{
			const auto nodeIt = std::ranges::find_if(selectedTransition->m_BlueprintNodes, [this](const AnimationControllerBlueprintNode& node)
			{
				return node.m_Id == m_SelectedBlueprintNodeId && node.m_Type != AnimationBlueprintNodeType::Start && node.m_Type != AnimationBlueprintNodeType::Result;
			});
			if (nodeIt != selectedTransition->m_BlueprintNodes.end())
			{
				PushHistory();
				AnimationControllerBlueprintNode duplicate = *nodeIt;
				duplicate.m_Id = selectedTransition->m_NextBlueprintNodeId++;
				duplicate.m_GraphPosition += glm::vec2{ 30.0f, 30.0f };
				selectedTransition->m_BlueprintNodes.push_back(duplicate);
				m_SelectedBlueprintNodeId = duplicate.m_Id;
				return true;
			}
		}

		PushHistory();
		const AnimationControllerTransition duplicate = *selectedTransition;
		if (m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource)
		{
			auto& transitions = m_CurrentController->GetAnyStateTransitions();
			transitions.push_back(duplicate);
			m_SelectedTransitionIndex = static_cast<int>(transitions.size()) - 1;
			return true;
		}

		auto& states = m_CurrentController->GetStates();
		if (m_SelectedTransitionSourceStateIndex >= 0 && std::cmp_less(m_SelectedTransitionSourceStateIndex, states.size()))
		{
			auto& transitions = states[m_SelectedTransitionSourceStateIndex].m_Transitions;
			transitions.push_back(duplicate);
			m_SelectedTransitionIndex = static_cast<int>(transitions.size()) - 1;
			return true;
		}
		return false;
	}

	auto& states = m_CurrentController->GetStates();
	if (states.empty())
	{
		AnimationControllerState& state = m_CurrentController->AddState("Entry");
		m_CurrentController->SetDefaultState(state.m_Name);
		m_SelectedControllerStateIndex = 0;
	}
	else if (m_SelectedControllerStateIndex < 0 || std::cmp_greater_equal(m_SelectedControllerStateIndex, states.size()))
	{
		const auto defaultIt = std::ranges::find_if(states, [this](const AnimationControllerState& state)
		{
			return state.m_Name == m_CurrentController->GetDefaultState();
		});
		m_SelectedControllerStateIndex = defaultIt != states.end() ? static_cast<int>(std::distance(states.begin(), defaultIt)) : 0;
	}
	if (m_SelectedControllerStateIndex < 0 || std::cmp_greater_equal(m_SelectedControllerStateIndex, states.size()))
		return false;
	PushHistory();
	return DuplicateSelectedControllerState();
}

bool AnimationEditorPanel::DeleteSelection()
{
	if (m_EditorMode == AnimationEditorMode::Clip)
	{
		if (!m_CurrentAnimation || m_SelectedFrameIndex < 0 || std::cmp_greater_equal(m_SelectedFrameIndex,
			m_CurrentAnimation->GetFrames().size()))
			return false;
		PushHistory();
		return DeleteSelectedFrame();
	}

	if (!m_CurrentController)
		return false;

	if (AnimationControllerTransition* selectedTransition = GetSelectedControllerTransition())
	{
		if (m_SelectedBlueprintNodeId != 0)
		{
			const auto nodeIt = std::ranges::find_if(selectedTransition->m_BlueprintNodes, [this](const AnimationControllerBlueprintNode& node)
			{
				return node.m_Id == m_SelectedBlueprintNodeId && node.m_Type != AnimationBlueprintNodeType::Start && node.m_Type != AnimationBlueprintNodeType::Result;
			});
			if (nodeIt != selectedTransition->m_BlueprintNodes.end())
			{
				PushHistory();
				std::erase_if(selectedTransition->m_BlueprintLinks, [this](const AnimationControllerBlueprintLink& link)
					{
						return link.m_OutputNode == m_SelectedBlueprintNodeId || link.m_InputNode == m_SelectedBlueprintNodeId;
					});
				selectedTransition->m_BlueprintNodes.erase(nodeIt);
				m_SelectedBlueprintNodeId = 0;
				return true;
			}
		}

		PushHistory();
		RemoveSelectedControllerTransition();
		return true;
	}

	if (m_SelectedControllerStateIndex >= 0 && std::cmp_less(m_SelectedControllerStateIndex, m_CurrentController->GetStates().size())
		&& m_CurrentController->GetStates().size() > 1)
	{
		PushHistory();
		return DeleteSelectedControllerState();
	}

	return false;
}

bool AnimationEditorPanel::DuplicateSelectedFrame()
{
	if (!m_CurrentAnimation || m_SelectedFrameIndex < 0 || std::cmp_greater_equal(m_SelectedFrameIndex,
		m_CurrentAnimation->GetFrames().size()))
		return false;

	auto& frames = m_CurrentAnimation->GetFrames();
	frames.insert(frames.begin() + m_SelectedFrameIndex + 1, frames[m_SelectedFrameIndex]);
	++m_SelectedFrameIndex;
	StopPreview(false);
	return true;
}

bool AnimationEditorPanel::DeleteSelectedFrame()
{
	if (!m_CurrentAnimation || m_SelectedFrameIndex < 0 || std::cmp_greater_equal(m_SelectedFrameIndex,
		m_CurrentAnimation->GetFrames().size()))
		return false;

	m_CurrentAnimation->RemoveFrame(m_SelectedFrameIndex);
	if (m_CurrentAnimation->GetFrames().empty())
		m_SelectedFrameIndex = -1;
	else
		m_SelectedFrameIndex = std::min(m_SelectedFrameIndex, static_cast<int>(m_CurrentAnimation->GetFrames().size()) - 1);
	StopPreview(false);
	return true;
}

bool AnimationEditorPanel::DuplicateSelectedControllerState()
{
	if (!m_CurrentController)
		return false;

	auto& states = m_CurrentController->GetStates();
	if (m_SelectedControllerStateIndex < 0 || std::cmp_greater_equal(m_SelectedControllerStateIndex, states.size()))
		return false;

	const AnimationControllerState sourceState = states[m_SelectedControllerStateIndex];
	PasteControllerState(sourceState);
	return true;
}

bool AnimationEditorPanel::DeleteSelectedControllerState()
{
	if (!m_CurrentController || m_CurrentController->GetStates().size() <= 1)
		return false;

	auto& states = m_CurrentController->GetStates();
	if (m_SelectedControllerStateIndex < 0 || std::cmp_greater_equal(m_SelectedControllerStateIndex, states.size()))
		return false;

	const std::string removedName = states[m_SelectedControllerStateIndex].m_Name;
	m_CurrentController->RemoveState(removedName);
	m_SelectedControllerStateIndex = std::clamp(m_SelectedControllerStateIndex, 0, static_cast<int>(m_CurrentController->GetStates().size()) - 1);
	ClearSelectedControllerTransition();
	return true;
}

void AnimationEditorPanel::PasteFrame(const AnimationFrame& frame)
{
	auto& frames = m_CurrentAnimation->GetFrames();
	const int insertIndex = m_SelectedFrameIndex >= 0 && std::cmp_less(m_SelectedFrameIndex, frames.size()) ? m_SelectedFrameIndex + 1 : static_cast<int>(frames.size());
	frames.insert(frames.begin() + insertIndex, frame);
	m_SelectedFrameIndex = insertIndex;
	StopPreview(false);
}

void AnimationEditorPanel::PasteControllerState(const AnimationControllerState& state)
{
	const AnimationControllerState& sourceState = state;
	AnimationControllerState& duplicate = m_CurrentController->AddState(sourceState.m_Name + " Copy", sourceState.m_Clip);
	const std::string uniqueName = duplicate.m_Name;
	duplicate = sourceState;
	duplicate.m_Name = uniqueName;
	duplicate.m_GraphPosition += glm::vec2{ 44.0f, 44.0f };
	duplicate.m_Transitions.clear();
	m_SelectedControllerStateIndex = static_cast<int>(m_CurrentController->GetStates().size()) - 1;
	ClearSelectedControllerTransition();
}

bool AnimationEditorPanel::PasteControllerTransition(const AnimationControllerTransition& transition)
{
	if (m_SelectedTransitionSourceStateIndex == AnyStateTransitionSource)
	{
		auto& transitions = m_CurrentController->GetAnyStateTransitions();
		transitions.push_back(transition);
		m_SelectedTransitionIndex = static_cast<int>(transitions.size()) - 1;
		return true;
	}

	auto& states = m_CurrentController->GetStates();
	int targetSourceIndex = m_SelectedControllerStateIndex;
	if (m_SelectedTransitionSourceStateIndex >= 0 && std::cmp_less(m_SelectedTransitionSourceStateIndex, states.size()))
		targetSourceIndex = m_SelectedTransitionSourceStateIndex;

	if (targetSourceIndex < 0 || std::cmp_greater_equal(targetSourceIndex, states.size()))
		return false;

	auto& transitions = states[targetSourceIndex].m_Transitions;
	transitions.push_back(transition);
	m_SelectedControllerStateIndex = targetSourceIndex;
	m_SelectedTransitionSourceStateIndex = targetSourceIndex;
	m_SelectedTransitionIndex = static_cast<int>(transitions.size()) - 1;
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

	if (m_SelectedFrameIndex < 0 || std::cmp_greater_equal(m_SelectedFrameIndex, frames.size()))
		m_SelectedFrameIndex = 0;

	m_PreviewElapsed += ImGui::GetIO().DeltaTime;
	const float currentDuration = std::max(frames[m_SelectedFrameIndex].m_Duration, 0.033f);
	if (m_PreviewElapsed < currentDuration)
		return;

	m_PreviewElapsed = 0.0f;
	const int nextFrame = m_SelectedFrameIndex + 1;
	if (std::cmp_less(nextFrame, frames.size()))
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

	if (m_SelectedFrameIndex < 0 || std::cmp_greater_equal(m_SelectedFrameIndex, frames.size()))
		m_SelectedFrameIndex = direction < 0 ? static_cast<int>(frames.size()) - 1 : 0;
	else
	{
		const int frameCount = static_cast<int>(frames.size());
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
