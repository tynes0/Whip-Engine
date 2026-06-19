#include <WhipPch.h>

#include <Whip-Editor/panels/ContentBrowserPanel.h>

#include <Whip-Editor/Helpers/IconManager.h>

#include <Whip/Asset/AssetManager.h>
#include <Whip/Asset/TextureSlicer.h>
#include <Whip/Asset/AssetUtils.h>
#include <Whip/Core/Application.h>
#include <Whip/Utils/FileExtensions.h>
#include <Whip/Project/Project.h>
#include <Whip-Editor/UI/UIHelpers.h>
#include <Whip/Utils/PlatformUtils.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <set>
#include <sstream>
#include <system_error>

_WHIP_START

namespace
{
	constexpr AssetType AssetTypeFilters[] =
	{
		AssetType::None,
		AssetType::Scene,
		AssetType::Texture2D,
		AssetType::Audio,
		AssetType::Font,
		AssetType::Animation,
		AssetType::AnimationController,
		AssetType::Entity
	};

	std::string ToLower(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	}

	bool PathComponentIsParentReference(const std::filesystem::path& path)
	{
		for (const auto& component : path)
			if (component == "..")
				return true;

		return false;
	}

	bool PathComponentIsCurrentReference(const std::filesystem::path& path)
	{
		return path.empty() || path == ".";
	}

	bool IsInternalProjectFile(const std::filesystem::path& relativePath)
	{
		return FileExtensions::IsAssetRegistryFilename(relativePath);
	}

	bool PathIsUnderDirectory(const std::filesystem::path& path, const std::filesystem::path& directory)
	{
		std::filesystem::path normalizedPath = path.lexically_normal();
		std::filesystem::path normalizedDirectory = directory.lexically_normal();
		if (normalizedPath == normalizedDirectory)
			return true;

		auto pathIt = normalizedPath.begin();
		auto directoryIt = normalizedDirectory.begin();
		for (; directoryIt != normalizedDirectory.end(); ++directoryIt, ++pathIt)
		{
			if (pathIt == normalizedPath.end() || *pathIt != *directoryIt)
				return false;
		}
		return true;
	}

	std::string ImportSummaryText(const ContentBrowserPanel::ImportSummary& summary)
	{
		std::ostringstream stream;
		stream << "Import: " << summary.m_Imported << " imported";
		if (summary.m_AlreadyImported > 0)
			stream << ", " << summary.m_AlreadyImported << " already registered";
		if (summary.m_Unsupported > 0)
			stream << ", " << summary.m_Unsupported << " unsupported";
		if (summary.m_Missing > 0)
			stream << ", " << summary.m_Missing << " missing";
		if (summary.m_Failed > 0)
			stream << ", " << summary.m_Failed << " failed";
		return stream.str();
	}
}

ContentBrowserPanel::ContentBrowserPanel()
{
}

ContentBrowserPanel::ContentBrowserPanel(Ref<Project> proj)
{
	Init(std::move(proj));
}

void ContentBrowserPanel::Init(Ref<Project> proj)
{
	m_Project = proj;
	m_ThumbnailCache = MakeRef<ThumbnailCache>(proj);
	m_CurrentDirectory = m_BaseDirectory = m_Project->GetAssetDirectory();

	std::error_code error;
	std::filesystem::create_directories(m_BaseDirectory, error);
	if (error)
		WHP_CORE_WARN("[Content Browser] Could not create Asset directory '{0}': {1}", m_BaseDirectory.string(), error.message());

	RefreshAssetTree();
	m_Mode = Mode::Filesystem;
	m_Initialized = true;
}

