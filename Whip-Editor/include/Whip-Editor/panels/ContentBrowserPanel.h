#pragma once

#include <Whip-Editor/panels/ThumbnailCache.h>

#include <Whip/Core/Core.h>
#include <Whip/Asset/Asset.h>
#include <Whip/Render/Texture.h>

#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>


_WHIP_START

class ContentBrowserPanel
{
public:
	ContentBrowserPanel();
	ContentBrowserPanel(Ref<Project> proj);

	struct Preferences
	{
		float m_ThumbnailSize = 64.0f;
		float m_Padding = 16.0f;
		bool m_ShowUnsupported = false;
		bool m_Open = true;
		int m_Mode = 0;
		int m_TypeFilter = 0;
		std::filesystem::path m_CurrentDirectory;
	};

	struct ImportSummary
	{
		size_t m_Imported = 0;
		size_t m_AlreadyImported = 0;
		size_t m_Unsupported = 0;
		size_t m_Failed = 0;
		size_t m_Missing = 0;
	};
	
	void Init(Ref<Project> proj);

	void OnImGuiRender();
	void OnSettingsPopup();
	void RefreshAssetTree();
	Preferences GetPreferences() const;
	void ApplyPreferences(const Preferences& prefs);
	bool ConsumePreferencesDirty();
	void SetOpen(bool open);
	bool IsOpen() const { return m_Open; }
	bool IsHovered() const { return m_Hovered; }
	void SetAssetOpenCallback(std::function<bool(AssetHandle)> callback) { m_AssetOpenCallback = std::move(callback); }
	void SetAssetInspectCallback(std::function<bool(AssetHandle)> callback) { m_AssetInspectCallback = std::move(callback); }
	bool HandleExternalDrop(const std::vector<std::filesystem::path>& paths);
private:
	enum class Mode
	{
		Filesystem = 0,
		Asset = 1
	};

	struct BrowserItem
	{
		std::filesystem::path m_AbsolutePath;
		std::filesystem::path m_RelativePath;
		std::string m_DisplayName;
		std::string m_DisplayText;
		std::string m_DrawId;
		std::string m_SortName;
		std::string m_SearchText;
		AssetHandle m_Handle = 0;
		AssetType m_Type = AssetType::None;
		int32_t m_TextureSpriteIndex = -1;
		size_t m_SubAssetCount = 0;
		bool m_Directory = false;
		bool m_Imported = false;
		bool m_Supported = false;
		bool m_Missing = false;
		bool m_SubAsset = false;
		bool m_ScriptFile = false;
	};

	struct DirectoryNode
	{
		std::filesystem::path m_Path;
		std::string m_Name;
		std::string m_SortName;
		std::vector<DirectoryNode> m_Children;
	};

	struct ItemMetrics
	{
		size_t m_Imported = 0;
		size_t m_Missing = 0;
		size_t m_Unsupported = 0;
	};

	enum class FileOperation
	{
		None = 0,
		Rename,
		Move,
		DeletePath,
		RemoveRegistry
	};

	void DrawToolbar();
	void DrawStatusBar();
	void DrawSidebar();
	void DrawDirectoryTree(const DirectoryNode& node);
	void DrawBreadcrumbs();
	void DrawContentGrid(const std::vector<BrowserItem>& items);
	void DrawItem(const BrowserItem& item);
	void DrawFileOperationModals();
	void DrawAutoSliceModal();
	void DrawTypeFilter();

	std::vector<BrowserItem> CollectItems() const;
	std::vector<BrowserItem> CollectFilesystemItems() const;
	std::vector<BrowserItem> CollectAssetItems() const;
	void RebuildCachedItems();
	void InvalidateItems();
	void RebuildDirectoryTree();
	DirectoryNode BuildDirectoryNode(const std::filesystem::path& directory) const;
	void FinalizeBrowserItem(BrowserItem& item) const;
	void AppendTextureSpriteItems(std::vector<BrowserItem>& items, const BrowserItem& parentItem, const AssetMetadata& metadata) const;
	bool AreTextureSpritesCollapsed(const BrowserItem& item) const;
	void ToggleTextureSprites(const BrowserItem& item);

