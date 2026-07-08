#pragma once

#include <Whip-Editor/Panels/EditorPanel.h>

#include <Whip/Core/Memory.h>
#include <Whip/Core/UUID.h>
#include <Whip/Asset/Asset.h>
#include <Whip/Scene/Scene.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <imgui.h>

_WHIP_START

class ProjectHealthPanel final : public EditorPanel
{
public:
	using SceneCallback = std::function<Ref<Scene>()>;
	using SelectEntityCallback = std::function<void(UUID)>;

	struct ExportScanSummary
	{
		int m_Errors = 0;
		int m_Warnings = 0;
		int m_Info = 0;
		std::vector<std::string> m_ErrorMessages;
		std::vector<std::string> m_WarningMessages;
	};

	ProjectHealthPanel();

	void OnImGuiRender() override;
	void RegisterShortcuts(EditorShortcutManager& shortcutManager) override;

	void SetSceneCallback(SceneCallback callback) { m_SceneCallback = std::move(callback); }
	void SetSelectEntityCallback(SelectEntityCallback callback) { m_SelectEntityCallback = std::move(callback); }
	void MarkDirty() { m_ScanDirty = true; }
	ExportScanSummary ScanForExport();

private:
	enum class IssueSeverity : uint8_t
	{
		Info = 0,
		Warning,
		Error
	};

	struct HealthIssue
	{
		IssueSeverity m_Severity = IssueSeverity::Info;
		std::string m_Category;
		std::string m_Title;
		std::string m_Detail;
		std::string m_Location;
		UUID m_EntityId = 0;
		AssetHandle m_AssetHandle = 0;
	};

	void Scan();
	void DrawToolbar();
	void DrawSummary() const;
	void DrawIssueTable();
	void AddIssue(IssueSeverity severity, std::string category, std::string title, std::string detail = {}, std::string location = {}, UUID entityId = 0, AssetHandle assetHandle = 0);

	bool IsIssueVisible(const HealthIssue& issue) const;
	bool ValidateAssetReference(AssetHandle handle, AssetType expectedType, const std::string& owner, const std::string& field, UUID entityId = 0);
	void ValidateProjectConfig();
	void ValidateAssetRegistry();
	void ValidateScene();
	void ValidateAnimationAsset(AssetHandle handle, const std::filesystem::path& absolutePath);
	void ValidateAnimationControllerAsset(AssetHandle handle, const std::filesystem::path& absolutePath);

	static const char* SeverityLabel(IssueSeverity severity);
	static ImVec4 SeverityColor(IssueSeverity severity);

	SceneCallback m_SceneCallback;
	SelectEntityCallback m_SelectEntityCallback;
	std::vector<HealthIssue> m_Issues;
	char m_Filter[128]{ 0 };
	int m_SeverityFilter = 0;
	bool m_AutoScan = true;
	bool m_ScanDirty = true;
};

_WHIP_END
