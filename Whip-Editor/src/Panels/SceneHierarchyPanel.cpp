#include <Whip-Editor/Panels/SceneHierarchyPanel.h>

#include <Whip-Editor/Managers/EditorShortcutManager.h>
#include <Whip-Editor/UI/UIHelpers.h>
#include <Whip-Editor/UI/UIScopedStyle.h>
#include <Whip-Editor/Helpers/ScriptFieldHelper.h>

#include <Whip/Core/Input.h>
#include <Whip/Core/KeyCodes.h>
#include <Whip/Utils/PlatformUtils.h>
#include <Whip/Scene/Components.h>
#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Project/Project.h>
#include <Whip/Asset/AssetManager.h>
#include <Whip/Asset/AssetMetadata.h>
#include <Whip/Animation/AnimationController.h>
#include <Whip/Audio/AudioSource.h>
#include <Whip/Math/Math.h>
#include <Whip/Debug/Instrumentor.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <cctype>
#include <cstring>
#include <array>
#include <cstdlib>
#include <type_traits>
#include <utility>
#include <vector>

#define BEGIN_COMPONENT_TABLE_ROW(...) do { ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text(__VA_ARGS__); ImGui::TableNextColumn(); ImGui::PushItemWidth(-1); } while(false)
#define END_COMPONENT_TABLE_ROW() do { ImGui::PopItemWidth(); } while(false)

_WHIP_START

namespace
{
	constexpr const char* SceneEntityPayloadType = "WHIP_SCENE_ENTITY";

	void DrawPropertySectionTitle(const char* title)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", title);
		ImGui::Separator();
		ImGui::Spacing();
	}

	void DrawMixedHint(const char* label, bool mixed)
	{
		if (mixed)
			ImGui::TextDisabled("%s: Mixed", label);
	}

	std::string AssetLabel(AssetHandle handle, AssetType expectedType)
	{
		if (handle == 0)
			return "None";

		if (!AssetManager::IsAssetHandleValid(handle) || AssetManager::GetAssetType(handle) != expectedType)
			return "Invalid";

		const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(handle);
		return metadata ? metadata.m_Filepath.filename().string() : "Invalid";
	}

	std::string TextureAssetLabel(AssetHandle handle, int32_t spriteIndex)
	{
		std::string label = AssetLabel(handle, AssetType::Texture2D);
		if (handle == 0 || label == "Invalid" || spriteIndex < 0 || !AssetManager::IsAssetHandleValid(handle) || AssetManager::GetAssetType(handle) != AssetType::Texture2D)
			return label;

		const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(handle);
		const auto& sprites = metadata.m_TextureSettings.m_Sprites;
		if (std::cmp_greater_equal(spriteIndex, sprites.size()))
			return label;

		return label + " / " + sprites[static_cast<size_t>(spriteIndex)].m_Name;
	}

	std::filesystem::path FindFileWithExtension(const std::filesystem::path& directory, const char* extension)
	{
		std::error_code error;
		if (!std::filesystem::exists(directory, error) || !std::filesystem::is_directory(directory, error))
			return {};

		for (const auto& entry : std::filesystem::directory_iterator(directory, error))
		{
			if (error)
				break;

			if (!entry.is_regular_file(error))
				continue;

			std::string entryExtension = entry.path().extension().string();
			std::ranges::transform(entryExtension, entryExtension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (entryExtension == extension)
				return entry.path();
		}

		return {};
	}

	std::filesystem::path ActiveScriptsDirectory()
	{
		if (!Project::GetActive())
			return {};

		const std::filesystem::path scriptsDirectory = Project::GetActiveAssetDirectory() / "Scripts";
		std::error_code error;
		return std::filesystem::exists(scriptsDirectory, error) && std::filesystem::is_directory(scriptsDirectory, error) ? scriptsDirectory : std::filesystem::path{};
	}

	std::filesystem::path ActiveScriptWorkspaceFile()
	{
		Ref<Project> activeProject = Project::GetActive();
		if (!activeProject)
			return {};

		const std::filesystem::path scriptsDirectory = ActiveScriptsDirectory();
		if (scriptsDirectory.empty())
			return {};

		const std::array<std::string, 2> preferredStems =
		{
			activeProject->GetConfig().m_ScriptModulePath.stem().string(),
			activeProject->GetProjectPath().stem().string()
		};

		std::error_code error;
		for (const std::string& stem : preferredStems)
		{
			if (stem.empty())
				continue;

			std::filesystem::path preferred = scriptsDirectory / (stem + ".sln");
			if (std::filesystem::exists(preferred, error))
				return preferred;

			preferred = scriptsDirectory / (stem + ".csproj");
			if (std::filesystem::exists(preferred, error))
				return preferred;
		}

		std::filesystem::path workspaceFile = FindFileWithExtension(scriptsDirectory, ".sln");
		if (!workspaceFile.empty())
			return workspaceFile;

		return FindFileWithExtension(scriptsDirectory, ".csproj");
	}

	std::filesystem::path FirstExistingPath(const std::vector<std::filesystem::path>& paths)
	{
		std::error_code error;
		for (const auto& path : paths)
			if (!path.empty() && std::filesystem::exists(path, error))
				return path;

		return {};
	}

	std::string Lowercase(std::string value)
	{
		std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value;
	}

	std::string EntitySortKey(Entity entityIn)
	{
		if (!entityIn || !entityIn.HasComponent<TagComponent>())
			return {};
		return Lowercase(entityIn.GetComponent<TagComponent>().m_Tag);
	}

	bool EntityDisplayLess(Entity left, Entity right)
	{
		const std::string leftKey = EntitySortKey(left);
		const std::string rightKey = EntitySortKey(right);
		if (leftKey == rightKey)
			return static_cast<uint64_t>(left.GetUUID()) < static_cast<uint64_t>(right.GetUUID());
		return leftKey < rightKey;
	}

	bool FilenameMatches(const std::filesystem::path& path, const std::vector<std::string>& filenames)
	{
		const std::string filename = Lowercase(path.filename().string());
		return std::ranges::find(filenames, filename) != filenames.end();
	}

	std::filesystem::path EnvironmentPath(const char* name)
	{
		const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
		return value ? std::filesystem::path(value) : std::filesystem::path{};
	}

	std::filesystem::path FindOnPath(const std::vector<std::string>& filenames)
	{
		const char* pathValue = std::getenv("PATH"); // NOLINT(concurrency-mt-unsafe)
		if (!pathValue)
			return {};

		std::string pathText(pathValue);
		size_t start = 0;
		std::error_code error;
		while (start <= pathText.size())
		{
			size_t end = pathText.find(';', start);
			if (end == std::string::npos)
				end = pathText.size();

			std::filesystem::path directory = pathText.substr(start, end - start);
			for (const std::string& filename : filenames)
			{
				std::filesystem::path candidate = directory / filename;
				if (std::filesystem::exists(candidate, error))
					return candidate;
				error.clear();
			}

			start = end + 1;
		}

		return {};
	}

	std::filesystem::path FindFirstNamedFileUnder(const std::filesystem::path& root, const std::vector<std::string>& filenames, int maxDepth)
	{
		std::error_code error;
		if (root.empty() || !std::filesystem::exists(root, error))
			return {};

		std::vector<std::pair<std::filesystem::path, int>> pending;
		pending.emplace_back(root, 0);

		while (!pending.empty())
		{
			const auto [directory, depth] = pending.back();
			pending.pop_back();

			for (const auto& entry : std::filesystem::directory_iterator(directory, error))
			{
				if (error)
					break;

				if (entry.is_regular_file(error) && FilenameMatches(entry.path(), filenames))
					return entry.path();

				if (depth < maxDepth && entry.is_directory(error))
					pending.emplace_back(entry.path(), depth + 1);
			}

			error.clear();
		}

		return {};
	}

	std::filesystem::path VisualStudioExecutable()
	{
		static const std::filesystem::path executable = FirstExistingPath({
			"C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/devenv.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Professional/Common7/IDE/devenv.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/devenv.exe",
			"C:/Program Files/Microsoft Visual Studio/2022/Preview/Common7/IDE/devenv.exe"
			});
		return executable.empty() ? std::filesystem::path("devenv.exe") : executable;
	}

	std::filesystem::path RiderExecutable()
	{
		static const std::filesystem::path executable = []()
			{
				const std::filesystem::path programFiles = EnvironmentPath("ProgramFiles");
				const std::filesystem::path programFilesX86 = EnvironmentPath("ProgramFiles(x86)");
				const std::filesystem::path localAppData = EnvironmentPath("LOCALAPPDATA");
				const std::vector<std::string> riderFiles = { "rider64.exe", "rider.exe", "rider.cmd", "rider.bat" };

				std::filesystem::path candidate = FindOnPath(riderFiles);
				if (!candidate.empty())
					return candidate;

				candidate = FirstExistingPath({
					localAppData / "JetBrains" / "Toolbox" / "scripts" / "rider.cmd",
					localAppData / "JetBrains" / "Toolbox" / "scripts" / "rider.bat",
					localAppData / "Programs" / "Rider" / "bin" / "rider64.exe",
					localAppData / "Programs" / "Rider" / "bin" / "rider.exe"
					});
				if (!candidate.empty())
					return candidate;

				candidate = FindFirstNamedFileUnder(localAppData / "JetBrains" / "Toolbox" / "apps" / "Rider", riderFiles, 6);
				if (!candidate.empty())
					return candidate;

				candidate = FindFirstNamedFileUnder(programFiles / "JetBrains", riderFiles, 4);
				if (!candidate.empty())
					return candidate;

				candidate = FindFirstNamedFileUnder(programFilesX86 / "JetBrains", riderFiles, 4);
				if (!candidate.empty())
					return candidate;

				candidate = FindFirstNamedFileUnder(localAppData / "Programs", riderFiles, 4);
				return candidate;
			}();

		return executable.empty() ? std::filesystem::path("rider64.exe") : executable;
	}

	std::filesystem::path VSCodeExecutable()
	{
		static const std::filesystem::path executable = []()
			{
				const std::filesystem::path programFiles = EnvironmentPath("ProgramFiles");
				const std::filesystem::path programFilesX86 = EnvironmentPath("ProgramFiles(x86)");
				const std::filesystem::path localAppData = EnvironmentPath("LOCALAPPDATA");
				const std::vector<std::string> codeFiles = { "code.cmd", "code.exe", "Code.exe", "code-insiders.cmd", "Code - Insiders.exe" };

				std::filesystem::path candidate = FindOnPath(codeFiles);
				if (!candidate.empty())
					return candidate;

				candidate = FirstExistingPath({
					localAppData / "Programs" / "Microsoft VS Code" / "Code.exe",
					localAppData / "Programs" / "Microsoft VS Code Insiders" / "Code - Insiders.exe",
					programFiles / "Microsoft VS Code" / "Code.exe",
					programFiles / "Microsoft VS Code Insiders" / "Code - Insiders.exe",
					programFilesX86 / "Microsoft VS Code" / "Code.exe"
					});
				return candidate;
			}();

		return executable.empty() ? std::filesystem::path("code.cmd") : executable;
	}

	bool OpenWorkspaceWithDefaultApp(const std::filesystem::path& workspaceFile)
	{
		if (Utils::OpenExternalPath(workspaceFile))
			return true;

		WHP_CORE_WARN("[Script Workspace] Could not open '{0}'.", workspaceFile.string());
		return false;
	}

	bool OpenWorkspaceWithApp(const char* appName, const std::filesystem::path& executable, const std::filesystem::path& workspaceFile)
	{
		if (Utils::OpenExternalPathWith(executable, workspaceFile))
			return true;

		WHP_CORE_WARN("[Script Workspace] Could not open '{0}' with {1}.", workspaceFile.string(), appName);
		return false;
	}

	float EstimatedSmallButtonWidth(const char* label)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		return ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
	}

	void SameLineIfFits(float nextItemWidth)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		if (ImGui::GetContentRegionAvail().x >= nextItemWidth + style.ItemSpacing.x)
			ImGui::SameLine();
	}

	void DrawScriptWorkspaceActions()
	{
		ImGui::PushID("ScriptWorkspaceActions");

		const std::filesystem::path workspaceFile = ActiveScriptWorkspaceFile();
		const std::filesystem::path scriptsDirectory = ActiveScriptsDirectory();
		const bool hasWorkspace = !workspaceFile.empty();
		const bool hasScriptsDirectory = !scriptsDirectory.empty();

		ImGui::BeginDisabled(!hasWorkspace);
		if (ImGui::SmallButton("Open"))
			OpenWorkspaceWithDefaultApp(workspaceFile);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Open the C# solution with the default IDE");
		SameLineIfFits(EstimatedSmallButtonWidth("VS"));
		if (ImGui::SmallButton("VS"))
			OpenWorkspaceWithApp("Visual Studio", VisualStudioExecutable(), workspaceFile);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Open in Visual Studio");
		SameLineIfFits(EstimatedSmallButtonWidth("Rider"));
		if (ImGui::SmallButton("Rider"))
			OpenWorkspaceWithApp("Rider", RiderExecutable(), workspaceFile);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Open in JetBrains Rider");
		SameLineIfFits(EstimatedSmallButtonWidth("Code"));
		if (ImGui::SmallButton("Code"))
			OpenWorkspaceWithApp("Visual Studio Code", VSCodeExecutable(), workspaceFile);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Open in Visual Studio Code");
		ImGui::EndDisabled();

		ImGui::Spacing();
		ImGui::BeginDisabled(!hasScriptsDirectory);
		if (ImGui::SmallButton("Folder"))
			Utils::OpenExternalPath(scriptsDirectory);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Open Assets/Scripts");
		ImGui::EndDisabled();

		if (hasWorkspace)
			ImGui::TextDisabled("%s", workspaceFile.filename().string().c_str());
		else
			ImGui::TextDisabled("No C# solution found in Assets/Scripts.");

		if (ScriptEngine::IsDebuggerEnabled())
			ImGui::TextDisabled("Debugger: %s:%d", ScriptEngine::GetDebuggerHost().c_str(), ScriptEngine::GetDebuggerPort());
		else
			ImGui::TextDisabled("Debugger disabled");

		ImGui::PopID();
	}

	void DrawUITextAlignmentControls(UITextHorizontalAlignment& horizontalAlignment, UITextVerticalAlignment& verticalAlignment)
	{
		const char* horizontalLabels[] = { "Left", "Center", "Right" };
		int horizontal = static_cast<int>(horizontalAlignment);
		if (ImGui::Combo("Horizontal Align", &horizontal, horizontalLabels, IM_ARRAYSIZE(horizontalLabels)))
			horizontalAlignment = static_cast<UITextHorizontalAlignment>(horizontal);

		const char* verticalLabels[] = { "Top", "Center", "Bottom" };
		int vertical = static_cast<int>(verticalAlignment);
		if (ImGui::Combo("Vertical Align", &vertical, verticalLabels, IM_ARRAYSIZE(verticalLabels)))
			verticalAlignment = static_cast<UITextVerticalAlignment>(vertical);
	}

	void DrawUIPointerEventControls(UIPointerEventState& pointerEvents)
	{
		if (ImGui::TreeNodeEx("Pointer Events", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			ImGui::InputText("On Pointer Enter", &pointerEvents.m_OnPointerEnterCallback);
			ImGui::InputText("On Pointer Exit", &pointerEvents.m_OnPointerExitCallback);
			ImGui::InputText("On Pointer Down", &pointerEvents.m_OnPointerDownCallback);
			ImGui::InputText("On Pointer Up", &pointerEvents.m_OnPointerUpCallback);
			ImGui::InputText("On Pointer Drag", &pointerEvents.m_OnPointerDragCallback);
			ImGui::TextDisabled("Empty callback names fall back to UIEntity virtual methods.");
			ImGui::BeginDisabled();
			ImGui::Checkbox("Pointer Inside", &pointerEvents.m_PointerInside);
			ImGui::Checkbox("Dragging", &pointerEvents.m_Dragging);
			ImGui::EndDisabled();
			ImGui::TreePop();
		}
	}

	template<typename T, typename UIFunction>
	void DrawComponent(const std::string& name, Entity entityIn, const std::function<void()>& beforeChange, UIFunction uiFunction)
	{
		constexpr ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
		if (entityIn.HasComponent<T>())
		{
			auto& component = entityIn.GetComponent<T>();

			const ImGuiStyle& style = ImGui::GetStyle();
			ImGui::PushID(name.c_str());
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 10.0f, 7.0f });
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 8.0f, 7.0f });
			ImGui::PushStyleColor(ImGuiCol_Header, style.Colors[ImGuiCol_Header]);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style.Colors[ImGuiCol_HeaderHovered]);
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.Colors[ImGuiCol_HeaderActive]);
			bool open = ImGui::TreeNodeEx("##Component", treeNodeFlags, "%s", name.c_str());
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();
			ImGui::PopStyleVar();

			bool removeComponent = false;
			if (ImGui::BeginPopupContextItem("Component Settings"))
			{
				if constexpr (!std::is_same_v<T, TransformComponent>)
					if (ImGui::MenuItem("Remove component"))
						removeComponent = true;

				ImGui::EndPopup();
			}

			if (open)
			{
				ImGui::Spacing();
				ImGui::Indent(8.0f);
				uiFunction(component);
				ImGui::Unindent(8.0f);
				ImGui::TreePop();
			}

			if (removeComponent)
			{
				if (beforeChange)
					beforeChange();
				entityIn.RemoveComponent<T>();
			}

			ImGui::Spacing();
			ImGui::PopID();
		}
	}
}

SceneHierarchyPanel::SceneHierarchyPanel()
	: EditorPanel("Scene Hierarchy", true)
{
}

SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
	: EditorPanel("Scene Hierarchy", true)
{
	SetContext(context);
}

void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
{
	m_Context = context;
	ClearSelection();
	ClearSearchShortcut();
	m_RequestSearchFocus = false;
	MarkHierarchyDirty();
}

Ref<Scene>& SceneHierarchyPanel::GetContext()
{
	return m_Context;
}

void SceneHierarchyPanel::MarkHierarchyDirty()
{
	m_HierarchyCacheDirty = true;
}

void SceneHierarchyPanel::RebuildHierarchyCache()
{
	WHP_PROFILE_FUNCTION();
	m_RootEntityCache.clear();
	m_CachedEntityCount = 0;
	m_CanClipFlatHierarchy = true;
	if (!m_Context)
	{
		m_HierarchyCacheDirty = false;
		return;
	}

	auto group = m_Context->m_Registry.group<>(entt::get<IDComponent>);
	m_CachedEntityCount = group.size();
	m_RootEntityCache.reserve(m_CachedEntityCount);

	for (auto entityID : group)
	{
		Entity entity{ entityID, m_Context.get() };
		if (!entity.HasComponent<TagComponent>() || !entity.HasComponent<HierarchyComponent>())
			continue;

		auto& hierarchy = entity.GetComponent<HierarchyComponent>();
		if (hierarchy.m_Parent != 0 && !m_Context->FindEntityByUUID(hierarchy.m_Parent))
			hierarchy.m_Parent = 0;
		if (hierarchy.m_Parent == 0)
		{
			if (!hierarchy.m_Children.empty())
				m_CanClipFlatHierarchy = false;
			m_RootEntityCache.emplace_back(entity);
		}
	}

	std::sort(m_RootEntityCache.begin(), m_RootEntityCache.end(), EntityDisplayLess);
	m_HierarchyCacheDirty = false;
}

bool SceneHierarchyPanel::CanUseFlatHierarchyClipper() const
{
	return m_CanClipFlatHierarchy;
}

bool SceneHierarchyPanel::IsSearchActive() const
{
	return !m_CachedSearchQueryLower.empty();
}

bool SceneHierarchyPanel::EntityMatchesSearch(Entity entityIn) const
{
	if (!IsSearchActive())
		return true;
	if (!IsEntityAlive(entityIn) || !entityIn.HasComponent<TagComponent>() || !entityIn.HasComponent<HierarchyComponent>())
		return false;

	const auto& tag = entityIn.GetComponent<TagComponent>().m_Tag;
	const auto& hierarchy = entityIn.GetComponent<HierarchyComponent>();
	std::string searchable = Lowercase(tag + " " + std::to_string(static_cast<uint64_t>(entityIn.GetUUID())));
	if (hierarchy.m_IsGroup)
		searchable += " group";
	if (entityIn.HasComponent<CameraComponent>())
		searchable += " camera";
	if (entityIn.HasComponent<ScriptComponent>())
		searchable += " script";
	if (entityIn.HasComponent<SpriteRendererComponent>())
		searchable += " sprite renderer texture";
	if (entityIn.HasComponent<TextComponent>())
		searchable += " text";
	if (entityIn.HasComponent<UITransformComponent>())
		searchable += " ui";
	if (entityIn.HasComponent<UICanvasComponent>())
		searchable += " canvas ui root screen";
	if (entityIn.HasComponent<UIImageComponent>())
		searchable += " image";
	if (entityIn.HasComponent<UITextComponent>())
		searchable += " ui text";
	if (entityIn.HasComponent<UIButtonComponent>())
		searchable += " button ui button";
	if (entityIn.HasComponent<Rigidbody2DComponent>())
		searchable += " rigidbody physics";
	if (entityIn.HasComponent<BoxCollider2DComponent>())
		searchable += " box collider physics";
	if (entityIn.HasComponent<CircleCollider2DComponent>())
		searchable += " circle collider physics";

	return searchable.find(m_CachedSearchQueryLower) != std::string::npos;
}

bool SceneHierarchyPanel::EntityOrDescendantMatchesSearch(Entity entityIn) const
{
	if (!IsSearchActive())
		return true;
	if (EntityMatchesSearch(entityIn))
		return true;
	if (!IsEntityAlive(entityIn) || !entityIn.HasComponent<HierarchyComponent>())
		return false;

	const auto& hierarchy = entityIn.GetComponent<HierarchyComponent>();
	for (UUID childId : hierarchy.m_Children)
	{
		if (Entity child = m_Context ? m_Context->FindEntityByUUID(childId) : Entity{}; child && EntityOrDescendantMatchesSearch(child))
			return true;
	}
	return false;
}

bool SceneHierarchyPanel::IsEntityAlive(Entity entityIn) const
{
	return m_Context &&
		entityIn &&
		entityIn.GetScene() == m_Context.get() &&
		m_Context->m_Registry.valid(static_cast<entt::entity>(entityIn));
}

void SceneHierarchyPanel::ValidateSelection()
{
	if (!m_Context)
	{
		ClearSelection();
		return;
	}

	std::erase_if(m_SelectionContexts, [this](UUID entityId)
		{
			return !m_Context->FindEntityByUUID(entityId);
		});

	if (m_SelectionContexts.empty())
	{
		m_SelectionContext = {};
		return;
	}

	const bool primaryValid = IsEntityAlive(m_SelectionContext);
	const bool primaryStillSelected = primaryValid && std::ranges::find(m_SelectionContexts, m_SelectionContext.GetUUID()) != m_SelectionContexts.end();
	if (!primaryStillSelected)
		m_SelectionContext = m_Context->FindEntityByUUID(m_SelectionContexts.back());
}

void SceneHierarchyPanel::SetSceneChangeCallback(std::function<void()> callback)
{
	m_SceneChangeCallback = std::move(callback);
}

void SceneHierarchyPanel::SetSaveEntityTemplateCallback(std::function<void(Entity)> callback)
{
	m_SaveEntityTemplateCallback = std::move(callback);
}

void SceneHierarchyPanel::SetApplyEntityTemplateCallback(std::function<void(Entity)> callback)
{
	m_ApplyEntityTemplateCallback = std::move(callback);
}

void SceneHierarchyPanel::SetRevertEntityTemplateCallback(std::function<void(Entity)> callback)
{
	m_RevertEntityTemplateCallback = std::move(callback);
}

void SceneHierarchyPanel::SetUnpackEntityTemplateCallback(std::function<void(Entity)> callback)
{
	m_UnpackEntityTemplateCallback = std::move(callback);
}

void SceneHierarchyPanel::RegisterShortcuts(EditorShortcutManager& shortcuts)
{
	auto add = [this, &shortcuts](const char* id, const char* displayName, const char* category, const UI::ShortcutBinding& binding, std::function<bool()> callback, std::function<bool()> isAvailable = {})
	{
		shortcuts.Add(
			EditorShortcutScope::SceneHierarchy,
			std::string("scene_hierarchy.") + id,
			displayName,
			category,
			binding,
			std::move(callback),
			std::move(isAvailable),
			[this]() { return IsShortcutContextActive(); });
	};

	add("create_entity", "Create Entity", "Scene", { Key::N, true, true, false }, [this]() { return CreateEntityShortcut(); }, [this]() { return m_Context != nullptr; });
	add("create_group", "Create Group", "Scene", { Key::G, true, true, false }, [this]() { return CreateGroupShortcut(); }, [this]() { return m_Context != nullptr; });
	add("focus_search", "Focus Search", "Navigation", { Key::F, true, false, false }, [this]() { return FocusSearchShortcut(); }, [this]() { return m_Context != nullptr; });
	add("clear_search", "Clear Search", "Navigation", { Key::Escape, false, false, false }, [this]() { return ClearSearchShortcut(); }, [this]() { return IsSearchActive(); });
	add("clear_selection", "Clear Selection", "Selection", { Key::Escape, false, false, false }, [this]() { ClearSelection(); return true; }, [this]() { return m_SelectionContext; });
	add("move_to_root", "Move Selection To Root", "Hierarchy", { Key::Home, true, true, false }, [this]() { return MoveSelectionToRootShortcut(); }, [this]() { return m_SelectionContext; });
	add("save_template", "Save Selected Entity Template", "Entity Template", { Key::T, true, true, false }, [this]() { return SaveSelectedTemplateShortcut(); }, [this]() { return m_SelectionContext && m_SaveEntityTemplateCallback; });
	add("apply_template", "Apply Selected Entity Template", "Entity Template", { Key::T, true, false, false }, [this]() { return ApplySelectedTemplateShortcut(); }, [this]() { return m_SelectionContext && m_ApplyEntityTemplateCallback; });
	add("revert_template", "Revert Selected Entity Template", "Entity Template", { Key::T, true, false, true }, [this]() { return RevertSelectedTemplateShortcut(); }, [this]() { return m_SelectionContext && m_RevertEntityTemplateCallback; });
	add("unpack_template", "Unpack Selected Entity Template", "Entity Template", { Key::U, true, true, false }, [this]() { return UnpackSelectedTemplateShortcut(); }, [this]() { return m_SelectionContext && m_UnpackEntityTemplateCallback; });
}

bool SceneHierarchyPanel::IsOpen() const
{
	return m_Open;
}

Entity SceneHierarchyPanel::GetSelectedEntity() const
{
	return m_SelectionContext;
}

std::vector<UUID> SceneHierarchyPanel::GetSelectedEntityIds() const
{
	return m_SelectionContexts;
}

void SceneHierarchyPanel::OnImGuiRender()
{
	WHP_PROFILE_FUNCTION();
	if (!m_Open)
	{
		m_Focused = false;
		return;
	}

	bool open = m_Open;
	ImGui::Begin("Scene Hierarchy", &open);
	m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
	if (open != m_Open)
		SetOpen(open);

	if (m_Context)
	{
		ValidateSelection();
		const auto group = m_Context->m_Registry.group<>(entt::get<IDComponent>);
		if (m_HierarchyCacheDirty || m_CachedEntityCount != group.size())
			RebuildHierarchyCache();

		if (m_RequestSearchFocus)
		{
			ImGui::SetKeyboardFocusHere();
			m_RequestSearchFocus = false;
		}

		const bool showClearSearch = !m_SearchQuery.empty();
		const float clearSearchWidth = showClearSearch ? ImGui::CalcTextSize("Clear").x + ImGui::GetStyle().FramePadding.x * 2.0f : 0.0f;
		const float searchWidth = showClearSearch ? std::max(ImGui::GetContentRegionAvail().x - clearSearchWidth - ImGui::GetStyle().ItemSpacing.x, 80.0f) : -1.0f;
		ImGui::SetNextItemWidth(searchWidth);
		if (ImGui::InputTextWithHint("##SceneHierarchySearch", "Search entities, components, UUID...", &m_SearchQuery))
			m_CachedSearchQueryLower = Lowercase(m_SearchQuery);
		if (showClearSearch)
		{
			ImGui::SameLine();
			if (ImGui::Button("Clear"))
				ClearSearchShortcut();
		}
		ImGui::Separator();

		if (IsSearchActive())
		{
			size_t visibleCount = 0;
			for (Entity rootEntity : m_RootEntityCache)
			{
				if (!EntityOrDescendantMatchesSearch(rootEntity))
					continue;
				DrawEntityNode(rootEntity);
				++visibleCount;
			}
			if (visibleCount == 0)
				ImGui::TextDisabled("No matching entities.");
		}
		else if (CanUseFlatHierarchyClipper() && m_RootEntityCache.size() > 80)
		{
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(m_RootEntityCache.size()));
			while (clipper.Step())
			{
				for (int itemIndex = clipper.DisplayStart; itemIndex < clipper.DisplayEnd; ++itemIndex)
					DrawEntityNode(m_RootEntityCache[static_cast<size_t>(itemIndex)]);
			}
		}
		else
		{
			for (Entity rootEntity : m_RootEntityCache)
				DrawEntityNode(rootEntity);
		}

		if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
			ClearSelection();

		if (ImGui::BeginPopupContextWindow(nullptr, 1 | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Entity"))
			{
				NotifySceneChange();
				m_Context->CreateEntity("New Entity");
				MarkHierarchyDirty();
			}
			if (ImGui::MenuItem("Create Group"))
			{
				NotifySceneChange();
				Entity groupEntity = m_Context->CreateEntity("Group");
				groupEntity.GetComponent<HierarchyComponent>().m_IsGroup = true;
				MarkHierarchyDirty();
			}
			DrawCreateUIMenu();

			ImGui::EndPopup();
		}
	}

	ImGui::End();

	ImGui::Begin("Properties");
	m_Focused = m_Focused || ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
	if (!m_Context)
	{
		ImGui::TextDisabled("No scene loaded.");
	}
	else if (!m_SelectionContext)
	{
		ImGui::Dummy(ImVec2(0.0f, 8.0f));
		ImGui::TextDisabled("No entity selected.");
	}
	else
	{
		if (m_SelectionContexts.size() > 1)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
			ImGui::Text("%zu entities selected", m_SelectionContexts.size());
			ImGui::PopStyleColor();
			ImGui::TextDisabled("Showing primary selection.");
			ImGui::Separator();
		}

		ImGui::BeginChild("##PropertiesScroll", ImVec2(0.0f, 0.0f), false);
		if (m_SelectionContexts.size() > 1)
		{
			std::vector<Entity> selectedEntities = GetSelectedEntities();
			DrawMultiEditComponents(selectedEntities);
		}
		else
		{
			DrawComponents(m_SelectionContext);
		}
		TrackPropertyEditHistory();
		ImGui::EndChild();
	}

	ImGui::End();
}

void SceneHierarchyPanel::SetOpen(bool open)
{
	if (m_Open == open)
		return;
	m_Open = open;
	m_OpenDirty = true;
}

bool SceneHierarchyPanel::ConsumeOpenDirty()
{
	const bool dirty = m_OpenDirty;
	m_OpenDirty = false;
	return dirty;
}

std::vector<Entity> SceneHierarchyPanel::GetSelectedEntities() const
{
	std::vector<Entity> result;
	if (!m_Context)
		return result;

	result.reserve(m_SelectionContexts.size());
	for (UUID id : m_SelectionContexts)
	{
		if (Entity selected = m_Context->FindEntityByUUID(id); selected)
			result.push_back(selected);
	}
	return result;
}

void SceneHierarchyPanel::SetSelectedEntity(Entity entityIn, bool append)
{
	if (!IsEntityAlive(entityIn))
	{
		if (!append)
			ClearSelection();
		return;
	}

	if (!append)
		m_SelectionContexts.clear();

	UUID id = entityIn.GetUUID();
	auto it = std::ranges::find(m_SelectionContexts, id);
	if (append && it != m_SelectionContexts.end())
	{
		m_SelectionContexts.erase(it);
		m_SelectionContext = m_SelectionContexts.empty() ? Entity{} : m_Context->FindEntityByUUID(m_SelectionContexts.back());
		return;
	}

	if (it == m_SelectionContexts.end())
		m_SelectionContexts.push_back(id);
	m_SelectionContext = entityIn;
}

void SceneHierarchyPanel::SetSelectedEntityIds(const std::vector<UUID>& ids)
{
	ClearSelection();
	if (!m_Context)
		return;

	for (UUID id : ids)
	{
		Entity selected = m_Context->FindEntityByUUID(id);
		if (!selected)
			continue;

		m_SelectionContexts.push_back(id);
		m_SelectionContext = selected;
	}
}

void SceneHierarchyPanel::SelectAll()
{
	ClearSelection();
	if (!m_Context)
		return;

	auto view = m_Context->m_Registry.view<IDComponent>();
	for (auto entityId : view)
	{
		Entity selected{ entityId, m_Context.get() };
		m_SelectionContexts.push_back(selected.GetUUID());
		m_SelectionContext = selected;
	}
}

void SceneHierarchyPanel::ClearSelection()
{
	m_SelectionContext = {};
	m_SelectionContexts.clear();
}

bool SceneHierarchyPanel::IsShortcutContextActive() const
{
	return m_Open && m_Focused;
}

bool SceneHierarchyPanel::CreateEntityShortcut()
{
	if (!m_Context)
		return false;
	NotifySceneChange();
	SetSelectedEntity(m_Context->CreateEntity("New Entity"));
	MarkHierarchyDirty();
	return true;
}

bool SceneHierarchyPanel::CreateGroupShortcut()
{
	if (!m_Context)
		return false;
	NotifySceneChange();
	Entity groupEntity = m_Context->CreateEntity("Group");
	groupEntity.GetComponent<HierarchyComponent>().m_IsGroup = true;
	SetSelectedEntity(groupEntity);
	MarkHierarchyDirty();
	return true;
}

bool SceneHierarchyPanel::MoveSelectionToRootShortcut()
{
	std::vector<Entity> selectedEntities = GetSelectedEntities();
	if (selectedEntities.empty())
		return false;

	bool changed = false;
	for (Entity selected : selectedEntities)
	{
		if (!selected.HasComponent<HierarchyComponent>() || selected.GetComponent<HierarchyComponent>().m_Parent == 0)
			continue;
		SetEntityParent(selected, {});
		changed = true;
	}
	if (changed)
		NotifySceneChange();
	return changed;
}

bool SceneHierarchyPanel::SaveSelectedTemplateShortcut()
{
	if (!m_SelectionContext || !m_SaveEntityTemplateCallback)
		return false;
	m_SaveEntityTemplateCallback(m_SelectionContext);
	return true;
}

bool SceneHierarchyPanel::ApplySelectedTemplateShortcut()
{
	if (!m_SelectionContext || !m_ApplyEntityTemplateCallback)
		return false;
	Entity prefabRoot = FindPrefabRoot(m_SelectionContext);
	if (!prefabRoot)
		return false;
	m_ApplyEntityTemplateCallback(prefabRoot);
	return true;
}

