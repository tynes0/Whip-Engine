#include <WhipPch.h>

#include <Whip-Editor/Panels/ProjectHealthPanel.h>

#include <Whip-Editor/Managers/EditorShortcutManager.h>

#include <Whip/Animation/Animation2D.h>
#include <Whip/Animation/AnimationController.h>
#include <Whip/Asset/AssetManager.h>
#include <Whip/Asset/AssetUtils.h>
#include <Whip/Asset/EditorAssetManager.h>
#include <Whip/Project/Project.h>
#include <Whip/Scene/Entity.h>
#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Utils/FileExtensions.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <utility>

_WHIP_START

namespace
{
	std::string AssetHandleLabel(AssetHandle handle)
	{
		return handle == 0 ? std::string{} : std::to_string(static_cast<uint64_t>(handle));
	}

	std::string EntityLabel(Entity entity)
	{
		if (!entity)
			return {};

		return entity.GetName() + " (" + std::to_string(static_cast<uint64_t>(entity.GetUUID())) + ")";
	}

	std::string LowerCopy(std::string value)
	{
		std::ranges::transform(value, value.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
	}

	bool ContainsCaseInsensitive(const std::string& haystack, const char* needle)
	{
		if (!needle || needle[0] == '\0')
			return true;

		return LowerCopy(haystack).find(LowerCopy(needle)) != std::string::npos;
	}

	bool IsFinite(const glm::vec3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	bool HasSuspiciousScale(const glm::vec3& scale)
	{
		constexpr float MinReasonableScale = 0.0001f;
		constexpr float MaxReasonableScale = 10000.0f;
		return std::abs(scale.x) < MinReasonableScale ||
			std::abs(scale.y) < MinReasonableScale ||
			std::abs(scale.z) < MinReasonableScale ||
			std::abs(scale.x) > MaxReasonableScale ||
			std::abs(scale.y) > MaxReasonableScale ||
			std::abs(scale.z) > MaxReasonableScale;
	}

	bool HasParameter(const AnimationController& controller, const std::string& name)
	{
		return std::ranges::any_of(controller.GetParameters(), [&name](const AnimationControllerParameter& parameter)
		{
			return parameter.m_Name == name;
		});
	}

	bool HasState(const AnimationController& controller, const std::string& name)
	{
		return name == AnimationController::ExitStateName || controller.FindState(name) != nullptr;
	}
}

ProjectHealthPanel::ProjectHealthPanel()
	: EditorPanel("Project Health", false, true)
{
}

void ProjectHealthPanel::OnImGuiRender()
{
	WHP_PROFILE_FUNCTION();
	if (!m_Open)
		return;

	if (m_AutoScan && m_ScanDirty)
		Scan();

	bool open = m_Open;
	ImGui::SetNextWindowSize(ImVec2(760.0f, 460.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Project Health", &open))
	{
		SetOpen(open);
		ImGui::End();
		return;
	}
	SetOpen(open);

	DrawToolbar();
	DrawSummary();
	DrawIssueTable();

	ImGui::End();
}

void ProjectHealthPanel::RegisterShortcuts(EditorShortcutManager& shortcutManager)
{
	EditorShortcutOptions options;
	options.m_AllowWhenActiveWidget = true;
	shortcutManager.Add(
		EditorShortcutScope::Global,
		"window.open_project_health",
		"Open Project Health",
		"Window",
		{ Key::H, true, true, false },
		[this]()
		{
			SetOpen(true);
			MarkDirty();
			return true;
		},
		[]() { return Project::GetActive() && Project::Loaded(); },
		{},
		options);

	shortcutManager.Add(
		EditorShortcutScope::Global,
		"project_health.rescan",
		"Rescan Project Health",
		"Validation",
		{ Key::F5, false, true, false },
		[this]()
		{
			Scan();
			return true;
		},
		[this]() { return m_Open && Project::GetActive() && Project::Loaded(); },
		[this]() { return m_Open; },
		options);
}

void ProjectHealthPanel::Scan()
{
	WHP_PROFILE_FUNCTION();
	m_Issues.clear();
	m_ScanDirty = false;

	if (!Project::GetActive() || !Project::Loaded())
	{
		AddIssue(IssueSeverity::Info, "Project", "No project is loaded.");
		return;
	}

	ValidateProjectConfig();
	ValidateAssetRegistry();
	ValidateScene();
}

void ProjectHealthPanel::DrawToolbar()
{
	WHP_PROFILE_FUNCTION();
	if (ImGui::Button("Rescan"))
		Scan();
	ImGui::SameLine();
	if (ImGui::Checkbox("Auto", &m_AutoScan))
		m_ScanDirty = true;
	ImGui::SameLine();
	ImGui::SetNextItemWidth(140.0f);
	const char* severityPreview = m_SeverityFilter == 0 ? "All Severities" : (m_SeverityFilter == 1 ? "Errors" : (m_SeverityFilter == 2 ? "Warnings" : "Info"));
	if (ImGui::BeginCombo("##HealthSeverity", severityPreview))
	{
		if (ImGui::Selectable("All Severities", m_SeverityFilter == 0))
			m_SeverityFilter = 0;
		if (ImGui::Selectable("Errors", m_SeverityFilter == 1))
			m_SeverityFilter = 1;
		if (ImGui::Selectable("Warnings", m_SeverityFilter == 2))
			m_SeverityFilter = 2;
		if (ImGui::Selectable("Info", m_SeverityFilter == 3))
			m_SeverityFilter = 3;
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##HealthFilter", "Filter issue, category, entity, asset...", m_Filter, sizeof(m_Filter));
}

void ProjectHealthPanel::DrawSummary() const
{
	WHP_PROFILE_FUNCTION();
	int errors = 0;
	int warnings = 0;
	int info = 0;
	for (const HealthIssue& issue : m_Issues)
	{
		switch (issue.m_Severity)
		{
		case IssueSeverity::Error: ++errors; break;
		case IssueSeverity::Warning: ++warnings; break;
		case IssueSeverity::Info: ++info; break;
		default: break;
		}
	}

	ImGui::Spacing();
	ImGui::TextColored(SeverityColor(IssueSeverity::Error), "Errors: %d", errors);
	ImGui::SameLine();
	ImGui::TextColored(SeverityColor(IssueSeverity::Warning), "Warnings: %d", warnings);
	ImGui::SameLine();
	ImGui::TextColored(SeverityColor(IssueSeverity::Info), "Info: %d", info);
	ImGui::SameLine();
	ImGui::TextDisabled("| %zu total", m_Issues.size());
	ImGui::Separator();
}

void ProjectHealthPanel::DrawIssueTable()
{
	WHP_PROFILE_FUNCTION();
	if (m_Issues.empty())
	{
		ImGui::TextColored(ImVec4(0.58f, 0.78f, 0.55f, 1.0f), "Project looks healthy.");
		return;
	}

	if (!ImGui::BeginTable("##ProjectHealthIssues", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 0.0f)))
		return;

	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 86.0f);
	ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 118.0f);
	ImGui::TableSetupColumn("Issue", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthFixed, 220.0f);
	ImGui::TableSetupColumn("Reference", ImGuiTableColumnFlags_WidthFixed, 150.0f);
	ImGui::TableHeadersRow();

	for (const HealthIssue& issue : m_Issues)
	{
		if (!IsIssueVisible(issue))
			continue;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextColored(SeverityColor(issue.m_Severity), "%s", SeverityLabel(issue.m_Severity));
		ImGui::TableNextColumn();
		ImGui::TextDisabled("%s", issue.m_Category.c_str());
		ImGui::TableNextColumn();
		ImGui::PushID(&issue);
		const bool canSelectEntity = issue.m_EntityId != 0 && m_SelectEntityCallback;
		if (canSelectEntity)
		{
			if (ImGui::Selectable(issue.m_Title.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
				m_SelectEntityCallback(issue.m_EntityId);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Select entity");
		}
		else
		{
			ImGui::TextWrapped("%s", issue.m_Title.c_str());
		}
		if (!issue.m_Detail.empty())
			ImGui::TextDisabled("%s", issue.m_Detail.c_str());
		ImGui::PopID();
		ImGui::TableNextColumn();
		ImGui::TextWrapped("%s", issue.m_Location.c_str());
		ImGui::TableNextColumn();
		if (issue.m_AssetHandle != 0)
			ImGui::TextDisabled("%s", AssetHandleLabel(issue.m_AssetHandle).c_str());
		else if (issue.m_EntityId != 0)
			ImGui::TextDisabled("%s", std::to_string(static_cast<uint64_t>(issue.m_EntityId)).c_str());
		else
			ImGui::TextDisabled("-");
	}

	ImGui::EndTable();
}

void ProjectHealthPanel::AddIssue(IssueSeverity severity, std::string category, std::string title, std::string detail, std::string location, UUID entityId, AssetHandle assetHandle)
{
	m_Issues.push_back({
		severity,
		std::move(category),
		std::move(title),
		std::move(detail),
		std::move(location),
		entityId,
		assetHandle
	});
}

bool ProjectHealthPanel::IsIssueVisible(const HealthIssue& issue) const
{
	if (m_SeverityFilter == 1 && issue.m_Severity != IssueSeverity::Error)
		return false;
	if (m_SeverityFilter == 2 && issue.m_Severity != IssueSeverity::Warning)
		return false;
	if (m_SeverityFilter == 3 && issue.m_Severity != IssueSeverity::Info)
		return false;

	if (m_Filter[0] == '\0')
		return true;

	const std::string haystack = issue.m_Category + " " + issue.m_Title + " " + issue.m_Detail + " " + issue.m_Location + " " +
		AssetHandleLabel(issue.m_AssetHandle) + " " + std::to_string(static_cast<uint64_t>(issue.m_EntityId));
	return ContainsCaseInsensitive(haystack, m_Filter);
}

bool ProjectHealthPanel::ValidateAssetReference(AssetHandle handle, AssetType expectedType, const std::string& owner, const std::string& field, UUID entityId)
{
	if (handle == 0)
		return true;

	const Ref<Project> project = Project::GetActive();
	if (!project || !project->GetEditorAssetManager())
		return false;

	const Ref<EditorAssetManager> assetManager = project->GetEditorAssetManager();
	if (!assetManager->IsAssetHandleValid(handle))
	{
		AddIssue(IssueSeverity::Error, "Reference", owner + " has missing " + field, "Asset handle is not registered.", owner, entityId, handle);
		return false;
	}

	const AssetType actualType = assetManager->GetAssetType(handle);
	if (actualType != expectedType)
	{
		AddIssue(IssueSeverity::Error, "Reference", owner + " has wrong asset type in " + field,
			"Expected " + std::string(frenum::to_string(expectedType)) + ", got " + std::string(frenum::to_string(actualType)) + ".",
			owner, entityId, handle);
		return false;
	}

	const AssetMetadata& metadata = assetManager->GetMetadata(handle);
	std::error_code error;
	if (!std::filesystem::exists(Project::GetActiveAssetDirectory() / metadata.m_Filepath, error))
	{
		AddIssue(IssueSeverity::Error, "Reference", owner + " references deleted " + field, metadata.m_Filepath.generic_string(), owner, entityId, handle);
		return false;
	}

	return true;
}

void ProjectHealthPanel::ValidateProjectConfig()
{
	WHP_PROFILE_FUNCTION();
	const Ref<Project> project = Project::GetActive();
	if (!project)
		return;

	const ProjectConfig& config = project->GetConfig();
	std::error_code error;
	if (config.m_Name.empty())
		AddIssue(IssueSeverity::Warning, "Project", "Project name is empty.", {}, project->GetProjectPath().string());

	if (config.m_AssetDirectory.empty())
	{
		AddIssue(IssueSeverity::Error, "Project", "Project asset directory is not configured.", {}, project->GetProjectPath().string());
	}
	else if (!std::filesystem::exists(project->GetAssetDirectory(), error))
	{
		AddIssue(IssueSeverity::Error, "Project", "Project asset directory is missing.", project->GetAssetDirectory().string(), project->GetProjectPath().string());
	}

	error.clear();
	if (config.m_AssetRegistryPath.empty())
	{
		AddIssue(IssueSeverity::Error, "Project", "Asset registry path is not configured.", {}, project->GetProjectPath().string());
	}
	else if (!std::filesystem::exists(project->GetAssetRegistryPath(), error))
	{
		AddIssue(IssueSeverity::Error, "Project", "Asset registry file is missing.", project->GetAssetRegistryPath().string(), project->GetProjectPath().string());
	}

	if (config.m_ScriptModulePath.empty())
	{
		AddIssue(IssueSeverity::Warning, "Scripts", "Script module path is not configured.", {}, project->GetProjectPath().string());
	}
	else
	{
		error.clear();
		const std::filesystem::path scriptsDirectory = project->GetAssetDirectory() / "Scripts";
		if (!std::filesystem::exists(scriptsDirectory, error))
			AddIssue(IssueSeverity::Warning, "Scripts", "Script workspace directory is missing.", scriptsDirectory.string(), project->GetProjectPath().string());

		error.clear();
		const std::filesystem::path sourceDirectory = scriptsDirectory / "Source";
		if (!std::filesystem::exists(sourceDirectory, error))
			AddIssue(IssueSeverity::Warning, "Scripts", "Script source directory is missing.", sourceDirectory.string(), project->GetProjectPath().string());
	}

	if (!project->GetEditorAssetManager())
		return;

	const auto& scenes = project->GetEditorAssetManager()->GetAssetRegistry().GetFiltered(AssetType::Scene);
	if (scenes.empty())
		AddIssue(IssueSeverity::Warning, "Project", "Project has no registered scenes.", "Create or import a scene and set it as the start scene.", project->GetProjectPath().string());

	if (config.m_StartScene == 0)
	{
		AddIssue(IssueSeverity::Warning, "Project", "Project has no start scene.", "Build/export should point to a registered scene.", project->GetProjectPath().string());
		return;
	}

	if (!project->GetEditorAssetManager()->IsAssetHandleValid(config.m_StartScene) ||
		project->GetEditorAssetManager()->GetAssetType(config.m_StartScene) != AssetType::Scene)
	{
		AddIssue(IssueSeverity::Error, "Project", "Project start scene handle is invalid.", {}, project->GetProjectPath().string(), 0, config.m_StartScene);
		return;
	}

	const AssetMetadata& startSceneMetadata = project->GetEditorAssetManager()->GetMetadata(config.m_StartScene);
	error.clear();
	if (!std::filesystem::exists(project->GetAssetDirectory() / startSceneMetadata.m_Filepath, error))
		AddIssue(IssueSeverity::Error, "Project", "Project start scene file is missing.", startSceneMetadata.m_Filepath.generic_string(), project->GetProjectPath().string(), 0, config.m_StartScene);
}

void ProjectHealthPanel::ValidateAssetRegistry()
{
	WHP_PROFILE_FUNCTION();
	const Ref<Project> project = Project::GetActive();
	if (!project || !project->GetEditorAssetManager())
		return;

	const Ref<EditorAssetManager> assetManager = project->GetEditorAssetManager();
	std::unordered_map<std::string, AssetHandle> seenPaths;
	assetManager->GetAssetRegistry().Foreach([&](const AssetRegistry::ValueType& value)
	{
		const AssetHandle handle = value.first;
		const AssetMetadata& metadata = value.second;
		const std::filesystem::path absolutePath = Project::GetActiveAssetDirectory() / metadata.m_Filepath;
		const std::string relativePath = metadata.m_Filepath.generic_string();

		if (relativePath.empty())
		{
			AddIssue(IssueSeverity::Error, "Assets", "Asset has an empty filepath.", "Registry entry cannot be resolved.", {}, 0, handle);
			return;
		}

		const std::string normalizedPath = LowerCopy(metadata.m_Filepath.lexically_normal().generic_string());
		if (auto [it, inserted] = seenPaths.emplace(normalizedPath, handle); !inserted)
		{
			AddIssue(IssueSeverity::Warning, "Assets", "Duplicate asset filepath registration.",
				"Also registered by handle " + AssetHandleLabel(it->second) + ".", relativePath, 0, handle);
		}

		std::error_code error;
		if (!std::filesystem::exists(absolutePath, error))
		{
			AddIssue(IssueSeverity::Error, "Assets", "Registered asset file is missing.", relativePath, relativePath, 0, handle);
			return;
		}

		const AssetType extensionType = Utils::TryGetAssetTypeFromFileExtension(metadata.m_Filepath.extension());
		if (extensionType != AssetType::None && extensionType != metadata.m_Type)
		{
			AddIssue(IssueSeverity::Warning, "Assets", "Asset type does not match file extension.",
				"Registry says " + std::string(frenum::to_string(metadata.m_Type)) + ", extension maps to " + std::string(frenum::to_string(extensionType)) + ".",
				relativePath, 0, handle);
		}

		if (metadata.m_Type == AssetType::Texture2D)
		{
			const auto& sprites = metadata.m_TextureSettings.m_Sprites;
			for (size_t spriteIndex = 0; spriteIndex < sprites.size(); ++spriteIndex)
			{
				const TextureSpriteRect& sprite = sprites[spriteIndex];
				if (sprite.m_Width == 0 || sprite.m_Height == 0)
				{
					AddIssue(IssueSeverity::Error, "Texture", "Texture sprite has zero size.",
						sprite.m_Name, relativePath, 0, handle);
				}
			}
		}
		else if (metadata.m_Type == AssetType::Animation)
		{
			ValidateAnimationAsset(handle, absolutePath);
		}
		else if (metadata.m_Type == AssetType::AnimationController)
		{
			ValidateAnimationControllerAsset(handle, absolutePath);
		}
	});
}

void ProjectHealthPanel::ValidateScene()
{
	WHP_PROFILE_FUNCTION();
	const Ref<Scene> scene = m_SceneCallback ? m_SceneCallback() : nullptr;
	if (!scene)
	{
		AddIssue(IssueSeverity::Warning, "Scene", "No active scene is available.");
		return;
	}

	std::unordered_set<uint64_t> ids;
	auto idView = scene->GetAllEntitiesWith<IDComponent, TagComponent, TransformComponent>();
	for (auto entityHandle : idView)
	{
		Entity entity(entityHandle, scene.get());
		const IDComponent& id = entity.GetComponent<IDComponent>();
		const TagComponent& tag = entity.GetComponent<TagComponent>();
		const TransformComponent& transform = entity.GetComponent<TransformComponent>();
		const std::string owner = EntityLabel(entity);

		if (id.m_ID == 0)
			AddIssue(IssueSeverity::Error, "Scene", "Entity has an invalid UUID.", tag.m_Tag, owner, id.m_ID);
		if (!ids.insert(static_cast<uint64_t>(id.m_ID)).second)
			AddIssue(IssueSeverity::Error, "Scene", "Duplicate entity UUID.", tag.m_Tag, owner, id.m_ID);
		if (tag.m_Tag.empty())
			AddIssue(IssueSeverity::Warning, "Scene", "Entity has an empty name.", {}, owner, id.m_ID);
		if (!IsFinite(transform.m_Translation) || !IsFinite(transform.m_Rotation) || !IsFinite(transform.m_Scale))
			AddIssue(IssueSeverity::Error, "Transform", "Entity transform contains non-finite values.", {}, owner, id.m_ID);
		if (HasSuspiciousScale(transform.m_Scale))
			AddIssue(IssueSeverity::Warning, "Transform", "Entity has suspicious transform scale.", "Very small, zero, or huge scale can cause selection, physics, and rendering issues.", owner, id.m_ID);

		if (entity.HasComponent<HierarchyComponent>())
		{
			const HierarchyComponent& hierarchy = entity.GetComponent<HierarchyComponent>();
			if (hierarchy.m_Parent != 0 && !scene->FindEntityByUUID(hierarchy.m_Parent))
				AddIssue(IssueSeverity::Error, "Hierarchy", "Entity has a missing parent reference.", {}, owner, id.m_ID);
			for (UUID childId : hierarchy.m_Children)
			{
				if (childId == 0 || !scene->FindEntityByUUID(childId))
					AddIssue(IssueSeverity::Error, "Hierarchy", "Entity has a missing child reference.", std::to_string(static_cast<uint64_t>(childId)), owner, id.m_ID);
			}
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			const ScriptComponent& script = entity.GetComponent<ScriptComponent>();
			if (script.m_ClassName.empty())
				AddIssue(IssueSeverity::Warning, "Script", "Script component has no class selected.", {}, owner, id.m_ID);
			else if (!ScriptEngine::EntityClassExists(script.m_ClassName))
				AddIssue(IssueSeverity::Error, "Script", "Script class was not found.", script.m_ClassName, owner, id.m_ID);
		}

		if (entity.HasComponent<SpriteRendererComponent>())
		{
			const SpriteRendererComponent& sprite = entity.GetComponent<SpriteRendererComponent>();
			if (ValidateAssetReference(sprite.m_Texture, AssetType::Texture2D, owner, "Texture", id.m_ID) && sprite.m_TextureSpriteIndex >= 0)
			{
				const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(sprite.m_Texture);
				if (!std::cmp_less(sprite.m_TextureSpriteIndex, metadata.m_TextureSettings.m_Sprites.size()))
					AddIssue(IssueSeverity::Error, "Sprite", "Sprite Renderer has an invalid sprite index.",
						"Index " + std::to_string(sprite.m_TextureSpriteIndex) + " is outside the texture slice list.", owner, id.m_ID, sprite.m_Texture);
			}
		}

		if (entity.HasComponent<TextComponent>())
		{
			const TextComponent& text = entity.GetComponent<TextComponent>();
			ValidateAssetReference(text.m_Font, AssetType::Font, owner, "Font", id.m_ID);
		}

		if (entity.HasComponent<AnimatorComponent>())
		{
			const AnimatorComponent& animator = entity.GetComponent<AnimatorComponent>();
			if (ValidateAssetReference(animator.m_Controller, AssetType::AnimationController, owner, "Animation Controller", id.m_ID))
			{
				Ref<AnimationController> controller = AssetManager::GetAsset<AnimationController>(animator.m_Controller);
				if (!controller)
					AddIssue(IssueSeverity::Error, "Animation", "Animator controller could not be loaded.", {}, owner, id.m_ID, animator.m_Controller);
				else if (!animator.m_InitialState.empty() && !controller->FindState(animator.m_InitialState))
					AddIssue(IssueSeverity::Error, "Animation", "Animator initial state does not exist.", animator.m_InitialState, owner, id.m_ID, animator.m_Controller);
			}
		}

		if (entity.HasComponent<Rigidbody2DComponent>() && !entity.HasComponent<BoxCollider2DComponent>() && !entity.HasComponent<CircleCollider2DComponent>())
			AddIssue(IssueSeverity::Info, "Physics", "Rigidbody2D has no collider.", "The body can move, but it will not collide until a collider is added.", owner, id.m_ID);

		if ((entity.HasComponent<BoxCollider2DComponent>() || entity.HasComponent<CircleCollider2DComponent>()) && !entity.HasComponent<Rigidbody2DComponent>())
			AddIssue(IssueSeverity::Warning, "Physics", "Collider exists without Rigidbody2D.", "Physics runtime normally expects a Rigidbody2D on collider entities.", owner, id.m_ID);

		if (entity.HasComponent<AudioComponent>())
		{
			const AudioComponent& audio = entity.GetComponent<AudioComponent>();
			std::unordered_set<std::string> audioTags;
			for (const AudioComponent::AudioData& data : audio.m_AudioDatas)
			{
				ValidateAssetReference(data.m_Audio, AssetType::Audio, owner, "Audio", id.m_ID);
				if (!audioTags.insert(data.m_Tag).second)
					AddIssue(IssueSeverity::Warning, "Audio", "Audio component has duplicate clip tag.", data.m_Tag, owner, id.m_ID, data.m_Audio);
				if (data.m_ClipEnd > 0.0f && data.m_ClipStart > data.m_ClipEnd)
					AddIssue(IssueSeverity::Error, "Audio", "Audio clip range is inverted.", data.m_Tag, owner, id.m_ID, data.m_Audio);
			}
		}
	}

	if (!scene->GetPrimaryCameraEntity())
		AddIssue(IssueSeverity::Warning, "Scene", "Scene has no primary camera.");
}

void ProjectHealthPanel::ValidateAnimationAsset(AssetHandle handle, const std::filesystem::path& absolutePath)
{
	WHP_PROFILE_FUNCTION();
	if (!std::filesystem::exists(absolutePath))
		return;

	Ref<Animation2D> animation = AssetManager::GetAsset<Animation2D>(handle);
	if (!animation)
	{
		AddIssue(IssueSeverity::Error, "Animation", "Animation could not be loaded.", absolutePath.filename().string(), absolutePath.generic_string(), 0, handle);
		return;
	}

	const std::string owner = animation->GetName().empty() ? absolutePath.filename().string() : animation->GetName();
	if (animation->GetFrames().empty() && animation->GetEvents().empty())
		AddIssue(IssueSeverity::Warning, "Animation", "Animation has no frames or events.", owner, absolutePath.generic_string(), 0, handle);

	for (size_t i = 0; i < animation->GetFrames().size(); ++i)
	{
		const AnimationFrame& frame = animation->GetFrames()[i];
		const std::string frameLabel = owner + " frame " + std::to_string(i);
		if (frame.m_Duration <= 0.0f)
			AddIssue(IssueSeverity::Warning, "Animation", "Animation frame has non-positive duration.", frameLabel, absolutePath.generic_string(), 0, handle);
		if (ValidateAssetReference(frame.m_Texture, AssetType::Texture2D, frameLabel, "Texture", 0) && frame.m_TextureSpriteIndex >= 0)
		{
			const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(frame.m_Texture);
			if (!std::cmp_less(frame.m_TextureSpriteIndex, metadata.m_TextureSettings.m_Sprites.size()))
				AddIssue(IssueSeverity::Error, "Animation", "Animation frame has invalid sprite index.", frameLabel, absolutePath.generic_string(), 0, frame.m_Texture);
		}
	}
}

void ProjectHealthPanel::ValidateAnimationControllerAsset(AssetHandle handle, const std::filesystem::path& absolutePath)
{
	WHP_PROFILE_FUNCTION();
	if (!std::filesystem::exists(absolutePath))
		return;

	Ref<AnimationController> controller = AssetManager::GetAsset<AnimationController>(handle);
	if (!controller)
	{
		AddIssue(IssueSeverity::Error, "Animation", "Animation controller could not be loaded.", absolutePath.filename().string(), absolutePath.generic_string(), 0, handle);
		return;
	}

	const std::string owner = absolutePath.filename().string();
	if (controller->GetStates().empty())
		AddIssue(IssueSeverity::Warning, "Animation", "Animation controller has no states.", owner, absolutePath.generic_string(), 0, handle);
	if (controller->GetDefaultState().empty())
		AddIssue(IssueSeverity::Warning, "Animation", "Animation controller has no default state.", owner, absolutePath.generic_string(), 0, handle);
	else if (!controller->FindState(controller->GetDefaultState()))
		AddIssue(IssueSeverity::Error, "Animation", "Animation controller default state is missing.", controller->GetDefaultState(), absolutePath.generic_string(), 0, handle);

	std::unordered_set<std::string> stateNames;
	for (const AnimationControllerState& state : controller->GetStates())
	{
		if (state.m_Name.empty())
			AddIssue(IssueSeverity::Error, "Animation", "Animation controller contains an unnamed state.", owner, absolutePath.generic_string(), 0, handle);
		else if (!stateNames.insert(state.m_Name).second)
			AddIssue(IssueSeverity::Error, "Animation", "Animation controller has duplicate state.", state.m_Name, absolutePath.generic_string(), 0, handle);

		if (state.m_MotionType == AnimationMotionType::Clip)
			ValidateAssetReference(state.m_Clip, AssetType::Animation, owner + " / " + state.m_Name, "Clip", 0);
		else if (state.m_MotionType == AnimationMotionType::BlendTree1D)
		{
			if (state.m_BlendParameter.empty() || !HasParameter(*controller, state.m_BlendParameter))
				AddIssue(IssueSeverity::Error, "Animation", "Blend tree references a missing parameter.", state.m_BlendParameter, absolutePath.generic_string(), 0, handle);
			if (state.m_BlendChildren.empty())
				AddIssue(IssueSeverity::Warning, "Animation", "Blend tree state has no children.", state.m_Name, absolutePath.generic_string(), 0, handle);
			for (const AnimationBlendChild& child : state.m_BlendChildren)
				ValidateAssetReference(child.m_Clip, AssetType::Animation, owner + " / " + state.m_Name, "Blend Clip", 0);
		}

		for (const AnimationControllerTransition& transition : state.m_Transitions)
		{
			if (!HasState(*controller, transition.m_TargetState))
				AddIssue(IssueSeverity::Error, "Animation", "Transition targets a missing state.", state.m_Name + " -> " + transition.m_TargetState, absolutePath.generic_string(), 0, handle);
			for (const AnimationControllerCondition& condition : transition.m_Conditions)
			{
				if (!HasParameter(*controller, condition.m_Parameter))
					AddIssue(IssueSeverity::Error, "Animation", "Transition condition references a missing parameter.", condition.m_Parameter, absolutePath.generic_string(), 0, handle);
			}
			for (const AnimationControllerBlueprintNode& node : transition.m_BlueprintNodes)
			{
				if (node.m_Type == AnimationBlueprintNodeType::Parameter && !HasParameter(*controller, node.m_Parameter))
					AddIssue(IssueSeverity::Error, "Animation", "Transition blueprint parameter node is missing its parameter.", node.m_Parameter, absolutePath.generic_string(), 0, handle);
			}
		}
	}

	for (const AnimationControllerTransition& transition : controller->GetAnyStateTransitions())
	{
		if (!HasState(*controller, transition.m_TargetState))
			AddIssue(IssueSeverity::Error, "Animation", "Any State transition targets a missing state.", transition.m_TargetState, absolutePath.generic_string(), 0, handle);
	}
}

const char* ProjectHealthPanel::SeverityLabel(IssueSeverity severity)
{
	switch (severity)
	{
	case IssueSeverity::Error: return "Error";
	case IssueSeverity::Warning: return "Warning";
	case IssueSeverity::Info: return "Info";
	default: return "Info";
	}
}

ImVec4 ProjectHealthPanel::SeverityColor(IssueSeverity severity)
{
	switch (severity)
	{
	case IssueSeverity::Error: return ImVec4(0.93f, 0.38f, 0.34f, 1.0f);
	case IssueSeverity::Warning: return ImVec4(0.95f, 0.70f, 0.32f, 1.0f);
	case IssueSeverity::Info: return ImVec4(0.52f, 0.70f, 0.92f, 1.0f);
	default: return ImGui::GetStyleColorVec4(ImGuiCol_Text);
	}
}

_WHIP_END
