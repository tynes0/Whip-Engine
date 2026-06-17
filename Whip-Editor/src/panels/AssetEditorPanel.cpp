#include "AssetEditorPanel.h"

#include <Whip/Asset/AssetManager.h>
#include <Whip/Audio/AudioSource.h>
#include <Whip/Render/Font.h>
#include <Whip/UI/UIHelpers.h>
#include <Whip/Utils/PlatformUtils.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>

_WHIP_START

namespace
{
	enum class WindowControlType
	{
		Minimize,
		Maximize,
		Restore
	};

	const char* AssetTypeName(AssetType type)
	{
		switch (type)
		{
		case AssetType::Scene: return "Scene";
		case AssetType::Texture2D: return "Texture";
		case AssetType::Audio: return "Audio";
		case AssetType::Font: return "Font";
		case AssetType::Animation: return "Animation";
		case AssetType::AnimationController: return "Animation Controller";
		case AssetType::Entity: return "Entity Template";
		default: return "Asset";
		}
	}

	std::string FormatFileSize(const std::filesystem::path& path)
	{
		std::error_code error;
		if (!std::filesystem::exists(path, error))
			return "Missing";

		const std::uintmax_t size = std::filesystem::file_size(path, error);
		if (error)
			return "Unknown";

		if (size < 1024)
			return std::to_string(size) + " B";
		if (size < 1024 * 1024)
			return std::to_string(size / 1024) + " KB";

		char buffer[32]{};
		std::snprintf(buffer, sizeof(buffer), "%.2f MB", static_cast<double>(size) / (1024.0 * 1024.0));
		return buffer;
	}

	std::string FormatDuration(float seconds)
	{
		const int totalSeconds = static_cast<int>(std::max(seconds, 0.0f));
		const int minutes = totalSeconds / 60;
		const int remainingSeconds = totalSeconds % 60;
		char buffer[32]{};
		std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, remainingSeconds);
		return buffer;
	}

	ImVec2 FitImageSize(float width, float height, ImVec2 available)
	{
		if (width <= 0.0f || height <= 0.0f)
			return { 96.0f, 96.0f };

		available.x = std::max(96.0f, available.x);
		available.y = std::max(96.0f, available.y);
		const float scale = std::min(available.x / width, available.y / height);
		return { width * scale, height * scale };
	}

	ImVec2 ClampDefaultWindowSize(ImVec2 requested)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		const ImVec2 maxSize(
			std::min(1280.0f, viewport->WorkSize.x - 40.0f),
			std::min(720.0f, viewport->WorkSize.y - 40.0f));
		return {
			std::clamp(requested.x, 360.0f, std::max(360.0f, maxSize.x)),
			std::clamp(requested.y, 240.0f, std::max(240.0f, maxSize.y))
		};
	}

	ImVec2 DefaultWorkspaceSize()
	{
		return ClampDefaultWindowSize({ 1040.0f, 640.0f });
	}

	bool DrawWindowControl(const char* id, WindowControlType type)
	{
		const ImVec2 size(28.0f, 22.0f);
		ImGui::InvisibleButton(id, size);
		const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		const bool hovered = ImGui::IsItemHovered();
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImU32 hoverColor = IM_COL32(255, 255, 255, hovered ? 24 : 0);
		drawList->AddRectFilled(min, max, hoverColor, 3.0f);
		const ImU32 color = IM_COL32(226, 226, 226, 230);
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

void AssetEditorPanel::OpenAsset(AssetHandle handle)
{
	if (handle == 0 || !Project::GetActive() || !Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(handle))
		return;

	for (AssetEditorDocument& document : m_Documents)
		document.m_FocusRequested = false;

	for (AssetEditorDocument& document : m_Documents)
	{
		if (document.m_Handle == handle)
		{
			document.m_Open = true;
			document.m_FocusRequested = true;
			m_Open = true;
			m_Minimized = false;
			m_ActiveDocument = handle;
			m_FocusRequested = true;
			m_OpenDirty = true;
			return;
		}
	}

	AssetEditorDocument document;
	document.m_Handle = handle;
	m_Documents.push_back(document);
	m_Open = true;
	m_Minimized = false;
	m_ActiveDocument = handle;
	m_FocusRequested = true;
	m_OpenDirty = true;
}

void AssetEditorPanel::CloseAll()
{
	if (m_Documents.empty())
		return;

	m_Documents.clear();
	m_Open = false;
	m_Minimized = false;
	m_Fullscreen = false;
	m_FullscreenRequested = false;
	m_ActiveDocument = 0;
	m_EmbeddedAnimationHandle = 0;
	m_OpenDirty = true;
}

bool AssetEditorPanel::HasOpenEditors() const
{
	return !m_Documents.empty();
}

bool AssetEditorPanel::ConsumeOpenDirty()
{
	const bool dirty = m_OpenDirty;
	m_OpenDirty = false;
	return dirty;
}

void AssetEditorPanel::OnImGuiRender()
{
	if (m_Documents.empty())
	{
		m_Open = false;
		return;
	}

	if (!m_Open)
		return;

	HandleWorkspaceTabShortcut();

	if (m_Minimized)
	{
		DrawMinimizedStrip();
		return;
	}

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
		ImGui::SetNextWindowSize(DefaultWorkspaceSize(), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(340.0f, 220.0f), ImVec2(FLT_MAX, FLT_MAX));
	bool open = m_Open;
	constexpr ImGuiWindowFlags WorkspaceFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (!ImGui::Begin("Asset Workspace###AssetWorkspace", &open, WorkspaceFlags))
	{
		m_Open = open;
		ImGui::End();
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_DockHierarchy))
		m_FocusRequested = false;

	if (m_Fullscreen && TitlebarDragStarted())
		RestoreWorkspaceRect(true);
	else if (!m_Fullscreen && !ImGui::IsWindowDocked())
		CaptureWorkspaceRect();

	DrawWorkspaceHeader();
	ImGui::Separator();
	DrawWorkspaceTabs();

	m_Open = open;
	ImGui::End();

	if (!m_Open)
		CloseAll();
}