void ContentBrowserPanel::OnImGuiRender()
{
	if (!m_Open)
	{
		m_Hovered = false;
		return;
	}

	bool open = m_Open;
	ImGui::Begin("Content Browser", &open);
	m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
	if (open != m_Open)
		SetOpen(open);

	if (!m_Initialized || !m_Project)
	{
		ImGui::TextDisabled("No Project loaded.");
		ImGui::End();
		return;
	}

	DrawToolbar();
	DrawStatusBar();
	ImGui::Separator();

	if (ImGui::BeginTable("##ContentBrowserLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Items", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		DrawSidebar();

		ImGui::TableNextColumn();
		DrawBreadcrumbs();
		ImGui::Separator();

		if (m_ItemsDirty)
			RebuildCachedItems();
		DrawContentGrid(m_CachedItems);

		ImGui::EndTable();
	}

	if (ImGui::BeginPopupContextWindow("##ContentBrowserWindowMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("Refresh"))
			RefreshAssetTree();
		if (ImGui::MenuItem("Open Current Folder in Explorer"))
			Utils::OpenExternalPath(m_CurrentDirectory);
		if (ImGui::MenuItem("Settings"))
			m_ShowSettingsPopup = true;
		ImGui::EndPopup();
	}

	DrawFileOperationModals();
	DrawAutoSliceModal();
	OnSettingsPopup();
	ImGui::End();
}

void ContentBrowserPanel::DrawToolbar()
{
	if (ImGui::RadioButton("Files", m_Mode == Mode::Filesystem))
	{
		m_Mode = Mode::Filesystem;
		m_PreferencesDirty = true;
		InvalidateItems();
	}

	ImGui::SameLine();
	if (ImGui::RadioButton("Imported", m_Mode == Mode::Asset))
	{
		m_Mode = Mode::Asset;
		m_PreferencesDirty = true;
		InvalidateItems();
	}

	ImGui::SameLine();
	if (ImGui::Button("Refresh"))
		RefreshAssetTree();

	ImGui::SameLine();
	if (m_CurrentDirectory != m_BaseDirectory && ImGui::Button("Up"))
		SetCurrentDirectory(m_CurrentDirectory.parent_path());

	ImGui::SameLine();
	if (m_Mode == Mode::Filesystem)
	{
		if (ImGui::Button("Import Folder"))
			ImportCurrentDirectory(false);

		ImGui::SameLine();
		if (ImGui::Button("Import Recursive"))
			ImportCurrentDirectory(true);

		ImGui::SameLine();
	}

	DrawTypeFilter();

	ImGui::SameLine();
	ImGui::SetNextItemWidth(std::max(160.0f, ImGui::GetContentRegionAvail().x - 154.0f));
	if (ImGui::InputTextWithHint("##ContentBrowserSearch", "Search assets and files", &m_SearchQuery))
		InvalidateItems();

	if (!m_SearchQuery.empty())
	{
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
		{
			m_SearchQuery.clear();
			InvalidateItems();
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Settings"))
		m_ShowSettingsPopup = true;
}

void ContentBrowserPanel::DrawStatusBar()
{
	if (m_StatusMessage.empty())
		return;

	ImGui::Spacing();
	ImGui::TextColored(m_StatusError ? ImVec4(0.95f, 0.50f, 0.34f, 1.0f) : ImVec4(0.72f, 0.78f, 0.54f, 1.0f), "%s", m_StatusMessage.c_str());
}

void ContentBrowserPanel::DrawTypeFilter()
{
	const std::string label = AssetTypeFilterLabel();
	ImGui::SetNextItemWidth(128.0f);
	if (ImGui::BeginCombo("##ContentBrowserTypeFilter", label.c_str()))
	{
		for (AssetType type : AssetTypeFilters)
		{
			const bool selected = m_TypeFilter == type;
			std::string itemLabel = type == AssetType::None ? "All Types" : std::string(frenum::to_string(type));
			if (ImGui::Selectable(itemLabel.c_str(), selected))
			{
				m_TypeFilter = type;
				m_PreferencesDirty = true;
				InvalidateItems();
			}

			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

void ContentBrowserPanel::DrawSidebar()
{
	ImGui::BeginChild("##ContentBrowserSidebar", ImVec2(0.0f, 0.0f), false);
	if (m_DirectoryTreeDirty)
		RebuildDirectoryTree();

	ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (m_CurrentDirectory == m_BaseDirectory)
		rootFlags |= ImGuiTreeNodeFlags_Selected;

	const bool open = ImGui::TreeNodeEx("Assets", rootFlags);
	if (ImGui::IsItemClicked())
		SetCurrentDirectory(m_BaseDirectory);
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_PATH"))
		{
			std::filesystem::path sourceRelativePath(std::string(static_cast<const char*>(payload->Data), payload->DataSize));
			MovePathToDirectory(sourceRelativePath, m_BaseDirectory);
		}
		ImGui::EndDragDropTarget();
	}

	if (open)
	{
		DrawDirectoryTree(m_DirectoryTree);
		ImGui::TreePop();
	}

	ImGui::EndChild();
}

void ContentBrowserPanel::DrawDirectoryTree(const DirectoryNode& node)
{
	for (const DirectoryNode& child : node.m_Children)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (child.m_Path == m_CurrentDirectory)
			flags |= ImGuiTreeNodeFlags_Selected;

		const bool open = ImGui::TreeNodeEx(child.m_Name.c_str(), flags);
		if (ImGui::IsItemClicked())
			SetCurrentDirectory(child.m_Path);
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_PATH"))
			{
				std::filesystem::path sourceRelativePath(std::string(static_cast<const char*>(payload->Data), payload->DataSize));
				MovePathToDirectory(sourceRelativePath, child.m_Path);
			}
			ImGui::EndDragDropTarget();
		}

		if (open)
		{
			DrawDirectoryTree(child);
			ImGui::TreePop();
		}
	}
}

void ContentBrowserPanel::DrawBreadcrumbs()
{
	if (ImGui::Button("Assets##ContentBrowserBreadcrumbRoot"))
		SetCurrentDirectory(m_BaseDirectory);

	std::error_code error;
	std::filesystem::path relativePath = std::filesystem::relative(m_CurrentDirectory, m_BaseDirectory, error);
	if (error || PathComponentIsCurrentReference(relativePath))
		return;

	std::filesystem::path cursor = m_BaseDirectory;
	for (const auto& part : relativePath)
	{
		cursor /= part;
		ImGui::SameLine();
		ImGui::TextUnformatted("/");
		ImGui::SameLine();
		ImGui::PushID(cursor.generic_string().c_str());
		if (ImGui::Button(part.string().c_str()))
			SetCurrentDirectory(cursor);
		ImGui::PopID();
	}
}

void ContentBrowserPanel::DrawContentGrid(const std::vector<BrowserItem>& items)
{
	const char* modeLabel = m_Mode == Mode::Filesystem ? "filesystem" : "imported";
	ImGui::TextDisabled("%zu item(s) in %s view | %zu imported | %zu missing | %zu unsupported %s",
		items.size(), modeLabel, m_CachedItemMetrics.m_Imported, m_CachedItemMetrics.m_Missing, m_CachedItemMetrics.m_Unsupported, m_ShowUnsupported ? "visible" : "hidden");

	if (items.empty())
	{
		ImGui::Spacing();
		ImGui::TextDisabled(m_SearchQuery.empty() ? "This folder is empty." : "No matches.");
		return;
	}

	float cellSize = m_ThumbnailSize + m_Padding;
	float panelWidth = ImGui::GetContentRegionAvail().x;
	int columnCount = static_cast<int>(panelWidth / cellSize);
	if (columnCount < 1)
		columnCount = 1;

	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 8.0f));
	if (ImGui::BeginTable("##ContentBrowserGrid", columnCount, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX))
	{
		for (int column = 0; column < columnCount; ++column)
			ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, cellSize);

		const int rowCount = static_cast<int>((items.size() + static_cast<size_t>(columnCount) - 1) / static_cast<size_t>(columnCount));
		ImGuiListClipper clipper;
		clipper.Begin(rowCount);
		while (clipper.Step())
		{
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
			{
				ImGui::TableNextRow();
				for (int column = 0; column < columnCount; ++column)
				{
					const size_t itemIndex = static_cast<size_t>(row) * static_cast<size_t>(columnCount) + static_cast<size_t>(column);
					if (itemIndex >= items.size())
						break;

					ImGui::TableSetColumnIndex(column);
					DrawItem(items[itemIndex]);
				}
			}
		}
		ImGui::EndTable();
	}
	ImGui::PopStyleVar();
}

void ContentBrowserPanel::DrawItem(const BrowserItem& item)
{
	std::string itemId = item.m_RelativePath.generic_string();
	if (item.m_SubAsset)
		itemId += "::sprite:" + std::to_string(item.m_TextureSpriteIndex);
	const std::string displayName = item.m_DisplayName.empty() ? item.m_RelativePath.filename().string() : item.m_DisplayName;
	ImGui::PushID(itemId.c_str());
	ImGui::BeginGroup();

	Ref<Texture2D> thumbnail = item.m_Directory ? IconManager::Get().GetIcon(Icon::Directory) : nullptr;
	ImVec2 thumbnailUv0 = { 0.0f, 1.0f };
	ImVec2 thumbnailUv1 = { 1.0f, 0.0f };
	if (!thumbnail && item.m_SubAsset && item.m_Type == AssetType::Texture2D && item.m_Handle != 0 && AssetManager::IsAssetHandleValid(item.m_Handle))
	{
		Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(item.m_Handle);
		const AssetMetadata& metadata = AssetManager::GetAssetMetadata(item.m_Handle);
		const auto& sprites = metadata.m_TextureSettings.m_Sprites;
		if (texture && item.m_TextureSpriteIndex >= 0 && item.m_TextureSpriteIndex < static_cast<int32_t>(sprites.size()))
		{
			const TextureSpriteRect& sprite = sprites[static_cast<size_t>(item.m_TextureSpriteIndex)];
			const float textureWidth = static_cast<float>(texture->GetWidth());
			const float textureHeight = static_cast<float>(texture->GetHeight());
			if (textureWidth > 0.0f && textureHeight > 0.0f)
			{
				thumbnail = texture;
				thumbnailUv0 = { static_cast<float>(sprite.m_X) / textureWidth, 1.0f - static_cast<float>(sprite.m_Y) / textureHeight };
				thumbnailUv1 = { static_cast<float>(sprite.m_X + sprite.m_Width) / textureWidth, 1.0f - static_cast<float>(sprite.m_Y + sprite.m_Height) / textureHeight };
			}
		}
	}
	if (!thumbnail && item.m_Type == AssetType::Texture2D && !item.m_Missing)
		thumbnail = m_ThumbnailCache->GetOrCreateThumbnail(item.m_RelativePath);
	if (!thumbnail)
		thumbnail = IconManager::Get().GetIcon(Icon::File);

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	const bool iconClicked = UI::ImageButton("##ContentBrowserItemIcon", UI::ToImGuiTextureId(thumbnail->GetRendererId()), { m_ThumbnailSize, m_ThumbnailSize }, thumbnailUv0, thumbnailUv1);
	ImGui::PopStyleColor();

	if (iconClicked && item.m_Directory)
		SetCurrentDirectory(item.m_AbsolutePath);

	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && item.m_Directory)
		SetCurrentDirectory(item.m_AbsolutePath);
	else if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !item.m_Directory)
	{
		if (!InspectAsset(item))
			OpenAsset(item);
	}

	if (ImGui::BeginDragDropSource())
	{
		std::string relativePath = item.m_RelativePath.generic_string();
		if (!item.m_SubAsset)
			ImGui::SetDragDropPayload("CONTENT_BROWSER_PATH", relativePath.data(), relativePath.size());
		if (item.m_Supported && !item.m_Directory && !item.m_Missing)
		{
			AssetHandle handle = item.m_Handle;
			if (handle == 0 && !item.m_SubAsset)
			{
				ImportFile(item.m_RelativePath);
				handle = FindAssetHandle(item.m_RelativePath);
			}
			if (handle != 0)
			{
				UI::AssetReferencePayload assetPayload;
				assetPayload.m_Handle = handle;
				assetPayload.m_TextureSpriteIndex = item.m_TextureSpriteIndex;
				ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", &assetPayload, sizeof(UI::AssetReferencePayload));
			}
		}
		ImGui::TextUnformatted(displayName.c_str());
		ImGui::EndDragDropSource();
	}

	if (item.m_Directory && ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_PATH"))
		{
			std::filesystem::path sourceRelativePath(std::string(static_cast<const char*>(payload->Data), payload->DataSize));
			MovePathToDirectory(sourceRelativePath, item.m_AbsolutePath);
		}
		ImGui::EndDragDropTarget();
	}

	if (ImGui::BeginPopupContextItem())
	{
		if (item.m_Directory)
		{
			if (ImGui::MenuItem("Open"))
				SetCurrentDirectory(item.m_AbsolutePath);
			if (ImGui::MenuItem("Open in Explorer"))
				Utils::OpenExternalPath(item.m_AbsolutePath);
			if (ImGui::MenuItem("Rename"))
				RequestRenameItem(item);
			if (ImGui::MenuItem("Move To..."))
				RequestMoveItem(item);
			if (ImGui::MenuItem("Duplicate"))
				DuplicateItem(item);
			if (ImGui::MenuItem("Delete"))
				RequestDeleteItem(item);
			ImGui::Separator();
			if (m_Mode == Mode::Filesystem && ImGui::MenuItem("Import Folder"))
			{
				SetCurrentDirectory(item.m_AbsolutePath);
				ImportCurrentDirectory(false);
			}
			if (m_Mode == Mode::Filesystem && ImGui::MenuItem("Import Recursive"))
			{
				SetCurrentDirectory(item.m_AbsolutePath);
				ImportCurrentDirectory(true);
			}
		}
		else
		{
			if (item.m_SubAsset)
			{
				if (ImGui::MenuItem("Open Parent Texture Editor"))
					InspectAsset(item);
				if (ImGui::MenuItem("Show Parent in Explorer"))
					Utils::OpenExternalPath(item.m_AbsolutePath.parent_path());
				ImGui::TextDisabled("Texture Sprite Slice");
			}
			else if (item.m_Missing)
			{
				if (ImGui::MenuItem("Remove Missing Registration"))
					RequestRemoveAsset(item.m_Handle, item.m_RelativePath);
			}
			else
			{
				if (item.m_Imported && item.m_Type == AssetType::Scene)
				{
					if (ImGui::MenuItem("Open Scene"))
						OpenAsset(item);
					if (ImGui::MenuItem("Open Asset Editor"))
						InspectAsset(item);
					const bool isStartScene = m_Project && m_Project->GetConfig().m_StartScene == item.m_Handle;
					if (ImGui::MenuItem("Set as Start Scene", nullptr, isStartScene))
						SetSceneAsStartScene(item);
					ImGui::Separator();
				}
				if (item.m_Supported && item.m_Type == AssetType::Entity)
				{
					if (ImGui::MenuItem("Instantiate Entity Template"))
						OpenAsset(item);
					if (ImGui::MenuItem("Open Asset Editor"))
						InspectAsset(item);
					ImGui::Separator();
				}
				if (item.m_Supported && item.m_Type != AssetType::Scene && item.m_Type != AssetType::Entity)
				{
					if (ImGui::MenuItem("Open Asset Editor"))
						InspectAsset(item);
					if (item.m_Type == AssetType::Texture2D && ImGui::MenuItem("Slice Texture..."))
					{
						BrowserItem sliceItem = item;
						if (!sliceItem.m_Imported || sliceItem.m_Handle == 0)
						{
							ImportFile(sliceItem.m_RelativePath);
							sliceItem.m_Handle = FindAssetHandle(sliceItem.m_RelativePath);
							sliceItem.m_Imported = sliceItem.m_Handle != 0;
						}
						if (sliceItem.m_Handle != 0)
							RequestAutoSliceTexture(sliceItem);
					}
					ImGui::Separator();
				}

				if (ImGui::MenuItem("Show in Explorer"))
					Utils::OpenExternalPath(item.m_AbsolutePath.parent_path());
				if (item.m_Supported && !item.m_Imported && ImGui::MenuItem("Import"))
					ImportFile(item.m_RelativePath);
				if (ImGui::MenuItem("Rename"))
					RequestRenameItem(item);
				if (ImGui::MenuItem("Move To..."))
					RequestMoveItem(item);
				if (ImGui::MenuItem("Duplicate"))
					DuplicateItem(item);
				if (ImGui::MenuItem("Delete"))
					RequestDeleteItem(item);
			}
			if (!item.m_SubAsset && item.m_Imported && !item.m_Missing && ImGui::MenuItem("Remove from Registry"))
				RequestRemoveAsset(item.m_Handle, item.m_RelativePath);
			if (!item.m_Supported && !item.m_Missing)
				ImGui::TextDisabled("Unsupported Asset type");
		}

		ImGui::EndPopup();
	}

	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + m_ThumbnailSize);
	ImGui::TextWrapped(displayName.c_str());
	ImGui::PopTextWrapPos();
	if (item.m_Missing)
		ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Missing %s", ItemTypeLabel(item).c_str());
	else if (item.m_Imported && !item.m_Directory)
		ImGui::TextColored(ImVec4(0.42f, 0.72f, 0.52f, 1.0f), "%s", ItemTypeLabel(item).c_str());
	else if (item.m_Supported || item.m_Directory)
		ImGui::TextDisabled("%s", ItemTypeLabel(item).c_str());
	else
		ImGui::TextColored(ImVec4(0.86f, 0.62f, 0.34f, 1.0f), "%s", ItemTypeLabel(item).c_str());
	ImGui::EndGroup();
	ImGui::PopID();
}

