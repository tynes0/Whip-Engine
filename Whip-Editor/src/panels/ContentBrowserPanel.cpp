#include <WhipPch.h>

#include "ContentBrowserPanel.h"

#include "../Helpers/IconManager.h"

#include <Whip/Asset/AssetManager.h>
#include <Whip/Asset/AssetUtils.h>
#include <Whip/Core/Application.h>
#include <Whip/Utils/FileExtensions.h>
#include <Whip/Project/Project.h>
#include <Whip/UI/UIHelpers.h>
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
	Init(proj);
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

		std::vector<BrowserItem> items = CollectItems();
		DrawContentGrid(items);

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
	OnSettingsPopup();
	ImGui::End();
}

void ContentBrowserPanel::DrawToolbar()
{
	if (ImGui::RadioButton("Files", m_Mode == Mode::Filesystem))
	{
		m_Mode = Mode::Filesystem;
		m_PreferencesDirty = true;
	}

	ImGui::SameLine();
	if (ImGui::RadioButton("Imported", m_Mode == Mode::Asset))
	{
		m_Mode = Mode::Asset;
		m_PreferencesDirty = true;
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
	ImGui::InputTextWithHint("##ContentBrowserSearch", "Search assets and files", &m_SearchQuery);

	if (!m_SearchQuery.empty())
	{
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
			m_SearchQuery.clear();
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
		DrawDirectoryTree(m_BaseDirectory);
		ImGui::TreePop();
	}

	ImGui::EndChild();
}

void ContentBrowserPanel::DrawDirectoryTree(const std::filesystem::path& directory)
{
	std::vector<std::filesystem::path> directories;
	std::error_code error;
	for (const auto& entry : std::filesystem::directory_iterator(directory, error))
	{
		if (entry.is_directory(error))
			directories.push_back(entry.path());
	}

	std::sort(directories.begin(), directories.end(), [](const auto& left, const auto& right)
		{
			return ToLower(left.filename().string()) < ToLower(right.filename().string());
		});

	for (const auto& childDirectory : directories)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (childDirectory == m_CurrentDirectory)
			flags |= ImGuiTreeNodeFlags_Selected;

		const bool open = ImGui::TreeNodeEx(childDirectory.filename().string().c_str(), flags);
		if (ImGui::IsItemClicked())
			SetCurrentDirectory(childDirectory);
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_PATH"))
			{
				std::filesystem::path sourceRelativePath(std::string(static_cast<const char*>(payload->Data), payload->DataSize));
				MovePathToDirectory(sourceRelativePath, childDirectory);
			}
			ImGui::EndDragDropTarget();
		}

		if (open)
		{
			DrawDirectoryTree(childDirectory);
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
	const size_t importedCount = static_cast<size_t>(std::count_if(items.begin(), items.end(), [](const BrowserItem& item) { return item.m_Imported && !item.m_Directory; }));
	const size_t missingCount = static_cast<size_t>(std::count_if(items.begin(), items.end(), [](const BrowserItem& item) { return item.m_Missing; }));
	const size_t unsupportedCount = static_cast<size_t>(std::count_if(items.begin(), items.end(), [](const BrowserItem& item) { return !item.m_Directory && !item.m_Supported; }));
	ImGui::TextDisabled("%zu item(s) in %s view | %zu imported | %zu missing | %zu unsupported %s",
		items.size(), modeLabel, importedCount, missingCount, unsupportedCount, m_ShowUnsupported ? "visible" : "hidden");

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

		int columnIndex = 0;
		for (const BrowserItem& item : items)
		{
			if (columnIndex == 0)
				ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(columnIndex);
			DrawItem(item);

			columnIndex = (columnIndex + 1) % columnCount;
		}
		ImGui::EndTable();
	}
	ImGui::PopStyleVar();
}