void AssetEditorPanel::HandleWorkspaceTabShortcut()
{
	if (m_Documents.size() < 2)
		return;

	const ImGuiIO& io = ImGui::GetIO();
	if ((io.KeyAlt || io.KeyCtrl) && ImGui::IsKeyPressed(ImGuiKey_Tab, false))
		FocusNextEditor();
}

void AssetEditorPanel::FocusNextEditor()
{
	if (m_Documents.empty())
		return;

	size_t index = 0;
	for (size_t i = 0; i < m_Documents.size(); ++i)
	{
		if (m_Documents[i].m_Handle == m_ActiveDocument)
		{
			index = (i + 1) % m_Documents.size();
			break;
		}
	}

	m_ActiveDocument = m_Documents[index].m_Handle;
	for (AssetEditorDocument& document : m_Documents)
		document.m_FocusRequested = false;
	m_Documents[index].m_FocusRequested = true;
	m_Minimized = false;
	m_FocusRequested = true;
}

void AssetEditorPanel::DrawMinimizedStrip()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	constexpr float stripHeight = 42.0f;
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 10.0f, viewport->WorkPos.y + viewport->WorkSize.y - stripHeight - 10.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(std::min(viewport->WorkSize.x - 20.0f, 360.0f), stripHeight), ImGuiCond_Always);
	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;

	if (!ImGui::Begin("##MinimizedAssetWorkspace", nullptr, flags))
	{
		ImGui::End();
		return;
	}

	std::string label = "Asset Workspace";
	if (m_ActiveDocument != 0 && Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(m_ActiveDocument))
	{
		const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(m_ActiveDocument);
		if (metadata)
			label += std::string("  ") + metadata.m_Filepath.filename().string();
	}
	label += "###RestoreAssetWorkspace";

	if (ImGui::Button(label.c_str(), ImVec2(330.0f, 24.0f)))
	{
		m_Minimized = false;
		m_FocusRequested = true;
		m_OpenDirty = true;
	}

	ImGui::End();
}