void ContentBrowserPanel::DrawFileOperationModals()
{
	if (m_PendingOperation == FileOperation::None)
		return;

	const char* popupName = "Content Browser Operation";
	ImGui::OpenPopup(popupName);
	if (!ImGui::BeginPopupModal(popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	if (m_PendingOperation == FileOperation::Rename)
	{
		ImGui::TextUnformatted("Rename Asset");
		ImGui::TextDisabled("%s", m_PendingOperationPath.generic_string().c_str());
		ImGui::SetNextItemWidth(360.0f);
		ImGui::InputText("Name", &m_OperationText);
	}
	else if (m_PendingOperation == FileOperation::Move)
	{
		ImGui::TextUnformatted("Move Asset");
		ImGui::TextDisabled("%s", m_PendingOperationPath.generic_string().c_str());
		ImGui::SetNextItemWidth(360.0f);
		ImGui::InputTextWithHint("Destination", "Relative folder under Assets, e.g. textures/ui", &m_OperationText);
	}
	else if (m_PendingOperation == FileOperation::DeletePath)
	{
		ImGui::TextWrapped("Delete this %s from disk?", m_PendingOperationIsDirectory ? "folder" : "file");
		ImGui::TextDisabled("%s", m_PendingOperationPath.generic_string().c_str());
	}
	else if (m_PendingOperation == FileOperation::RemoveRegistry)
	{
		ImGui::TextWrapped("Remove this Asset from the registry?");
		ImGui::TextDisabled("%s", m_PendingOperationPath.generic_string().c_str());
	}

	if (!m_OperationError.empty())
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "%s", m_OperationError.c_str());
	}

	ImGui::Spacing();
	const char* confirmLabel = m_PendingOperation == FileOperation::DeletePath ? "Delete" :
		m_PendingOperation == FileOperation::RemoveRegistry ? "Remove" :
		m_PendingOperation == FileOperation::Move ? "Move" : "Rename";
	if (ImGui::Button(confirmLabel, ImVec2(108.0f, 0.0f)))
	{
		bool success = false;
		switch (m_PendingOperation)
		{
		case FileOperation::Rename: success = RenamePendingItem(); break;
		case FileOperation::Move: success = MovePendingItem(); break;
		case FileOperation::DeletePath: success = DeletePendingItem(); break;
		case FileOperation::RemoveRegistry: success = RemovePendingRegistryEntry(); break;
		default: break;
		}

		if (success)
		{
			ClearPendingOperation();
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(108.0f, 0.0f)))
	{
		ClearPendingOperation();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void ContentBrowserPanel::DrawAutoSliceModal()
{
	const char* popupName = "Smart Slice Texture";
	if (m_ShowAutoSlicePopup)
	{
		ImGui::OpenPopup(popupName);
		m_ShowAutoSlicePopup = false;
	}

	if (!ImGui::BeginPopupModal(popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	ImGui::TextUnformatted("Smart Sprite Slicer");
	ImGui::TextDisabled("%s", m_AutoSliceRelativePath.generic_string().c_str());
	ImGui::Separator();
	ImGui::SetNextItemWidth(260.0f);
	ImGui::InputInt("Min Pixels", &m_AutoSliceMinPixels);
	ImGui::SetNextItemWidth(260.0f);
	ImGui::InputInt("Background Tolerance", &m_AutoSliceBackgroundTolerance);
	ImGui::SetNextItemWidth(260.0f);
	ImGui::InputInt("Fragment Merge Gap", &m_AutoSliceMergeGap);
	ImGui::SetNextItemWidth(260.0f);
	ImGui::InputInt("Padding", &m_AutoSlicePadding);
	ImGui::SetNextItemWidth(260.0f);
	ImGui::InputInt("Extrude Pixels", &m_AutoSliceExtrudePixels);
	m_AutoSliceMinPixels = std::max(1, m_AutoSliceMinPixels);
	m_AutoSliceBackgroundTolerance = std::clamp(m_AutoSliceBackgroundTolerance, 0, 255);
	m_AutoSliceMergeGap = std::max(0, m_AutoSliceMergeGap);
	m_AutoSlicePadding = std::max(0, m_AutoSlicePadding);
	m_AutoSliceExtrudePixels = std::clamp(m_AutoSliceExtrudePixels, 0, 16);
	ImGui::Checkbox("Separate Diagonal Touches", &m_AutoSliceSeparateDiagonalTouches);
	ImGui::Checkbox("Replace Existing Sprite Slices", &m_AutoSliceReplaceExisting);
	ImGui::Checkbox("Export Cropped PNGs", &m_AutoSliceExportPngs);
	if (m_AutoSliceExportPngs)
		ImGui::TextDisabled("Exports to <texture-name>_slices and imports the generated PNG files.");

	if (!m_OperationError.empty())
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "%s", m_OperationError.c_str());
	}

	ImGui::Spacing();
	if (ImGui::Button("Slice", ImVec2(108.0f, 0.0f)))
	{
		if (RunPendingAutoSlice())
			ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(108.0f, 0.0f)))
	{
		m_AutoSliceHandle = 0;
		m_AutoSliceRelativePath.clear();
		m_OperationError.clear();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

std::vector<ContentBrowserPanel::BrowserItem> ContentBrowserPanel::CollectItems() const
{
	std::vector<BrowserItem> items = m_Mode == Mode::Filesystem ? CollectFilesystemItems() : CollectAssetItems();
	items.erase(std::remove_if(items.begin(), items.end(), [this](const BrowserItem& item)
		{
			if (!MatchesSearch(item) || !PassesTypeFilter(item))
				return true;

			if (!m_ShowUnsupported && !item.m_Directory && !item.m_Supported)
				return true;

			return false;
		}), items.end());

	std::sort(items.begin(), items.end(), [](const BrowserItem& left, const BrowserItem& right)
		{
			if (left.m_Directory != right.m_Directory)
				return left.m_Directory > right.m_Directory;

			if (left.m_Handle != 0 && left.m_Handle == right.m_Handle && left.m_SubAsset != right.m_SubAsset)
				return !left.m_SubAsset;

			if (left.m_Handle != 0 && left.m_Handle == right.m_Handle && left.m_SubAsset && right.m_SubAsset)
				return left.m_TextureSpriteIndex < right.m_TextureSpriteIndex;

			return left.m_SortName < right.m_SortName;
		});

	return items;
}

void ContentBrowserPanel::RebuildCachedItems()
{
	m_CachedSearchQueryLower = ToLower(m_SearchQuery);
	m_CachedItems = CollectItems();
	m_CachedItemMetrics = {};
	for (const BrowserItem& item : m_CachedItems)
	{
		if (item.m_Imported && !item.m_Directory)
			++m_CachedItemMetrics.m_Imported;
		if (item.m_Missing)
			++m_CachedItemMetrics.m_Missing;
		if (!item.m_Directory && !item.m_Supported)
			++m_CachedItemMetrics.m_Unsupported;
	}
	m_ItemsDirty = false;
}

void ContentBrowserPanel::InvalidateItems()
{
	m_ItemsDirty = true;
}

void ContentBrowserPanel::RebuildDirectoryTree()
{
	m_DirectoryTree = BuildDirectoryNode(m_BaseDirectory);
	m_DirectoryTreeDirty = false;
}

ContentBrowserPanel::DirectoryNode ContentBrowserPanel::BuildDirectoryNode(const std::filesystem::path& directory) const
{
	DirectoryNode node;
	node.m_Path = directory.lexically_normal();
	node.m_Name = directory == m_BaseDirectory ? "Assets" : directory.filename().string();
	node.m_SortName = ToLower(node.m_Name);

	std::error_code error;
	for (const auto& entry : std::filesystem::directory_iterator(directory, error))
	{
		if (entry.is_directory(error))
			node.m_Children.push_back(BuildDirectoryNode(entry.path()));
	}

	std::sort(node.m_Children.begin(), node.m_Children.end(), [](const DirectoryNode& left, const DirectoryNode& right)
		{
			return left.m_SortName < right.m_SortName;
		});
	return node;
}

void ContentBrowserPanel::FinalizeBrowserItem(BrowserItem& item) const
{
	const std::string displayName = item.m_DisplayName.empty() ? item.m_RelativePath.filename().string() : item.m_DisplayName;
	const std::string typeLabel = ItemTypeLabel(item);
	item.m_SortName = ToLower(displayName);
	item.m_SearchText = ToLower(item.m_RelativePath.filename().string() + " " + item.m_DisplayName + " " + item.m_RelativePath.generic_string() + " " + typeLabel);
}

std::vector<ContentBrowserPanel::BrowserItem> ContentBrowserPanel::CollectFilesystemItems() const
{
	std::vector<BrowserItem> items;
	std::error_code error;
	auto appendItem = [&](BrowserItem item)
		{
			FinalizeBrowserItem(item);
			items.push_back(item);
			if (!item.m_Imported || item.m_Missing || item.m_Type != AssetType::Texture2D || item.m_Handle == 0)
				return;

			const AssetMetadata& metadata = m_Project->GetEditorAssetManager()->GetMetadata(item.m_Handle);
			const auto& sprites = metadata.m_TextureSettings.m_Sprites;
			for (int32_t spriteIndex = 0; spriteIndex < static_cast<int32_t>(sprites.size()); ++spriteIndex)
			{
				const TextureSpriteRect& sprite = sprites[static_cast<size_t>(spriteIndex)];
				BrowserItem spriteItem = item;
				spriteItem.m_DisplayName = sprite.m_Name;
				spriteItem.m_TextureSpriteIndex = spriteIndex;
				spriteItem.m_SubAsset = true;
				FinalizeBrowserItem(spriteItem);
				items.push_back(std::move(spriteItem));
			}
		};

	if (m_SearchQuery.empty())
	{
		for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory, error))
		{
			BrowserItem item = MakeFilesystemItem(entry);
			if (!IsInternalProjectFile(item.m_RelativePath))
				appendItem(item);
		}
	}
	else
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(m_BaseDirectory, error))
		{
			BrowserItem item = MakeFilesystemItem(entry);
			if (!IsInternalProjectFile(item.m_RelativePath))
				appendItem(item);
		}
	}

	return items;
}