void ContentBrowserPanel::DrawItem(const BrowserItem& item)
{
	const std::string itemId = item.m_RelativePath.generic_string();
	ImGui::PushID(itemId.c_str());
	ImGui::BeginGroup();

	Ref<Texture2D> thumbnail = item.m_Directory ? IconManager::Get().GetIcon(Icon::Directory) : nullptr;
	if (!thumbnail && item.m_Type == AssetType::Texture2D && std::filesystem::exists(item.m_AbsolutePath))
		thumbnail = m_ThumbnailCache->GetOrCreateThumbnail(item.m_RelativePath);
	if (!thumbnail)
		thumbnail = IconManager::Get().GetIcon(Icon::File);

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	const bool iconClicked = UI::ImageButton("##ContentBrowserItemIcon", UI::ToImGuiTextureId(thumbnail->GetRendererId()), { m_ThumbnailSize, m_ThumbnailSize }, { 0, 1 }, { 1, 0 });
	ImGui::PopStyleColor();

	if (iconClicked && item.m_Directory)
		SetCurrentDirectory(item.m_AbsolutePath);

	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && item.m_Directory)
		SetCurrentDirectory(item.m_AbsolutePath);
	else if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !item.m_Directory)
		OpenAsset(item);

	if (ImGui::BeginDragDropSource())
	{
		std::string relativePath = item.m_RelativePath.generic_string();
		ImGui::SetDragDropPayload("CONTENT_BROWSER_PATH", relativePath.data(), relativePath.size());
		if (item.m_Supported && !item.m_Directory && !item.m_Missing)
		{
			AssetHandle handle = item.m_Handle;
			if (handle == 0)
			{
				ImportFile(item.m_RelativePath);
				handle = FindAssetHandle(item.m_RelativePath);
			}
			if (handle != 0)
				ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", &handle, sizeof(AssetHandle));
		}
		ImGui::TextUnformatted(item.m_RelativePath.filename().string().c_str());
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
			if (item.m_Missing)
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
					const bool isStartScene = m_Project && m_Project->GetConfig().m_StartScene == item.m_Handle;
					if (ImGui::MenuItem("Set as Start Scene", nullptr, isStartScene))
						SetSceneAsStartScene(item);
					ImGui::Separator();
				}
				if (item.m_Supported && item.m_Type == AssetType::Entity)
				{
					if (ImGui::MenuItem("Instantiate Entity Template"))
						OpenAsset(item);
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
			if (item.m_Imported && !item.m_Missing && ImGui::MenuItem("Remove from Registry"))
				RequestRemoveAsset(item.m_Handle, item.m_RelativePath);
			if (!item.m_Supported && !item.m_Missing)
				ImGui::TextDisabled("Unsupported Asset type");
		}

		ImGui::EndPopup();
	}

	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + m_ThumbnailSize);
	ImGui::TextWrapped(item.m_RelativePath.filename().string().c_str());
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

			return ToLower(left.m_RelativePath.filename().string()) < ToLower(right.m_RelativePath.filename().string());
		});

	return items;
}

std::vector<ContentBrowserPanel::BrowserItem> ContentBrowserPanel::CollectFilesystemItems() const
{
	std::vector<BrowserItem> items;
	std::error_code error;

	if (m_SearchQuery.empty())
	{
		for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory, error))
		{
			BrowserItem item = MakeFilesystemItem(entry);
			if (!IsInternalProjectFile(item.m_RelativePath))
				items.push_back(item);
		}
	}
	else
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(m_BaseDirectory, error))
		{
			BrowserItem item = MakeFilesystemItem(entry);
			if (!IsInternalProjectFile(item.m_RelativePath))
				items.push_back(item);
		}
	}

	return items;
}

std::vector<ContentBrowserPanel::BrowserItem> ContentBrowserPanel::CollectAssetItems() const
{
	std::vector<BrowserItem> items;
	std::set<std::filesystem::path> directoryPaths;
	const auto& registry = m_Project->GetEditorAssetManager()->GetAssetRegistry();

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
				items.push_back(item);
				return;
			}

			if (item.m_AbsolutePath.parent_path() == m_CurrentDirectory)
			{
				items.push_back(item);
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
	if (m_SearchQuery.empty())
		return true;

	const std::string query = ToLower(m_SearchQuery);
	const std::string filename = ToLower(item.m_RelativePath.filename().string());
	const std::string path = ToLower(item.m_RelativePath.generic_string());
	const std::string type = ToLower(ItemTypeLabel(item));

	return filename.find(query) != std::string::npos || path.find(query) != std::string::npos || type.find(query) != std::string::npos;
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
				m_PreferencesDirty = true;
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