void AssetEditorPanel::DrawWorkspaceHeader()
{
	ImGui::TextUnformatted("Asset Workspace");
	ImGui::SameLine();
	ImGui::TextDisabled("%zu open", m_Documents.size());

	const float controlsWidth = 28.0f * 2.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
	if (ImGui::GetContentRegionAvail().x > controlsWidth)
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - controlsWidth);
	else
		ImGui::SameLine();

	if (DrawWindowControl("##AssetWorkspaceMinimize", WindowControlType::Minimize))
	{
		m_Minimized = true;
		m_Fullscreen = false;
		m_OpenDirty = true;
	}
	ImGui::SameLine();
	if (DrawWindowControl("##AssetWorkspaceMaximize", m_Fullscreen ? WindowControlType::Restore : WindowControlType::Maximize))
	{
		if (m_Fullscreen)
			RestoreWorkspaceRect();
		else
			RequestFullscreen();
	}
}

void AssetEditorPanel::DrawWorkspaceTabs()
{
	if (m_ActiveDocument == 0 && !m_Documents.empty())
	{
		m_ActiveDocument = m_Documents.front().m_Handle;
		m_Documents.front().m_FocusRequested = true;
	}

	if (ImGui::BeginTabBar("##AssetWorkspaceTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll))
	{
		for (AssetEditorDocument& document : m_Documents)
		{
			if (!Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(document.m_Handle))
			{
				document.m_Open = false;
				continue;
			}

			const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(document.m_Handle);
			if (!metadata)
			{
				document.m_Open = false;
				continue;
			}

			bool open = document.m_Open;
			const bool focusRequested = document.m_FocusRequested;
			ImGuiTabItemFlags flags = focusRequested ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
			const std::string label = MakeTabLabel(document.m_Handle, metadata);
			if (ImGui::BeginTabItem(label.c_str(), &open, flags))
			{
				m_ActiveDocument = document.m_Handle;
				DrawDocumentContent(document);
				ImGui::EndTabItem();
			}
			if (focusRequested)
				document.m_FocusRequested = false;
			document.m_Open = open;
		}
		ImGui::EndTabBar();
	}

	for (size_t i = 0; i < m_Documents.size();)
	{
		if (!m_Documents[i].m_Open)
		{
			if (m_Documents[i].m_Handle == m_ActiveDocument)
				m_ActiveDocument = 0;
			if (m_Documents[i].m_Handle == m_EmbeddedAnimationHandle)
				m_EmbeddedAnimationHandle = 0;
			m_Documents.erase(m_Documents.begin() + static_cast<std::ptrdiff_t>(i));
			m_OpenDirty = true;
			continue;
		}
		++i;
	}

	if (m_ActiveDocument == 0 && !m_Documents.empty())
		m_ActiveDocument = m_Documents.front().m_Handle;
	if (m_Documents.empty())
	{
		m_Open = false;
		m_OpenDirty = true;
	}
}

void AssetEditorPanel::DrawDocumentContent(AssetEditorDocument& document)
{
	if (!Project::GetActive() || !Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(document.m_Handle))
	{
		document.m_Open = false;
		return;
	}

	const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(document.m_Handle);
	if (!metadata)
	{
		document.m_Open = false;
		return;
	}

	DrawDocumentToolbar(document.m_Handle, metadata);
	ImGui::Separator();

	switch (metadata.m_Type)
	{
	case AssetType::Texture2D:
		DrawTextureInspector(document.m_Handle, metadata, false);
		break;
	case AssetType::Audio:
		DrawAudioInspector(document.m_Handle, false);
		break;
	case AssetType::Font:
		DrawFontInspector(document.m_Handle, metadata, false);
		break;
	case AssetType::Scene:
		DrawSceneInspector(document.m_Handle, false);
		break;
	case AssetType::Animation:
	case AssetType::AnimationController:
		DrawAnimationInspector(document.m_Handle, metadata, false);
		break;
	case AssetType::Entity:
		DrawEntityInspector(document.m_Handle, metadata, false);
		break;
	default:
		DrawMetadata(document.m_Handle, metadata);
		break;
	}
}

void AssetEditorPanel::DrawDocumentToolbar(AssetHandle handle, const AssetMetadata& metadata) const
{
	ImGui::TextUnformatted(AssetTypeName(metadata.m_Type));
	ImGui::SameLine();
	ImGui::TextDisabled("%s", metadata.m_Filepath.filename().string().c_str());

	constexpr float folderWidth = 58.0f;
	if (ImGui::GetContentRegionAvail().x > folderWidth)
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - folderWidth);
	else
		ImGui::NewLine();

	ImGui::PushID(static_cast<int>(handle));
	if (ImGui::SmallButton("Folder"))
		Utils::OpenExternalPath((Project::GetActiveAssetDirectory() / metadata.m_Filepath).parent_path());
	ImGui::PopID();
}

void AssetEditorPanel::CaptureWorkspaceRect()
{
	if (m_Fullscreen || ImGui::IsWindowDocked())
		return;

	const ImVec2 pos = ImGui::GetWindowPos();
	const ImVec2 size = ImGui::GetWindowSize();
	if (size.x <= 0.0f || size.y <= 0.0f)
		return;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (size.x >= viewport->WorkSize.x - 24.0f && size.y >= viewport->WorkSize.y - 24.0f)
		return;

	m_RestorePosition = { pos.x, pos.y };
	m_RestoreSize = { size.x, size.y };
	m_HasRestoreRect = true;
}

void AssetEditorPanel::RequestFullscreen()
{
	CaptureWorkspaceRect();
	m_Minimized = false;
	m_Fullscreen = true;
	m_FullscreenRequested = true;
	m_FocusRequested = true;
}

void AssetEditorPanel::RestoreWorkspaceRect(bool anchorToMouse)
{
	m_Fullscreen = false;
	m_FullscreenRequested = false;
	const ImVec2 size(
		m_HasRestoreRect ? m_RestoreSize.x : 1040.0f,
		m_HasRestoreRect ? m_RestoreSize.y : 640.0f);
	ImVec2 pos(
		m_HasRestoreRect ? m_RestorePosition.x : ImGui::GetMainViewport()->WorkPos.x + 80.0f,
		m_HasRestoreRect ? m_RestorePosition.y : ImGui::GetMainViewport()->WorkPos.y + 80.0f);
	if (anchorToMouse)
	{
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		const ImVec2 currentPos = ImGui::GetWindowPos();
		const ImVec2 currentSize = ImGui::GetWindowSize();
		const float mouseRatioX = currentSize.x > 1.0f ? std::clamp((mouse.x - currentPos.x) / currentSize.x, 0.08f, 0.92f) : 0.5f;
		pos.x = mouse.x - size.x * mouseRatioX;
		pos.y = mouse.y - ImGui::GetFrameHeight() * 0.5f;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		pos.x = std::clamp(pos.x, viewport->WorkPos.x, viewport->WorkPos.x + std::max(0.0f, viewport->WorkSize.x - size.x));
		pos.y = std::clamp(pos.y, viewport->WorkPos.y, viewport->WorkPos.y + std::max(0.0f, viewport->WorkSize.y - size.y));
	}
	ImGui::SetWindowPos(pos, ImGuiCond_Always);
	ImGui::SetWindowSize(size, ImGuiCond_Always);
}

void AssetEditorPanel::DrawMetadata(AssetHandle handle, const AssetMetadata& metadata) const
{
	const std::filesystem::path absolutePath = Project::GetActiveAssetDirectory() / metadata.m_Filepath;

	if (ImGui::BeginTable("##AssetMetadata", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
	{
		auto row = [](const char* key, const std::string& value)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", key);
				ImGui::TableNextColumn();
				ImGui::TextWrapped("%s", value.c_str());
			};

		row("Path", metadata.m_Filepath.generic_string());
		row("Type", AssetTypeName(metadata.m_Type));
		row("Size", FormatFileSize(absolutePath));
		row("Handle", std::to_string(static_cast<uint64_t>(handle)));
		ImGui::EndTable();
	}
}

void AssetEditorPanel::DrawTextureInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const
{
	DrawMetadata(handle, metadata);
	if (compact)
		return;

	Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);
	if (!texture || !texture->IsLoaded())
	{
		ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Texture is not loaded.");
		return;
	}

	ImGui::SeparatorText("Preview");
	ImGui::TextDisabled("%u x %u", texture->GetWidth(), texture->GetHeight());
	const ImVec2 previewSize = FitImageSize(static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight()), ImGui::GetContentRegionAvail());
	UI::Image(UI::ToImGuiTextureId(texture->GetRendererId()), previewSize, { 0, 1 }, { 1, 0 });
}

