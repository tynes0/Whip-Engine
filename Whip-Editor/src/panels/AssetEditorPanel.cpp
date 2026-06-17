#include "AssetEditorPanel.h"

#include <Whip/Asset/AssetManager.h>
#include <Whip/Audio/AudioSource.h>
#include <Whip/Render/Font.h>
#include <Whip/UI/UIHelpers.h>
#include <Whip/Utils/PlatformUtils.h>

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>

_WHIP_START

namespace
{
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
}

void AssetEditorPanel::OpenAsset(AssetHandle handle)
{
	if (handle == 0 || !Project::GetActive() || !Project::GetActive()->GetEditorAssetManager()->IsAssetHandleValid(handle))
		return;

	for (AssetEditorDocument& document : m_Documents)
	{
		if (document.m_Handle == handle)
		{
			document.m_Open = true;
			document.m_FocusRequested = true;
			m_OpenDirty = true;
			return;
		}
	}

	AssetEditorDocument document;
	document.m_Handle = handle;
	m_Documents.push_back(document);
	m_OpenDirty = true;
}

void AssetEditorPanel::CloseAll()
{
	if (m_Documents.empty())
		return;

	m_Documents.clear();
	m_LastFocusedEditor = 0;
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
		return;

	HandleEditorTabShortcut();

	for (size_t i = 0; i < m_Documents.size();)
	{
		AssetEditorDocument& document = m_Documents[i];
		DrawDocument(document);
		if (!document.m_Open)
		{
			m_Documents.erase(m_Documents.begin() + static_cast<std::ptrdiff_t>(i));
			m_OpenDirty = true;
			continue;
		}
		++i;
	}
}

void AssetEditorPanel::HandleEditorTabShortcut()
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
		if (m_Documents[i].m_Handle == m_LastFocusedEditor)
		{
			index = (i + 1) % m_Documents.size();
			break;
		}
	}

	m_Documents[index].m_FocusRequested = true;
}

void AssetEditorPanel::DrawDocument(AssetEditorDocument& document)
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

	if (document.m_FullscreenRequested)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
		document.m_FullscreenRequested = false;
	}
	if (document.m_FocusRequested)
	{
		ImGui::SetNextWindowFocus();
		document.m_FocusRequested = false;
	}

	ImGui::SetNextWindowSizeConstraints(ImVec2(340.0f, 220.0f), ImVec2(FLT_MAX, FLT_MAX));
	const std::string title = MakeWindowTitle(document.m_Handle, metadata);
	bool open = document.m_Open;
	if (!ImGui::Begin(title.c_str(), &open))
	{
		document.m_Open = open;
		ImGui::End();
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_DockHierarchy))
		m_LastFocusedEditor = document.m_Handle;

	DrawToolbar(document, metadata);
	ImGui::Separator();

	if (document.m_Compact)
	{
		DrawMetadata(document.m_Handle, metadata);
	}
	else
	{
		switch (metadata.m_Type)
		{
		case AssetType::Texture2D:
			DrawTextureInspector(document.m_Handle, metadata, document.m_Compact);
			break;
		case AssetType::Audio:
			DrawAudioInspector(document.m_Handle, document.m_Compact);
			break;
		case AssetType::Font:
			DrawFontInspector(document.m_Handle, metadata, document.m_Compact);
			break;
		case AssetType::Scene:
			DrawSceneInspector(document.m_Handle, document.m_Compact);
			break;
		case AssetType::Animation:
		case AssetType::AnimationController:
			DrawAnimationInspector(document.m_Handle, metadata, document.m_Compact);
			break;
		case AssetType::Entity:
			DrawEntityInspector(document.m_Handle, metadata, document.m_Compact);
			break;
		default:
			DrawMetadata(document.m_Handle, metadata);
			break;
		}
	}

	document.m_Open = open;
	ImGui::End();
}

void AssetEditorPanel::DrawToolbar(AssetEditorDocument& document, const AssetMetadata& metadata)
{
	ImGui::TextUnformatted(AssetTypeName(metadata.m_Type));
	ImGui::SameLine();
	ImGui::TextDisabled("%s", metadata.m_Filepath.filename().string().c_str());

	const float buttonWidth = 72.0f;
	const float totalButtonWidth = buttonWidth * 3.0f + ImGui::GetStyle().ItemSpacing.x * 3.0f;
	if (ImGui::GetContentRegionAvail().x > totalButtonWidth)
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - totalButtonWidth);
	else
		ImGui::NewLine();

	if (ImGui::SmallButton(document.m_Compact ? "Expand" : "Mini"))
		document.m_Compact = !document.m_Compact;
	ImGui::SameLine();
	if (ImGui::SmallButton("Full"))
		document.m_FullscreenRequested = true;
	ImGui::SameLine();
	if (ImGui::SmallButton("Folder"))
		Utils::OpenExternalPath((Project::GetActiveAssetDirectory() / metadata.m_Filepath).parent_path());
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

void AssetEditorPanel::DrawAnimationInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const
{
	DrawMetadata(handle, metadata);
	if (compact)
		return;

	ImGui::SeparatorText("Animation");
	if (ImGui::Button(metadata.m_Type == AssetType::AnimationController ? "Open Controller Editor" : "Open Animation Editor", ImVec2(180.0f, 0.0f)) && m_OpenAnimationCallback)
		m_OpenAnimationCallback(handle);
}

void AssetEditorPanel::DrawEntityInspector(AssetHandle handle, const AssetMetadata& metadata, bool compact) const
{
	DrawMetadata(handle, metadata);
	if (compact)
		return;

	ImGui::SeparatorText("Entity Template");
	ImGui::TextDisabled("Entity template preview and override inspection will live here.");
}

std::string AssetEditorPanel::MakeWindowTitle(AssetHandle handle, const AssetMetadata& metadata) const
{
	std::string title = std::string(AssetTypeName(metadata.m_Type)) + " - " + metadata.m_Filepath.filename().string();
	title += "###AssetEditor_";
	title += std::to_string(static_cast<uint64_t>(handle));
	return title;
}

_WHIP_END