	void SetCurrentDirectory(const std::filesystem::path& directory);
	bool ImportFile(const std::filesystem::path& relativePath, ImportSummary* summary = nullptr);
	void ImportCurrentDirectory(bool recursive);
	void RequestRenameItem(const BrowserItem& item);
	void RequestMoveItem(const BrowserItem& item);
	void RequestDeleteItem(const BrowserItem& item);
	void RequestRemoveAsset(AssetHandle handle, const std::filesystem::path& relativePath);
	void RequestAutoSliceTexture(const BrowserItem& item);
	bool RunPendingAutoSlice();
	bool OpenAsset(const BrowserItem& item);
	bool InspectAsset(const BrowserItem& item);
	bool SetSceneAsStartScene(const BrowserItem& item);
	bool RemoveSpriteSlice(const BrowserItem& item);
	bool ClearTextureSprites(const BrowserItem& item);
	bool CreateAnimationFromSelection();
	void ClearPendingOperation();
	bool RenamePendingItem();
	bool MovePendingItem();
	bool DeletePendingItem();
	bool RemovePendingRegistryEntry();
	bool DuplicateItem(const BrowserItem& item);
	bool MovePathToDirectory(const std::filesystem::path& sourceRelativePath, const std::filesystem::path& destinationDirectory);
	void ImportSupportedFilesUnder(const std::filesystem::path& directory, ImportSummary& summary);
	bool ImportExternalPath(const std::filesystem::path& sourcePath, ImportSummary& summary);

	bool IsInsideBaseDirectory(const std::filesystem::path& path) const;
	bool MatchesSearch(const BrowserItem& item) const;
	bool PassesTypeFilter(const BrowserItem& item) const;
	bool IsItemSelected(const BrowserItem& item) const;
	void SelectItem(const BrowserItem& item, bool additive);
	std::vector<BrowserItem> GetSelectedItems() const;
	bool SelectionContainsOnlyTextures(const std::vector<BrowserItem>& items) const;
	BrowserItem MakeFilesystemItem(const std::filesystem::directory_entry& entry) const;
	AssetHandle FindAssetHandle(const std::filesystem::path& relativePath) const;
	std::filesystem::path MakeRelativePath(const std::filesystem::path& absolutePath) const;
	std::filesystem::path MakeUniqueCopyPath(const std::filesystem::path& absolutePath) const;
	std::filesystem::path MakeUniqueImportPath(const std::filesystem::path& absolutePath) const;
	void SetStatus(std::string message, bool error = false);
	std::string DisplayPath(const std::filesystem::path& path) const;
	std::string ItemTypeLabel(const BrowserItem& item) const;
	std::string AssetTypeFilterLabel() const;

	Ref<Project> m_Project;
	Ref<ThumbnailCache> m_ThumbnailCache;

	// directories
	std::filesystem::path m_BaseDirectory;
	std::filesystem::path m_CurrentDirectory;

	// style
	float m_ThumbnailSize = 64.0f;
	float m_Padding = 16.0f;

	// popup
	bool m_ShowSettingsPopup = false;
	FileOperation m_PendingOperation = FileOperation::None;
	AssetHandle m_PendingOperationHandle = 0;
	std::filesystem::path m_PendingOperationPath;
	bool m_PendingOperationIsDirectory = false;
	std::string m_OperationText;
	std::string m_OperationError;
	bool m_ShowAutoSlicePopup = false;
	AssetHandle m_AutoSliceHandle = 0;
	std::filesystem::path m_AutoSliceRelativePath;
	int m_AutoSliceMinPixels = 24;
	int m_AutoSliceBackgroundTolerance = 24;
	int m_AutoSliceMergeGap = 0;
	int m_AutoSlicePadding = 1;
	int m_AutoSliceExtrudePixels = 0;
	bool m_AutoSliceSeparateDiagonalTouches = true;
	bool m_AutoSliceExportPngs = false;
	bool m_AutoSliceReplaceExisting = true;
	std::function<bool(AssetHandle)> m_AssetOpenCallback;
	std::function<bool(AssetHandle)> m_AssetInspectCallback;

	std::string m_SearchQuery;
	std::string m_CachedSearchQueryLower;
	std::string m_StatusMessage;
	bool m_StatusError = false;
	AssetType m_TypeFilter = AssetType::None;
	Mode m_Mode = Mode::Asset;
	bool m_ShowUnsupported = false;
	bool m_Initialized = false;
	bool m_PreferencesDirty = false;
	bool m_Open = true;
	bool m_Hovered = false;
	bool m_ItemsDirty = true;
	bool m_DirectoryTreeDirty = true;
	std::vector<BrowserItem> m_CachedItems;
	ItemMetrics m_CachedItemMetrics;
	DirectoryNode m_DirectoryTree;
	std::set<std::string> m_SelectedItemIds;
	std::set<std::string> m_CollapsedTextureSpriteParents;
};

_WHIP_END