std::vector<ContentBrowserPanel::BrowserItem> ContentBrowserPanel::CollectAssetItems() const
{
	std::vector<BrowserItem> items;
	std::set<std::filesystem::path> directoryPaths;
	const auto& registry = m_Project->GetEditorAssetManager()->GetAssetRegistry();
	auto appendAssetItem = [&](BrowserItem item, const AssetMetadata& metadata)
		{
			FinalizeBrowserItem(item);
			items.push_back(item);
			if (item.m_Missing || item.m_Type != AssetType::Texture2D)
				return;

			const auto& sprites = metadata.m_TextureSettings.m_Sprites;
			for (int32_t spriteIndex = 0; spriteIndex < static_cast<int32_t>(sprites.size()); ++spriteIndex)
			{
				const TextureSpriteRect& sprite = sprites[static_cast<size_t>(spriteIndex)];
				BrowserItem spriteItem = item;
				spriteItem.m_DisplayName = sprite.m_Name;
				spriteItem.m_TextureSpriteIndex = spriteIndex;
				spriteItem.m_SubAsset = true;
				FinalizeBrowserItem(spriteItem);
				items.push_back(std::move(spriteItem));
			}
		};

	registry.Foreach([&](const AssetRegistry::ValueType& value)
		{
			BrowserItem item;
			item.m_RelativePath = value.second.m_Filepath;
			item.m_AbsolutePath = m_BaseDirectory / item.m_RelativePath;
			item.m_Handle = value.first;
			item.m_Type = value.second.m_Type;
			item.m_Imported = true;
			item.m_Supported = true;
			item.m_Missing = !std::filesystem::exists(item.m_AbsolutePath);

			if (!m_SearchQuery.empty())
			{
				appendAssetItem(item, value.second);
				return;
			}

			if (item.m_AbsolutePath.parent_path() == m_CurrentDirectory)
			{
				appendAssetItem(item, value.second);
				return;
			}

			if (!IsInsideBaseDirectory(item.m_AbsolutePath))
				return;

			std::error_code error;
			std::filesystem::path relativeToCurrent = std::filesystem::relative(item.m_AbsolutePath, m_CurrentDirectory, error);
			if (error || PathComponentIsParentReference(relativeToCurrent) || PathComponentIsCurrentReference(relativeToCurrent))
				return;

			auto part = relativeToCurrent.begin();
			if (part == relativeToCurrent.end() || std::next(part) == relativeToCurrent.end())
				return;

			directoryPaths.insert(m_CurrentDirectory / *part);
		});

	for (const auto& directory : directoryPaths)
	{
		BrowserItem item;
		item.m_AbsolutePath = directory;
		item.m_RelativePath = std::filesystem::relative(directory, m_BaseDirectory);
		item.m_Directory = true;
		FinalizeBrowserItem(item);
		items.push_back(item);
	}

	return items;
}