void AssetEditorPanel::DrawAudioInspector(AssetHandle handle, bool compact) const
{
	const AssetMetadata& metadata = AssetManager::GetAssetMetadata(handle);
	DrawMetadata(handle, metadata);
	if (compact)
		return;

	Ref<AudioSource> audio = AssetManager::GetAsset<AudioSource>(handle);
	if (!audio || !audio->IsLoaded())
	{
		ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Audio is not loaded.");
		return;
	}

	ImGui::SeparatorText("Audio");
	ImGui::Text("Length: %s", FormatDuration(audio->GetLength()).c_str());
	ImGui::Text("Gain: %.2f", audio->GetGain());
	ImGui::Text("Pitch: %.2f", audio->GetPitch());
	ImGui::Text("Loop: %s", audio->IsLoop() ? "true" : "false");
	ImGui::Text("Spatial: %s", audio->IsSpitial() ? "true" : "false");
	ImGui::Text("Streaming: %s", audio->IsStreaming() ? "true" : "false");
}

void AssetEditorPanel::DrawFontInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const
{
	DrawMetadata(handle, metadata);
	if (compact)
		return;

	Ref<Font> font = AssetManager::GetAsset<Font>(handle);
	if (!font)
	{
		ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Font is not loaded.");
		return;
	}

	Ref<Texture2D> atlas = font->GetAtlasTexture();
	if (!atlas || !atlas->IsLoaded())
	{
		ImGui::TextDisabled("Font atlas is not available.");
		return;
	}

	ImGui::SeparatorText("Atlas");
	ImGui::TextDisabled("%u x %u", atlas->GetWidth(), atlas->GetHeight());
	const ImVec2 previewSize = FitImageSize(static_cast<float>(atlas->GetWidth()), static_cast<float>(atlas->GetHeight()), ImGui::GetContentRegionAvail());
	UI::Image(UI::ToImGuiTextureId(atlas->GetRendererId()), previewSize, { 0, 1 }, { 1, 0 });
}

