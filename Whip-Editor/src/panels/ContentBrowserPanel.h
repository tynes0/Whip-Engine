#pragma once

#include "ThumbnailCache.h"

#include <Whip/Core/Core.h>
#include <Whip/Asset/Asset.h>
#include <Whip/Render/Texture.h>

#include <filesystem>
#include <functional>
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
		AssetHandle m_Handle = 0;
		AssetType m_Type = AssetType::None;
		bool m_Directory = false;
		bool m_Imported = false;
		bool m_Supported = false;
		bool m_Missing = false;
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
	void DrawDirectoryTree(const std::filesystem::path& directory);
	void DrawBreadcrumbs();
	void DrawContentGrid(const std::vector<BrowserItem>& items);
	void DrawItem(const BrowserItem& item);
	void DrawFileOperationModals();
	void DrawTypeFilter();

	std::vector<BrowserItem> CollectItems() const;
	std::vector<BrowserItem> CollectFilesystemItems() const;
	std::vector<BrowserItem> CollectAssetItems() const;

	void SetCurrentDirectory(const std::filesystem::path& directory);
	bool ImportFile(const std::filesystem::path& relativePath, ImportSummary* summary = nullptr);
	void ImportCurrentDirectory(bool recursive);
	void RequestRenameItem(const BrowserItem& item);
	void RequestMoveItem(const BrowserItem& item);
	void RequestDeleteItem(const BrowserItem& item);
	void RequestRemoveAsset(AssetHandle handle, const std::filesystem::path& relativePath);
	bool OpenAsset(const BrowserItem& item);
	bool InspectAsset(const BrowserItem& item);
	bool SetSceneAsStartScene(const BrowserItem& item);
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
	std::function<bool(AssetHandle)> m_AssetOpenCallback;
	std::function<bool(AssetHandle)> m_AssetInspectCallback;

	std::string m_SearchQuery;
	std::string m_StatusMessage;
	bool m_StatusError = false;
	AssetType m_TypeFilter = AssetType::None;
	Mode m_Mode = Mode::Asset;
	bool m_ShowUnsupported = false;
	bool m_Initialized = false;
	bool m_PreferencesDirty = false;
	bool m_Open = true;
	bool m_Hovered = false;
};

_WHIP_END
