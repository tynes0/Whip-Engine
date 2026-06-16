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

#include <algorithm>

_WHIP_START

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
			if (m_RefreshAssetTreeCallback)
				m_RefreshAssetTreeCallback();
		}
	}

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

	ImGui::SetNextItemWidth(std::min(260.0f, ImGui::GetContentRegionAvail().x));
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
	ImGui::EndChild();

	ImGui::Spacing();
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