void AssetEditorPanel::DrawSceneInspector(AssetHandle handle, bool compact) const
{
	const AssetMetadata& metadata = AssetManager::GetAssetMetadata(handle);
	DrawMetadata(handle, metadata);
	if (compact)
		return;

	ImGui::SeparatorText("Scene");
	if (ImGui::Button("Open Scene", ImVec2(120.0f, 0.0f)) && m_OpenSceneCallback)
		m_OpenSceneCallback(handle);
	ImGui::SameLine();
	const bool isStartScene = Project::GetActive() && Project::GetActive()->GetConfig().m_StartScene == handle;
	ImGui::BeginDisabled(isStartScene);
	if (ImGui::Button(isStartScene ? "Start Scene" : "Set Start Scene", ImVec2(128.0f, 0.0f)) && m_SetStartSceneCallback)
		m_SetStartSceneCallback(handle);
	ImGui::EndDisabled();
}

void AssetEditorPanel::DrawAnimationInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact)
{
	if (compact)
	{
		DrawMetadata(handle, metadata);
		return;
	}

	if (m_OpenAnimationCallback && m_EmbeddedAnimationHandle != handle)
	{
		if (m_OpenAnimationCallback(handle))
			m_EmbeddedAnimationHandle = handle;
	}

	if (m_DrawAnimationEditorCallback && m_EmbeddedAnimationHandle == handle)
	{
		m_DrawAnimationEditorCallback();
		return;
	}

	DrawMetadata(handle, metadata);
	ImGui::SeparatorText("Animation");
	ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Animation editor could not load this asset.");
}

void AssetEditorPanel::DrawEntityInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const
{
	DrawMetadata(handle, metadata);
	if (compact)
		return;

	ImGui::SeparatorText("Entity Template");
	ImGui::TextDisabled("Entity template preview and override inspection will live here.");
}

std::string AssetEditorPanel::MakeTabLabel(AssetHandle handle, const AssetMetadata& metadata) const
{
	std::string label = metadata.m_Filepath.filename().string();
	if (label.empty())
		label = AssetTypeName(metadata.m_Type);
	label += "###AssetTab_";
	label += std::to_string(static_cast<uint64_t>(handle));
	return label;
}

_WHIP_END