void ContentBrowserPanel::SetCurrentDirectory(const std::filesystem::path& directory)
{
	std::error_code error;
	if (!std::filesystem::exists(directory, error) || !std::filesystem::is_directory(directory, error))
		return;

	if (!IsInsideBaseDirectory(directory))
		return;

	m_CurrentDirectory = directory.lexically_normal();
	m_PreferencesDirty = true;
	InvalidateItems();
}

bool ContentBrowserPanel::ImportFile(const std::filesystem::path& relativePath, ImportSummary* summary)
{
	const std::filesystem::path absolutePath = m_BaseDirectory / relativePath;
	if (!std::filesystem::exists(absolutePath))
	{
		if (summary)
			++summary->m_Missing;
		SetStatus("Import failed: file is missing.", true);
		return false;
	}

	if (Utils::TryGetAssetTypeFromFileExtension(relativePath.extension()) == AssetType::None)
	{
		if (summary)
			++summary->m_Unsupported;
		SetStatus("Import skipped: unsupported file format.", true);
		return false;
	}

	if (FindAssetHandle(relativePath) != 0)
	{
		if (summary)
			++summary->m_AlreadyImported;
		SetStatus("Asset is already registered.");
		return true;
	}

	AssetHandle handle = m_Project->GetEditorAssetManager()->ImportAsset(relativePath);
	if (handle != 0)
	{
		if (summary)
			++summary->m_Imported;
		SetStatus("Asset imported: " + relativePath.generic_string());
		RefreshAssetTree();
		return true;
	}

	if (summary)
		++summary->m_Failed;
	SetStatus("Import failed: " + relativePath.generic_string(), true);
	return false;
}

void ContentBrowserPanel::ImportCurrentDirectory(bool recursive)
{
	std::error_code error;
	if (!std::filesystem::exists(m_CurrentDirectory, error))
		return;

	ImportSummary summary;
	if (recursive)
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(m_CurrentDirectory, error))
		{
			if (!entry.is_regular_file(error))
				continue;

			BrowserItem item = MakeFilesystemItem(entry);
			ImportFile(item.m_RelativePath, &summary);
		}
	}
	else
	{
		for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory, error))
		{
			if (!entry.is_regular_file(error))
				continue;

			BrowserItem item = MakeFilesystemItem(entry);
			ImportFile(item.m_RelativePath, &summary);
		}
	}

	RefreshAssetTree();
	SetStatus(ImportSummaryText(summary), summary.m_Failed > 0 || summary.m_Missing > 0);
}

void ContentBrowserPanel::RequestRemoveAsset(AssetHandle handle, const std::filesystem::path& relativePath)
{
	m_PendingOperation = FileOperation::RemoveRegistry;
	m_PendingOperationHandle = handle;
	m_PendingOperationPath = relativePath;
	m_PendingOperationIsDirectory = false;
	m_OperationText.clear();
	m_OperationError.clear();
}

void ContentBrowserPanel::RequestAutoSliceTexture(const BrowserItem& item)
{
	m_AutoSliceHandle = item.m_Handle;
	m_AutoSliceRelativePath = item.m_RelativePath;
	m_AutoSliceMinPixels = 24;
	m_AutoSliceBackgroundTolerance = 24;
	m_AutoSliceMergeGap = 0;
	m_AutoSlicePadding = 1;
	m_AutoSliceExtrudePixels = 0;
	m_AutoSliceSeparateDiagonalTouches = true;
	m_AutoSliceExportPngs = false;
	m_AutoSliceReplaceExisting = true;
	m_OperationError.clear();
	m_ShowAutoSlicePopup = true;
}

bool ContentBrowserPanel::RunPendingAutoSlice()
{
	if (!m_Project || !m_Project->GetEditorAssetManager() || m_AutoSliceHandle == 0)
	{
		m_OperationError = "No texture selected.";
		return false;
	}

	if (!m_Project->GetEditorAssetManager()->IsAssetHandleValid(m_AutoSliceHandle) ||
		m_Project->GetEditorAssetManager()->GetAssetType(m_AutoSliceHandle) != AssetType::Texture2D)
	{
		m_OperationError = "Selected Asset is not a texture.";
		return false;
	}

	TextureSlicer::PixelBuffer buffer;
	std::string error;
	if (!TextureSlicer::LoadTexturePixels(m_AutoSliceHandle, buffer, error))
	{
		m_OperationError = error;
		return false;
	}

	AssetMetadata metadata = m_Project->GetEditorAssetManager()->GetMetadata(m_AutoSliceHandle);
	const std::string stem = metadata.m_Filepath.stem().empty() ? "sprite" : metadata.m_Filepath.stem().string();
	TextureSlicer::AutoSliceOptions options;
	options.m_MinPixels = static_cast<uint32_t>(m_AutoSliceMinPixels);
	options.m_BackgroundTolerance = m_AutoSliceBackgroundTolerance;
	options.m_MergeGap = static_cast<uint32_t>(m_AutoSliceMergeGap);
	options.m_Padding = static_cast<uint32_t>(m_AutoSlicePadding);
	options.m_ExtrudePixels = static_cast<uint32_t>(m_AutoSliceExtrudePixels);
	options.m_SeparateDiagonalTouches = m_AutoSliceSeparateDiagonalTouches;

	TextureSlicer::AutoSliceResult result = TextureSlicer::DetectSprites(buffer, stem, options);
	if (result.m_Sprites.empty())
	{
		m_OperationError = result.m_Error;
		return false;
	}

	metadata.m_TextureSettings.m_SpriteMode = TextureSpriteMode::Multiple;
	if (m_AutoSliceReplaceExisting)
		metadata.m_TextureSettings.m_Sprites = result.m_Sprites;
	else
		metadata.m_TextureSettings.m_Sprites.insert(metadata.m_TextureSettings.m_Sprites.end(), result.m_Sprites.begin(), result.m_Sprites.end());

	if (!m_Project->GetEditorAssetManager()->UpdateAssetMetadata(m_AutoSliceHandle, metadata))
	{
		m_OperationError = "Could not update texture metadata.";
		return false;
	}

	size_t exportedCount = 0;
	if (m_AutoSliceExportPngs)
	{
		const std::filesystem::path outputDirectory = m_BaseDirectory / metadata.m_Filepath.parent_path() / (stem + "_slices");
		std::vector<std::filesystem::path> exportedPaths;
		if (!TextureSlicer::ExportSpritePngs(buffer, result, outputDirectory, stem, exportedPaths, error, options.m_ExtrudePixels))
		{
			m_OperationError = error;
			return false;
		}

		for (const std::filesystem::path& exportedPath : exportedPaths)
		{
			std::error_code relativeError;
			const std::filesystem::path relativePath = std::filesystem::relative(exportedPath, m_BaseDirectory, relativeError).lexically_normal();
			if (!relativeError)
			{
				m_Project->GetEditorAssetManager()->ImportAsset(relativePath);
				++exportedCount;
			}
		}
	}

	RefreshAssetTree();
	SetStatus("Smart sliced " + std::to_string(result.m_Sprites.size()) + " sprite(s)" +
		(exportedCount > 0 ? ", exported " + std::to_string(exportedCount) + " PNG(s)" : "") +
		(result.m_UsedAlpha ? " using alpha." : " using background model (tol " + std::to_string(result.m_EffectiveBackgroundTolerance) + ")."));
	m_AutoSliceHandle = 0;
	m_AutoSliceRelativePath.clear();
	m_OperationError.clear();
	return true;
}