bool SceneHierarchyPanel::RevertSelectedTemplateShortcut()
{
	if (!m_SelectionContext || !m_RevertEntityTemplateCallback)
		return false;
	Entity prefabRoot = FindPrefabRoot(m_SelectionContext);
	if (!prefabRoot)
		return false;
	m_RevertEntityTemplateCallback(prefabRoot);
	return true;
}

bool SceneHierarchyPanel::UnpackSelectedTemplateShortcut()
{
	if (!m_SelectionContext || !m_UnpackEntityTemplateCallback)
		return false;
	Entity prefabRoot = FindPrefabRoot(m_SelectionContext);
	if (!prefabRoot)
		return false;
	m_UnpackEntityTemplateCallback(prefabRoot);
	return true;
}

bool SceneHierarchyPanel::FocusSearchShortcut()
{
	if (!m_Context)
		return false;
	m_RequestSearchFocus = true;
	return true;
}

bool SceneHierarchyPanel::ClearSearchShortcut()
{
	if (m_SearchQuery.empty() && m_CachedSearchQueryLower.empty())
		return false;
	m_SearchQuery.clear();
	m_CachedSearchQueryLower.clear();
	return true;
}

void SceneHierarchyPanel::DrawEntityNode(Entity entityIn)
{
	if (!IsEntityAlive(entityIn) || !entityIn.HasComponent<TagComponent>() || !entityIn.HasComponent<HierarchyComponent>())
	{
		MarkHierarchyDirty();
		return;
	}
	if (IsSearchActive() && !EntityOrDescendantMatchesSearch(entityIn))
		return;

	auto& tag = entityIn.GetComponent<TagComponent>().m_Tag;
	auto& hierarchy = entityIn.GetComponent<HierarchyComponent>();

	ImGui::PushID(static_cast<int>(static_cast<uint32_t>(entityIn)));
	ImGuiTreeNodeFlags flags = (IsSelected(entityIn) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
	flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
	if (hierarchy.m_Children.empty())
		flags |= ImGuiTreeNodeFlags_Leaf;
	else if (IsSearchActive())
		flags |= ImGuiTreeNodeFlags_DefaultOpen;

	std::string groupLabel;
	const char* label = tag.c_str();
	if (hierarchy.m_IsGroup)
	{
		groupLabel = "[Group] " + tag;
		label = groupLabel.c_str();
	}
	bool opened = ImGui::TreeNodeEx("##EntityNode", flags, "%s", label);

	if (ImGui::IsItemClicked())
		SetSelectedEntity(entityIn, Input::IsKeyDown(Key::LeftControl) || Input::IsKeyDown(Key::RightControl));

		if (ImGui::BeginDragDropSource())
		{
			UUID entityId = entityIn.GetUUID();
			ImGui::SetDragDropPayload(SceneEntityPayloadType, &entityId, sizeof(UUID));
			ImGui::TextUnformatted(tag.c_str());
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(SceneEntityPayloadType))
			{
				UUID childId = *static_cast<UUID*>(payload->Data);
				Entity child = m_Context->FindEntityByUUID(childId);
				if (child && CanParentEntity(child, entityIn))
				{
					NotifySceneChange();
					SetEntityParent(child, entityIn);
				}
			}
			ImGui::EndDragDropTarget();
		}

		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Create Child"))
			{
				NotifySceneChange();
				Entity child = m_Context->CreateEntity("New Entity");
				SetEntityParent(child, entityIn);
				MarkHierarchyDirty();
			}
			if (ImGui::MenuItem("Create Child Group"))
			{
				NotifySceneChange();
				Entity childGroup = m_Context->CreateEntity("Group");
				childGroup.GetComponent<HierarchyComponent>().m_IsGroup = true;
				SetEntityParent(childGroup, entityIn);
				MarkHierarchyDirty();
			}
			DrawCreateUIMenu(entityIn);
			if (hierarchy.m_Parent != 0 && ImGui::MenuItem("Move To Root"))
			{
				NotifySceneChange();
				SetEntityParent(entityIn, {});
			}
			if (m_SaveEntityTemplateCallback && ImGui::MenuItem("Save as Entity Template"))
				m_SaveEntityTemplateCallback(entityIn);
			if (entityIn.HasComponent<PrefabComponent>())
			{
				Entity prefabRoot = FindPrefabRoot(entityIn);
				ImGui::Separator();
				if (prefabRoot)
				{
					if (m_ApplyEntityTemplateCallback && ImGui::MenuItem("Apply to Entity Template"))
						m_ApplyEntityTemplateCallback(prefabRoot);
					if (m_RevertEntityTemplateCallback && ImGui::MenuItem("Revert Instance"))
						m_RevertEntityTemplateCallback(prefabRoot);
					if (m_UnpackEntityTemplateCallback && ImGui::MenuItem("Unpack Instance"))
						m_UnpackEntityTemplateCallback(prefabRoot);
				}
				else
				{
					ImGui::TextDisabled("Prefab child");
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete Entity"))
				entityDeleted = true;

			ImGui::EndPopup();
		}

		if (opened)
		{
			std::vector<Entity> children;
			children.reserve(hierarchy.m_Children.size());
			for (UUID childId : hierarchy.m_Children)
			{
				if (Entity child = m_Context->FindEntityByUUID(childId); child)
					children.push_back(child);
			}
			std::sort(children.begin(), children.end(), EntityDisplayLess);
			for (Entity child : children)
				DrawEntityNode(child);
			ImGui::TreePop();
		}

		if (entityDeleted)
		{
			NotifySceneChange();
			DestroyEntityWithSelection(entityIn);
		}
		ImGui::PopID();
}

void SceneHierarchyPanel::SetEntityParent(Entity child, Entity parent)
{
	if (!IsEntityAlive(child) || child == parent || !child.HasComponent<HierarchyComponent>() || (parent && !IsEntityAlive(parent)))
		return;

	auto& childHierarchy = child.GetComponent<HierarchyComponent>();
	if (childHierarchy.m_Parent != 0)
	{
		Entity oldParent = m_Context->FindEntityByUUID(childHierarchy.m_Parent);
		if (oldParent && oldParent.HasComponent<HierarchyComponent>())
		{
			auto& oldParentHierarchy = oldParent.GetComponent<HierarchyComponent>();
			std::erase(oldParentHierarchy.m_Children, child.GetUUID());
		}
	}

	childHierarchy.m_Parent = parent ? parent.GetUUID() : UUID(0);
	if (parent && parent.HasComponent<HierarchyComponent>())
	{
		auto& parentHierarchy = parent.GetComponent<HierarchyComponent>();
		if (std::ranges::find(parentHierarchy.m_Children, child.GetUUID()) == parentHierarchy.m_Children.end())
			parentHierarchy.m_Children.push_back(child.GetUUID());
	}
	MarkHierarchyDirty();
}

bool SceneHierarchyPanel::CanParentEntity(Entity child, Entity parent) const
{
	if (!child || !parent || child == parent)
		return false;

	return !IsDescendantOf(parent, child.GetUUID());
}

bool SceneHierarchyPanel::IsDescendantOf(Entity entityIn, UUID ancestorId) const
{
	if (!entityIn || !entityIn.HasComponent<HierarchyComponent>())
		return false;

	const auto& hierarchy = entityIn.GetComponent<HierarchyComponent>();
	if (hierarchy.m_Parent == 0)
		return false;
	if (hierarchy.m_Parent == ancestorId)
		return true;

	return IsDescendantOf(m_Context->FindEntityByUUID(hierarchy.m_Parent), ancestorId);
}

Entity SceneHierarchyPanel::FindPrefabRoot(Entity entityIn) const
{
	if (!entityIn || !entityIn.HasComponent<PrefabComponent>())
		return {};

	const AssetHandle source = entityIn.GetComponent<PrefabComponent>().m_Source;
	Entity current = entityIn;
	while (current && current.HasComponent<HierarchyComponent>())
	{
		if (current.HasComponent<PrefabComponent>())
		{
			const auto& prefab = current.GetComponent<PrefabComponent>();
			if (prefab.m_Source == source && prefab.m_Root)
				return current;
		}

		const auto& hierarchy = current.GetComponent<HierarchyComponent>();
		if (hierarchy.m_Parent == 0)
			break;

		current = m_Context ? m_Context->FindEntityByUUID(hierarchy.m_Parent) : Entity{};
	}

	return entityIn.GetComponent<PrefabComponent>().m_Root ? entityIn : Entity{};
}

Entity SceneHierarchyPanel::FindFirstUICanvas() const
{
	if (!m_Context)
		return {};

	auto view = m_Context->m_Registry.view<UITransformComponent, UICanvasComponent>();
	for (auto entityId : view)
		return Entity{ entityId, m_Context.get() };
	return {};
}

void SceneHierarchyPanel::ConfigureCanvasTransform(UITransformComponent& transform)
{
	transform.m_AnchorMin = { 0.0f, 0.0f };
	transform.m_AnchorMax = { 1.0f, 1.0f };
	transform.m_Pivot = { 0.5f, 0.5f };
	transform.m_AnchoredPosition = { 0.0f, 0.0f };
	transform.m_Size = { 0.0f, 0.0f };
	transform.m_Scale = { 1.0f, 1.0f };
	transform.m_Rotation = 0.0f;
	transform.m_SortOrder = 0;
	transform.m_Visible = true;
}

Entity SceneHierarchyPanel::FindOrCreateUICanvas()
{
	if (Entity canvas = FindFirstUICanvas())
		return canvas;

	Entity canvas = m_Context->CreateEntity("Canvas");
	auto& transform = canvas.AddComponent<UITransformComponent>();
	ConfigureCanvasTransform(transform);
	canvas.AddComponent<UICanvasComponent>();
	MarkHierarchyDirty();
	return canvas;
}

Entity SceneHierarchyPanel::ResolveUIParent(Entity requestedParent)
{
	if (requestedParent && requestedParent.HasComponent<UITransformComponent>())
		return requestedParent;
	return FindOrCreateUICanvas();
}

Entity SceneHierarchyPanel::CreateUIElement(UIElementKind kind, Entity parent)
{
	if (!m_Context)
		return {};

	NotifySceneChange();

	const char* name = "UI Element";
	switch (kind)
	{
	case UIElementKind::Panel: name = "UI Panel"; break;
	case UIElementKind::Canvas: name = "Canvas"; break;
	case UIElementKind::Image: name = "UI Image"; break;
	case UIElementKind::Text: name = "UI Text"; break;
	case UIElementKind::Button: name = "UI Button"; break;
	case UIElementKind::Toggle: name = "UI Toggle"; break;
	case UIElementKind::Slider: name = "UI Slider"; break;
	case UIElementKind::InputField: name = "UI Input Field"; break;
	case UIElementKind::VerticalLayout: name = "UI Vertical Layout"; break;
	case UIElementKind::HorizontalLayout: name = "UI Horizontal Layout"; break;
	}

	Entity entity = m_Context->CreateEntity(name);
	auto& transform = entity.AddComponent<UITransformComponent>();
	Entity resolvedParent = kind == UIElementKind::Canvas ? Entity{} : ResolveUIParent(parent);
	transform.m_Size = { 220.0f, 64.0f };
	transform.m_SortOrder = resolvedParent && resolvedParent.HasComponent<UITransformComponent>() ? resolvedParent.GetComponent<UITransformComponent>().m_SortOrder + 1 : 0;

	if (resolvedParent)
		SetEntityParent(entity, resolvedParent);

	switch (kind)
	{
	case UIElementKind::Canvas:
	{
		ConfigureCanvasTransform(transform);
		entity.AddComponent<UICanvasComponent>();
		break;
	}
	case UIElementKind::Panel:
	{
		transform.m_Size = { 360.0f, 240.0f };
		auto& panel = entity.AddComponent<UIPanelComponent>();
		panel.m_Color = { 0.05f, 0.08f, 0.10f, 0.86f };
		break;
	}
	case UIElementKind::Image:
	{
		transform.m_Size = { 160.0f, 160.0f };
		entity.AddComponent<UIImageComponent>();
		break;
	}
	case UIElementKind::Text:
	{
		transform.m_Size = { 260.0f, 48.0f };
		auto& text = entity.AddComponent<UITextComponent>();
		text.m_TextString = "New Text";
		text.m_FontSize = 24.0f;
		break;
	}
	case UIElementKind::Button:
	{
		transform.m_Size = { 200.0f, 56.0f };
		auto& button = entity.AddComponent<UIButtonComponent>();
		button.m_Text = "Button";
		break;
	}
	case UIElementKind::Toggle:
	{
		transform.m_Size = { 220.0f, 52.0f };
		auto& toggle = entity.AddComponent<UIToggleComponent>();
		toggle.m_Label = "Toggle";
		break;
	}
	case UIElementKind::Slider:
	{
		transform.m_Size = { 260.0f, 48.0f };
		auto& slider = entity.AddComponent<UISliderComponent>();
		slider.m_Value = 0.5f;
		break;
	}
	case UIElementKind::InputField:
	{
		transform.m_Size = { 280.0f, 54.0f };
		auto& inputField = entity.AddComponent<UIInputFieldComponent>();
		inputField.m_Placeholder = "Enter text";
		break;
	}
	case UIElementKind::VerticalLayout:
	case UIElementKind::HorizontalLayout:
	{
		transform.m_Size = { 360.0f, 260.0f };
		auto& image = entity.AddComponent<UIImageComponent>();
		image.m_Color = { 0.04f, 0.07f, 0.09f, 0.72f };
		auto& layout = entity.AddComponent<UIStackLayoutComponent>();
		layout.m_Axis = kind == UIElementKind::HorizontalLayout ? UIStackLayoutComponent::Axis::Horizontal : UIStackLayoutComponent::Axis::Vertical;
		layout.m_Alignment = UIStackLayoutComponent::Alignment::Center;
		break;
	}
	}

	SetSelectedEntity(entity);
	MarkHierarchyDirty();
	return entity;
}

Entity SceneHierarchyPanel::CreateUITemplate(UITemplateKind kind, Entity parent)
{
	if (!m_Context)
		return {};

	NotifySceneChange();
	Entity resolvedRootParent = parent && parent.HasComponent<UITransformComponent>() ? parent : Entity{};
	if (kind == UITemplateKind::PauseMenu && !resolvedRootParent)
		resolvedRootParent = FindOrCreateUICanvas();

	auto createUIEntity = [this, resolvedRootParent](const char* name, Entity explicitParent = {}) -> Entity
		{
			Entity entity = m_Context->CreateEntity(name);
			entity.AddComponent<UITransformComponent>();
			Entity resolvedParent = explicitParent ? explicitParent : resolvedRootParent;
			if (resolvedParent)
				SetEntityParent(entity, resolvedParent);
			return entity;
		};

	Entity root;
	if (kind == UITemplateKind::HUD)
	{
		root = createUIEntity("HUD Canvas");
		auto& rootTransform = root.GetComponent<UITransformComponent>();
		ConfigureCanvasTransform(rootTransform);
		rootTransform.m_SortOrder = parent && parent.HasComponent<UITransformComponent>() ? parent.GetComponent<UITransformComponent>().m_SortOrder + 1 : 0;
		root.AddComponent<UICanvasComponent>();

		Entity score = createUIEntity("Score Text", root);
		auto& scoreTransform = score.GetComponent<UITransformComponent>();
		scoreTransform.m_AnchorMin = { 0.0f, 1.0f };
		scoreTransform.m_AnchorMax = { 0.0f, 1.0f };
		scoreTransform.m_Pivot = { 0.0f, 1.0f };
		scoreTransform.m_AnchoredPosition = { 24.0f, -24.0f };
		scoreTransform.m_Size = { 260.0f, 44.0f };
		scoreTransform.m_SortOrder = rootTransform.m_SortOrder + 1;
		auto& scoreText = score.AddComponent<UITextComponent>();
		scoreText.m_TextString = "Score: 0";
		scoreText.m_FontSize = 28.0f;

		Entity menu = createUIEntity("Menu Button", root);
		auto& menuTransform = menu.GetComponent<UITransformComponent>();
		menuTransform.m_AnchorMin = { 1.0f, 1.0f };
		menuTransform.m_AnchorMax = { 1.0f, 1.0f };
		menuTransform.m_Pivot = { 1.0f, 1.0f };
		menuTransform.m_AnchoredPosition = { -24.0f, -24.0f };
		menuTransform.m_Size = { 150.0f, 48.0f };
		menuTransform.m_SortOrder = rootTransform.m_SortOrder + 2;
		menu.AddComponent<UIButtonComponent>().m_Text = "Menu";
	}
	else
	{
		root = createUIEntity("Pause Menu");
		auto& rootTransform = root.GetComponent<UITransformComponent>();
		rootTransform.m_Size = { 420.0f, 330.0f };
		rootTransform.m_SortOrder = parent && parent.HasComponent<UITransformComponent>() ? parent.GetComponent<UITransformComponent>().m_SortOrder + 10 : 10;
		auto& panel = root.AddComponent<UIImageComponent>();
		panel.m_Color = { 0.04f, 0.07f, 0.09f, 0.92f };

		Entity title = createUIEntity("Pause Title", root);
		auto& titleTransform = title.GetComponent<UITransformComponent>();
		titleTransform.m_AnchoredPosition = { 0.0f, 108.0f };
		titleTransform.m_Size = { 300.0f, 48.0f };
		titleTransform.m_SortOrder = rootTransform.m_SortOrder + 1;
		auto& titleText = title.AddComponent<UITextComponent>();
		titleText.m_TextString = "Paused";
		titleText.m_FontSize = 34.0f;

		constexpr std::array<std::pair<const char*, const char*>, 3> buttons =
		{
			std::pair{ "Resume Button", "Resume" },
			std::pair{ "Restart Button", "Restart" },
			std::pair{ "Quit Button", "Quit" }
		};

		for (size_t index = 0; index < buttons.size(); ++index)
		{
			Entity buttonEntity = createUIEntity(buttons[index].first, root);
			auto& buttonTransform = buttonEntity.GetComponent<UITransformComponent>();
			buttonTransform.m_AnchoredPosition = { 0.0f, 40.0f - static_cast<float>(index) * 68.0f };
			buttonTransform.m_Size = { 250.0f, 54.0f };
			buttonTransform.m_SortOrder = rootTransform.m_SortOrder + 2 + static_cast<int32_t>(index);
			buttonEntity.AddComponent<UIButtonComponent>().m_Text = buttons[index].second;
		}
	}

	SetSelectedEntity(root);
	MarkHierarchyDirty();
	return root;
}

void SceneHierarchyPanel::DrawCreateUIMenu(Entity parent)
{
	if (ImGui::BeginMenu("Create UI"))
	{
		if (ImGui::MenuItem("Canvas"))
			CreateUIElement(UIElementKind::Canvas, parent);
		ImGui::Separator();
		if (ImGui::MenuItem("Panel"))
			CreateUIElement(UIElementKind::Panel, parent);
		if (ImGui::MenuItem("Image"))
			CreateUIElement(UIElementKind::Image, parent);
		if (ImGui::MenuItem("Text"))
			CreateUIElement(UIElementKind::Text, parent);
		if (ImGui::MenuItem("Button"))
			CreateUIElement(UIElementKind::Button, parent);
		if (ImGui::MenuItem("Toggle"))
			CreateUIElement(UIElementKind::Toggle, parent);
		if (ImGui::MenuItem("Slider"))
			CreateUIElement(UIElementKind::Slider, parent);
		if (ImGui::MenuItem("Input Field"))
			CreateUIElement(UIElementKind::InputField, parent);
		ImGui::Separator();
		if (ImGui::MenuItem("Vertical Layout"))
			CreateUIElement(UIElementKind::VerticalLayout, parent);
		if (ImGui::MenuItem("Horizontal Layout"))
			CreateUIElement(UIElementKind::HorizontalLayout, parent);
		ImGui::Separator();
		if (ImGui::MenuItem("HUD Template"))
			CreateUITemplate(UITemplateKind::HUD, parent);
		if (ImGui::MenuItem("Pause Menu Template"))
			CreateUITemplate(UITemplateKind::PauseMenu, parent);
		ImGui::EndMenu();
	}
}

void SceneHierarchyPanel::DestroyEntityWithSelection(Entity entityIn)
{
	if (!entityIn)
		return;

	if (m_SelectionContext == entityIn || IsDescendantOf(m_SelectionContext, entityIn.GetUUID()))
		ClearSelection();

	m_Context->DestroyEntity(entityIn);
	MarkHierarchyDirty();
}

bool SceneHierarchyPanel::IsSelected(Entity entityIn) const
{
	if (!entityIn)
		return false;

	UUID id = entityIn.GetUUID();
	return std::ranges::find(m_SelectionContexts, id) != m_SelectionContexts.end();
}

void SceneHierarchyPanel::NotifySceneChange()
{
	if (m_SceneChangeCallback)
		m_SceneChangeCallback();
}

void SceneHierarchyPanel::BeginPropertyEditHistory()
{
	if (m_PropertyEditHistoryActive)
		return;

	NotifySceneChange();
	m_PropertyEditHistoryActive = true;
}

void SceneHierarchyPanel::TrackPropertyEditHistory()
{
	const bool editingProperty = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::IsAnyItemActive() && ImGui::GetActiveID() != 0;
	if (editingProperty && !m_PropertyEditHistoryActive)
	{
		NotifySceneChange();
		m_PropertyEditHistoryActive = true;
	}
	if (!editingProperty)
		m_PropertyEditHistoryActive = false;
}

void SceneHierarchyPanel::DrawMultiEditComponents(const std::vector<Entity>& selectedEntities)
{
	WHP_PROFILE_FUNCTION();
	DrawPropertySectionTitle("Multi Edit");
	ImGui::TextDisabled("%zu entities selected", selectedEntities.size());

	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	if (ImGui::Button("Add Component To Selection", ImVec2(-1.0f, 0.0f)))
		ImGui::OpenPopup("Add Component");
	ImGui::PopStyleColor(3);

	if (ImGui::BeginPopup("Add Component"))
	{
		DisplayAddComponentEntry<CameraComponent>("Camera");
		DisplayAddComponentEntry<ScriptComponent>("Script");
		DisplayAddComponentEntry<AnimatorComponent>("Animator");
		DisplayAddComponentEntry<SpriteRendererComponent>("Sprite Renderer");
		DisplayAddComponentEntry<CircleRendererComponent>("Circle Renderer");
		DisplayAddComponentEntry<TextComponent>("Text");
		DisplayAddComponentEntry<UITransformComponent>("UI Transform");
		DisplayAddComponentEntry<UICanvasComponent>("UI Canvas");
		DisplayAddComponentEntry<UIPanelComponent>("UI Panel");
		DisplayAddComponentEntry<UIImageComponent>("UI Image");
		DisplayAddComponentEntry<UITextComponent>("UI Text");
		DisplayAddComponentEntry<UIButtonComponent>("UI Button");
		DisplayAddComponentEntry<UIToggleComponent>("UI Toggle");
		DisplayAddComponentEntry<UISliderComponent>("UI Slider");
		DisplayAddComponentEntry<UIInputFieldComponent>("UI Input Field");
		DisplayAddComponentEntry<UIStackLayoutComponent>("UI Stack Layout");
		DisplayAddComponentEntry<Rigidbody2DComponent>("Rigidbody 2D");
		DisplayAddComponentEntry<BoxCollider2DComponent>("Box Collider 2D");
		DisplayAddComponentEntry<CircleCollider2DComponent>("Circle Collider 2D");
		DisplayAddComponentEntry<AudioComponent>("Audio");
		ImGui::EndPopup();
	}

	DrawPropertySectionTitle("Shared Transform");
	Entity primary = selectedEntities.front();
	glm::vec3 translation = primary.GetComponent<TransformComponent>().m_Translation;
	glm::vec3 rotation = glm::degrees(primary.GetComponent<TransformComponent>().m_Rotation);
	glm::vec3 scale = primary.GetComponent<TransformComponent>().m_Scale;
	bool translationMixed = false;
	bool rotationMixed = false;
	bool scaleMixed = false;
	for (Entity selected : selectedEntities)
	{
		const auto& transform = selected.GetComponent<TransformComponent>();
		translationMixed |= (transform.m_Translation != translation);
		rotationMixed |= glm::degrees(transform.m_Rotation) != rotation;
		scaleMixed |= (transform.m_Scale != scale);
	}

	if (translationMixed)
		ImGui::TextDisabled("Translation has mixed values.");
	glm::vec3 previousTranslation = translation;
	UI::DrawVec3Control("Translation", translation, 0, 100, ImGui::GetStyle().IndentSpacing);
	if (translation != previousTranslation)
	{
		BeginPropertyEditHistory();
		for (Entity selected : selectedEntities)
			selected.GetComponent<TransformComponent>().m_Translation = translation;
	}

	if (rotationMixed)
		ImGui::TextDisabled("Rotation has mixed values.");
	glm::vec3 previousRotation = rotation;
	UI::DrawVec3Control("Rotation", rotation, 0, 100, ImGui::GetStyle().IndentSpacing);
	if (rotation != previousRotation)
	{
		BeginPropertyEditHistory();
		for (Entity selected : selectedEntities)
			selected.GetComponent<TransformComponent>().m_Rotation = glm::radians(rotation);
	}

	if (scaleMixed)
		ImGui::TextDisabled("Scale has mixed values.");
	glm::vec3 previousScale = scale;
	UI::DrawVec3Control("Scale", scale, 1.0f, 100, ImGui::GetStyle().IndentSpacing);
	if (scale != previousScale)
	{
		BeginPropertyEditHistory();
		for (Entity selected : selectedEntities)
			selected.GetComponent<TransformComponent>().m_Scale = scale;
	}

	DrawMultiSharedComponents(selectedEntities);

	DrawPropertySectionTitle("Component Coverage");
	DrawMultiComponentSummary<CameraComponent>("Camera", selectedEntities.size());
	DrawMultiComponentSummary<ScriptComponent>("Script", selectedEntities.size());
	DrawMultiComponentSummary<AnimatorComponent>("Animator", selectedEntities.size());
	DrawMultiComponentSummary<SpriteRendererComponent>("Sprite Renderer", selectedEntities.size());
	DrawMultiComponentSummary<CircleRendererComponent>("Circle Renderer", selectedEntities.size());
	DrawMultiComponentSummary<TextComponent>("Text Renderer", selectedEntities.size());
	DrawMultiComponentSummary<Rigidbody2DComponent>("Rigidbody 2D", selectedEntities.size());
	DrawMultiComponentSummary<BoxCollider2DComponent>("Box Collider 2D", selectedEntities.size());
	DrawMultiComponentSummary<CircleCollider2DComponent>("Circle Collider 2D", selectedEntities.size());
	DrawMultiComponentSummary<AudioComponent>("Audio", selectedEntities.size());
}

void SceneHierarchyPanel::DrawMultiSharedComponents(const std::vector<Entity>& selectedEntities)
{
	WHP_PROFILE_FUNCTION();
	const size_t selectedCount = selectedEntities.size();
	bool drewComponent = false;

	DrawPropertySectionTitle("Shared Components");
	if (CountSelectedWithComponent<CameraComponent>() == selectedCount)
	{
		DrawMultiCameraComponent(selectedEntities);
		drewComponent = true;
	}
	if (CountSelectedWithComponent<ScriptComponent>() == selectedCount)
	{
		DrawMultiScriptComponent(selectedEntities);
		drewComponent = true;
	}
	if (CountSelectedWithComponent<SpriteRendererComponent>() == selectedCount)
	{
		DrawMultiSpriteRendererComponent(selectedEntities);
		drewComponent = true;
	}
	if (CountSelectedWithComponent<CircleRendererComponent>() == selectedCount)
	{
		DrawMultiCircleRendererComponent(selectedEntities);
		drewComponent = true;
	}
	if (CountSelectedWithComponent<TextComponent>() == selectedCount)
	{
		DrawMultiTextComponent(selectedEntities);
		drewComponent = true;
	}
	if (CountSelectedWithComponent<Rigidbody2DComponent>() == selectedCount)
	{
		DrawMultiRigidbody2DComponent(selectedEntities);
		drewComponent = true;
	}
	if (CountSelectedWithComponent<BoxCollider2DComponent>() == selectedCount)
	{
		DrawMultiBoxCollider2DComponent(selectedEntities);
		drewComponent = true;
	}
	if (CountSelectedWithComponent<CircleCollider2DComponent>() == selectedCount)
	{
		DrawMultiCircleCollider2DComponent(selectedEntities);
		drewComponent = true;
	}

	if (!drewComponent)
		ImGui::TextDisabled("No shared editable components in the current selection.");
}

void SceneHierarchyPanel::DrawMultiCameraComponent(const std::vector<Entity>& selectedEntities)
{
	WHP_PROFILE_FUNCTION();
	ImGui::PushID("MultiCamera");
	if (ImGui::TreeNodeEx("Camera", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		Entity primaryEntity = selectedEntities.front();
		CameraComponent& primaryComponent = primaryEntity.GetComponent<CameraComponent>();
		bool primary = primaryComponent.m_Primary;
		bool primaryMixed = false;
		bool fixedAspect = primaryComponent.m_FixedAspectRatio;
		bool fixedAspectMixed = false;
		SceneCamera::ProjectionType projection = primaryComponent.m_Camera.GetProjectionType();
		bool projectionMixed = false;

		for (Entity selected : selectedEntities)
		{
			const auto& component = selected.GetComponent<CameraComponent>();
			primaryMixed |= component.m_Primary != primary;
			fixedAspectMixed |= component.m_FixedAspectRatio != fixedAspect;
			projectionMixed |= component.m_Camera.GetProjectionType() != projection;
		}

		DrawMixedHint("Primary", primaryMixed);
		if (ImGui::Checkbox("Primary", &primary))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<CameraComponent>().m_Primary = primary;
		}

		const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
		const char* currentProjectionTypeString = projectionMixed ? "Mixed" : projectionTypeStrings[static_cast<int>(projection)];
		DrawMixedHint("Projection", projectionMixed);
		if (ImGui::BeginCombo("Projection", currentProjectionTypeString))
		{
			for (int i = 0; i < 2; i++)
			{
				bool isSelected = !projectionMixed && projection == static_cast<SceneCamera::ProjectionType>(i);
				if (ImGui::Selectable(projectionTypeStrings[i], isSelected))
				{
					BeginPropertyEditHistory();
					for (Entity selected : selectedEntities)
						selected.GetComponent<CameraComponent>().m_Camera.SetProjectionType(static_cast<SceneCamera::ProjectionType>(i));
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (!projectionMixed && projection == SceneCamera::ProjectionType::Perspective)
		{
			float fov = glm::degrees(primaryComponent.m_Camera.GetPerspectiveVerticalFOV());
			float nearClip = primaryComponent.m_Camera.GetPerspectiveNearClip();
			float farClip = primaryComponent.m_Camera.GetPerspectiveFarClip();
			bool fovMixed = false;
			bool nearMixed = false;
			bool farMixed = false;
			for (Entity selected : selectedEntities)
			{
				const auto& camera = selected.GetComponent<CameraComponent>().m_Camera;
				fovMixed |= !Math::EqualF(camera.GetPerspectiveVerticalFOV(), primaryComponent.m_Camera.GetPerspectiveVerticalFOV());
				nearMixed |= !Math::EqualF(camera.GetPerspectiveNearClip(), nearClip);
				farMixed |= !Math::EqualF(camera.GetPerspectiveFarClip(), farClip);
			}

			DrawMixedHint("Vertical FOV", fovMixed);
			if (ImGui::DragFloat("Vertical FOV", &fov))
			{
				BeginPropertyEditHistory();
				for (Entity selected : selectedEntities)
					selected.GetComponent<CameraComponent>().m_Camera.SetPerspectiveVerticalFOV(glm::radians(fov));
			}
			DrawMixedHint("Near", nearMixed);
			if (ImGui::DragFloat("Near", &nearClip))
			{
				BeginPropertyEditHistory();
				for (Entity selected : selectedEntities)
					selected.GetComponent<CameraComponent>().m_Camera.SetPerspectiveNearClip(nearClip);
			}
			DrawMixedHint("Far", farMixed);
			if (ImGui::DragFloat("Far", &farClip))
			{
				BeginPropertyEditHistory();
				for (Entity selected : selectedEntities)
					selected.GetComponent<CameraComponent>().m_Camera.SetPerspectiveFarClip(farClip);
			}
		}

		if (!projectionMixed && projection == SceneCamera::ProjectionType::Orthographic)
		{
			float size = primaryComponent.m_Camera.GetOrthographicSize();
			float nearClip = primaryComponent.m_Camera.GetOrthographicNearClip();
			float farClip = primaryComponent.m_Camera.GetOrthographicFarClip();
			bool sizeMixed = false;
			bool nearMixed = false;
			bool farMixed = false;
			for (Entity selected : selectedEntities)
			{
				const auto& camera = selected.GetComponent<CameraComponent>().m_Camera;
				sizeMixed |= !Math::EqualF(camera.GetOrthographicSize(), size);
				nearMixed |= !Math::EqualF(camera.GetOrthographicNearClip(), nearClip);
				farMixed |= !Math::EqualF(camera.GetOrthographicFarClip(), farClip);
			}

			DrawMixedHint("Size", sizeMixed);
			if (ImGui::DragFloat("Size", &size))
			{
				BeginPropertyEditHistory();
				for (Entity selected : selectedEntities)
					selected.GetComponent<CameraComponent>().m_Camera.SetOrthographicSize(size);
			}
			DrawMixedHint("Near", nearMixed);
			if (ImGui::DragFloat("Near", &nearClip))
			{
				BeginPropertyEditHistory();
				for (Entity selected : selectedEntities)
					selected.GetComponent<CameraComponent>().m_Camera.SetOrthographicNearClip(nearClip);
			}
			DrawMixedHint("Far", farMixed);
			if (ImGui::DragFloat("Far", &farClip))
			{
				BeginPropertyEditHistory();
				for (Entity selected : selectedEntities)
					selected.GetComponent<CameraComponent>().m_Camera.SetOrthographicFarClip(farClip);
			}

			DrawMixedHint("Fixed Aspect Ratio", fixedAspectMixed);
			if (ImGui::Checkbox("Fixed Aspect Ratio", &fixedAspect))
			{
				BeginPropertyEditHistory();
				for (Entity selected : selectedEntities)
					selected.GetComponent<CameraComponent>().m_FixedAspectRatio = fixedAspect;
			}
		}

		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void SceneHierarchyPanel::DrawMultiScriptComponent(const std::vector<Entity>& selectedEntities)
{
	WHP_PROFILE_FUNCTION();
	ImGui::PushID("MultiScript");
	if (ImGui::TreeNodeEx("Script", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		Entity primaryEntity = selectedEntities.front();
		std::string className = primaryEntity.GetComponent<ScriptComponent>().m_ClassName;
		bool classMixed = false;
		for (Entity selected : selectedEntities)
			classMixed |= selected.GetComponent<ScriptComponent>().m_ClassName != className;

		DrawScriptWorkspaceActions();
		ImGui::Separator();

		DrawMixedHint("Class", classMixed);
		const char* label = classMixed ? "Mixed" : (className.empty() ? "None" : className.c_str());
		if (ImGui::BeginCombo("Class", label))
		{
			const auto& entityClasses = ScriptEngine::GetEntityClasses();
			for (const auto& [name, ScriptClass] : entityClasses)
			{
				bool isSelected = !classMixed && className == name;
				if (ImGui::Selectable(name.c_str(), isSelected))
				{
					BeginPropertyEditHistory();
					for (Entity selected : selectedEntities)
						selected.GetComponent<ScriptComponent>().m_ClassName = name;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::TextDisabled("Script fields are edited per entity while runtime field mapping is entity-specific.");
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void SceneHierarchyPanel::DrawMultiSpriteRendererComponent(const std::vector<Entity>& selectedEntities)
{
	WHP_PROFILE_FUNCTION();
	ImGui::PushID("MultiSpriteRenderer");
	if (ImGui::TreeNodeEx("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		Entity primaryEntity = selectedEntities.front();
		SpriteRendererComponent& primary = primaryEntity.GetComponent<SpriteRendererComponent>();
		glm::vec4 color = primary.m_Color;
		AssetHandle texture = primary.m_Texture;
		int32_t textureSpriteIndex = primary.m_TextureSpriteIndex;
		float tilingFactor = primary.m_TilingFactor;
		bool colorMixed = false;
		bool textureMixed = false;
		bool textureSpriteMixed = false;
		bool tilingMixed = false;

		for (Entity selected : selectedEntities)
		{
			const auto& component = selected.GetComponent<SpriteRendererComponent>();
			colorMixed |= (component.m_Color != color);
			textureMixed |= component.m_Texture != texture;
			textureSpriteMixed |= component.m_TextureSpriteIndex != textureSpriteIndex;
			tilingMixed |= !Math::EqualF(component.m_TilingFactor, tilingFactor);
		}

		DrawMixedHint("Color", colorMixed);
		if (ImGui::ColorEdit4("Color", glm::value_ptr(color)))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<SpriteRendererComponent>().m_Color = color;
		}

		DrawMixedHint("Texture", textureMixed || textureSpriteMixed);
		std::string label = (textureMixed || textureSpriteMixed) ? "Mixed" : TextureAssetLabel(texture, textureSpriteIndex);
		const auto dragDropCallback = [this, &selectedEntities](AssetHandle handle)
			{
				BeginPropertyEditHistory();
				for (Entity selected : selectedEntities)
				{
					auto& component = selected.GetComponent<SpriteRendererComponent>();
					component.m_Texture = handle;
					component.m_TextureSpriteIndex = -1;
				}
			};
		const auto assetReferenceCallback = [this, &selectedEntities](AssetHandle handle, int32_t spriteIndex)
			{
				BeginPropertyEditHistory();
				for (Entity selected : selectedEntities)
				{
					auto& component = selected.GetComponent<SpriteRendererComponent>();
					component.m_Texture = handle;
					component.m_TextureSpriteIndex = spriteIndex;
				}
			};
		UI::DragDropTarget(AssetType::Texture2D, dragDropCallback, label.c_str(), true, glm::max<float>(100.0f, ImGui::CalcTextSize(label.c_str()).x + 20.0f), 0.0f, true, nullptr, assetReferenceCallback);
		ImGui::SameLine();
		if (ImGui::Button("Clear Texture"))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
			{
				auto& component = selected.GetComponent<SpriteRendererComponent>();
				component.m_Texture = 0;
				component.m_TextureSpriteIndex = -1;
			}
		}

		DrawMixedHint("Tiling Factor", tilingMixed);
		if (ImGui::DragFloat("Tiling Factor", &tilingFactor, 0.1f, 0.0f, 100.0f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<SpriteRendererComponent>().m_TilingFactor = tilingFactor;
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void SceneHierarchyPanel::DrawMultiCircleRendererComponent(const std::vector<Entity>& selectedEntities)
{
	WHP_PROFILE_FUNCTION();
	ImGui::PushID("MultiCircleRenderer");
	if (ImGui::TreeNodeEx("Circle Renderer", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		Entity primaryEntity = selectedEntities.front();
		CircleRendererComponent& primary = primaryEntity.GetComponent<CircleRendererComponent>();
		glm::vec4 color = primary.m_Color;
		float thickness = primary.m_Thickness;
		float fade = primary.m_Fade;
		bool colorMixed = false;
		bool thicknessMixed = false;
		bool fadeMixed = false;
		for (Entity selected : selectedEntities)
		{
			const auto& component = selected.GetComponent<CircleRendererComponent>();
			colorMixed |= (component.m_Color != color);
			thicknessMixed |= !Math::EqualF(component.m_Thickness, thickness);
			fadeMixed |= !Math::EqualF(component.m_Fade, fade);
		}

		DrawMixedHint("Color", colorMixed);
		if (ImGui::ColorEdit4("Color", glm::value_ptr(color)))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<CircleRendererComponent>().m_Color = color;
		}
		DrawMixedHint("Thickness", thicknessMixed);
		if (ImGui::DragFloat("Thickness", &thickness, 0.025f, 0.0f, 1.0f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<CircleRendererComponent>().m_Thickness = thickness;
		}
		DrawMixedHint("Fade", fadeMixed);
		if (ImGui::DragFloat("Fade", &fade, 0.00025f, 0.0f, 1.0f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<CircleRendererComponent>().m_Fade = fade;
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void SceneHierarchyPanel::DrawMultiTextComponent(const std::vector<Entity>& selectedEntities)
{
	WHP_PROFILE_FUNCTION();
	ImGui::PushID("MultiTextRenderer");
	if (ImGui::TreeNodeEx("Text Renderer", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		Entity primaryEntity = selectedEntities.front();
		TextComponent& primary = primaryEntity.GetComponent<TextComponent>();
		std::string text = primary.m_TextString;
		AssetHandle font = primary.m_Font;
		glm::vec4 color = primary.m_Color;
		float kerning = primary.m_Kerning;
		float lineSpacing = primary.m_LineSpacing;
		bool textMixed = false;
		bool fontMixed = false;
		bool colorMixed = false;
		bool kerningMixed = false;
		bool lineSpacingMixed = false;
		for (Entity selected : selectedEntities)
		{
			const auto& component = selected.GetComponent<TextComponent>();
			textMixed |= component.m_TextString != text;
			fontMixed |= component.m_Font != font;
			colorMixed |= (component.m_Color != color);
			kerningMixed |= !Math::EqualF(component.m_Kerning, kerning);
			lineSpacingMixed |= Math::EqualF(component.m_LineSpacing, lineSpacing);
		}

		DrawMixedHint("Text", textMixed);
		if (ImGui::InputTextMultiline("Text String", &text))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<TextComponent>().m_TextString = text;
		}
		DrawMixedHint("Color", colorMixed);
		if (ImGui::ColorEdit4("Color", glm::value_ptr(color)))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<TextComponent>().m_Color = color;
		}
		DrawMixedHint("Font", fontMixed);
		std::string label = fontMixed ? "Mixed" : AssetLabel(font, AssetType::Font);
		const auto dragDropCallback = [this, &selectedEntities](AssetHandle handle)
			{
				BeginPropertyEditHistory();
				for (Entity selected : selectedEntities)
					selected.GetComponent<TextComponent>().m_Font = handle;
			};
		UI::DragDropTarget(AssetType::Font, dragDropCallback, label.c_str(), true, glm::max<float>(100.0f, ImGui::CalcTextSize(label.c_str()).x + 20.0f), 0.0f);
		ImGui::SameLine();
		if (ImGui::Button("Clear Font"))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<TextComponent>().m_Font = 0;
		}
		DrawMixedHint("Kerning", kerningMixed);
		if (ImGui::DragFloat("Kerning", &kerning, 0.025f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<TextComponent>().m_Kerning = kerning;
		}
		DrawMixedHint("Line Spacing", lineSpacingMixed);
		if (ImGui::DragFloat("Line Spacing", &lineSpacing, 0.025f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<TextComponent>().m_LineSpacing = lineSpacing;
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void SceneHierarchyPanel::DrawMultiRigidbody2DComponent(const std::vector<Entity>& selectedEntities)
{
	WHP_PROFILE_FUNCTION();
	ImGui::PushID("MultiRigidbody2D");
	if (ImGui::TreeNodeEx("Rigidbody 2D", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		Entity primaryEntity = selectedEntities.front();
		Rigidbody2DComponent& primary = primaryEntity.GetComponent<Rigidbody2DComponent>();
		Rigidbody2DComponent::BodyType bodyType = primary.m_Type;
		float gravityScale = primary.m_GravityScale;
		bool fixedRotation = primary.m_FixedRotation;
		bool typeMixed = false;
		bool gravityMixed = false;
		bool fixedRotationMixed = false;
		for (Entity selected : selectedEntities)
		{
			const auto& component = selected.GetComponent<Rigidbody2DComponent>();
			typeMixed |= component.m_Type != bodyType;
			gravityMixed |= !Math::EqualF(component.m_GravityScale, gravityScale);
			fixedRotationMixed |= component.m_FixedRotation != fixedRotation;
		}

		const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
		DrawMixedHint("Body Type", typeMixed);
		if (ImGui::BeginCombo("Body Type", typeMixed ? "Mixed" : bodyTypeStrings[static_cast<int>(bodyType)]))
		{
			for (int i = 0; i < 3; i++)
			{
				bool isSelected = !typeMixed && bodyType == static_cast<Rigidbody2DComponent::BodyType>(i);
				if (ImGui::Selectable(bodyTypeStrings[i], isSelected))
				{
					BeginPropertyEditHistory();
					for (Entity selected : selectedEntities)
						selected.GetComponent<Rigidbody2DComponent>().m_Type = static_cast<Rigidbody2DComponent::BodyType>(i);
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		DrawMixedHint("Gravity Scale", gravityMixed);
		if (ImGui::DragFloat("Gravity Scale", &gravityScale, 0.01f, 0.0f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<Rigidbody2DComponent>().m_GravityScale = gravityScale;
		}
		DrawMixedHint("Fixed Rotation", fixedRotationMixed);
		if (ImGui::Checkbox("Fixed Rotation", &fixedRotation))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<Rigidbody2DComponent>().m_FixedRotation = fixedRotation;
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void SceneHierarchyPanel::DrawMultiBoxCollider2DComponent(const std::vector<Entity>& selectedEntities)
{
	WHP_PROFILE_FUNCTION();
	ImGui::PushID("MultiBoxCollider2D");
	if (ImGui::TreeNodeEx("Box Collider 2D", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		Entity primaryEntity = selectedEntities.front();
		BoxCollider2DComponent& primary = primaryEntity.GetComponent<BoxCollider2DComponent>();
		std::string tag = primary.m_Tag;
		bool sensor = primary.m_Sensor;
		glm::vec2 offset = primary.m_Offset;
		glm::vec2 size = primary.m_Size;
		float density = primary.m_Density;
		float friction = primary.m_Friction;
		float restitution = primary.m_Restitution;
		float restitutionThreshold = primary.m_RestitutionThreshold;
		bool tagMixed = false, sensorMixed = false, offsetMixed = false, sizeMixed = false;
		bool densityMixed = false, frictionMixed = false, restitutionMixed = false, thresholdMixed = false;
		for (Entity selected : selectedEntities)
		{
			const auto& component = selected.GetComponent<BoxCollider2DComponent>();
			tagMixed |= component.m_Tag != tag;
			sensorMixed |= component.m_Sensor != sensor;
			offsetMixed |= (component.m_Offset != offset);
			sizeMixed |= (component.m_Size != size);
			densityMixed |= !Math::EqualF(component.m_Density, density);
			frictionMixed |= !Math::EqualF(component.m_Friction, friction);
			restitutionMixed |= !Math::EqualF(component.m_Restitution, restitution);
			thresholdMixed |= !Math::EqualF(component.m_RestitutionThreshold, restitutionThreshold);
		}

		DrawMixedHint("Tag", tagMixed);
		if (ImGui::InputText("Tag", &tag))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<BoxCollider2DComponent>().m_Tag = tag;
		}
		DrawMixedHint("Is Sensor", sensorMixed);
		if (ImGui::Checkbox("Is Sensor", &sensor))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<BoxCollider2DComponent>().m_Sensor = sensor;
		}
		DrawMixedHint("Offset", offsetMixed);
		if (ImGui::DragFloat2("Offset", glm::value_ptr(offset)))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<BoxCollider2DComponent>().m_Offset = offset;
		}
		DrawMixedHint("Size", sizeMixed);
		if (ImGui::DragFloat2("Size", glm::value_ptr(size)))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<BoxCollider2DComponent>().m_Size = size;
		}
		DrawMixedHint("Density", densityMixed);
		if (ImGui::DragFloat("Density", &density, 0.01f, 0.0f, 1.0f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<BoxCollider2DComponent>().m_Density = density;
		}
		DrawMixedHint("Friction", frictionMixed);
		if (ImGui::DragFloat("Friction", &friction, 0.01f, 0.0f, 1.0f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<BoxCollider2DComponent>().m_Friction = friction;
		}
		DrawMixedHint("Restitution", restitutionMixed);
		if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<BoxCollider2DComponent>().m_Restitution = restitution;
		}
		DrawMixedHint("Restitution Threshold", thresholdMixed);
		if (ImGui::DragFloat("Restitution Threshold", &restitutionThreshold, 0.01f, 0.0f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<BoxCollider2DComponent>().m_RestitutionThreshold = restitutionThreshold;
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void SceneHierarchyPanel::DrawMultiCircleCollider2DComponent(const std::vector<Entity>& selectedEntities)
{
	WHP_PROFILE_FUNCTION();
	ImGui::PushID("MultiCircleCollider2D");
	if (ImGui::TreeNodeEx("Circle Collider 2D", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding))
	{
		Entity primaryEntity = selectedEntities.front();
		CircleCollider2DComponent& primary = primaryEntity.GetComponent<CircleCollider2DComponent>();
		std::string tag = primary.m_Tag;
		bool sensor = primary.m_Sensor;
		glm::vec2 offset = primary.m_Offset;
		float radius = primary.m_Radius;
		float density = primary.m_Density;
		float friction = primary.m_Friction;
		float restitution = primary.m_Restitution;
		float restitutionThreshold = primary.m_RestitutionThreshold;
		bool tagMixed = false, sensorMixed = false, offsetMixed = false, radiusMixed = false;
		bool densityMixed = false, frictionMixed = false, restitutionMixed = false, thresholdMixed = false;
		for (Entity selected : selectedEntities)
		{
			const auto& component = selected.GetComponent<CircleCollider2DComponent>();
			tagMixed |= component.m_Tag != tag;
			sensorMixed |= component.m_Sensor != sensor;
			offsetMixed |= (component.m_Offset != offset);
			radiusMixed |= !Math::EqualF(component.m_Radius, radius);
			densityMixed |= !Math::EqualF(component.m_Density, density);
			frictionMixed |= !Math::EqualF(component.m_Friction, friction);
			restitutionMixed |= !Math::EqualF(component.m_Restitution, restitution);
			thresholdMixed |= !Math::EqualF(component.m_RestitutionThreshold, restitutionThreshold);
		}

		DrawMixedHint("Tag", tagMixed);
		if (ImGui::InputText("Tag", &tag))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<CircleCollider2DComponent>().m_Tag = tag;
		}
		DrawMixedHint("Is Sensor", sensorMixed);
		if (ImGui::Checkbox("Is Sensor", &sensor))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<CircleCollider2DComponent>().m_Sensor = sensor;
		}
		DrawMixedHint("Offset", offsetMixed);
		if (ImGui::DragFloat2("Offset", glm::value_ptr(offset)))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<CircleCollider2DComponent>().m_Offset = offset;
		}
		DrawMixedHint("Radius", radiusMixed);
		if (ImGui::DragFloat("Radius", &radius))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<CircleCollider2DComponent>().m_Radius = radius;
		}
		DrawMixedHint("Density", densityMixed);
		if (ImGui::DragFloat("Density", &density, 0.01f, 0.0f, 1.0f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<CircleCollider2DComponent>().m_Density = density;
		}
		DrawMixedHint("Friction", frictionMixed);
		if (ImGui::DragFloat("Friction", &friction, 0.01f, 0.0f, 1.0f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<CircleCollider2DComponent>().m_Friction = friction;
		}
		DrawMixedHint("Restitution", restitutionMixed);
		if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<CircleCollider2DComponent>().m_Restitution = restitution;
		}
		DrawMixedHint("Restitution Threshold", thresholdMixed);
		if (ImGui::DragFloat("Restitution Threshold", &restitutionThreshold, 0.01f, 0.0f))
		{
			BeginPropertyEditHistory();
			for (Entity selected : selectedEntities)
				selected.GetComponent<CircleCollider2DComponent>().m_RestitutionThreshold = restitutionThreshold;
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	ImGui::Spacing();
}

void SceneHierarchyPanel::DrawComponents(Entity entityIn)
{
	WHP_PROFILE_FUNCTION();
	DrawPropertySectionTitle("Entity");
	if (entityIn.HasComponent<TagComponent>())
	{
		auto& tag = entityIn.GetComponent<TagComponent>().m_Tag;

		if (ImGui::BeginTable("##EntityProperties", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 96.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Name");
			ImGui::TableNextColumn();
			char buffer[256] = {};
			strncpy_s(buffer, sizeof(buffer), tag.c_str(), sizeof(buffer));
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
			{
				m_Context->m_UniqueNameManager.RemoveName(tag);
				tag = m_Context->m_UniqueNameManager.AddName(buffer);
			}

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("UUID");
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%llu", static_cast<unsigned long long>(entityIn.GetUUID()));

			if (entityIn.HasComponent<HierarchyComponent>())
			{
				auto& hierarchy = entityIn.GetComponent<HierarchyComponent>();
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Group");
				ImGui::TableNextColumn();
				ImGui::Checkbox("##IsGroup", &hierarchy.m_IsGroup);
			}

			ImGui::EndTable();
		}
	}

	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
		ImGui::OpenPopup("Add Component");
	ImGui::PopStyleColor(3);

	if (ImGui::BeginPopup("Add Component"))
	{
		DisplayAddComponentEntry<CameraComponent>("Camera");
		DisplayAddComponentEntry<ScriptComponent>("Script");
		DisplayAddComponentEntry<AnimatorComponent>("Animator");
		DisplayAddComponentEntry<SpriteRendererComponent>("Sprite Renderer");
		DisplayAddComponentEntry<CircleRendererComponent>("Circle Renderer");
		DisplayAddComponentEntry<TextComponent>("Text");
		DisplayAddComponentEntry<UITransformComponent>("UI Transform");
		DisplayAddComponentEntry<UICanvasComponent>("UI Canvas");
		DisplayAddComponentEntry<UIPanelComponent>("UI Panel");
		DisplayAddComponentEntry<UIImageComponent>("UI Image");
		DisplayAddComponentEntry<UITextComponent>("UI Text");
		DisplayAddComponentEntry<UIButtonComponent>("UI Button");
		DisplayAddComponentEntry<UIToggleComponent>("UI Toggle");
		DisplayAddComponentEntry<UISliderComponent>("UI Slider");
		DisplayAddComponentEntry<UIInputFieldComponent>("UI Input Field");
		DisplayAddComponentEntry<UIStackLayoutComponent>("UI Stack Layout");
		DisplayAddComponentEntry<Rigidbody2DComponent>("Rigidbody 2D");
		DisplayAddComponentEntry<BoxCollider2DComponent>("Box Collider 2D");
		DisplayAddComponentEntry<CircleCollider2DComponent>("Circle Collider 2D");
		DisplayAddComponentEntry<AudioComponent>("Audio");
		ImGui::EndPopup();
	}

	DrawPropertySectionTitle("Components");
	DrawComponent<PrefabComponent>("Prefab", entityIn, m_SceneChangeCallback, [this, entityIn](auto& component)
		{
			const std::string sourceLabel = AssetLabel(component.m_Source, AssetType::Entity);
			if (ImGui::BeginTable("PrefabTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 96.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Source");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(sourceLabel.c_str());

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Entity");
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%llu", static_cast<unsigned long long>(component.m_SourceEntity));

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Role");
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", component.m_Root ? "Root" : "Child");

				ImGui::EndTable();
			}

			Entity prefabRoot = FindPrefabRoot(entityIn);
			ImGui::BeginDisabled(!prefabRoot);
			if (ImGui::Button("Apply", ImVec2(92.0f, 0.0f)) && m_ApplyEntityTemplateCallback)
				m_ApplyEntityTemplateCallback(prefabRoot);
			ImGui::SameLine();
			if (ImGui::Button("Revert", ImVec2(92.0f, 0.0f)) && m_RevertEntityTemplateCallback)
				m_RevertEntityTemplateCallback(prefabRoot);
			ImGui::SameLine();
			if (ImGui::Button("Unpack", ImVec2(92.0f, 0.0f)) && m_UnpackEntityTemplateCallback)
				m_UnpackEntityTemplateCallback(prefabRoot);
			ImGui::EndDisabled();
		});
	ImGui::Spacing();
	const bool isUIEntity = entityIn.HasComponent<UITransformComponent>();
	if (isUIEntity)
	{
		ImGui::TextDisabled("Screen-space UI entity. Use UI Transform for position, size, anchors, and pivot.");
	}
	else
	{
		DrawComponent<TransformComponent>("Transform", entityIn, m_SceneChangeCallback, [](auto& component)
			{
				float spacing = ImGui::GetStyle().IndentSpacing;
				UI::DrawVec3Control("Translation", component.m_Translation, 0, 100, spacing);
				glm::vec3 rotation = glm::degrees(component.m_Rotation);
				UI::DrawVec3Control("Rotation", rotation, 0, 100, spacing);
				component.m_Rotation = glm::radians(rotation);
				UI::DrawVec3Control("Scale", component.m_Scale, 1.0f, 100, spacing);
			});
	}
	ImGui::Spacing();
	DrawComponent<CameraComponent>("Camera", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			auto& camera = component.m_Camera;

			ImGui::Checkbox("Primary", &component.m_Primary);

			const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
			const char* currentProjectionTypeString = projectionTypeStrings[static_cast<int>(camera.GetProjectionType())];
			if (ImGui::BeginCombo("Projection", currentProjectionTypeString))
			{
				for (int i = 0; i < 2; i++)
				{
					bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
					if (ImGui::Selectable(projectionTypeStrings[i], isSelected))
					{
						currentProjectionTypeString = projectionTypeStrings[i];
						camera.SetProjectionType(static_cast<SceneCamera::ProjectionType>(i));
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
			{
				float perspectiveVerticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
				if (ImGui::DragFloat("Vertical FOV", &perspectiveVerticalFov))
					camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));

				float perspectiveNear = camera.GetPerspectiveNearClip();
				if (ImGui::DragFloat("Near", &perspectiveNear))
					camera.SetPerspectiveNearClip(perspectiveNear);

				float perspectiveFar = camera.GetPerspectiveFarClip();
				if (ImGui::DragFloat("Far", &perspectiveFar))
					camera.SetPerspectiveFarClip(perspectiveFar);
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
			{
				float orthoSize = camera.GetOrthographicSize();
				if (ImGui::DragFloat("Size", &orthoSize))
					camera.SetOrthographicSize(orthoSize);

				float orthoNear = camera.GetOrthographicNearClip();
				if (ImGui::DragFloat("Near", &orthoNear))
					camera.SetOrthographicNearClip(orthoNear);

				float orthoFar = camera.GetOrthographicFarClip();
				if (ImGui::DragFloat("Far", &orthoFar))
					camera.SetOrthographicFarClip(orthoFar);

				ImGui::Checkbox("Fixed Aspect Ratio", &component.m_FixedAspectRatio);
			}
		});
	ImGui::Spacing();
	DrawComponent<ScriptComponent>("Script", entityIn, m_SceneChangeCallback, [entityIn, sceneIn = m_Context](auto& component) mutable
		{
			bool scriptClassExists = ScriptEngine::EntityClassExists(component.m_ClassName);
			const auto& entityClasses = ScriptEngine::GetEntityClasses();
			if (ImGui::BeginTable("ScriptTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			{
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Workspace");
					ImGui::TableNextColumn();
					DrawScriptWorkspaceActions();
				}

				{
					UI::ScopedStyleColor scopeColor(ImGuiCol_Text, ImVec4(0.8f, 0.3f, 0.3f, 1.0f), !scriptClassExists);
					BEGIN_COMPONENT_TABLE_ROW("Class");
					if (ImGui::BeginCombo("Class", component.m_ClassName.c_str()))
					{
						for (const auto& [first, second] : entityClasses)
						{
							bool isSelected = component.m_ClassName == first;

							if (ImGui::Selectable(first.c_str(), isSelected))
							{
								component.m_ClassName = first.c_str();
							}

							if (isSelected)
								ImGui::SetItemDefaultFocus();
						}

						ImGui::EndCombo();
					}
					END_COMPONENT_TABLE_ROW();
				}
				ImGui::Separator();

				if (bool sceneRunning = sceneIn->IsRunning(); sceneRunning)
				{
					if (Ref<ScriptInstance> scriptInstance = ScriptEngine::GetEntityScriptInstance(entityIn.GetUUID()); scriptInstance)
					{
						const auto& fields = scriptInstance->GetScriptClass()->GetFields();
						for (const auto& [name, field] : fields)
						{
							UI::DrawFieldByType(UI::ScriptFieldDraw::WhileSceneRunning, field, entityIn, component.m_ClassName, true);
						}
					}
				}
				else
				{
					if (scriptClassExists)
					{
						Ref<ScriptClass> entityClass = ScriptEngine::GetEntityClass(component.m_ClassName);
						const auto& fields = entityClass->GetFields();

						auto& entityFields = ScriptEngine::GetScriptFieldMap(entityIn);
						for (const auto& [name, field] : fields)
						{
							// Field has been set in editor
							if (entityFields.contains(name))
							{
								UI::DrawFieldByType(UI::ScriptFieldDraw::SetInTheEditor, field, entityIn, component.m_ClassName, true);
							}
							else
							{
								UI::DrawFieldByType(UI::ScriptFieldDraw::WithBaseValue, field, entityIn, component.m_ClassName, true);
							}
						}
					}
				}
			ImGui::EndTable();
			}
		});
	ImGui::Spacing();
	DrawComponent<AnimatorComponent>("Animator", entityIn, m_SceneChangeCallback, [entityIn, sceneIn = m_Context](auto& component) mutable
		{
			bool isControllerValid = false;
			Ref<AnimationController> controller = nullptr;
			std::string controllerLabel = AssetLabel(component.m_Controller, AssetType::AnimationController);
			if (component.m_Controller != 0 && AssetManager::IsAssetHandleValid(component.m_Controller) && AssetManager::GetAssetType(component.m_Controller) == AssetType::AnimationController)
			{
				isControllerValid = true;
				controller = AssetManager::GetAsset<AnimationController>(component.m_Controller);
			}

			const auto dragDropCallback = [&component](AssetHandle handle)
				{
					component.m_Controller = handle;
				};

			if (ImGui::BeginTable("AnimatorTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			{
				BEGIN_COMPONENT_TABLE_ROW("Controller");
				const float controllerButtonWidth = std::max(140.0f, ImGui::GetContentRegionAvail().x - 36.0f);
				UI::DragDropTarget(AssetType::AnimationController, dragDropCallback, controllerLabel.c_str(), true, controllerButtonWidth, 0.0f);
				if (isControllerValid)
				{
					ImGui::SameLine();
					if (ImGui::Button("X##ClearAnimatorController"))
					{
						component.m_Controller = 0;
						component.m_InitialState.clear();
					}
				}
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Initial State");
				if (controller)
				{
					const std::string preview = component.m_InitialState.empty() ? "Default" : component.m_InitialState;
					if (ImGui::BeginCombo("##AnimatorInitialState", preview.c_str()))
					{
						if (ImGui::Selectable("Default", component.m_InitialState.empty()))
							component.m_InitialState.clear();

						for (const AnimationControllerState& state : controller->GetStates())
						{
							const bool selected = component.m_InitialState == state.m_Name;
							if (ImGui::Selectable(state.m_Name.c_str(), selected))
								component.m_InitialState = state.m_Name;
							if (selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
				else
				{
					ImGui::InputText("##AnimatorInitialStateText", &component.m_InitialState);
				}
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Play On Start");
				ImGui::Checkbox("##AnimatorPlayOnStart", &component.m_PlayOnStart);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Speed");
				ImGui::DragFloat("##AnimatorSpeed", &component.m_Speed, 0.01f, 0.0f, 20.0f, "%.2f");
				END_COMPONENT_TABLE_ROW();

				if (controller)
				{
					BEGIN_COMPONENT_TABLE_ROW("Default");
					ImGui::TextDisabled("%s", controller->GetDefaultState().empty() ? "None" : controller->GetDefaultState().c_str());
					END_COMPONENT_TABLE_ROW();
				}

				if (sceneIn && sceneIn->IsRunning())
				{
					const auto runtimeIt = sceneIn->m_AnimatorRuntimes.find(entityIn.GetUUID());
					BEGIN_COMPONENT_TABLE_ROW("Runtime");
					if (runtimeIt != sceneIn->m_AnimatorRuntimes.end())
					{
						if (runtimeIt->second.IsTransitioning())
						{
							ImGui::TextDisabled(
								"%s -> %s  %.0f%%",
								runtimeIt->second.GetCurrentStateName().c_str(),
								runtimeIt->second.GetTransitionTargetStateName().c_str(),
								runtimeIt->second.GetTransitionProgress() * 100.0f);
						}
						else
							ImGui::TextDisabled("%s  %.2fs", runtimeIt->second.GetCurrentStateName().c_str(), runtimeIt->second.GetStateTime());
					}
					else
						ImGui::TextDisabled("Not playing");
					END_COMPONENT_TABLE_ROW();

					if (runtimeIt != sceneIn->m_AnimatorRuntimes.end())
					{
						BEGIN_COMPONENT_TABLE_ROW("Last Transition");
						if (!runtimeIt->second.GetLastTransitionTargetName().empty())
						{
							ImGui::TextDisabled(
								"%s -> %s  %.2fs ago",
								runtimeIt->second.GetLastTransitionSourceName().c_str(),
								runtimeIt->second.GetLastTransitionTargetName().c_str(),
								runtimeIt->second.GetTransitionDebugTime());
						}
						else
							ImGui::TextDisabled("None");
						END_COMPONENT_TABLE_ROW();

						BEGIN_COMPONENT_TABLE_ROW("Parameters");
						if (ImGui::TreeNode("Live Values"))
						{
							for (const auto& [name, value] : runtimeIt->second.GetBoolParameters())
								ImGui::TextDisabled("%s = %s", name.c_str(), value ? "true" : "false");
							for (const auto& [name, value] : runtimeIt->second.GetIntParameters())
								ImGui::TextDisabled("%s = %d", name.c_str(), value);
							for (const auto& [name, value] : runtimeIt->second.GetFloatParameters())
								ImGui::TextDisabled("%s = %.3f", name.c_str(), value);
							for (const auto& name : runtimeIt->second.GetTriggerParameters())
								ImGui::TextDisabled("%s = trigger", name.c_str());
							ImGui::TreePop();
						}
						END_COMPONENT_TABLE_ROW();
					}
				}

				ImGui::EndTable();
			}
		});
	ImGui::Spacing();
	DrawComponent<SpriteRendererComponent>("Sprite Renderer", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.m_Color));
			std::string label = "None";
			bool isTextureValid = false;
			if (component.m_Texture != 0)
			{
				if (AssetManager::IsAssetHandleValid(component.m_Texture) && AssetManager::GetAssetType(component.m_Texture) == AssetType::Texture2D)
				{
					label = TextureAssetLabel(component.m_Texture, component.m_TextureSpriteIndex);
					isTextureValid = true;
				}
				else
				{
					label = "Invalid";
				}
			}

			ImVec2 buttonLabelSize = ImGui::CalcTextSize(label.c_str());
			buttonLabelSize.x += 20.0f;
			float buttonLabelWidth = glm::max<float>(100.0f, buttonLabelSize.x);

			const auto dragDropCallback = [&component](AssetHandle handle)
				{
					component.m_Texture = handle;
					component.m_TextureSpriteIndex = -1;
				};
			const auto assetReferenceCallback = [&component](AssetHandle handle, int32_t spriteIndex)
				{
					component.m_Texture = handle;
					component.m_TextureSpriteIndex = spriteIndex;
				};

			UI::DragDropTarget(AssetType::Texture2D, dragDropCallback, label.c_str(), true, buttonLabelWidth, 0.0f, true, nullptr, assetReferenceCallback);

			if (isTextureValid)
			{
				ImGui::SameLine();
				ImVec2 xLabelSize = ImGui::CalcTextSize("X");
				float buttonSize = xLabelSize.y + ImGui::GetStyle().FramePadding.y * 2.0f;
				if (ImGui::Button("X", ImVec2(buttonSize, buttonSize)))
				{
					component.m_Texture = 0;
					component.m_TextureSpriteIndex = -1;
				}
			}

			ImGui::SameLine();
			ImGui::Text("Texture");

			if (isTextureValid)
			{
				const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(component.m_Texture);
				const auto& sprites = metadata.m_TextureSettings.m_Sprites;
				if (!sprites.empty())
				{
					const bool validSpriteIndex = component.m_TextureSpriteIndex >= 0 && component.m_TextureSpriteIndex < static_cast<int32_t>(sprites.size());
					const char* preview = validSpriteIndex ? sprites[static_cast<size_t>(component.m_TextureSpriteIndex)].m_Name.c_str() : "Full Texture";
					if (ImGui::BeginCombo("Sprite", preview))
					{
						if (ImGui::Selectable("Full Texture", component.m_TextureSpriteIndex < 0))
							component.m_TextureSpriteIndex = -1;
						for (int32_t spriteIndex = 0; std::cmp_less(spriteIndex, sprites.size()); ++spriteIndex)
						{
							const bool selected = component.m_TextureSpriteIndex == spriteIndex;
							if (ImGui::Selectable(sprites[static_cast<size_t>(spriteIndex)].m_Name.c_str(), selected))
								component.m_TextureSpriteIndex = spriteIndex;
							if (selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
			}

			ImGui::DragFloat("Tiling Factor", &component.m_TilingFactor, 0.1f, 0.0f, 100.0f);
		});
	ImGui::Spacing();
	DrawComponent<CircleRendererComponent>("Circle Renderer", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.m_Color));
			ImGui::DragFloat("Thickness", &component.m_Thickness, 0.025f, 0.0f, 1.0f);
			ImGui::DragFloat("Fade", &component.m_Fade, 0.00025f, 0.0f, 1.0f);
		});
	ImGui::Spacing();
	DrawComponent<TextComponent>("Text Renderer", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			ImGui::InputTextMultiline("Text String", &component.m_TextString);
			ImGui::ColorEdit4("Color", glm::value_ptr(component.m_Color));
			ImGui::DragFloat("Kerning", &component.m_Kerning, 0.025f);
			ImGui::DragFloat("Line Spacing", &component.m_LineSpacing, 0.025f);

			std::string label = "None";
			bool isFontValid = false;
			if (component.m_Font != 0)
			{
				if (AssetManager::IsAssetHandleValid(component.m_Font) && AssetManager::GetAssetType(component.m_Font) == AssetType::Font)
				{
					const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(component.m_Font);
					label = metadata.m_Filepath.filename().string();
					isFontValid = true;
				}
				else
				{
					label = "Invalid";
				}
			}

			ImVec2 buttonLabelSize = ImGui::CalcTextSize(label.c_str());
			buttonLabelSize.x += 20.0f;
			float buttonLabelWidth = glm::max<float>(100.0f, buttonLabelSize.x);

			const auto dragDropCallback = [&component](AssetHandle handle)
				{
					component.m_Font = handle;
				};

			UI::DragDropTarget(AssetType::Font, dragDropCallback, label.c_str(), true, buttonLabelWidth, 0.0f);

			if (isFontValid)
			{
				ImGui::SameLine();
				ImVec2 xLabelSize = ImGui::CalcTextSize("X");
				float buttonSize = xLabelSize.y + ImGui::GetStyle().FramePadding.y * 2.0f;
				if (ImGui::Button("X", ImVec2(buttonSize, buttonSize)))
				{
					component.m_Font = 0;
				}
			}

			ImGui::SameLine();
			ImGui::Text("Font");
		});
	ImGui::Spacing();
	DrawComponent<UICanvasComponent>("UI Canvas", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			if (ImGui::BeginTable("UICanvasTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			{
				BEGIN_COMPONENT_TABLE_ROW("Runtime Visible");
				ImGui::Checkbox("##UICanvasVisible", &component.m_Visible);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Editor Visible");
				ImGui::Checkbox("##UICanvasShowInEditor", &component.m_ShowInEditor);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Safe Area");
				ImGui::Checkbox("##UICanvasSafeArea", &component.m_ShowSafeAreaInEditor);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Scale Mode");
				const char* scaleModeLabels[] = { "Constant Pixel Size", "Scale With Screen Size" };
				int scaleMode = static_cast<int>(component.m_ScaleMode);
				if (ImGui::Combo("##UICanvasScaleMode", &scaleMode, scaleModeLabels, IM_ARRAYSIZE(scaleModeLabels)))
					component.m_ScaleMode = static_cast<UICanvasComponent::ScaleMode>(scaleMode);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Reference");
				ImGui::DragFloat2("##UICanvasReferenceResolution", glm::value_ptr(component.m_ReferenceResolution), 1.0f, 1.0f, 8192.0f);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Match W/H");
				ImGui::SliderFloat("##UICanvasMatch", &component.m_MatchWidthOrHeight, 0.0f, 1.0f, "%.2f");
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Scale Factor");
				ImGui::DragFloat("##UICanvasScaleFactor", &component.m_ScaleFactor, 0.01f, 0.01f, 16.0f, "%.2f");
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Safe Insets");
				ImGui::DragFloat4("##UICanvasSafeInsets", glm::value_ptr(component.m_SafeAreaInsets), 0.005f, 0.0f, 0.45f, "%.3f");
				END_COMPONENT_TABLE_ROW();

				ImGui::EndTable();
			}
		});
	ImGui::Spacing();
	DrawComponent<UITransformComponent>("UI Transform", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			if (ImGui::BeginTable("UITransformTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			{
				BEGIN_COMPONENT_TABLE_ROW("Visible");
				ImGui::Checkbox("##UIVisible", &component.m_Visible);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Presets");
				auto applyAnchorPreset = [&component](const glm::vec2& anchorMin, const glm::vec2& anchorMax, const glm::vec2& pivot, const glm::vec2& size)
					{
						component.m_AnchorMin = anchorMin;
						component.m_AnchorMax = anchorMax;
						component.m_Pivot = pivot;
						component.m_AnchoredPosition = { 0.0f, 0.0f };
						component.m_Size = size;
					};
				if (ImGui::SmallButton("Center"))
					applyAnchorPreset({ 0.5f, 0.5f }, { 0.5f, 0.5f }, { 0.5f, 0.5f }, component.m_Size);
				ImGui::SameLine();
				if (ImGui::SmallButton("Top Left"))
					applyAnchorPreset({ 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 1.0f }, component.m_Size);
				ImGui::SameLine();
				if (ImGui::SmallButton("Top Right"))
					applyAnchorPreset({ 1.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f }, component.m_Size);
				ImGui::SameLine();
				if (ImGui::SmallButton("Stretch"))
					applyAnchorPreset({ 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, 0.0f });
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Anchor Min");
				ImGui::DragFloat2("##UIAnchorMin", glm::value_ptr(component.m_AnchorMin), 0.01f, 0.0f, 1.0f);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Anchor Max");
				ImGui::DragFloat2("##UIAnchorMax", glm::value_ptr(component.m_AnchorMax), 0.01f, 0.0f, 1.0f);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Pivot");
				ImGui::DragFloat2("##UIPivot", glm::value_ptr(component.m_Pivot), 0.01f, 0.0f, 1.0f);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Position");
				ImGui::DragFloat2("##UIAnchoredPosition", glm::value_ptr(component.m_AnchoredPosition), 1.0f);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Size");
				ImGui::DragFloat2("##UISize", glm::value_ptr(component.m_Size), 1.0f, 1.0f, 8192.0f);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Scale");
				ImGui::DragFloat2("##UIScale", glm::value_ptr(component.m_Scale), 0.01f, 0.01f, 100.0f);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Rotation");
				ImGui::DragFloat("##UIRotation", &component.m_Rotation, 0.25f, -360.0f, 360.0f);
				END_COMPONENT_TABLE_ROW();

				BEGIN_COMPONENT_TABLE_ROW("Sort Order");
				ImGui::InputInt("##UISortOrder", &component.m_SortOrder);
				END_COMPONENT_TABLE_ROW();

				ImGui::EndTable();
			}
		});
	ImGui::Spacing();
	DrawComponent<UIPanelComponent>("UI Panel", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.m_Color));
			ImGui::DragFloat("Radius", &component.m_Radius, 0.25f, 0.0f, 512.0f);
			ImGui::DragFloat("Border Thickness", &component.m_BorderThickness, 0.25f, 0.0f, 128.0f);
			ImGui::ColorEdit4("Border Color", glm::value_ptr(component.m_BorderColor));
			ImGui::Checkbox("Raycast Target", &component.m_RaycastTarget);
		});
	ImGui::Spacing();
	DrawComponent<UIImageComponent>("UI Image", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.m_Color));
			ImGui::Checkbox("Raycast Target", &component.m_RaycastTarget);

			std::string label = "None";
			bool isTextureValid = false;
			if (component.m_Texture != 0)
			{
				if (AssetManager::IsAssetHandleValid(component.m_Texture) && AssetManager::GetAssetType(component.m_Texture) == AssetType::Texture2D)
				{
					label = TextureAssetLabel(component.m_Texture, component.m_TextureSpriteIndex);
					isTextureValid = true;
				}
				else
					label = "Invalid";
			}

			const float buttonLabelWidth = glm::max<float>(100.0f, ImGui::CalcTextSize(label.c_str()).x + 20.0f);
			const auto dragDropCallback = [&component](AssetHandle handle)
				{
					component.m_Texture = handle;
					component.m_TextureSpriteIndex = -1;
				};
			const auto assetReferenceCallback = [&component](AssetHandle handle, int32_t spriteIndex)
				{
					component.m_Texture = handle;
					component.m_TextureSpriteIndex = spriteIndex;
				};

			UI::DragDropTarget(AssetType::Texture2D, dragDropCallback, label.c_str(), true, buttonLabelWidth, 0.0f, true, nullptr, assetReferenceCallback);
			if (isTextureValid)
			{
				ImGui::SameLine();
				if (ImGui::Button("X##ClearUIImageTexture"))
				{
					component.m_Texture = 0;
					component.m_TextureSpriteIndex = -1;
				}
			}
			ImGui::SameLine();
			ImGui::Text("Texture");

			if (isTextureValid)
			{
				const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(component.m_Texture);
				const auto& sprites = metadata.m_TextureSettings.m_Sprites;
				if (!sprites.empty())
				{
					const bool validSpriteIndex = component.m_TextureSpriteIndex >= 0 && component.m_TextureSpriteIndex < static_cast<int32_t>(sprites.size());
					const char* preview = validSpriteIndex ? sprites[static_cast<size_t>(component.m_TextureSpriteIndex)].m_Name.c_str() : "Full Texture";
					if (ImGui::BeginCombo("Sprite", preview))
					{
						if (ImGui::Selectable("Full Texture", component.m_TextureSpriteIndex < 0))
							component.m_TextureSpriteIndex = -1;
						for (int32_t spriteIndex = 0; std::cmp_less(spriteIndex, sprites.size()); ++spriteIndex)
						{
							const bool selected = component.m_TextureSpriteIndex == spriteIndex;
							if (ImGui::Selectable(sprites[static_cast<size_t>(spriteIndex)].m_Name.c_str(), selected))
								component.m_TextureSpriteIndex = spriteIndex;
							if (selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
			}
		});
	ImGui::Spacing();
	DrawComponent<UITextComponent>("UI Text", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			ImGui::InputTextMultiline("Text", &component.m_TextString);
			ImGui::ColorEdit4("Color", glm::value_ptr(component.m_Color));
			ImGui::DragFloat("Font Size", &component.m_FontSize, 1.0f, 1.0f, 256.0f);
			ImGui::DragFloat("Kerning", &component.m_Kerning, 0.025f);
			ImGui::DragFloat("Line Spacing", &component.m_LineSpacing, 0.025f);
			DrawUITextAlignmentControls(component.m_HorizontalAlignment, component.m_VerticalAlignment);

			std::string label = "None";
			bool isFontValid = false;
			if (component.m_Font != 0)
			{
				if (AssetManager::IsAssetHandleValid(component.m_Font) && AssetManager::GetAssetType(component.m_Font) == AssetType::Font)
				{
					const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(component.m_Font);
					label = metadata.m_Filepath.filename().string();
					isFontValid = true;
				}
				else
					label = "Invalid";
			}

			const float buttonLabelWidth = glm::max<float>(100.0f, ImGui::CalcTextSize(label.c_str()).x + 20.0f);
			UI::DragDropTarget(AssetType::Font, [&component](AssetHandle handle) { component.m_Font = handle; }, label.c_str(), true, buttonLabelWidth, 0.0f);
			if (isFontValid)
			{
				ImGui::SameLine();
				if (ImGui::Button("X##ClearUITextFont"))
					component.m_Font = 0;
			}
			ImGui::SameLine();
			ImGui::Text("Font");
		});
	ImGui::Spacing();
	DrawComponent<UIButtonComponent>("UI Button", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			ImGui::InputText("Text", &component.m_Text);
			ImGui::Checkbox("Interactable", &component.m_Interactable);
			ImGui::Checkbox("Raycast Target", &component.m_RaycastTarget);
			ImGui::Checkbox("Keyboard Navigation", &component.m_NavigationEnabled);
			ImGui::DragFloat("Font Size", &component.m_FontSize, 1.0f, 1.0f, 256.0f);
			ImGui::DragFloat("Radius", &component.m_Radius, 0.25f, 0.0f, 512.0f);
			ImGui::DragFloat("Border Thickness", &component.m_BorderThickness, 0.25f, 0.0f, 128.0f);
			DrawUITextAlignmentControls(component.m_TextHorizontalAlignment, component.m_TextVerticalAlignment);
			ImGui::ColorEdit4("Normal", glm::value_ptr(component.m_NormalColor));
			ImGui::ColorEdit4("Hovered", glm::value_ptr(component.m_HoveredColor));
			ImGui::ColorEdit4("Pressed", glm::value_ptr(component.m_PressedColor));
			ImGui::ColorEdit4("Disabled", glm::value_ptr(component.m_DisabledColor));
			ImGui::ColorEdit4("Focus", glm::value_ptr(component.m_FocusColor));
			ImGui::ColorEdit4("Border", glm::value_ptr(component.m_BorderColor));
			ImGui::ColorEdit4("Text Color", glm::value_ptr(component.m_TextColor));
			ImGui::InputText("On Click", &component.m_OnClickCallback);
			DrawUIPointerEventControls(component.m_PointerEvents);

			std::string label = "None";
			bool isFontValid = false;
			if (component.m_Font != 0)
			{
				if (AssetManager::IsAssetHandleValid(component.m_Font) && AssetManager::GetAssetType(component.m_Font) == AssetType::Font)
				{
					const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(component.m_Font);
					label = metadata.m_Filepath.filename().string();
					isFontValid = true;
				}
				else
					label = "Invalid";
			}

			const float buttonLabelWidth = glm::max<float>(100.0f, ImGui::CalcTextSize(label.c_str()).x + 20.0f);
			UI::DragDropTarget(AssetType::Font, [&component](AssetHandle handle) { component.m_Font = handle; }, label.c_str(), true, buttonLabelWidth, 0.0f);
			if (isFontValid)
			{
				ImGui::SameLine();
				if (ImGui::Button("X##ClearUIButtonFont"))
					component.m_Font = 0;
			}
			ImGui::SameLine();
			ImGui::Text("Font");

			ImGui::Separator();
			ImGui::TextDisabled("Runtime");
			ImGui::BeginDisabled();
			ImGui::Checkbox("Hovered", &component.m_Hovered);
			ImGui::Checkbox("Pressed", &component.m_Pressed);
			ImGui::Checkbox("Focused", &component.m_Focused);
			ImGui::Checkbox("Clicked This Frame", &component.m_ClickedThisFrame);
			ImGui::Checkbox("Submitted This Frame", &component.m_SubmittedThisFrame);
			ImGui::EndDisabled();
		});
	ImGui::Spacing();
	DrawComponent<UIToggleComponent>("UI Toggle", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			ImGui::InputText("Label", &component.m_Label);
			ImGui::Checkbox("Checked", &component.m_Checked);
			ImGui::Checkbox("Interactable", &component.m_Interactable);
			ImGui::Checkbox("Raycast Target", &component.m_RaycastTarget);
			ImGui::Checkbox("Keyboard Navigation", &component.m_NavigationEnabled);
			ImGui::DragFloat("Font Size", &component.m_FontSize, 1.0f, 1.0f, 256.0f);
			ImGui::DragFloat("Box Radius", &component.m_BoxRadius, 0.25f, 0.0f, 256.0f);
			ImGui::InputText("On Value Changed", &component.m_OnValueChangedCallback);
			ImGui::ColorEdit4("Box", glm::value_ptr(component.m_BoxColor));
			ImGui::ColorEdit4("Check", glm::value_ptr(component.m_CheckColor));
			ImGui::ColorEdit4("Hovered", glm::value_ptr(component.m_HoveredColor));
			ImGui::ColorEdit4("Text Color", glm::value_ptr(component.m_TextColor));
			DrawUIPointerEventControls(component.m_PointerEvents);
			ImGui::Separator();
			ImGui::TextDisabled("Runtime");
			ImGui::BeginDisabled();
			ImGui::Checkbox("Hovered", &component.m_Hovered);
			ImGui::Checkbox("Pressed", &component.m_Pressed);
			ImGui::Checkbox("Focused", &component.m_Focused);
			ImGui::Checkbox("Changed This Frame", &component.m_ChangedThisFrame);
			ImGui::EndDisabled();
		});
	ImGui::Spacing();
	DrawComponent<UISliderComponent>("UI Slider", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			ImGui::DragFloat("Value", &component.m_Value, 0.01f, std::min(component.m_MinValue, component.m_MaxValue), std::max(component.m_MinValue, component.m_MaxValue));
			ImGui::DragFloat("Min", &component.m_MinValue, 0.01f);
			ImGui::DragFloat("Max", &component.m_MaxValue, 0.01f);
			ImGui::Checkbox("Interactable", &component.m_Interactable);
			ImGui::Checkbox("Raycast Target", &component.m_RaycastTarget);
			ImGui::InputText("On Value Changed", &component.m_OnValueChangedCallback);
			ImGui::ColorEdit4("Background", glm::value_ptr(component.m_BackgroundColor));
			ImGui::ColorEdit4("Fill", glm::value_ptr(component.m_FillColor));
			ImGui::ColorEdit4("Handle", glm::value_ptr(component.m_HandleColor));
			ImGui::DragFloat("Track Radius", &component.m_TrackRadius, 0.25f, 0.0f, 256.0f);
			ImGui::DragFloat("Handle Radius", &component.m_HandleRadius, 0.25f, 0.0f, 256.0f);
			DrawUIPointerEventControls(component.m_PointerEvents);
			ImGui::Separator();
			ImGui::TextDisabled("Runtime");
			ImGui::BeginDisabled();
			ImGui::Checkbox("Hovered", &component.m_Hovered);
			ImGui::Checkbox("Pressed", &component.m_Pressed);
			ImGui::Checkbox("Focused", &component.m_Focused);
			ImGui::Checkbox("Changed This Frame", &component.m_ChangedThisFrame);
			ImGui::EndDisabled();
		});
	ImGui::Spacing();
	DrawComponent<UIInputFieldComponent>("UI Input Field", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			ImGui::InputText("Text", &component.m_Text);
			ImGui::InputText("Placeholder", &component.m_Placeholder);
			ImGui::DragFloat("Font Size", &component.m_FontSize, 1.0f, 1.0f, 256.0f);
			ImGui::InputInt("Max Characters", &component.m_MaxCharacters);
			component.m_MaxCharacters = std::max(component.m_MaxCharacters, 0);
			ImGui::Checkbox("Interactable", &component.m_Interactable);
			ImGui::Checkbox("Raycast Target", &component.m_RaycastTarget);
			ImGui::InputText("On Submit", &component.m_OnSubmitCallback);
			ImGui::InputText("On Value Changed", &component.m_OnValueChangedCallback);
			ImGui::ColorEdit4("Background", glm::value_ptr(component.m_BackgroundColor));
			ImGui::ColorEdit4("Focused", glm::value_ptr(component.m_FocusedColor));
			ImGui::ColorEdit4("Text Color", glm::value_ptr(component.m_TextColor));
			ImGui::ColorEdit4("Placeholder", glm::value_ptr(component.m_PlaceholderColor));
			ImGui::ColorEdit4("Selection", glm::value_ptr(component.m_SelectionColor));
			ImGui::ColorEdit4("Caret", glm::value_ptr(component.m_CaretColor));
			ImGui::DragFloat("Radius", &component.m_Radius, 0.25f, 0.0f, 512.0f);
			ImGui::DragFloat("Border Thickness", &component.m_BorderThickness, 0.25f, 0.0f, 128.0f);
			ImGui::ColorEdit4("Border", glm::value_ptr(component.m_BorderColor));
			DrawUIPointerEventControls(component.m_PointerEvents);
			ImGui::Separator();
			ImGui::TextDisabled("Runtime");
			ImGui::BeginDisabled();
			ImGui::Checkbox("Hovered", &component.m_Hovered);
			ImGui::Checkbox("Focused", &component.m_Focused);
			ImGui::Checkbox("Changed This Frame", &component.m_ChangedThisFrame);
			ImGui::Checkbox("Submitted This Frame", &component.m_SubmittedThisFrame);
			ImGui::InputInt("Caret Index", &component.m_CaretIndex);
			ImGui::InputInt("Selection Anchor", &component.m_SelectionAnchor);
			ImGui::Checkbox("Selecting", &component.m_Selecting);
			ImGui::EndDisabled();
		});
	ImGui::Spacing();
	DrawComponent<UIStackLayoutComponent>("UI Stack Layout", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			const char* axisLabels[] = { "Horizontal", "Vertical" };
			int axis = static_cast<int>(component.m_Axis);
			if (ImGui::Combo("Axis", &axis, axisLabels, IM_ARRAYSIZE(axisLabels)))
				component.m_Axis = static_cast<UIStackLayoutComponent::Axis>(axis);

			const char* alignmentLabels[] = { "Start", "Center", "End", "Stretch" };
			int alignment = static_cast<int>(component.m_Alignment);
			if (ImGui::Combo("Alignment", &alignment, alignmentLabels, IM_ARRAYSIZE(alignmentLabels)))
				component.m_Alignment = static_cast<UIStackLayoutComponent::Alignment>(alignment);

			ImGui::DragFloat4("Padding L/T/R/B", glm::value_ptr(component.m_Padding), 1.0f, 0.0f, 4096.0f);
			ImGui::DragFloat("Spacing", &component.m_Spacing, 1.0f, 0.0f, 4096.0f);
			ImGui::DragFloat2("Child Size", glm::value_ptr(component.m_ChildSize), 1.0f, 1.0f, 4096.0f);
			ImGui::Checkbox("Control Child Width", &component.m_ControlChildWidth);
			ImGui::Checkbox("Control Child Height", &component.m_ControlChildHeight);
			ImGui::Checkbox("Reverse", &component.m_Reverse);
		});
	ImGui::Spacing();
	DrawComponent<Rigidbody2DComponent>("Rigidbody 2D", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
			const char* currentBodyTypeString = bodyTypeStrings[static_cast<int>(component.m_Type)];
			if (ImGui::BeginCombo("Body Type", currentBodyTypeString))
			{
				for (int i = 0; i < 3; i++)
				{
					bool isSelected = currentBodyTypeString == bodyTypeStrings[i];
					if (ImGui::Selectable(bodyTypeStrings[i], isSelected))
					{
						currentBodyTypeString = bodyTypeStrings[i];
						component.m_Type = static_cast<Rigidbody2DComponent::BodyType>(i);
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			ImGui::DragFloat("Gravity Scale", &component.m_GravityScale, 0.01f, 0.0f);
			ImGui::Checkbox("Fixed Rotation", &component.m_FixedRotation);
		});
	ImGui::Spacing();
	DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			char buffer[256] = {};
			strncpy_s(buffer, sizeof(buffer), component.m_Tag.c_str(), sizeof(buffer));
			if (ImGui::InputText("Tag", buffer, sizeof(buffer)))
				component.m_Tag = buffer;
			ImGui::Checkbox("Is Sensor", &component.m_Sensor);
			ImGui::DragFloat2("Offset", glm::value_ptr(component.m_Offset));
			ImGui::DragFloat2("Size", glm::value_ptr(component.m_Size));
			ImGui::DragFloat("Density", &component.m_Density, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Friction", &component.m_Friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.m_Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold", &component.m_RestitutionThreshold, 0.01f, 0.0f);
		});
	ImGui::Spacing();
	DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			char buffer[256] = {};
			strncpy_s(buffer, sizeof(buffer), component.m_Tag.c_str(), sizeof(buffer));
			if (ImGui::InputText("Tag", buffer, sizeof(buffer)))
				component.m_Tag = buffer;
			ImGui::Checkbox("Is Sensor", &component.m_Sensor);
			ImGui::DragFloat2("Offset", glm::value_ptr(component.m_Offset));
			ImGui::DragFloat("Radius", &component.m_Radius);
			ImGui::DragFloat("Density", &component.m_Density, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Friction", &component.m_Friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.m_Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold", &component.m_RestitutionThreshold, 0.01f, 0.0f);
		});
	ImGui::Spacing();
	DrawComponent<AudioComponent>("Audio", entityIn, m_SceneChangeCallback, [](auto& component)
		{
			if (ImGui::Button("Add Audio", ImVec2(component.m_SelectedAudioIndex != npos<size_t> ? ImGui::GetColumnWidth() / 2 : ImGui::GetColumnWidth(), 0)))
			{
				AudioComponent::AudioData newHandle{};
				newHandle.m_Tag = component.m_UniqueNameManager.AddName(AudioComponent::AudioData::DefaultTag);
				newHandle.m_ID = UUID32{};
				component.m_AudioDatas.push_back(newHandle);
				component.m_SelectedAudioIndex = component.m_AudioDatas.size() - 1;
			}
			if (component.m_SelectedAudioIndex != npos<size_t>)
			{
				ImGui::SameLine();
				if (ImGui::Button("Delete Audio", ImVec2(ImGui::GetColumnWidth(), 0)))
				{
					component.m_UniqueNameManager.RemoveName(component.m_AudioDatas[component.m_SelectedAudioIndex].m_Tag);
					component.m_AudioDatas.erase(component.m_AudioDatas.begin() + component.m_SelectedAudioIndex);
					component.m_SelectedAudioIndex = npos<size_t>;
				}
			}

			ImGui::Spacing();
			if (ImGui::BeginTable("AudioTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
			{
				if (component.m_SelectedAudioIndex == npos<size_t>)
				{
					BEGIN_COMPONENT_TABLE_ROW("Select Audio");
					if (ImGui::BeginCombo("##Audio Handle", ""))
					{
						size_t index = 0;
						for (auto& _audio_data : component.m_AudioDatas)
						{
							if (ImGui::Selectable(_audio_data.m_Tag.c_str(), false))
								component.m_SelectedAudioIndex = index;
							index++;
						}

						ImGui::EndCombo();
					}
					END_COMPONENT_TABLE_ROW();
				}
				else
				{
					BEGIN_COMPONENT_TABLE_ROW("Audio");
					if (ImGui::BeginCombo("##Audio Handle", component.m_AudioDatas[component.m_SelectedAudioIndex].m_Tag.c_str()))
					{
						size_t index = 0;
						for (auto& audioHandle : component.m_AudioDatas)
						{
							bool isSelected = component.m_AudioDatas[component.m_SelectedAudioIndex].m_Tag == audioHandle.m_Tag;

							if (ImGui::Selectable(audioHandle.m_Tag.c_str(), isSelected))
								component.m_SelectedAudioIndex = index;

							if (isSelected)
								ImGui::SetItemDefaultFocus();
							index++;
						}

						ImGui::EndCombo();
					}
					END_COMPONENT_TABLE_ROW();
					ImGui::Separator();
					{
						BEGIN_COMPONENT_TABLE_ROW("ID");
						ImGui::Text("%u", static_cast<uint32_t>(component.m_AudioDatas[component.m_SelectedAudioIndex].m_ID));
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Tag");
						char buffer[256] = {};
						strncpy_s(buffer, sizeof(buffer), component.m_AudioDatas[component.m_SelectedAudioIndex].m_Tag.c_str(), sizeof(buffer));
						if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
						{
							component.m_UniqueNameManager.RemoveName(component.m_AudioDatas[component.m_SelectedAudioIndex].m_Tag);
							component.m_AudioDatas[component.m_SelectedAudioIndex].m_Tag = component.m_UniqueNameManager.AddName(buffer);
						}
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Audio File");
						std::string label = "None";
						bool isAudioValid = false;
						if (component.m_AudioDatas[component.m_SelectedAudioIndex].m_Audio != 0)
						{
							if (AssetManager::IsAssetHandleValid(component.m_AudioDatas[component.m_SelectedAudioIndex].m_Audio) && AssetManager::GetAssetType(component.m_AudioDatas[component.m_SelectedAudioIndex].m_Audio) == AssetType::Audio)
							{
								const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(component.m_AudioDatas[component.m_SelectedAudioIndex].m_Audio);
								label = metadata.m_Filepath.filename().string();
								isAudioValid = true;
							}
							else
							{
								label = "Invalid";
							}
						}

						float buttonSize = 0.0f;
						float padding = 0.0f;
						if (isAudioValid)
						{
							ImVec2 xLabelSize = ImGui::CalcTextSize("X");
							buttonSize = xLabelSize.y + ImGui::GetStyle().FramePadding.y * 2.0f;
							padding = buttonSize / 2.5f;
						}

						float width = ImGui::GetColumnWidth() - (buttonSize + padding);

						static const auto dragDropCallback = [&component](AssetHandle handle)
							{
								component.m_AudioDatas[component.m_SelectedAudioIndex].m_Audio = handle;
								auto audioAsset = AssetManager::GetAsset<AudioSource>(handle);
								component.m_AudioDatas[component.m_SelectedAudioIndex].m_FullClipLength = audioAsset->GetLength();
								component.m_AudioDatas[component.m_SelectedAudioIndex].m_ClipStart = 0.0f;
								component.m_AudioDatas[component.m_SelectedAudioIndex].m_ClipEnd = component.m_AudioDatas[component.m_SelectedAudioIndex].m_FullClipLength;
							};

						UI::DragDropTarget(AssetType::Audio, dragDropCallback, label.c_str(), true, width, 0.0f);

						if (isAudioValid)
						{
							ImGui::SameLine();
							if (ImGui::Button("X", ImVec2(buttonSize, buttonSize)))
							{
								component.m_AudioDatas[component.m_SelectedAudioIndex].m_Audio = 0;
								component.m_AudioDatas[component.m_SelectedAudioIndex].m_ClipStart = component.m_AudioDatas[component.m_SelectedAudioIndex].m_ClipEnd = component.m_AudioDatas[component.m_SelectedAudioIndex].m_FullClipLength = 0;
							}
						}
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Spitial");
						ImGui::Checkbox("##Spitial", &component.m_AudioDatas[component.m_SelectedAudioIndex].m_Spatial);
						END_COMPONENT_TABLE_ROW();
					}
					if(component.m_AudioDatas[component.m_SelectedAudioIndex].m_Spatial)
					{
						BEGIN_COMPONENT_TABLE_ROW("Translation");
						UI::DrawFieldVec3Control("##Translation", component.m_AudioDatas[component.m_SelectedAudioIndex].m_Translation, 0, ImGui::GetColumnWidth());
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Loop");
						ImGui::Checkbox("##Loop", &component.m_AudioDatas[component.m_SelectedAudioIndex].m_Loop);
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Gain");
						ImGui::DragFloat("##Gain", &component.m_AudioDatas[component.m_SelectedAudioIndex].m_Gain, 0.01f, 0.0f, FLT_MAX);
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Pitch");
						ImGui::DragFloat("##Pitch", &component.m_AudioDatas[component.m_SelectedAudioIndex].m_Pitch, 0.01f, 0.5f, 2.0f);
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Full Clip Length");
						ImGui::Text("%.2fs", component.m_AudioDatas[component.m_SelectedAudioIndex].m_FullClipLength);
						END_COMPONENT_TABLE_ROW();
					}
					{
						BEGIN_COMPONENT_TABLE_ROW("Clip Part (%.2fs - %.2fs)", component.m_AudioDatas[component.m_SelectedAudioIndex].m_ClipStart, component.m_AudioDatas[component.m_SelectedAudioIndex].m_ClipEnd);
						UI::DrawDualHandleSlider(0.0f, component.m_AudioDatas[component.m_SelectedAudioIndex].m_FullClipLength, &component.m_AudioDatas[component.m_SelectedAudioIndex].m_ClipStart, &component.m_AudioDatas[component.m_SelectedAudioIndex].m_ClipEnd, 0.0f, 0.0f, false);
						END_COMPONENT_TABLE_ROW();
					}
				}
				ImGui::EndTable();
			}
		});
}

template<class T>
void SceneHierarchyPanel::DisplayAddComponentEntry(const std::string& entryName)
{
	const size_t selectedCount = m_SelectionContexts.empty() ? 0 : m_SelectionContexts.size();
	const size_t componentCount = CountSelectedWithComponent<T>();
	if (componentCount < selectedCount)
	{
		if (ImGui::MenuItem(entryName.c_str()))
		{
			NotifySceneChange();
			AddComponentToSelection<T>();
			ImGui::CloseCurrentPopup();
		}
	}
}

template<class T>
size_t SceneHierarchyPanel::CountSelectedWithComponent() const
{
	size_t count = 0;
	for (Entity selected : GetSelectedEntities())
		if (selected.HasComponent<T>())
			++count;
	return count;
}

template<class T>
void SceneHierarchyPanel::AddComponentToSelection()
{
	WHP_PROFILE_FUNCTION();
	for (Entity selected : GetSelectedEntities())
	{
		if (!selected || selected.HasComponent<T>())
			continue;

		if constexpr (std::is_same_v<T, UICanvasComponent>)
		{
			UITransformComponent& transform = selected.HasComponent<UITransformComponent>() ? selected.GetComponent<UITransformComponent>() : selected.AddComponent<UITransformComponent>();
			ConfigureCanvasTransform(transform);
			if (selected.HasComponent<HierarchyComponent>())
				SetEntityParent(selected, {});
			selected.AddComponent<UICanvasComponent>();
		}
		else if constexpr (std::is_same_v<T, UITransformComponent>)
		{
			selected.AddComponent<UITransformComponent>();
			if (!selected.HasComponent<UICanvasComponent>())
				SetEntityParent(selected, FindOrCreateUICanvas());
		}
		else if constexpr (std::is_same_v<T, UIPanelComponent> || std::is_same_v<T, UIImageComponent> || std::is_same_v<T, UITextComponent> || std::is_same_v<T, UIButtonComponent> || std::is_same_v<T, UIToggleComponent> || std::is_same_v<T, UISliderComponent> || std::is_same_v<T, UIInputFieldComponent> || std::is_same_v<T, UIStackLayoutComponent>)
		{
			if (!selected.HasComponent<UITransformComponent>())
				selected.AddComponent<UITransformComponent>();
			if (!selected.HasComponent<UICanvasComponent>())
			{
				bool hasUIParent = false;
				if (selected.HasComponent<HierarchyComponent>())
				{
					const auto& hierarchy = selected.GetComponent<HierarchyComponent>();
					Entity parent = hierarchy.m_Parent != 0 ? m_Context->FindEntityByUUID(hierarchy.m_Parent) : Entity{};
					hasUIParent = parent && parent.HasComponent<UITransformComponent>();
				}
				if (!hasUIParent)
					SetEntityParent(selected, FindOrCreateUICanvas());
			}
			selected.AddComponent<T>();
		}
		else
		{
			selected.AddComponent<T>();
		}
	}
}

template<class T>
void SceneHierarchyPanel::RemoveComponentFromSelection()
{
	WHP_PROFILE_FUNCTION();
	for (Entity selected : GetSelectedEntities())
		if (selected && selected.HasComponent<T>())
			selected.RemoveComponent<T>();
}

template<class T>
void SceneHierarchyPanel::DrawMultiComponentSummary(const char* name, size_t selectedCount)
{
	const size_t componentCount = CountSelectedWithComponent<T>();
	if (componentCount == 0)
		return;

	ImGui::PushID(name);
	if (ImGui::BeginTable("##MultiComponentSummary", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Coverage", ImGuiTableColumnFlags_WidthFixed, 96.0f);
		ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 176.0f);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(name);
		ImGui::TableNextColumn();
		if (componentCount == selectedCount)
			ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.48f, 1.0f), "All");
		else
			ImGui::TextColored(ImVec4(0.90f, 0.68f, 0.32f, 1.0f), "%zu/%zu", componentCount, selectedCount);
		ImGui::TableNextColumn();
		if (componentCount < selectedCount && ImGui::SmallButton("Add Missing"))
		{
			NotifySceneChange();
			AddComponentToSelection<T>();
		}
		if (componentCount < selectedCount)
			ImGui::SameLine();
		if constexpr (!std::is_same_v<T, TransformComponent>)
		{
			if (ImGui::SmallButton("Remove"))
			{
				NotifySceneChange();
				RemoveComponentFromSelection<T>();
			}
		}
		ImGui::EndTable();
	}
	ImGui::PopID();
}

_WHIP_END

#undef BEGIN_COMPONENT_TABLE_ROW