void ContentBrowserPanel::RequestRenameItem(const BrowserItem& item)
{
	m_PendingOperation = FileOperation::Rename;
	m_PendingOperationHandle = item.m_Handle;
	m_PendingOperationPath = item.m_RelativePath;
	m_PendingOperationIsDirectory = item.m_Directory;
	m_OperationText = item.m_RelativePath.filename().string();
	m_OperationError.clear();
}

void ContentBrowserPanel::RequestMoveItem(const BrowserItem& item)
{
	m_PendingOperation = FileOperation::Move;
	m_PendingOperationHandle = item.m_Handle;
	m_PendingOperationPath = item.m_RelativePath;
	m_PendingOperationIsDirectory = item.m_Directory;
	std::filesystem::path parent = item.m_RelativePath.parent_path();
	m_OperationText = parent.empty() ? "" : parent.generic_string();
	m_OperationError.clear();
}

void ContentBrowserPanel::RequestDeleteItem(const BrowserItem& item)
{
	m_PendingOperation = FileOperation::DeletePath;
	m_PendingOperationHandle = item.m_Handle;
	m_PendingOperationPath = item.m_RelativePath;
	m_PendingOperationIsDirectory = item.m_Directory;
	m_OperationText.clear();
	m_OperationError.clear();
}

bool ContentBrowserPanel::OpenAsset(const BrowserItem& item)
{
	if (item.m_Directory || item.m_Missing || !item.m_Supported)
		return false;

	AssetHandle handle = item.m_Handle;
	if (handle == 0)
	{
		if (!ImportFile(item.m_RelativePath))
			return false;
		handle = FindAssetHandle(item.m_RelativePath);
	}

	if (handle == 0 || !m_AssetOpenCallback)
		return false;

	if (m_AssetOpenCallback(handle))
	{
		SetStatus("Opened: " + item.m_RelativePath.generic_string());
		return true;
	}

	return false;
}

bool ContentBrowserPanel::InspectAsset(const BrowserItem& item)
{
	if (item.m_Directory || item.m_Missing || !item.m_Supported || !m_AssetInspectCallback)
		return false;

	AssetHandle handle = item.m_Handle;
	if (handle == 0)
	{
		if (!ImportFile(item.m_RelativePath))
			return false;
		handle = FindAssetHandle(item.m_RelativePath);
	}

	if (handle == 0)
		return false;

	if (m_AssetInspectCallback(handle))
	{
		SetStatus("Editing: " + item.m_RelativePath.generic_string());
		return true;
	}

	return false;
}

bool ContentBrowserPanel::SetSceneAsStartScene(const BrowserItem& item)
{
	if (!m_Project || item.m_Directory || item.m_Missing || item.m_Type != AssetType::Scene)
		return false;

	AssetHandle handle = item.m_Handle;
	if (handle == 0)
	{
		if (!ImportFile(item.m_RelativePath))
			return false;
		handle = FindAssetHandle(item.m_RelativePath);
	}

	if (handle == 0)
		return false;

	m_Project->GetConfig().m_StartScene = handle;
	Project::SaveActive();
	SetStatus("Start scene set: " + item.m_RelativePath.generic_string());
	return true;
}

void ContentBrowserPanel::ClearPendingOperation()
{
	m_PendingOperation = FileOperation::None;
	m_PendingOperationHandle = 0;
	m_PendingOperationPath.clear();
	m_PendingOperationIsDirectory = false;
	m_OperationText.clear();
	m_OperationError.clear();
}

bool ContentBrowserPanel::RenamePendingItem()
{
	if (m_OperationText.empty())
	{
		m_OperationError = "Name is required.";
		return false;
	}

	std::filesystem::path source = m_BaseDirectory / m_PendingOperationPath;
	if (!IsInsideBaseDirectory(source))
	{
		m_OperationError = "Source must stay inside Assets.";
		return false;
	}

	if (!std::filesystem::exists(source))
	{
		m_OperationError = "Source item is missing.";
		return false;
	}

	std::filesystem::path newName = m_OperationText;
	if (!m_PendingOperationIsDirectory && newName.extension().empty())
		newName.replace_extension(m_PendingOperationPath.extension());
	std::filesystem::path target = source.parent_path() / newName.filename();

	if (!IsInsideBaseDirectory(target))
	{
		m_OperationError = "Target must stay inside Assets.";
		return false;
	}
	if (std::filesystem::exists(target))
	{
		m_OperationError = "An item with that name already exists.";
		return false;
	}

	std::error_code error;
	std::filesystem::rename(source, target, error);
	if (error)
	{
		m_OperationError = error.message();
		return false;
	}

	const std::filesystem::path targetRelative = MakeRelativePath(target);
	if (m_PendingOperationIsDirectory)
		m_Project->GetEditorAssetManager()->UpdateAssetDirectoryPaths(m_PendingOperationPath, targetRelative);
	else if (m_PendingOperationHandle != 0)
		m_Project->GetEditorAssetManager()->UpdateAssetFilepath(m_PendingOperationHandle, targetRelative);

	if (PathIsUnderDirectory(m_CurrentDirectory, source))
		m_CurrentDirectory = target;
	RefreshAssetTree();
	SetStatus("Renamed: " + targetRelative.generic_string());
	return true;
}

bool ContentBrowserPanel::MovePendingItem()
{
	std::filesystem::path destinationDirectory = m_OperationText.empty() ? m_BaseDirectory : m_BaseDirectory / m_OperationText;
	return MovePathToDirectory(m_PendingOperationPath, destinationDirectory);
}

bool ContentBrowserPanel::DeletePendingItem()
{
	if (!m_Project || !m_Project->GetEditorAssetManager())
	{
		m_OperationError = "No Project Asset manager is available.";
		return false;
	}

	const std::filesystem::path absolutePath = m_BaseDirectory / m_PendingOperationPath;
	if (!IsInsideBaseDirectory(absolutePath))
	{
		m_OperationError = "Target must stay inside Assets.";
		return false;
	}

	std::error_code error;
	const bool pathExists = std::filesystem::exists(absolutePath, error);
	if (error)
	{
		m_OperationError = error.message();
		return false;
	}

	Ref<Project> activeProject = Project::GetActive();
	bool clearsStartScene = false;
	if (m_PendingOperationIsDirectory)
	{
		if (activeProject && activeProject->GetEditorAssetManager() && activeProject->GetConfig().m_StartScene != 0 && activeProject->GetEditorAssetManager()->IsAssetHandleValid(activeProject->GetConfig().m_StartScene))
		{
			const std::filesystem::path startScenePath = activeProject->GetEditorAssetManager()->GetFilepath(activeProject->GetConfig().m_StartScene);
			if (PathIsUnderDirectory(startScenePath, m_PendingOperationPath))
				clearsStartScene = true;
		}

		if (pathExists)
			std::filesystem::remove_all(absolutePath, error);
	}
	else
	{
		if (!pathExists && m_PendingOperationHandle == 0)
		{
			m_OperationError = "Item is missing.";
			return false;
		}

		clearsStartScene = activeProject && m_PendingOperationHandle != 0 && activeProject->GetConfig().m_StartScene == m_PendingOperationHandle;

		if (pathExists)
			std::filesystem::remove(absolutePath, error);
	}

	if (error)
	{
		m_OperationError = error.message();
		return false;
	}

	if (clearsStartScene && activeProject)
	{
		activeProject->GetConfig().m_StartScene = 0;
		Project::SaveActive();
	}

	if (m_PendingOperationIsDirectory)
		m_Project->GetEditorAssetManager()->DeleteAssetsUnderDirectory(m_PendingOperationPath);
	else if (m_PendingOperationHandle != 0)
		m_Project->GetEditorAssetManager()->DeleteAsset(m_PendingOperationHandle);

	if (PathIsUnderDirectory(m_CurrentDirectory, absolutePath))
		m_CurrentDirectory = m_BaseDirectory;
	RefreshAssetTree();
	SetStatus("Deleted: " + m_PendingOperationPath.generic_string());
	return true;
}

bool ContentBrowserPanel::RemovePendingRegistryEntry()
{
	if (!m_Project || !m_Project->GetEditorAssetManager())
	{
		m_OperationError = "No Project Asset manager is available.";
		return false;
	}

	if (m_PendingOperationHandle == 0)
	{
		m_OperationError = "No Asset registration selected.";
		return false;
	}

	Ref<Project> activeProject = Project::GetActive();
	if (activeProject && activeProject->GetConfig().m_StartScene == m_PendingOperationHandle)
	{
		activeProject->GetConfig().m_StartScene = 0;
		Project::SaveActive();
	}

	m_Project->GetEditorAssetManager()->DeleteAsset(m_PendingOperationHandle);
	RefreshAssetTree();
	SetStatus("Removed registry entry: " + m_PendingOperationPath.generic_string());
	return true;
}

bool ContentBrowserPanel::DuplicateItem(const BrowserItem& item)
{
	if (item.m_Missing)
	{
		SetStatus("Duplicate failed: source Asset is missing.", true);
		return false;
	}

	std::filesystem::path target = MakeUniqueCopyPath(item.m_AbsolutePath);
	std::error_code error;
	if (item.m_Directory)
		std::filesystem::copy(item.m_AbsolutePath, target, std::filesystem::copy_options::recursive, error);
	else
		std::filesystem::copy_file(item.m_AbsolutePath, target, std::filesystem::copy_options::none, error);

	if (error)
	{
		SetStatus("Duplicate failed: " + error.message(), true);
		return false;
	}

	ImportSummary summary;
	if (item.m_Directory)
		ImportSupportedFilesUnder(target, summary);
	else if (Utils::TryGetAssetTypeFromFileExtension(target.extension()) != AssetType::None)
		ImportFile(MakeRelativePath(target), &summary);

	RefreshAssetTree();
	if (summary.m_Imported > 0 || summary.m_Failed > 0 || summary.m_Unsupported > 0)
		SetStatus("Duplicated: " + MakeRelativePath(target).generic_string() + " | " + ImportSummaryText(summary), summary.m_Failed > 0);
	else
		SetStatus("Duplicated: " + MakeRelativePath(target).generic_string());
	return true;
}

bool ContentBrowserPanel::MovePathToDirectory(const std::filesystem::path& sourceRelativePath, const std::filesystem::path& destinationDirectory)
{
	const std::filesystem::path source = m_BaseDirectory / sourceRelativePath;
	if (!IsInsideBaseDirectory(source))
	{
		m_OperationError = "Source must stay inside Assets.";
		SetStatus("Move failed: source must stay inside Assets.", true);
		return false;
	}

	if (!std::filesystem::exists(source))
	{
		m_OperationError = "Source item is missing.";
		SetStatus("Move failed: source item is missing.", true);
		return false;
	}
	if (!std::filesystem::exists(destinationDirectory) || !std::filesystem::is_directory(destinationDirectory))
	{
		m_OperationError = "Destination folder does not exist.";
		SetStatus("Move failed: destination folder does not exist.", true);
		return false;
	}
	if (!IsInsideBaseDirectory(destinationDirectory))
	{
		m_OperationError = "Destination must stay inside Assets.";
		SetStatus("Move failed: destination must stay inside Assets.", true);
		return false;
	}
	if (PathIsUnderDirectory(destinationDirectory, source))
	{
		m_OperationError = "Cannot move a folder into itself.";
		SetStatus("Move failed: cannot move a folder into itself.", true);
		return false;
	}

	std::filesystem::path target = destinationDirectory / source.filename();
	if (source.lexically_normal() == target.lexically_normal())
		return true;
	if (std::filesystem::exists(target))
	{
		m_OperationError = "Destination already contains an item with this name.";
		SetStatus("Move failed: destination item already exists.", true);
		return false;
	}

	const bool sourceIsDirectory = std::filesystem::is_directory(source);
	std::error_code error;
	std::filesystem::rename(source, target, error);
	if (error)
	{
		m_OperationError = error.message();
		SetStatus("Move failed: " + error.message(), true);
		return false;
	}

	const std::filesystem::path targetRelative = MakeRelativePath(target);
	if (sourceIsDirectory)
		m_Project->GetEditorAssetManager()->UpdateAssetDirectoryPaths(sourceRelativePath, targetRelative);
	else if (AssetHandle handle = FindAssetHandle(sourceRelativePath); handle != 0)
		m_Project->GetEditorAssetManager()->UpdateAssetFilepath(handle, targetRelative);

	if (PathIsUnderDirectory(m_CurrentDirectory, source))
		m_CurrentDirectory = target;
	RefreshAssetTree();
	SetStatus("Moved: " + sourceRelativePath.generic_string() + " -> " + targetRelative.generic_string());
	return true;
}

void ContentBrowserPanel::ImportSupportedFilesUnder(const std::filesystem::path& directory, ImportSummary& summary)
{
	std::error_code error;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, error))
	{
		if (!entry.is_regular_file(error))
			continue;
		ImportFile(MakeRelativePath(entry.path()), &summary);
	}
}

bool ContentBrowserPanel::HandleExternalDrop(const std::vector<std::filesystem::path>& paths)
{
	if (!m_Project || paths.empty())
		return false;

	ImportSummary summary;
	bool handled = false;
	for (const auto& sourcePath : paths)
		handled = ImportExternalPath(sourcePath, summary) || handled;

	RefreshAssetTree();
	SetStatus("External drop: " + ImportSummaryText(summary), summary.m_Failed > 0 || summary.m_Missing > 0);
	return handled;
}

bool ContentBrowserPanel::ImportExternalPath(const std::filesystem::path& sourcePath, ImportSummary& summary)
{
	std::error_code error;
	if (!std::filesystem::exists(sourcePath, error))
	{
		++summary.m_Missing;
		return false;
	}

	const bool sourceInsideAssets = IsInsideBaseDirectory(sourcePath);
	std::filesystem::path importPath = sourcePath;
	if (!sourceInsideAssets)
	{
		std::filesystem::create_directories(m_CurrentDirectory, error);
		if (error)
		{
			++summary.m_Failed;
			return false;
		}

		importPath = MakeUniqueImportPath(m_CurrentDirectory / sourcePath.filename());
		error.clear();
		if (std::filesystem::is_directory(sourcePath, error))
			std::filesystem::copy(sourcePath, importPath, std::filesystem::copy_options::recursive, error);
		else if (std::filesystem::is_regular_file(sourcePath, error))
			std::filesystem::copy_file(sourcePath, importPath, std::filesystem::copy_options::none, error);
		else
		{
			++summary.m_Unsupported;
			return false;
		}

		if (error)
		{
			++summary.m_Failed;
			SetStatus("External drop failed: " + error.message(), true);
			return false;
		}
	}

	error.clear();
	if (std::filesystem::is_directory(importPath, error))
	{
		ImportSupportedFilesUnder(importPath, summary);
		return true;
	}

	if (std::filesystem::is_regular_file(importPath, error))
		return ImportFile(MakeRelativePath(importPath), &summary);

	++summary.m_Unsupported;
	return false;
}

bool ContentBrowserPanel::IsInsideBaseDirectory(const std::filesystem::path& path) const
{
	std::error_code error;
	std::filesystem::path relativePath = std::filesystem::relative(path, m_BaseDirectory, error);
	if (error)
		return false;

	return !PathComponentIsParentReference(relativePath);
}

bool ContentBrowserPanel::MatchesSearch(const BrowserItem& item) const
{
	if (m_CachedSearchQueryLower.empty())
		return true;

	return item.m_SearchText.find(m_CachedSearchQueryLower) != std::string::npos;
}

bool ContentBrowserPanel::PassesTypeFilter(const BrowserItem& item) const
{
	if (m_TypeFilter == AssetType::None || item.m_Directory)
		return true;

	return item.m_Type == m_TypeFilter;
}

ContentBrowserPanel::BrowserItem ContentBrowserPanel::MakeFilesystemItem(const std::filesystem::directory_entry& entry) const
{
	std::error_code error;
	BrowserItem item;
	item.m_AbsolutePath = entry.path().lexically_normal();
	item.m_RelativePath = std::filesystem::relative(item.m_AbsolutePath, m_BaseDirectory, error);
	item.m_Directory = entry.is_directory(error);

	if (!item.m_Directory)
	{
		item.m_Handle = FindAssetHandle(item.m_RelativePath);
		item.m_Imported = item.m_Handle != 0;
		item.m_Type = item.m_Imported ? m_Project->GetEditorAssetManager()->GetAssetType(item.m_Handle) : Utils::TryGetAssetTypeFromFileExtension(item.m_RelativePath.extension());
		item.m_Supported = item.m_Type != AssetType::None;
	}

	return item;
}

AssetHandle ContentBrowserPanel::FindAssetHandle(const std::filesystem::path& relativePath) const
{
	return m_Project->GetEditorAssetManager()->GetHandleFromFilepath(relativePath);
}

std::filesystem::path ContentBrowserPanel::MakeRelativePath(const std::filesystem::path& absolutePath) const
{
	std::error_code error;
	std::filesystem::path relativePath = std::filesystem::relative(absolutePath, m_BaseDirectory, error);
	if (error)
		return absolutePath.filename();
	return relativePath.lexically_normal();
}

std::filesystem::path ContentBrowserPanel::MakeUniqueCopyPath(const std::filesystem::path& absolutePath) const
{
	const std::filesystem::path parent = absolutePath.parent_path();
	const std::string stem = absolutePath.stem().string();
	const std::string extension = absolutePath.extension().string();
	std::filesystem::path candidate = parent / (stem + " Copy" + extension);
	int suffix = 2;
	while (std::filesystem::exists(candidate))
		candidate = parent / (stem + " Copy " + std::to_string(suffix++) + extension);
	return candidate;
}

std::filesystem::path ContentBrowserPanel::MakeUniqueImportPath(const std::filesystem::path& absolutePath) const
{
	if (!std::filesystem::exists(absolutePath))
		return absolutePath;
	return MakeUniqueCopyPath(absolutePath);
}

void ContentBrowserPanel::SetStatus(std::string message, bool error)
{
	m_StatusMessage = std::move(message);
	m_StatusError = error;
}

std::string ContentBrowserPanel::DisplayPath(const std::filesystem::path& path) const
{
	if (path.empty())
		return "Assets";

	return path.generic_string();
}

std::string ContentBrowserPanel::ItemTypeLabel(const BrowserItem& item) const
{
	if (item.m_Directory)
		return "Folder";

	if (item.m_SubAsset)
		return "Texture Sprite";

	if (item.m_Imported)
		return std::string(frenum::to_string(item.m_Type)) + " Asset";

	if (item.m_Supported)
		return std::string(frenum::to_string(item.m_Type)) + " file";

	return "Unsupported";
}

std::string ContentBrowserPanel::AssetTypeFilterLabel() const
{
	if (m_TypeFilter == AssetType::None)
		return "All Types";

	return std::string(frenum::to_string(m_TypeFilter));
}

void ContentBrowserPanel::OnSettingsPopup()
{
	if (m_ShowSettingsPopup)
	{
		ImVec2 windowSize{ (float)Application::Get().GetWindow().GetWidth(), (float)Application::Get().GetWindow().GetHeight() };
		ImVec2 windowPos{ (float)Application::Get().GetWindow().GetPosition().first, (float)Application::Get().GetWindow().GetPosition().second };
		ImVec2 popupSize(352, 200);
		ImVec2 popupPos = ImVec2{ ((windowSize.x - popupSize.x) * 0.5f) + windowPos.x, ((windowSize.y - popupSize.y) * 0.5f) + windowPos.y };

		ImGui::SetNextWindowSize(popupSize);
		ImGui::SetNextWindowPos(popupPos);

		ImGui::OpenPopup("Content Browser Settings");

		if (ImGui::BeginPopupModal("Content Browser Settings", NULL, ImGuiWindowFlags_NoResize))
		{
			ImGui::Text("Thumbnail Size");
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::DragFloat("##Thumbnail Size", &m_ThumbnailSize, 0.5f, 16, 512))
				m_PreferencesDirty = true;
			ImGui::Text("Padding");
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::DragFloat("##Padding", &m_Padding, 0.05f, 0, 32))
				m_PreferencesDirty = true;
			if (ImGui::Checkbox("Show unsupported files", &m_ShowUnsupported))
			{
				m_PreferencesDirty = true;
				InvalidateItems();
			}
			const float footerY = ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().WindowPadding.y;
			if (ImGui::GetCursorPosY() < footerY)
				ImGui::SetCursorPosY(footerY);
			ImGui::Separator();
			ImGui::SetCursorPosX(ImGui::GetWindowSize().x - 112.0f);
			if (ImGui::Button("OK", ImVec2(96.0f, 0.0f)))
			{
				ImGui::CloseCurrentPopup();
				m_ShowSettingsPopup = false;
			}
			ImGui::EndPopup();
		}
	}
}

void ContentBrowserPanel::RefreshAssetTree()
{
	std::error_code error;
	if (!std::filesystem::exists(m_BaseDirectory, error))
		std::filesystem::create_directories(m_BaseDirectory, error);

	if (!std::filesystem::exists(m_CurrentDirectory, error) || !IsInsideBaseDirectory(m_CurrentDirectory))
		m_CurrentDirectory = m_BaseDirectory;

	m_DirectoryTreeDirty = true;
	InvalidateItems();
}

ContentBrowserPanel::Preferences ContentBrowserPanel::GetPreferences() const
{
	Preferences prefs;
	prefs.m_ThumbnailSize = m_ThumbnailSize;
	prefs.m_Padding = m_Padding;
	prefs.m_ShowUnsupported = m_ShowUnsupported;
	prefs.m_Open = m_Open;
	prefs.m_Mode = static_cast<int>(m_Mode);
	prefs.m_TypeFilter = static_cast<int>(m_TypeFilter);
	prefs.m_CurrentDirectory = m_CurrentDirectory;
	return prefs;
}

void ContentBrowserPanel::ApplyPreferences(const Preferences& prefs)
{
	m_ThumbnailSize = std::clamp(prefs.m_ThumbnailSize, 16.0f, 512.0f);
	m_Padding = std::clamp(prefs.m_Padding, 0.0f, 32.0f);
	m_ShowUnsupported = prefs.m_ShowUnsupported;
	m_Open = prefs.m_Open;
	m_Mode = prefs.m_Mode == static_cast<int>(Mode::Asset) ? Mode::Asset : Mode::Filesystem;
	m_TypeFilter = static_cast<AssetType>(prefs.m_TypeFilter);
	if (!prefs.m_CurrentDirectory.empty())
		SetCurrentDirectory(prefs.m_CurrentDirectory);
	InvalidateItems();
	m_PreferencesDirty = false;
}

bool ContentBrowserPanel::ConsumePreferencesDirty()
{
	bool dirty = m_PreferencesDirty;
	m_PreferencesDirty = false;
	return dirty;
}

void ContentBrowserPanel::SetOpen(bool open)
{
	if (m_Open == open)
		return;
	m_Open = open;
	m_PreferencesDirty = true;
}

_WHIP_END
