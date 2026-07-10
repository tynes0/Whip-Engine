#include <Whip-Editor/EditorLayer.h>

#include <Whip/Core/EntryPoint.h>
#include <Whip/Debug/Instrumentor.h>
#include <Whip/Scene/SceneSerializer.h>
#include <Whip-Editor/UI/UIHelpers.h>
#include <Whip-Editor/UI/UIProjectLoader.h>
#include <Whip/Math/Math.h>
#include <Whip/Asset/AssetUtils.h>

#include <Whip-Editor/Helpers/IconManager.h>
#include <Whip-Editor/Panels/ConsolePanel.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <entt.hpp>
#include <ImGuizmo.h>

#include "Whip-Editor/Helpers/Utils.h"

_WHIP_START
	namespace
{
	enum class ShellWindowControl : uint8_t
	{
		Minimize,
		Maximize,
		Restore,
		Close
	};

	struct GameViewResolutionPreset
	{
		const char* m_Name = "Free Aspect";
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
	};

	constexpr std::array<GameViewResolutionPreset, 11> s_GameViewResolutionPresets =
	{ {
		{ "Free Aspect", 0, 0 },
		{ "HD 16:9 (1280 x 720)", 1280, 720 },
		{ "Full HD 16:9 (1920 x 1080)", 1920, 1080 },
		{ "Steam Deck (1280 x 800)", 1280, 800 },
		{ "iPhone 12 (390 x 844)", 390, 844 },
		{ "iPhone 12 Landscape (844 x 390)", 844, 390 },
		{ "iPhone 15 Pro (393 x 852)", 393, 852 },
		{ "Galaxy S24 (360 x 780)", 360, 780 },
		{ "Galaxy S24 Landscape (780 x 360)", 780, 360 },
		{ "Pixel 8 (412 x 915)", 412, 915 },
		{ "iPad Portrait (820 x 1180)", 820, 1180 }
	} };

	glm::vec2 FitSizeToRegion(const glm::vec2& sourceSize, const glm::vec2& regionSize)
	{
		if (sourceSize.x <= 0.0f || sourceSize.y <= 0.0f || regionSize.x <= 0.0f || regionSize.y <= 0.0f)
			return { 0.0f, 0.0f };

		const float scale = glm::min(regionSize.x / sourceSize.x, regionSize.y / sourceSize.y);
		return glm::floor(sourceSize * glm::max(scale, 0.0f));
	}

	bool DrawShellWindowControlButton(const char* id, ShellWindowControl control, ImVec2 size)
	{
		ImGui::InvisibleButton(id, size);
		const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();

		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImU32 background = IM_COL32(255, 255, 255, hovered ? 28 : 0);
		if (control == ShellWindowControl::Close && hovered)
			background = IM_COL32(196, 58, 46, active ? 230 : 205);
		else if (active)
			background = IM_COL32(255, 255, 255, 42);

		drawList->AddRectFilled(min, max, background, 0.0f);
		const ImU32 iconColor = control == ShellWindowControl::Close && hovered ? IM_COL32(255, 244, 234, 255) : IM_COL32(226, 218, 202, 235);

		switch (control)
		{
		case ShellWindowControl::Minimize:
			drawList->AddLine(ImVec2(center.x - 5.0f, center.y + 5.0f), ImVec2(center.x + 5.0f, center.y + 5.0f), iconColor, 1.35f);
			break;
		case ShellWindowControl::Maximize:
			drawList->AddRect(ImVec2(center.x - 5.0f, center.y - 5.0f), ImVec2(center.x + 5.0f, center.y + 5.0f), iconColor, 0.0f, 0, 1.25f);
			break;
		case ShellWindowControl::Restore:
			drawList->AddRect(ImVec2(center.x - 3.0f, center.y - 6.0f), ImVec2(center.x + 6.0f, center.y + 3.0f), iconColor, 0.0f, 0, 1.1f);
			drawList->AddRect(ImVec2(center.x - 7.0f, center.y - 2.0f), ImVec2(center.x + 2.0f, center.y + 7.0f), iconColor, 0.0f, 0, 1.1f);
			break;
		case ShellWindowControl::Close:
			drawList->AddLine(ImVec2(center.x - 5.0f, center.y - 5.0f), ImVec2(center.x + 5.0f, center.y + 5.0f), iconColor, 1.35f);
			drawList->AddLine(ImVec2(center.x + 5.0f, center.y - 5.0f), ImVec2(center.x - 5.0f, center.y + 5.0f), iconColor, 1.35f);
			break;
		}

		return clicked;
	}

	Ref<Texture2D> GetWhipBrandTexture()
	{
		static Ref<Texture2D> texture = TextureImporter::LoadTexture2D("resources/icons/whip_editor_logo.png");
		return texture;
	}

	void DrawWhipBrandMark(ImDrawList* drawList, const ImVec2& min)
	{
		const ImVec2 max(min.x + 20.0f, min.y + 20.0f);
		if (Ref<Texture2D> texture = GetWhipBrandTexture(); texture && texture->IsLoaded())
		{
			drawList->AddImage(UI::ToImGuiTextureId(texture->GetRendererId()), min, max, ImVec2(0, 1), ImVec2(1, 0));
			return;
		}

		const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
		const ImVec2 mark[] =
		{
			ImVec2(center.x, min.y + 2.0f),
			ImVec2(max.x - 3.0f, center.y),
			ImVec2(center.x, max.y - 2.0f),
			ImVec2(min.x + 3.0f, center.y)
		};
		drawList->AddConvexPolyFilled(mark, 4, IM_COL32(245, 248, 252, 245));
		drawList->AddPolyline(mark, 4, IM_COL32(110, 128, 146, 190), ImDrawFlags_Closed, 1.2f);
	}

	std::string LowerCopy(std::string value)
	{
		std::ranges::transform(value, value.begin(),
		                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
	}

	std::string TrimCopy(std::string_view value)
	{
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
			value.remove_prefix(1);
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
			value.remove_suffix(1);
		return std::string(value);
	}

	std::string NormalizeAssistantName(std::string_view value)
	{
		std::string result;
		result.reserve(value.size());
		for (const unsigned char character : value)
		{
			if (std::isalnum(character))
				result += static_cast<char>(std::tolower(character));
		}
		return result;
	}

	bool ParseAssistantFloat(std::string_view value, float& result)
	{
		try
		{
			const std::string text = TrimCopy(value);
			if (text.empty())
				return false;
			size_t parsed = 0;
			result = std::stof(text, &parsed);
			return parsed == text.size();
		}
		catch (...)
		{
			return false;
		}
	}

	bool ParseAssistantBool(std::string_view value, bool& result)
	{
		const std::string normalized = NormalizeAssistantName(value);
		if (normalized == "true" || normalized == "yes" || normalized == "on" || normalized == "1")
		{
			result = true;
			return true;
		}
		if (normalized == "false" || normalized == "no" || normalized == "off" || normalized == "0")
		{
			result = false;
			return true;
		}
		return false;
	}

	bool ParseAssistantFloatList(std::string value, float* values, size_t count)
	{
		for (char& character : value)
		{
			if (character == ',' || character == ';' || character == '|' || character == '(' || character == ')' || character == '[' || character == ']')
				character = ' ';
		}

		std::stringstream stream(value);
		for (size_t i = 0; i < count; ++i)
		{
			if (!(stream >> values[i]))
				return false;
		}
		return true;
	}

	bool ParseAssistantVec2(const std::string& value, glm::vec2& result)
	{
		float values[2]{};
		if (!ParseAssistantFloatList(value, values, 2))
			return false;
		result = { values[0], values[1] };
		return true;
	}

	bool ParseAssistantVec3(const std::string& value, glm::vec3& result)
	{
		float values[3]{};
		if (!ParseAssistantFloatList(value, values, 3))
			return false;
		result = { values[0], values[1], values[2] };
		return true;
	}

	bool ParseAssistantVec4(const std::string& value, glm::vec4& result)
	{
		float values[4]{};
		if (!ParseAssistantFloatList(value, values, 4))
			return false;

		if (const float maxValue = (std::max)({values[0], values[1], values[2], values[3]}); maxValue > 1.0f)
		{
			for (float& channel : values)
				channel /= 255.0f;
		}

		result = { values[0], values[1], values[2], values[3] };
		return true;
	}

	bool ParseRigidbodyBodyType(std::string_view value, Rigidbody2DComponent::BodyType& result)
	{
		const std::string normalized = NormalizeAssistantName(value);
		if (normalized == "static" || normalized == "0")
		{
			result = Rigidbody2DComponent::BodyType::Static;
			return true;
		}
		if (normalized == "dynamic" || normalized == "1")
		{
			result = Rigidbody2DComponent::BodyType::Dynamic;
			return true;
		}
		if (normalized == "kinematic" || normalized == "2")
		{
			result = Rigidbody2DComponent::BodyType::Kinematic;
			return true;
		}
		return false;
	}

	bool ApplyTransformField(TransformComponent& component, const Assistant::ComponentFieldEdit& edit)
	{
		const std::string field = NormalizeAssistantName(edit.m_FieldName);
		if (field == "translation" || field == "position")
			return ParseAssistantVec3(edit.m_Value, component.m_Translation);
		if (field == "rotation")
			return ParseAssistantVec3(edit.m_Value, component.m_Rotation);
		if (field == "scale")
			return ParseAssistantVec3(edit.m_Value, component.m_Scale);
		return false;
	}

	bool ApplySpriteRendererField(SpriteRendererComponent& component, const Assistant::ComponentFieldEdit& edit)
	{
		const std::string field = NormalizeAssistantName(edit.m_FieldName);
		if (field == "color" || field == "tint")
			return ParseAssistantVec4(edit.m_Value, component.m_Color);
		if (field == "tiling" || field == "tilingfactor")
			return ParseAssistantFloat(edit.m_Value, component.m_TilingFactor);
		return false;
	}

	bool ApplyCircleRendererField(CircleRendererComponent& component, const Assistant::ComponentFieldEdit& edit)
	{
		const std::string field = NormalizeAssistantName(edit.m_FieldName);
		if (field == "color" || field == "tint")
			return ParseAssistantVec4(edit.m_Value, component.m_Color);
		if (field == "thickness")
			return ParseAssistantFloat(edit.m_Value, component.m_Thickness);
		if (field == "fade")
			return ParseAssistantFloat(edit.m_Value, component.m_Fade);
		return false;
	}

	bool ApplyTextField(TextComponent& component, const Assistant::ComponentFieldEdit& edit)
	{
		const std::string field = NormalizeAssistantName(edit.m_FieldName);
		if (field == "text" || field == "textstring" || field == "string")
		{
			component.m_TextString = edit.m_Value;
			return true;
		}
		if (field == "color" || field == "tint")
			return ParseAssistantVec4(edit.m_Value, component.m_Color);
		if (field == "kerning")
			return ParseAssistantFloat(edit.m_Value, component.m_Kerning);
		if (field == "linespacing")
			return ParseAssistantFloat(edit.m_Value, component.m_LineSpacing);
		return false;
	}

	bool ApplyCameraField(CameraComponent& component, const Assistant::ComponentFieldEdit& edit)
	{
		const std::string field = NormalizeAssistantName(edit.m_FieldName);
		bool boolValue = false;
		float floatValue = 0.0f;
		if (field == "primary" && ParseAssistantBool(edit.m_Value, boolValue))
		{
			component.m_Primary = boolValue;
			return true;
		}
		if (field == "fixedaspectratio" && ParseAssistantBool(edit.m_Value, boolValue))
		{
			component.m_FixedAspectRatio = boolValue;
			return true;
		}
		if (field == "projection" || field == "projectiontype")
		{
			const std::string projection = NormalizeAssistantName(edit.m_Value);
			if (projection == "perspective")
			{
				component.m_Camera.SetProjectionType(SceneCamera::ProjectionType::Perspective);
				return true;
			}
			if (projection == "orthographic")
			{
				component.m_Camera.SetProjectionType(SceneCamera::ProjectionType::Orthographic);
				return true;
			}
		}
		if ((field == "orthographicsize" || field == "ortho") && ParseAssistantFloat(edit.m_Value, floatValue))
		{
			component.m_Camera.SetOrthographicSize(floatValue);
			return true;
		}
		if ((field == "orthographicnearclip" || field == "orthonear") && ParseAssistantFloat(edit.m_Value, floatValue))
		{
			component.m_Camera.SetOrthographicNearClip(floatValue);
			return true;
		}
		if ((field == "orthographicfarclip" || field == "orthofar") && ParseAssistantFloat(edit.m_Value, floatValue))
		{
			component.m_Camera.SetOrthographicFarClip(floatValue);
			return true;
		}
		if ((field == "perspectiveverticalfov" || field == "perspectivefov" || field == "fov") && ParseAssistantFloat(edit.m_Value, floatValue))
		{
			component.m_Camera.SetPerspectiveVerticalFOV(floatValue);
			return true;
		}
		if ((field == "perspectivenearclip" || field == "perspectivenear") && ParseAssistantFloat(edit.m_Value, floatValue))
		{
			component.m_Camera.SetPerspectiveNearClip(floatValue);
			return true;
		}
		if ((field == "perspectivefarclip" || field == "perspectivefar") && ParseAssistantFloat(edit.m_Value, floatValue))
		{
			component.m_Camera.SetPerspectiveFarClip(floatValue);
			return true;
		}
		return false;
	}

	bool ApplyScriptField(ScriptComponent& component, const Assistant::ComponentFieldEdit& edit)
	{
		const std::string field = NormalizeAssistantName(edit.m_FieldName);
		if (field == "class" || field == "classname" || field == "script")
		{
			component.m_ClassName = edit.m_Value;
			return true;
		}
		return false;
	}

	bool ApplyAnimatorField(AnimatorComponent& component, const Assistant::ComponentFieldEdit& edit)
	{
		const std::string field = NormalizeAssistantName(edit.m_FieldName);
		bool boolValue = false;
		if (field == "initialstate" || field == "state")
		{
			component.m_InitialState = edit.m_Value;
			return true;
		}
		if (field == "playonstart" && ParseAssistantBool(edit.m_Value, boolValue))
		{
			component.m_PlayOnStart = boolValue;
			return true;
		}
		if (field == "speed")
			return ParseAssistantFloat(edit.m_Value, component.m_Speed);
		return false;
	}

	bool ApplyRigidbody2DField(Rigidbody2DComponent& component, const Assistant::ComponentFieldEdit& edit)
	{
		const std::string field = NormalizeAssistantName(edit.m_FieldName);
		bool boolValue = false;
		if (field == "type" || field == "bodytype")
			return ParseRigidbodyBodyType(edit.m_Value, component.m_Type);
		if (field == "fixedrotation" && ParseAssistantBool(edit.m_Value, boolValue))
		{
			component.m_FixedRotation = boolValue;
			return true;
		}
		if (field == "gravity" || field == "gravityscale")
			return ParseAssistantFloat(edit.m_Value, component.m_GravityScale);
		return false;
	}

	bool ApplyBoxCollider2DField(BoxCollider2DComponent& component, const Assistant::ComponentFieldEdit& edit)
	{
		const std::string field = NormalizeAssistantName(edit.m_FieldName);
		bool boolValue = false;
		if (field == "tag")
		{
			component.m_Tag = edit.m_Value;
			return true;
		}
		if ((field == "sensor" || field == "issensor") && ParseAssistantBool(edit.m_Value, boolValue))
		{
			component.m_Sensor = boolValue;
			return true;
		}
		if (field == "offset")
			return ParseAssistantVec2(edit.m_Value, component.m_Offset);
		if (field == "size")
			return ParseAssistantVec2(edit.m_Value, component.m_Size);
		if (field == "density")
			return ParseAssistantFloat(edit.m_Value, component.m_Density);
		if (field == "friction")
			return ParseAssistantFloat(edit.m_Value, component.m_Friction);
		if (field == "restitution" || field == "bounciness")
			return ParseAssistantFloat(edit.m_Value, component.m_Restitution);
		if (field == "restitutionthreshold" || field == "threshold")
			return ParseAssistantFloat(edit.m_Value, component.m_RestitutionThreshold);
		return false;
	}

	bool ApplyCircleCollider2DField(CircleCollider2DComponent& component, const Assistant::ComponentFieldEdit& edit)
	{
		const std::string field = NormalizeAssistantName(edit.m_FieldName);
		bool boolValue = false;
		if (field == "tag")
		{
			component.m_Tag = edit.m_Value;
			return true;
		}
		if ((field == "sensor" || field == "issensor") && ParseAssistantBool(edit.m_Value, boolValue))
		{
			component.m_Sensor = boolValue;
			return true;
		}
		if (field == "offset")
			return ParseAssistantVec2(edit.m_Value, component.m_Offset);
		if (field == "radius")
			return ParseAssistantFloat(edit.m_Value, component.m_Radius);
		if (field == "density")
			return ParseAssistantFloat(edit.m_Value, component.m_Density);
		if (field == "friction")
			return ParseAssistantFloat(edit.m_Value, component.m_Friction);
		if (field == "restitution" || field == "bounciness")
			return ParseAssistantFloat(edit.m_Value, component.m_Restitution);
		if (field == "restitutionthreshold" || field == "threshold")
			return ParseAssistantFloat(edit.m_Value, component.m_RestitutionThreshold);
		return false;
	}

	bool ApplyAssistantComponentField(Entity target, const std::string& componentName, const Assistant::ComponentFieldEdit& edit)
	{
		const std::string component = NormalizeAssistantName(componentName);
		if (component == "transform" && target.HasComponent<TransformComponent>())
			return ApplyTransformField(target.GetComponent<TransformComponent>(), edit);
		if (component == "spriterenderer" && target.HasComponent<SpriteRendererComponent>())
			return ApplySpriteRendererField(target.GetComponent<SpriteRendererComponent>(), edit);
		if (component == "circlerenderer" && target.HasComponent<CircleRendererComponent>())
			return ApplyCircleRendererField(target.GetComponent<CircleRendererComponent>(), edit);
		if ((component == "textrenderer" || component == "text") && target.HasComponent<TextComponent>())
			return ApplyTextField(target.GetComponent<TextComponent>(), edit);
		if (component == "camera" && target.HasComponent<CameraComponent>())
			return ApplyCameraField(target.GetComponent<CameraComponent>(), edit);
		if (component == "script" && target.HasComponent<ScriptComponent>())
			return ApplyScriptField(target.GetComponent<ScriptComponent>(), edit);
		if (component == "animator" && target.HasComponent<AnimatorComponent>())
			return ApplyAnimatorField(target.GetComponent<AnimatorComponent>(), edit);
		if (component == "rigidbody2d" && target.HasComponent<Rigidbody2DComponent>())
			return ApplyRigidbody2DField(target.GetComponent<Rigidbody2DComponent>(), edit);
		if (component == "boxcollider2d" && target.HasComponent<BoxCollider2DComponent>())
			return ApplyBoxCollider2DField(target.GetComponent<BoxCollider2DComponent>(), edit);
		if (component == "circlecollider2d" && target.HasComponent<CircleCollider2DComponent>())
			return ApplyCircleCollider2DField(target.GetComponent<CircleCollider2DComponent>(), edit);
		return false;
	}

	bool IsAssistantVisibleAssetType(AssetType type)
	{
		switch (type)
		{
		case AssetType::Texture2D:
		case AssetType::Audio:
		case AssetType::Font:
		case AssetType::Animation:
		case AssetType::AnimationController:
		case AssetType::Scene:
		case AssetType::Entity:
			return true;
		case AssetType::None:
			return false;
		}
		return false;
	}

	void AppendAssistantComponentNames(Entity entity, std::vector<std::string>& components)
	{
		if (entity.HasComponent<TransformComponent>())
			components.emplace_back("Transform");
		if (entity.HasComponent<SpriteRendererComponent>())
			components.emplace_back("Sprite Renderer");
		if (entity.HasComponent<CircleRendererComponent>())
			components.emplace_back("Circle Renderer");
		if (entity.HasComponent<TextComponent>())
			components.emplace_back("Text Renderer");
		if (entity.HasComponent<CameraComponent>())
			components.emplace_back("Camera");
		if (entity.HasComponent<ScriptComponent>())
			components.emplace_back("Script");
		if (entity.HasComponent<AnimatorComponent>())
			components.emplace_back("Animator");
		if (entity.HasComponent<Rigidbody2DComponent>())
			components.emplace_back("Rigidbody2D");
		if (entity.HasComponent<BoxCollider2DComponent>())
			components.emplace_back("BoxCollider2D");
		if (entity.HasComponent<CircleCollider2DComponent>())
			components.emplace_back("CircleCollider2D");
		if (entity.HasComponent<AudioComponent>())
			components.emplace_back("Audio");
	}

	void AppendAssistantAssetContext(const Ref<Project>& activeProject, Assistant::ContextSnapshot& context)
	{
		if (!activeProject || !activeProject->GetEditorAssetManager())
			return;

		constexpr size_t MaxAssets = 80;
		constexpr size_t MaxSpritesPerTexture = 128;
		std::vector<Assistant::ContextSnapshot::AssetSummary> assets;
		activeProject->GetEditorAssetManager()->GetAssetRegistry().Foreach(
			[&assets](const AssetRegistry::ValueType& value)
			{
				const AssetHandle handle = value.first;
				const AssetMetadata& metadata = value.second;
				if (!IsAssistantVisibleAssetType(metadata.m_Type))
					return;

				Assistant::ContextSnapshot::AssetSummary summary;
				summary.m_Handle = static_cast<uint64_t>(handle);
				summary.m_Type = metadata.m_Type;
				summary.m_Path = metadata.m_Filepath.generic_string();
				const std::filesystem::path absoluteAssetPath = Project::GetActiveAssetDirectory() / metadata.m_Filepath;
				summary.m_AbsolutePath = absoluteAssetPath.lexically_normal().string();
				summary.m_Name = metadata.m_Filepath.filename().string();

				if (metadata.m_Type == AssetType::Texture2D)
				{
					const std::vector<TextureSpriteRect>& sprites = metadata.m_TextureSettings.m_Sprites;
					summary.m_SpriteCount = sprites.size();
					for (size_t i = 0; i < sprites.size() && i < MaxSpritesPerTexture; ++i)
					{
						summary.m_Sprites.push_back(sprites[i].m_Name);
						summary.m_SpriteDetails.push_back({
							.m_Name = sprites[i].m_Name,
							.m_X = sprites[i].m_X,
							.m_Y = sprites[i].m_Y,
							.m_Width = sprites[i].m_Width,
							.m_Height = sprites[i].m_Height
						});
					}
				}

				assets.push_back(std::move(summary));
			});

		std::ranges::sort(assets, [](const auto& left, const auto& right)
		{
			if (left.m_Type != right.m_Type)
				return static_cast<uint8_t>(left.m_Type) < static_cast<uint8_t>(right.m_Type);
			return left.m_Path < right.m_Path;
		});

		if (assets.size() > MaxAssets)
			assets.resize(MaxAssets);
		context.m_ProjectAssets = std::move(assets);
	}

	void AppendAssistantSceneContext(const Ref<Scene>& scene, Assistant::ContextSnapshot& context)
	{
		if (!scene)
			return;

		constexpr size_t MaxSceneEntities = 80;
		Ref<Project> activeProject = Project::GetActive();
		EditorAssetManager* assetManager = activeProject && activeProject->GetEditorAssetManager() ? activeProject->GetEditorAssetManager().get() : nullptr;

		auto view = scene->GetAllEntitiesWith<IDComponent>();
		for (entt::entity entityHandle : view)
		{
			if (context.m_SceneEntities.size() >= MaxSceneEntities)
				break;

			Entity entity(entityHandle, scene.get());
			Assistant::ContextSnapshot::EntitySummary summary;
			summary.m_Id = static_cast<uint64_t>(entity.GetUUID());
			summary.m_Name = entity.HasComponent<TagComponent>() ? entity.GetName() : "Entity";
			AppendAssistantComponentNames(entity, summary.m_Components);

			if (entity.HasComponent<TransformComponent>())
			{
				const TransformComponent& transform = entity.GetComponent<TransformComponent>();
				summary.m_HasTransform = true;
				summary.m_Translation = transform.m_Translation;
				summary.m_Scale = transform.m_Scale;
			}

			if (entity.HasComponent<SpriteRendererComponent>())
			{
				const SpriteRendererComponent& spriteRenderer = entity.GetComponent<SpriteRendererComponent>();
				summary.m_TextureHandle = static_cast<uint64_t>(spriteRenderer.m_Texture);
				summary.m_TextureSpriteIndex = spriteRenderer.m_TextureSpriteIndex;

				if (assetManager && spriteRenderer.m_Texture != 0 && assetManager->IsAssetHandleValid(spriteRenderer.m_Texture) && assetManager->GetAssetType(spriteRenderer.m_Texture) == AssetType::Texture2D)
				{
					const AssetMetadata& metadata = assetManager->GetMetadata(spriteRenderer.m_Texture);
					summary.m_TexturePath = metadata.m_Filepath.generic_string();
					const std::vector<TextureSpriteRect>& sprites = metadata.m_TextureSettings.m_Sprites;
					if (spriteRenderer.m_TextureSpriteIndex >= 0 && std::cmp_less(spriteRenderer.m_TextureSpriteIndex, sprites.size()))
						summary.m_SpriteName = sprites[static_cast<size_t>(spriteRenderer.m_TextureSpriteIndex)].m_Name;
				}
			}

			context.m_SceneEntities.push_back(std::move(summary));
		}
	}

	AssetType ExpectedAssistantAssetType(const std::string& componentName, const std::string& fieldName)
	{
		const std::string component = NormalizeAssistantName(componentName);
		const std::string field = NormalizeAssistantName(fieldName);
		if (component == "spriterenderer" && (field == "texture" || field == "sprite" || field == "image"))
			return AssetType::Texture2D;
		if ((component == "textrenderer" || component == "text") && field == "font")
			return AssetType::Font;
		if (component == "animator" && (field == "controller" || field == "animationcontroller"))
			return AssetType::AnimationController;
		if (component == "audio" && (field == "audio" || field == "audiofile" || field == "clip"))
			return AssetType::Audio;
		return AssetType::None;
	}

	bool ResolveAssistantAssetHandle(const Assistant::ToolProposal& proposal, AssetType expectedType, AssetHandle& outHandle)
	{
		Ref<Project> activeProject = Project::GetActive();
		if (!activeProject || !activeProject->GetEditorAssetManager())
			return false;

		EditorAssetManager& assetManager = *activeProject->GetEditorAssetManager();
		const AssetType acceptedType = expectedType != AssetType::None ? expectedType : proposal.m_AssetType;

		if (proposal.m_AssetHandle != 0)
		{
			const AssetHandle handle = AssetHandle(proposal.m_AssetHandle);
			if (assetManager.IsAssetHandleValid(handle) && (acceptedType == AssetType::None || assetManager.GetAssetType(handle) == acceptedType))
			{
				outHandle = handle;
				return true;
			}
		}

		const std::string pathNeedle = LowerCopy(proposal.m_AssetPath);
		const std::string nameNeedle = LowerCopy(proposal.m_AssetName);
		if (pathNeedle.empty() && nameNeedle.empty())
			return false;

		struct Candidate
		{
			AssetHandle m_Handle = 0;
			int m_Score = 0;
		};
		Candidate best;
		const auto consider = [&](const AssetRegistry::ValueType& value)
		{
			const AssetHandle handle = value.first;
			const AssetMetadata& metadata = value.second;
			if (acceptedType != AssetType::None && metadata.m_Type != acceptedType)
				return;

			const std::string path = LowerCopy(metadata.m_Filepath.generic_string());
			const std::string filename = LowerCopy(metadata.m_Filepath.filename().string());
			const std::string stem = LowerCopy(metadata.m_Filepath.stem().string());
			int score = 0;
			if (!pathNeedle.empty())
			{
				if (path == pathNeedle)
					score = std::max(score, 100);
				else if (filename == pathNeedle)
					score = std::max(score, 90);
				else if (path.find(pathNeedle) != std::string::npos)
					score = std::max(score, 60);
			}
			if (!nameNeedle.empty())
			{
				if (filename == nameNeedle)
					score = std::max(score, 95);
				else if (stem == nameNeedle)
					score = std::max(score, 85);
				else if (filename.find(nameNeedle) != std::string::npos || stem.find(nameNeedle) != std::string::npos)
					score = std::max(score, 50);
			}

			if (score > best.m_Score)
			{
				best = {
					.m_Handle = handle,
					.m_Score = score
				};
			}
		};

		if (acceptedType != AssetType::None)
			assetManager.GetAssetRegistry().Foreach(acceptedType, consider);
		else
			assetManager.GetAssetRegistry().Foreach(consider);

		if (best.m_Handle == 0)
			return false;
		outHandle = best.m_Handle;
		return true;
	}

	int32_t ResolveAssistantTextureSpriteIndex(AssetHandle textureHandle, const Assistant::ToolProposal& proposal)
	{
		if (!AssetManager::IsAssetHandleValid(textureHandle) || AssetManager::GetAssetType(textureHandle) != AssetType::Texture2D)
			return -1;

		const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(textureHandle);
		const std::vector<TextureSpriteRect>& sprites = metadata.m_TextureSettings.m_Sprites;
		if (proposal.m_AssetSubresourceIndex >= 0 && std::cmp_less(proposal.m_AssetSubresourceIndex, sprites.size()))
			return proposal.m_AssetSubresourceIndex;
		if (proposal.m_AssetSubresource.empty())
			return -1;

		const std::string needle = NormalizeAssistantName(proposal.m_AssetSubresource);
		for (int32_t spriteIndex = 0; std::cmp_less(spriteIndex, sprites.size()); ++spriteIndex)
		{
			if (NormalizeAssistantName(sprites[static_cast<size_t>(spriteIndex)].m_Name) == needle)
				return spriteIndex;
		}
		return -1;
	}

	bool ApplyAssistantAssetOperation(Entity target, const Assistant::ToolProposal& proposal)
	{
		if (!target)
			return false;

		const AssetType expectedType = ExpectedAssistantAssetType(proposal.m_ComponentName, proposal.m_AssetField);
		if (expectedType == AssetType::None)
			return false;

		AssetHandle assetHandle = 0;
		if (!ResolveAssistantAssetHandle(proposal, expectedType, assetHandle))
			return false;

		const std::string component = NormalizeAssistantName(proposal.m_ComponentName);
		if (component == "spriterenderer" && target.HasComponent<SpriteRendererComponent>())
		{
			SpriteRendererComponent& sprite = target.GetComponent<SpriteRendererComponent>();
			sprite.m_Texture = assetHandle;
			sprite.m_TextureSpriteIndex = ResolveAssistantTextureSpriteIndex(assetHandle, proposal);
			return true;
		}
		if ((component == "textrenderer" || component == "text") && target.HasComponent<TextComponent>())
		{
			target.GetComponent<TextComponent>().m_Font = assetHandle;
			return true;
		}
		if (component == "animator" && target.HasComponent<AnimatorComponent>())
		{
			target.GetComponent<AnimatorComponent>().m_Controller = assetHandle;
			return true;
		}
		if (component == "audio" && target.HasComponent<AudioComponent>())
		{
			AudioComponent& audio = target.GetComponent<AudioComponent>();
			if (audio.m_AudioDatas.empty())
			{
				AudioComponent::AudioData data;
				data.m_Tag = audio.m_UniqueNameManager.AddName(AudioComponent::AudioData::DefaultTag);
				data.m_ID = UUID32{};
				audio.m_AudioDatas.push_back(data);
				audio.m_SelectedAudioIndex = 0;
			}
			if (audio.m_SelectedAudioIndex == npos<size_t> || audio.m_SelectedAudioIndex >= audio.m_AudioDatas.size())
				audio.m_SelectedAudioIndex = 0;

			AudioComponent::AudioData& data = audio.m_AudioDatas[audio.m_SelectedAudioIndex];
			data.m_Audio = assetHandle;
			if (Ref<AudioSource> audioAsset = AssetManager::GetAsset<AudioSource>(assetHandle))
			{
				data.m_FullClipLength = audioAsset->GetLength();
				data.m_ClipStart = 0.0f;
				data.m_ClipEnd = data.m_FullClipLength;
			}
			return true;
		}

		return false;
	}

	int32_t ExtractAssistantTrailingNumber(std::string_view value);
	uint32_t StableAssistantHash(std::string_view value);
	std::string BuildAssistantPlacementHint(const Assistant::SpriteLevelPlacement& placement);

	int32_t ResolveAssistantSpriteLevelPlacementIndex(AssetHandle textureHandle, const Assistant::SpriteLevelPlacement& placement)
	{
		if (!AssetManager::IsAssetHandleValid(textureHandle) || AssetManager::GetAssetType(textureHandle) != AssetType::Texture2D)
			return -1;

		const Ref<Project> activeProject = Project::GetActive();
		if (!activeProject || !activeProject->GetEditorAssetManager())
			return -1;

		const AssetMetadata& metadata = activeProject->GetEditorAssetManager()->GetMetadata(textureHandle);
		const std::vector<TextureSpriteRect>& sprites = metadata.m_TextureSettings.m_Sprites;
		if (placement.m_SpriteIndex >= 0 && std::cmp_less(placement.m_SpriteIndex, sprites.size()))
			return placement.m_SpriteIndex;
		if (placement.m_SpriteIndex >= 0 && !sprites.empty())
			return placement.m_SpriteIndex % static_cast<int32_t>(sprites.size());
		if (placement.m_SpriteName.empty())
			return -1;

		const std::string needle = NormalizeAssistantName(placement.m_SpriteName);
		for (int32_t spriteIndex = 0; std::cmp_less(spriteIndex, sprites.size()); ++spriteIndex)
		{
			if (NormalizeAssistantName(sprites[static_cast<size_t>(spriteIndex)].m_Name) == needle)
				return spriteIndex;
		}
		for (int32_t spriteIndex = 0; std::cmp_less(spriteIndex, sprites.size()); ++spriteIndex)
		{
			const std::string spriteName = NormalizeAssistantName(sprites[static_cast<size_t>(spriteIndex)].m_Name);
			if (!spriteName.empty() && (spriteName.find(needle) != std::string::npos || needle.find(spriteName) != std::string::npos))
				return spriteIndex;
		}

		const int32_t numberedIndex = ExtractAssistantTrailingNumber(needle);
		if (numberedIndex >= 0 && std::cmp_less(numberedIndex, sprites.size()))
			return numberedIndex;
		if (!sprites.empty())
			return static_cast<int32_t>(StableAssistantHash(BuildAssistantPlacementHint(placement)) % sprites.size());
		return -1;
	}

	bool HasAssistantAssetReference(const Assistant::ToolProposal& proposal)
	{
		return proposal.m_AssetHandle != 0 || !proposal.m_AssetPath.empty() || !proposal.m_AssetName.empty();
	}

	bool HasAssistantAssetReference(const Assistant::SpriteLevelPlacement& placement)
	{
		return placement.m_AssetHandle != 0 || !placement.m_AssetPath.empty() || !placement.m_AssetName.empty();
	}

	bool ContainsAnyToken(const std::string& haystack, std::initializer_list<std::string_view> needles)
	{
		for (std::string_view needle : needles)
			if (haystack.find(needle) != std::string::npos)
				return true;
		return false;
	}

	uint32_t StableAssistantHash(std::string_view value)
	{
		uint32_t hash = 2166136261u;
		for (unsigned char character : value)
		{
			hash ^= character;
			hash *= 16777619u;
		}
		return hash;
	}

	int32_t ExtractAssistantTrailingNumber(std::string_view value)
	{
		if (value.empty())
			return -1;

		size_t end = value.size();
		while (end > 0 && !std::isdigit(static_cast<unsigned char>(value[end - 1])))
			--end;
		if (end == 0)
			return -1;

		size_t begin = end;
		while (begin > 0 && std::isdigit(static_cast<unsigned char>(value[begin - 1])))
			--begin;

		try
		{
			return std::stoi(std::string(value.substr(begin, end - begin)));
		}
		catch (...)
		{
			return -1;
		}
	}

	std::string BuildAssistantPlacementHint(const Assistant::SpriteLevelPlacement& placement)
	{
		return LowerCopy(placement.m_EntityName + " " + placement.m_SpriteName + " " + placement.m_AssetName + " " + placement.m_AssetPath);
	}

	struct ResolvedAssistantSpriteLevelPlacement
	{
		const Assistant::SpriteLevelPlacement* m_Placement = nullptr;
		AssetHandle m_TextureHandle = 0;
		int32_t m_SpriteIndex = -1;
	};

	AssetHandle ResolveAssistantFallbackTextureHandle(const Assistant::SpriteLevelPlacement& placement, AssetHandle defaultTextureHandle)
	{
		const Ref<Project> activeProject = Project::GetActive();
		if (!activeProject || !activeProject->GetEditorAssetManager())
			return defaultTextureHandle;

		EditorAssetManager& assetManager = *activeProject->GetEditorAssetManager();
		if (defaultTextureHandle != 0 && assetManager.IsAssetHandleValid(defaultTextureHandle) && assetManager.GetAssetType(defaultTextureHandle) == AssetType::Texture2D)
			return defaultTextureHandle;

		const std::string hint = BuildAssistantPlacementHint(placement);
		struct Candidate
		{
			AssetHandle m_Handle = 0;
			int m_Score = -1;
		};

		Candidate best;
		assetManager.GetAssetRegistry().Foreach(AssetType::Texture2D, [&](const AssetRegistry::ValueType& value)
		{
			const AssetHandle handle = value.first;
			const AssetMetadata& metadata = value.second;
			const std::string path = LowerCopy(metadata.m_Filepath.generic_string());
			const std::string name = LowerCopy(metadata.m_Filepath.filename().string());

			int score = metadata.m_TextureSettings.m_Sprites.empty() ? 0 : 20;
			if (!placement.m_AssetPath.empty() && path.find(LowerCopy(placement.m_AssetPath)) != std::string::npos)
				score += 100;
			if (!placement.m_AssetName.empty() && name.find(LowerCopy(placement.m_AssetName)) != std::string::npos)
				score += 90;
			if (ContainsAnyToken(hint, { "ground", "zemin", "floor", "platform", "tile", "path", "wall" }) &&
				ContainsAnyToken(path + " " + name, { "tile", "tileset", "ground", "platform" }))
			{
				score += 55;
			}
			if (ContainsAnyToken(hint, { "tree", "bush", "grass", "flower", "rock", "crate", "barrel", "bench", "sign", "torch", "chest", "prop" }) &&
				ContainsAnyToken(path + " " + name, { "prop", "props", "village", "chest", "tileset" }))
			{
				score += 55;
			}
			if (ContainsAnyToken(hint, { "fx", "flame", "fire", "torch", "spark", "light" }) &&
				ContainsAnyToken(path + " " + name, { "fx", "flame", "fire" }))
			{
				score += 45;
			}

			if (score > best.m_Score)
			{
				best = {
					.m_Handle = handle,
					.m_Score = score
				};
			}
		});

		return best.m_Handle;
	}

	glm::vec3 ComputeAssistantSpriteLevelScale(AssetHandle textureHandle, int32_t spriteIndex, const Assistant::SpriteLevelPlacement& placement)
	{
		if (placement.m_HasScale)
			return { glm::max(placement.m_Scale.x, 0.01f), glm::max(placement.m_Scale.y, 0.01f), placement.m_Scale.z == 0.0f ? 1.0f : placement.m_Scale.z };

		const Ref<Project> activeProject = Project::GetActive();
		if (!activeProject || !activeProject->GetEditorAssetManager())
			return { 1.0f, 1.0f, 1.0f };

		const AssetMetadata& metadata = activeProject->GetEditorAssetManager()->GetMetadata(textureHandle);
		const std::vector<TextureSpriteRect>& sprites = metadata.m_TextureSettings.m_Sprites;
		const bool validSpriteIndex = spriteIndex >= 0 && std::cmp_less(spriteIndex, sprites.size());
		const TextureSpriteRect* spriteRect = validSpriteIndex ? &sprites[static_cast<size_t>(spriteIndex)] : nullptr;
		const Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(textureHandle);
		if (!texture || !texture->IsLoaded())
			return { 1.0f, 1.0f, 1.0f };

		const float pixelsPerUnit = metadata.m_TextureSettings.m_PixelsPerUnit > 0.0f ? metadata.m_TextureSettings.m_PixelsPerUnit : 100.0f;
		const float spriteWidth = spriteRect ? static_cast<float>(spriteRect->m_Width) : static_cast<float>(texture->GetWidth());
		const float spriteHeight = spriteRect ? static_cast<float>(spriteRect->m_Height) : static_cast<float>(texture->GetHeight());
		return {
			glm::max(spriteWidth / pixelsPerUnit, 0.1f),
			glm::max(spriteHeight / pixelsPerUnit, 0.1f),
			1.0f
		};
	}

	bool ApplyAssistantSpriteLevel(const Assistant::ToolProposal& proposal, const Ref<Scene>& scene, Entity& outLastCreated)
	{
		if (!scene || proposal.m_LevelPlacements.empty())
			return false;

		AssetHandle defaultTextureHandle = 0;
		if (HasAssistantAssetReference(proposal) && !ResolveAssistantAssetHandle(proposal, AssetType::Texture2D, defaultTextureHandle))
			return false;

		const Ref<Project> activeProject = Project::GetActive();
		if (!activeProject || !activeProject->GetEditorAssetManager())
			return false;

		std::vector<ResolvedAssistantSpriteLevelPlacement> resolvedPlacements;
		size_t skippedCount = 0;
		for (const Assistant::SpriteLevelPlacement& placement : proposal.m_LevelPlacements)
		{
			AssetHandle textureHandle = defaultTextureHandle;
			if (HasAssistantAssetReference(placement))
			{
				Assistant::ToolProposal placementAsset;
				placementAsset.m_AssetHandle = placement.m_AssetHandle;
				placementAsset.m_AssetPath = placement.m_AssetPath;
				placementAsset.m_AssetName = placement.m_AssetName;
				placementAsset.m_AssetType = AssetType::Texture2D;
				if (!ResolveAssistantAssetHandle(placementAsset, AssetType::Texture2D, textureHandle))
				{
					textureHandle = ResolveAssistantFallbackTextureHandle(placement, defaultTextureHandle);
					if (textureHandle == 0)
					{
						++skippedCount;
						continue;
					}
				}
			}
			else if (textureHandle == 0)
			{
				textureHandle = ResolveAssistantFallbackTextureHandle(placement, defaultTextureHandle);
			}

			if (textureHandle == 0 || !activeProject->GetEditorAssetManager()->IsAssetHandleValid(textureHandle) || activeProject->GetEditorAssetManager()->GetAssetType(textureHandle) != AssetType::Texture2D)
			{
				++skippedCount;
				continue;
			}

			const AssetMetadata& metadata = activeProject->GetEditorAssetManager()->GetMetadata(textureHandle);
			const std::vector<TextureSpriteRect>& sprites = metadata.m_TextureSettings.m_Sprites;
			const int32_t spriteIndex = ResolveAssistantSpriteLevelPlacementIndex(textureHandle, placement);
			const bool validSpriteIndex = spriteIndex >= 0 && std::cmp_less(spriteIndex, sprites.size());
			const bool requestedSprite = placement.m_SpriteIndex >= 0 || !placement.m_SpriteName.empty();
			if (requestedSprite && !validSpriteIndex)
			{
				++skippedCount;
				continue;
			}

			resolvedPlacements.push_back({
				.m_Placement = &placement,
				.m_TextureHandle = textureHandle,
				.m_SpriteIndex = validSpriteIndex ? spriteIndex : -1
			});
		}

		if (resolvedPlacements.empty())
		{
			WHP_CORE_WARN("[Whip Assistant] Sprite level proposal rejected: no valid placements resolved.");
			return false;
		}
		if (proposal.m_LevelPlacements.size() >= 6 && resolvedPlacements.size() < 6)
		{
			WHP_CORE_WARN("[Whip Assistant] Sprite level proposal rejected: only {0}/{1} placements resolved.", resolvedPlacements.size(), proposal.m_LevelPlacements.size());
			return false;
		}
		if (skippedCount > 0 && resolvedPlacements.size() * 2 < proposal.m_LevelPlacements.size())
		{
			WHP_CORE_WARN("[Whip Assistant] Sprite level proposal rejected: too many invalid placements ({0}/{1} skipped).", skippedCount, proposal.m_LevelPlacements.size());
			return false;
		}

		size_t createdCount = 0;
		for (const ResolvedAssistantSpriteLevelPlacement& resolved : resolvedPlacements)
		{
			const Assistant::SpriteLevelPlacement& placement = *resolved.m_Placement;
			const AssetHandle textureHandle = resolved.m_TextureHandle;
			const int32_t spriteIndex = resolved.m_SpriteIndex;
			const AssetMetadata& metadata = activeProject->GetEditorAssetManager()->GetMetadata(textureHandle);
			const std::vector<TextureSpriteRect>& sprites = metadata.m_TextureSettings.m_Sprites;
			const bool validSpriteIndex = spriteIndex >= 0 && std::cmp_less(spriteIndex, sprites.size());
			const TextureSpriteRect* spriteRect = validSpriteIndex ? &sprites[static_cast<size_t>(spriteIndex)] : nullptr;

			std::string entityName = placement.m_EntityName;
			if (entityName.empty())
				entityName = spriteRect ? spriteRect->m_Name : (metadata.m_Filepath.stem().empty() ? "Level Sprite" : metadata.m_Filepath.stem().string());

			Entity entity = scene->CreateEntity(entityName);
			TransformComponent& transform = entity.GetComponent<TransformComponent>();
			transform.m_Translation = placement.m_Translation;
			transform.m_Scale = ComputeAssistantSpriteLevelScale(textureHandle, spriteIndex, placement);
			transform.m_Rotation.z = std::abs(placement.m_RotationZ) > 6.28318530718f ? placement.m_RotationZ * 0.017453292519943295f : placement.m_RotationZ;

			SpriteRendererComponent& spriteRenderer = entity.AddComponent<SpriteRendererComponent>();
			spriteRenderer.m_Texture = textureHandle;
			spriteRenderer.m_TextureSpriteIndex = validSpriteIndex ? spriteIndex : -1;
			spriteRenderer.m_Color = glm::vec4(1.0f);

			outLastCreated = entity;
			++createdCount;
		}

		if (createdCount > 0)
		{
			std::string message = "[Whip Assistant] Created " + std::to_string(createdCount) + " sprite level entities";
			if (skippedCount > 0)
				message += " (" + std::to_string(skippedCount) + " skipped due to invalid asset references)";
			WHP_EDITOR_INFO(message);
		}
		return createdCount > 0;
	}

	std::string EditorActionShortcutId(UI::EditorShortcutAction action)
	{
		return std::string("global.") + UI::UISettings::GetActionStorageKey(action);
	}

	bool ShortcutMatchesCommandFilter(const EditorShortcut& shortcut, const char* filter)
	{
		if (!filter || filter[0] == '\0')
			return true;

		std::string needle = LowerCopy(filter);
		std::string haystack = LowerCopy(shortcut.m_DisplayName + " " + shortcut.m_Category + " " + shortcut.m_Id + " " + EditorShortcutManager::GetScopeName(shortcut.m_Scope));
		return haystack.find(needle) != std::string::npos;
	}

	int GizmoSnapIndex(int operation)
	{
		if (operation == ImGuizmo::OPERATION::TRANSLATE)
			return 0;
		if (operation == ImGuizmo::OPERATION::ROTATE)
			return 1;
		if (operation == ImGuizmo::OPERATION::SCALE || operation == ImGuizmo::OPERATION::SCALEU)
			return 2;
		return -1;
	}

	ImU32 ColorU32(float r, float g, float b, float a = 1.0f)
	{
		return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
	}

	bool IsIgnoredScriptDirectory(const std::filesystem::path& path)
	{
		const std::string name = LowerCopy(path.filename().string());
		return name == "binaries" || name == "intermediates" || name == "obj" || name == "whip-scriptcore";
	}

	std::string ShortClassName(std::string className)
	{
		const size_t dot = className.find_last_of('.');
		if (dot != std::string::npos && dot + 1 < className.size())
			return className.substr(dot + 1);
		return className;
	}

	bool ReadTextFile(const std::filesystem::path& path, std::string& content, uintmax_t maxBytes = 48ull * 1024ull)
	{
		std::error_code error;
		const uintmax_t size = std::filesystem::file_size(path, error);
		if (error || size > maxBytes)
			return false;

		std::ifstream input(path, std::ios::binary);
		if (!input)
			return false;

		content.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
		return true;
	}

	std::filesystem::path FindScriptSourcePath(const std::string& className)
	{
		if (!Project::GetActive() || className.empty())
			return {};

		const std::filesystem::path scriptsRoot = Project::GetActiveAssetDirectory() / "Scripts";
		const std::string shortName = ShortClassName(className);
		std::filesystem::path preferred = scriptsRoot / "Source" / (shortName + ".cs");
		std::error_code error;
		if (std::filesystem::exists(preferred, error) && std::filesystem::is_regular_file(preferred, error))
			return preferred;

		std::filesystem::path rootPreferred = scriptsRoot / (shortName + ".cs");
		error.clear();
		if (std::filesystem::exists(rootPreferred, error) && std::filesystem::is_regular_file(rootPreferred, error))
			return rootPreferred;

		error.clear();
		if (!std::filesystem::exists(scriptsRoot, error) || !std::filesystem::is_directory(scriptsRoot, error))
			return {};

		std::filesystem::path fallback;
		std::string fileContent;
		for (std::filesystem::recursive_directory_iterator it(scriptsRoot, std::filesystem::directory_options::skip_permission_denied, error), end; it != end && !error; it.increment(error))
		{
			if (it->is_directory(error))
			{
				if (IsIgnoredScriptDirectory(it->path()))
					it.disable_recursion_pending();
				continue;
			}

			if (!it->is_regular_file(error) || it->path().extension() != ".cs")
				continue;

			if (it->path().filename() == shortName + ".cs")
				return it->path();

			if (fallback.empty() && ReadTextFile(it->path(), fileContent, 128ull * 1024ull) && fileContent.find("class " + shortName) != std::string::npos)
				fallback = it->path();
		}

		return fallback;
	}

	std::string MakeAssistantScriptPath(const std::filesystem::path& absolutePath)
	{
		if (!Project::GetActive())
			return absolutePath.generic_string();

		std::error_code error;
		const std::filesystem::path relative = std::filesystem::relative(absolutePath, Project::GetActiveProjectDirectory(), error);
		if (!error && !relative.empty())
			return relative.generic_string();
		return absolutePath.generic_string();
	}

	bool IsPathInside(const std::filesystem::path& child, const std::filesystem::path& root)
	{
		std::error_code error;
		const std::filesystem::path relative = std::filesystem::relative(child, root, error);
		if (error || relative.empty() || relative.is_absolute())
			return false;

		for (const std::filesystem::path& part : relative)
			if (part == "..")
				return false;
		return true;
	}

	std::filesystem::path ResolveAssistantScriptEditPath(const std::string& requestedPath)
	{
		if (!Project::GetActive() || requestedPath.empty())
			return {};

		const std::filesystem::path& projectRoot = Project::GetActiveProjectDirectory();
		const std::filesystem::path assetRoot = Project::GetActiveAssetDirectory();
		const std::filesystem::path scriptsRoot = assetRoot / "Scripts";
		std::filesystem::path candidate(requestedPath);

		if (!candidate.is_absolute())
		{
			const std::string normalized = LowerCopy(candidate.generic_string());
			if (normalized.starts_with("assets/scripts/"))
				candidate = projectRoot / candidate;
			else if (normalized.starts_with("scripts/"))
				candidate = assetRoot / candidate;
			else
				candidate = scriptsRoot / candidate;
		}

		std::error_code error;
		if (!std::filesystem::exists(candidate, error) || !std::filesystem::is_regular_file(candidate, error) || candidate.extension() != ".cs")
			return {};

		std::filesystem::path canonicalCandidate = std::filesystem::weakly_canonical(candidate, error);
		if (error)
			return {};

		error.clear();
		const std::filesystem::path canonicalScriptsRoot = std::filesystem::weakly_canonical(scriptsRoot, error);
		if (error || !IsPathInside(canonicalCandidate, canonicalScriptsRoot))
			return {};

		return canonicalCandidate;
	}

}

EditorLayer::EditorLayer()
	: Layer("Fbox2D"),
	m_EditorCamera(),
	m_AssetInteractionManager(this),
	m_EntityTemplateManager(this),
	m_EventManager(this),
	m_HistoryManager(this),
	m_ProjectManager(this),
	m_ExportManager(this),
	m_ScriptManager(this),
	m_SceneManager(this),
	m_PanelManager(this),
	m_ShortcutManager(this),
	m_GizmoType(ImGuizmo::OPERATION::TRANSLATE)
{
}

void EditorLayer::OnAttach()
{
    WHP_PROFILE_FUNCTION();
	WHP_EDITOR_INFO("[Editor] Attaching EditorLayer.");

	m_AnimationEditorPanel.SetRefreshAssetTreeCallback([this]() {if (m_ContentBrowserPanel) { m_ContentBrowserPanel->RefreshAssetTree(); } });
	m_AssetEditorPanel.SetOpenSceneCallback([this](AssetHandle handle) { m_SceneManager.OpenScene(handle); });
	m_AssetEditorPanel.SetSetStartSceneCallback([this](AssetHandle handle) { m_AssetInteractionManager.SetStartScene(handle); });
	m_AssetEditorPanel.SetOpenAnimationCallback([this](AssetHandle handle) { return m_AnimationEditorPanel.OpenAsset(handle, false); });
	m_AssetEditorPanel.SetDrawAnimationEditorCallback([this]() { m_AnimationEditorPanel.OnImGuiRenderEmbedded(); });
	m_AssetEditorPanel.SetRefreshAssetTreeCallback([this]() { if (m_ContentBrowserPanel) { m_ContentBrowserPanel->RefreshAssetTree(); } });
	m_AssistantPanel.SetSettingsCallback([this]() -> const Assistant::Settings& { return m_UISettings.GetAssistantSettings(); });
	m_AssistantPanel.SetContextCallback([this]() { return BuildAssistantContextSnapshot(); });
	m_AssistantPanel.SetApplyProposalCallback([this](const Assistant::ToolProposal& proposal) { return ApplyAssistantProposal(proposal); });
	m_ExportPanel.SetExportManager(&m_ExportManager);
	m_ProjectHealthPanel.SetSceneCallback([this]() { return m_SceneManager.ActiveScene(); });
	m_ProjectHealthPanel.SetSelectEntityCallback([this](UUID entityId)
	{
		if (Ref<Scene> scene = m_SceneManager.ActiveScene())
		{
			if (Entity entity = scene->FindEntityByUUID(entityId))
				m_SceneHierarchyPanel.SetSelectedEntity(entity);
		}
	});
	m_SceneHierarchyPanel.SetSceneChangeCallback([this]()
	{
		m_HistoryManager.CaptureSceneHistory();
		m_ProjectHealthPanel.MarkDirty();
	});
	m_SceneHierarchyPanel.SetSaveEntityTemplateCallback([this](Entity entityIn) { m_EntityTemplateManager.SaveEntityTemplate(entityIn); });
	m_SceneHierarchyPanel.SetApplyEntityTemplateCallback([this](Entity entityIn) { m_EntityTemplateManager.ApplyEntityTemplate(entityIn); });
	m_SceneHierarchyPanel.SetRevertEntityTemplateCallback([this](Entity entityIn) { m_EntityTemplateManager.RevertEntityTemplate(entityIn); });
	m_SceneHierarchyPanel.SetUnpackEntityTemplateCallback([this](Entity entityIn) { m_EntityTemplateManager.UnpackEntityTemplate(entityIn); });
	m_UIProject.SetSceneCallbacks(
		[this](AssetHandle handle) { m_SceneManager.OpenScene(handle); },
		[this]() { m_SceneManager.CloseScene(); },
		[this]() { return m_SceneManager.EditorScenePath(); });
	m_UIProject.SetBeforeChangeCallback([this]() { m_HistoryManager.CaptureSceneHistory(true); });
	m_UIProject.SetEditorSettingsDrawer([this]() { m_UISettings.DrawContent(); });
	m_UISettings.SetShortcutManager(&m_ShortcutManager);
	RegisterEditorShortcuts();
	m_ProjectManager.SetupProjectLoader();
	m_ProjectManager.LoadEditorPreferences();
	m_ProjectManager.GetLoader().SetRecentProjects(m_ProjectManager.GetRecentProjects());

	// framebuffer
    FramebufferSpecification fbSpec{};
    fbSpec.m_Attachments = { FramebufferTextureFormat::Rgba8, FramebufferTextureFormat::RedInteger, FramebufferTextureFormat::Depth };
    fbSpec.m_Width = Application::Get().GetWindow().GetWidth();
    fbSpec.m_Height = Application::Get().GetWindow().GetHeight();
    m_SceneFramebuffer = Framebuffer::Create(fbSpec);
	m_GameFramebuffer = Framebuffer::Create(fbSpec);

	// scene
	m_SceneManager.ResetToEmptyScene();

	// Project
	auto commandLineArgs = Application::Get().GetSpecification().m_CommandLineArgs;
	if (commandLineArgs.m_Count > 1)
	{
		auto projectFilePath = commandLineArgs[1];
		WHP_EDITOR_INFO(std::string("[Project] Opening project from command line: ") + projectFilePath);
		if (m_ProjectManager.OpenProject(projectFilePath))
			m_ProjectManager.GetLoader().SetLoaded(Project::GetActive() != nullptr);
	}
	// camera
    m_EditorCamera = EditorCamera(30.0f, 1.778f, 0.1f, 1000.0f);
	ConsolePanel::Initialize();
	m_StatisticsPanelAdapter = MakeScope<CallbackEditorPanel>(
		"Statistics",
		[this]() { m_UIStatistics.OnImGuiRender(m_Ts); },
		[this]() { return m_UIStatistics.IsOpen(); },
		[this](bool open) { m_UIStatistics.SetOpen(open); },
		[this]() { return m_UIStatistics.ConsumeOpenDirty(); });
	m_ConsolePanelAdapter = MakeScope<CallbackEditorPanel>(
		"Console",
		[]() { ConsolePanel::OnImGuiRender(); },
		[]() { return ConsolePanel::IsOpen(); },
		[](bool open) { ConsolePanel::SetOpen(open); },
		[]() { return ConsolePanel::ConsumeOpenDirty(); },
		false);
	static float v1 = 0, v2 = 0;
	m_PopupHandler
		.SetPopupName("Popup Testing")
		.SetHeight(300.f)
		.SetWidth(400.f)
		.Add([]() { ImGui::Text("This is a text message for popup testing. Do not mind this Window if you see that."); })
		.Add([]() { static float fv = 0; ImGui::SliderFloat("##Float value", &fv, 0.0f, 10000.0f); })
		.SameLine()
		.Add([]() { static int iv = 0; ImGui::SliderInt("##Int value", &iv, 0, 1000000); })
		.AddDualHandleSlider(0, 100, &v1, &v2)
		.AddButton([this]() { m_PopupHandler.SetShowState(false); }, "Close", 100);

}

void EditorLayer::OnDetach()
{
	WHP_PROFILE_FUNCTION();
	Input::SetRuntimeInputEnabled(false);
	Input::SetCursorMode(CursorMode::Normal);
	Input::SetCursorModeOverride(false);
	m_ProjectManager.CancelAsyncOperations(true);
	m_ExportManager.CancelExport(true);
	m_SceneManager.WriteRecoverySnapshot("Editor shutdown");
	m_ScriptManager.StopSourceWatcher();
	if (m_SceneManager.State() == SceneState::Play || m_SceneManager.State() == SceneState::Simulate)
		m_SceneManager.OnSceneStop();
	m_ProjectManager.SaveEditorPreferences();
	ConsolePanel::Shutdown();

}

glm::vec2 EditorLayer::GetGameViewRenderSize() const
{
	const int presetIndex = std::clamp(m_GameViewPresetIndex, 0, static_cast<int>(s_GameViewResolutionPresets.size()) - 1);
	const GameViewResolutionPreset& preset = s_GameViewResolutionPresets[static_cast<size_t>(presetIndex)];
	if (preset.m_Width > 0 && preset.m_Height > 0)
		return { static_cast<float>(preset.m_Width), static_cast<float>(preset.m_Height) };

	glm::vec2 freeSize = m_GameViewportSize;
	if (m_GameViewAvailableSize.x > 1.0f && m_GameViewAvailableSize.y > 1.0f)
		freeSize = m_GameViewAvailableSize;
	else if (freeSize.x <= 1.0f || freeSize.y <= 1.0f)
		freeSize = m_ViewportSize;
	return glm::max(glm::floor(freeSize), glm::vec2(1.0f));
}

void EditorLayer::ResizeFramebufferIfNeeded(const Ref<Framebuffer>& framebuffer, const glm::vec2& size)
{
	if (!framebuffer)
		return;

	const uint32_t width = glm::max(static_cast<uint32_t>(size.x), 1u);
	const uint32_t height = glm::max(static_cast<uint32_t>(size.y), 1u);
	const FramebufferSpecification& spec = framebuffer->GetSpecification();
	if (spec.m_Width != width || spec.m_Height != height)
		framebuffer->Resize(width, height);
}

void EditorLayer::RenderGameView(Timestep ts)
{
	WHP_PROFILE_FUNCTION();
	Ref<Scene> activeScene = m_SceneManager.ActiveScene();
	if (!activeScene || !m_GameFramebuffer)
		return;

	m_GameRenderSize = GetGameViewRenderSize();
	ResizeFramebufferIfNeeded(m_GameFramebuffer, m_GameRenderSize);

	m_GameFramebuffer->Bind();
	RenderCommand::SetClearColor({ 0.045f, 0.052f, 0.058f, 1.0f });
	RenderCommand::Clear();
	m_GameFramebuffer->ClearAttachment(1, -1);

	activeScene->OnViewportResize(static_cast<uint32_t>(m_GameRenderSize.x), static_cast<uint32_t>(m_GameRenderSize.y));
	switch (m_SceneManager.State())
	{
	case SceneState::Edit:
		activeScene->RenderRuntimeScene();
		break;
	case SceneState::Play:
		activeScene->OnUpdateRuntimeSystems(ts);
		activeScene->RenderRuntimeScene();
		m_SceneManager.ProcessRuntimeSceneTransition();
		break;
	case SceneState::Simulate:
		activeScene->OnUpdateSimulationSystems(ts);
		activeScene->RenderRuntimeScene();
		m_SceneManager.ProcessRuntimeSceneTransition();
		break;
	}

	m_GameFramebuffer->Unbind();
}

void EditorLayer::RenderSceneView(Timestep ts)
{
	WHP_PROFILE_FUNCTION();
	Ref<Scene> activeScene = m_SceneManager.ActiveScene();
	if (!activeScene || !m_SceneFramebuffer)
		return;

	const glm::vec2 sceneRenderSize = glm::max(glm::floor(m_ViewportSize), glm::vec2(1.0f));
	ResizeFramebufferIfNeeded(m_SceneFramebuffer, sceneRenderSize);
	m_EditorCamera.SetViewportSize(sceneRenderSize.x, sceneRenderSize.y);

	m_SceneFramebuffer->Bind();
	RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
	RenderCommand::Clear();
	m_SceneFramebuffer->ClearAttachment(1, -1);

	activeScene->OnViewportResize(static_cast<uint32_t>(sceneRenderSize.x), static_cast<uint32_t>(sceneRenderSize.y));
	if (m_ViewportFocused && !m_GizmoUsing)
		m_EditorCamera.OnUpdate(ts);

	DrawEditorGrid();
	activeScene->RenderScene(m_EditorCamera);
	OnOverlayRender();

	m_SceneFramebuffer->Unbind();
}

void EditorLayer::DrawGameViewToolbar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
	ImGui::Dummy(ImVec2(8.0f, 0.0f));
	ImGui::SameLine(0.0f, 0.0f);

	const int presetIndex = std::clamp(m_GameViewPresetIndex, 0, static_cast<int>(s_GameViewResolutionPresets.size()) - 1);
	const GameViewResolutionPreset& currentPreset = s_GameViewResolutionPresets[static_cast<size_t>(presetIndex)];
	ImGui::SetNextItemWidth(260.0f);
	if (ImGui::BeginCombo("##GameViewResolutionPreset", currentPreset.m_Name))
	{
		for (size_t i = 0; i < s_GameViewResolutionPresets.size(); ++i)
		{
			const bool selected = std::cmp_equal(m_GameViewPresetIndex, i);
			if (ImGui::Selectable(s_GameViewResolutionPresets[i].m_Name, selected))
			{
				m_GameViewPresetIndex = static_cast<int>(i);
				m_GameRenderSize = GetGameViewRenderSize();
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Game render target resolution. Free Aspect follows the Game view image area.");

	ImGui::SameLine();
	ImGui::TextDisabled("%.0f x %.0f", m_GameRenderSize.x, m_GameRenderSize.y);

	const bool runtimeViewport = m_SceneManager.State() == SceneState::Play || m_SceneManager.State() == SceneState::Simulate;
	ImGui::SameLine();
	ImGui::TextColored(runtimeViewport ? ImVec4(0.52f, 0.86f, 0.62f, 1.0f) : ImVec4(0.58f, 0.66f, 0.74f, 1.0f), runtimeViewport ? "Live" : "Preview");

	if (runtimeViewport)
	{
		ImGui::SameLine();
		const char* cursorLabel = m_ViewportCursorMode == ViewportCursorMode::Game ? "Cursor: Game" : "Cursor: Editor";
		if (ImGui::SmallButton(cursorLabel))
			ToggleViewportCursorMode();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Ctrl+Shift+M toggles whether Game view uses the runtime cursor mode.");
	}

	ImGui::PopStyleVar(2);
}

void EditorLayer::OnUpdate(Timestep ts)
{
	WHP_PROFILE_FUNCTION();
	m_Ts = ts;
	Input::SetRuntimeInputEnabled((m_SceneManager.State() == SceneState::Play || m_SceneManager.State() == SceneState::Simulate) &&
		m_GameViewportHovered &&
		m_GameViewportFocused &&
		!m_GizmoUsing);
	UpdateViewportCursorMode();
	m_ProjectManager.UpdateAsyncOperations();
	m_ExportManager.UpdateAsyncOperations();
	m_ScriptManager.ProcessSourceChanges(m_SceneManager.State() == SceneState::Edit);
	if (m_SceneManager.IsSceneDirty() && m_SceneManager.State() == SceneState::Edit)
	{
		const auto now = std::chrono::steady_clock::now();
		if (m_SceneManager.LastRecoverySnapshot() == std::chrono::steady_clock::time_point{} || now - m_SceneManager.LastRecoverySnapshot() > std::chrono::seconds(30))
			m_SceneManager.WriteRecoverySnapshot("Autosave");
	}

	Renderer2D::ResetStats();
	RenderGameView(ts);
	RenderSceneView(ts);
}

_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(4312)
void EditorLayer::OnImGuiRender()
{
	WHP_PROFILE_FUNCTION();
	ImGuizmo::BeginFrame();
	m_GizmoHovered = false;
	m_GizmoUsing = false;
	const bool projectLoaded = HasProjectLoaded();
	if (!projectLoaded)
	{
		Application::Get().GetImGuiLayer()->BlockEvents(true);

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags hubHostFlags =
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
		ImGui::Begin("Whip Hub Host", nullptr, hubHostFlags);
		DrawEditorShellTitlebar(false);
		m_ProjectManager.GetLoader().Run();
		m_ProjectManager.DrawAsyncProgressOverlay();
		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
		return;
	}

	// dockspace
	{
		static bool pOpen = true;
		static ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		if (dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
			windowFlags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Editor DockSpace", &pOpen, windowFlags);
		ImGui::PopStyleVar(3);
		DrawEditorShellTitlebar(projectLoaded);
		DrawEditorMenuBar(projectLoaded);

		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 300.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspaceId = ImGui::GetID("Editor DockSpace");
			ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);
		}
		style.WindowMinSize.x = minWinSizeX;
	}
	// scene view
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
		ImGui::Begin("Scene View");
		auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
		auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		auto viewportOffset = ImGui::GetWindowPos();
		m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
		m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };
		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		if (m_SceneManager.State() == SceneState::Edit)
			Input::SetViewportState(m_ViewportHovered, m_ViewportFocused, m_ViewportBounds[0], m_ViewportBounds[1]);
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

		UI::Image(UI::ToImGuiTextureId(m_SceneFramebuffer->GetColorAttachmentRendererId()), viewportPanelSize, ImVec2{ 0.0f, 1.0f }, ImVec2{ 1.0f, 0.0f });
		if (ImGui::BeginDragDropTarget())
		{
			bool handledDrop = false;
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEMS"))
			{
				const std::vector<UI::AssetReferencePayload> assetPayloads = UI::ReadAssetReferenceListPayload(payload);
				std::vector<std::pair<AssetHandle, int32_t>> assetReferences;
				assetReferences.reserve(assetPayloads.size());
				for (const UI::AssetReferencePayload& assetPayload : assetPayloads)
					assetReferences.emplace_back(assetPayload.m_Handle, assetPayload.m_TextureSpriteIndex);
				handledDrop = m_AssetInteractionManager.HandleViewportAssetDrops(assetReferences);
			}

			if (!handledDrop)
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const UI::AssetReferencePayload assetPayload = UI::ReadAssetReferencePayload(payload);
					handledDrop = m_AssetInteractionManager.HandleViewportAssetDrop(assetPayload.m_Handle, assetPayload.m_TextureSpriteIndex);
				}
			}

			if (!handledDrop)
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_PATH"))
				{
					std::filesystem::path RelativePath(std::string(static_cast<const char*>(payload->Data), payload->DataSize));
					std::filesystem::path absolutePath = Project::GetActiveAssetDirectory() / RelativePath;
					if (std::filesystem::is_regular_file(absolutePath))
					{
						AssetHandle handle = Project::GetActive()->GetEditorAssetManager()->GetHandleFromFilepath(RelativePath);
						if (handle == 0 && Utils::TryGetAssetTypeFromFileExtension(RelativePath.extension()) != AssetType::None)
							handle = Project::GetActive()->GetEditorAssetManager()->ImportAsset(RelativePath);
						if (!m_AssetInteractionManager.HandleViewportAssetDrop(handle))
							WHP_EDITOR_WARN("[Viewport] Drag drop failed!");
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		// gizmos
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity && m_GizmoType != -1 && m_SceneManager.State() != SceneState::Play)
		{
		    ImGuizmo::SetDrawlist();
			ImGuizmo::AllowAxisFlip(false);
		    ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, m_ViewportBounds[1].x - m_ViewportBounds[0].x, m_ViewportBounds[1].y - m_ViewportBounds[0].y);

		    // Snapping
			const int snapIndex = GizmoSnapIndex(m_GizmoType);
		    bool snap = EditorUtils::IsControlDown() && snapIndex != -1;

			ImGuizmo::OPERATION operation = static_cast<ImGuizmo::OPERATION>(m_GizmoType);

			if (selectedEntity.HasComponent<UITransformComponent>())
			{
				glm::vec2 baseCenter{ 0.0f };
				glm::vec2 baseSize{ 1.0f };
				if (Ref<Scene> activeScene = m_SceneManager.ActiveScene(); activeScene && activeScene->TryResolveUIRect(selectedEntity, baseCenter, baseSize))
				{
					ImGuizmo::SetOrthographic(true);
					glm::mat4 cameraProjection = glm::ortho(0.0f, m_ViewportSize.x, 0.0f, m_ViewportSize.y, -1.0f, 1.0f);
					glm::mat4 cameraView{ 1.0f };
					const auto& uiTransform = selectedEntity.GetComponent<UITransformComponent>();
					glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(baseCenter, 0.0f))
						* glm::rotate(glm::mat4(1.0f), glm::radians(uiTransform.m_Rotation), glm::vec3(0.0f, 0.0f, 1.0f))
						* glm::scale(glm::mat4(1.0f), glm::vec3(baseSize, 1.0f));
					const float baseRotation = uiTransform.m_Rotation;

					ImGuizmo::Manipulate(
						glm::value_ptr(cameraView),
						glm::value_ptr(cameraProjection),
						operation,
						ImGuizmo::LOCAL,
						glm::value_ptr(transform),
						nullptr,
						snap ? const_cast<float*>(glm::value_ptr(m_UISettings.GetSnapValues(static_cast<uint32_t>(snapIndex)))) : nullptr);
					m_GizmoHovered = ImGuizmo::IsOver(operation);
					m_GizmoUsing = ImGuizmo::IsUsing();

					if (m_GizmoUsing)
					{
						if (!m_HistoryManager.IsGizmoHistoryActive())
						{
							m_HistoryManager.CaptureSceneHistory();
							m_HistoryManager.SetGizmoHistoryActive(true);
						}

						glm::vec3 translation, rotation, scale;
						if (!Math::DecomposeTransform(transform, translation, rotation, scale))
							WHP_CLIENT_WARN("UI Transform Decomposing error!");

						const glm::vec2 newCenter{ translation.x, translation.y };
						const glm::vec2 newSize = glm::max(glm::abs(glm::vec2(scale.x, scale.y)), glm::vec2(1.0f));
						const float newRotation = glm::degrees(rotation.z);
						const glm::vec2 deltaCenter = newCenter - baseCenter;
						const glm::vec2 sizeRatio{
							baseSize.x != 0.0f ? newSize.x / baseSize.x : 1.0f,
							baseSize.y != 0.0f ? newSize.y / baseSize.y : 1.0f
						};
						const float deltaRotation = newRotation - baseRotation;

						std::vector<Entity> selectedEntities = m_SceneHierarchyPanel.GetSelectedEntities();
						if (std::ranges::find(selectedEntities, selectedEntity) == selectedEntities.end())
							selectedEntities.push_back(selectedEntity);

						for (Entity selected : selectedEntities)
						{
							if (!selected || !selected.HasComponent<UITransformComponent>() || selected == selectedEntity)
								continue;

							glm::vec2 selectedCenter{ 0.0f };
							glm::vec2 selectedSize{ 1.0f };
							if (!activeScene->TryResolveUIRect(selected, selectedCenter, selectedSize))
								continue;

							const float selectedRotation = selected.GetComponent<UITransformComponent>().m_Rotation;
							activeScene->ApplyUIRectTransform(selected, selectedCenter + deltaCenter, selectedSize * sizeRatio, selectedRotation + deltaRotation);
						}

						activeScene->ApplyUIRectTransform(selectedEntity, newCenter, newSize, newRotation);
					}
				}
			}
			else if (selectedEntity.HasComponent<TransformComponent>())
			{
				ImGuizmo::SetOrthographic(false);
				const glm::mat4& cameraProjection = m_EditorCamera.GetProjection();
				glm::mat4 cameraView = m_EditorCamera.GetViewMatrix();
				auto& tc = selectedEntity.GetComponent<TransformComponent>();
				glm::mat4 transform = tc.GetTransform();
				const glm::vec3 baseTranslation = tc.m_Translation;
				const glm::vec3 baseRotation = tc.m_Rotation;
				const glm::vec3 baseScale = tc.m_Scale;

				ImGuizmo::Manipulate(
					glm::value_ptr(cameraView),
					glm::value_ptr(cameraProjection),
					operation,
					ImGuizmo::LOCAL,
					glm::value_ptr(transform),
					nullptr,
					snap ? const_cast<float*>(glm::value_ptr(m_UISettings.GetSnapValues(static_cast<uint32_t>(snapIndex)))) : nullptr);
				m_GizmoHovered = ImGuizmo::IsOver(operation);
				m_GizmoUsing = ImGuizmo::IsUsing();

				if (m_GizmoUsing)
				{
					if (!m_HistoryManager.IsGizmoHistoryActive())
					{
						m_HistoryManager.CaptureSceneHistory();
						m_HistoryManager.SetGizmoHistoryActive(true);
					}

					glm::vec3 translation, rotation, scale;
					if (!Math::DecomposeTransform(transform, translation, rotation, scale))
						WHP_CLIENT_WARN("Transform Decomposing error!");

					glm::vec3 deltaTranslation = translation - baseTranslation;
					glm::vec3 deltaRotation = rotation - baseRotation;
					glm::vec3 scaleRatio = glm::vec3(1.0f);
					scaleRatio.x = baseScale.x != 0.0f ? scale.x / baseScale.x : 1.0f;
					scaleRatio.y = baseScale.y != 0.0f ? scale.y / baseScale.y : 1.0f;
					scaleRatio.z = baseScale.z != 0.0f ? scale.z / baseScale.z : 1.0f;

					std::vector<Entity> selectedEntities = m_SceneHierarchyPanel.GetSelectedEntities();
					if (std::ranges::find(selectedEntities, selectedEntity) == selectedEntities.end())
						selectedEntities.push_back(selectedEntity);

					for (Entity selected : selectedEntities)
					{
						if (!selected || !selected.HasComponent<TransformComponent>() || selected.HasComponent<UITransformComponent>())
							continue;
						if (selected == selectedEntity)
							continue;

						auto& selectedTransform = selected.GetComponent<TransformComponent>();
						selectedTransform.m_Translation += deltaTranslation;
						selectedTransform.m_Rotation += deltaRotation;
						selectedTransform.m_Scale *= scaleRatio;
					}

					tc.m_Translation = translation;
					tc.m_Rotation = rotation;
					tc.m_Scale = scale;
				}
			}
		}
		if (!m_GizmoUsing)
			m_HistoryManager.SetGizmoHistoryActive(false);
		UIToolbar();
		ImGui::End();
		ImGui::PopStyleVar();
	} // scene view

	// game view
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
		ImGui::Begin("Game");
		m_GameViewportFocused = ImGui::IsWindowFocused();
		DrawGameViewToolbar();
		ImGui::Separator();
		const ImVec2 gamePanelSize = ImGui::GetContentRegionAvail();
		const glm::vec2 gameRegionSize{ glm::max(gamePanelSize.x, 1.0f), glm::max(gamePanelSize.y, 1.0f) };
		m_GameViewAvailableSize = gameRegionSize;
		const glm::vec2 imageSize = FitSizeToRegion(m_GameRenderSize, gameRegionSize);
		const ImVec2 imageOffset{
			glm::max((gamePanelSize.x - imageSize.x) * 0.5f, 0.0f),
			glm::max((gamePanelSize.y - imageSize.y) * 0.5f, 0.0f)
		};
		const ImVec2 imageCursor = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(imageCursor.x + imageOffset.x, imageCursor.y + imageOffset.y));
		if (imageSize.x > 0.0f && imageSize.y > 0.0f)
			UI::Image(UI::ToImGuiTextureId(m_GameFramebuffer->GetColorAttachmentRendererId()), ImVec2(imageSize.x, imageSize.y), ImVec2{ 0.0f, 1.0f }, ImVec2{ 1.0f, 0.0f });

		const ImVec2 imageMin = ImGui::GetItemRectMin();
		const ImVec2 imageMax = ImGui::GetItemRectMax();
		m_GameViewportBounds[0] = { imageMin.x, imageMin.y };
		m_GameViewportBounds[1] = { imageMax.x, imageMax.y };
		m_GameViewportSize = { glm::max(imageSize.x, 1.0f), glm::max(imageSize.y, 1.0f) };
		m_GameViewportHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

		const bool runtimeViewport = m_SceneManager.State() == SceneState::Play || m_SceneManager.State() == SceneState::Simulate;
		if (runtimeViewport)
			Input::SetViewportState(m_GameViewportHovered, m_GameViewportFocused, m_GameViewportBounds[0], m_GameViewportBounds[1]);
		else if (m_GameViewportHovered)
			ImGui::SetTooltip("Play or simulate to route input through Game view.");

		ImGui::End();
		ImGui::PopStyleVar();
	} // game view

	const bool runtimeViewport = m_SceneManager.State() == SceneState::Play || m_SceneManager.State() == SceneState::Simulate;
	const bool interactiveViewportHovered = m_ViewportHovered || (runtimeViewport && m_GameViewportHovered);
	Application::Get().GetImGuiLayer()->BlockEvents(!interactiveViewportHovered || m_GizmoHovered || m_GizmoUsing);
	UpdateViewportCursorMode();

	m_UIProject.OnImGuiRender(); // should be in dockspace

	ImGui::End(); // dockspace

	// other renders
	RebuildEditorPanelRegistry();
	m_PanelManager.OnImGuiRender();
	if (Application::Get().GetImGuiLayer()->IsBlockingEvents() && ImGui::GetIO().WantCaptureKeyboard)
	{
		const bool hasActiveWidget = Application::Get().GetImGuiLayer()->GetActiveWidgetID() != 0;
		m_ShortcutManager.HandleImGuiShortcuts(hasActiveWidget);
	}
	DrawCommandPalette();
	if (m_UISettings.ConsumeDirty()
		|| m_ShortcutManager.ConsumeDirty()
		|| m_PanelManager.ConsumeOpenDirty()
		|| m_AnimationEditorPanel.ConsumeLayoutDirty()
		|| m_AssetEditorPanel.ConsumeLayoutDirty()
		|| (m_ContentBrowserPanel && m_ContentBrowserPanel->ConsumePreferencesDirty()))
		m_ProjectManager.SaveEditorPreferences();
	m_PopupHandler.OnImGuiRender();
	m_ProjectManager.DrawAsyncProgressOverlay();
	m_ExportManager.DrawAsyncProgressOverlay();

}
_WHP_PRAGMA_WARNING(pop)

void EditorLayer::OnEvent(Event& event)
{
	if (m_ViewportHovered && !m_GizmoHovered && !m_GizmoUsing && Application::Get().GetImGuiLayer()->GetActiveWidgetID() == 0)
		m_EditorCamera.OnEvent(event);
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>([this]<typename... T0>(T0&&... args) -> decltype(auto) { return m_EventManager.OnKeyPressed(std::forward<T0>(args)...); });
    dispatcher.Dispatch<MouseButtonPressedEvent>([this]<typename... T0>(T0&&... args) -> decltype(auto) { return m_EventManager.OnMouseButtonPressed(std::forward<T0>(args)...); });
	dispatcher.Dispatch<WindowDropEvent>([this]<typename... T0>(T0&&... args) -> decltype(auto) { return m_EventManager.OnWindowDrop(std::forward<T0>(args)...); });
}

void EditorLayer::DrawEditorShellTitlebar(bool projectLoaded)
{
	constexpr float TitlebarHeight = 30.0f;
	constexpr float ControlWidth = 46.0f;

	ImGui::BeginChild("##EditorShellTitlebar", ImVec2(0.0f, TitlebarHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const ImVec2 min = ImGui::GetWindowPos();
	const ImVec2 size = ImGui::GetWindowSize();
	const ImVec2 max(min.x + size.x, min.y + size.y);
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	constexpr ImU32 titleTop = IM_COL32(21, 29, 37, 255);
	constexpr ImU32 titleBottom = IM_COL32(8, 12, 16, 255);
	drawList->AddRectFilledMultiColor(min, max, titleTop, titleTop, titleBottom, titleBottom);
	drawList->AddLine(ImVec2(min.x, max.y - 1.0f), ImVec2(max.x, max.y - 1.0f), IM_COL32(46, 58, 70, 210), 1.0f);
	drawList->AddRectFilled(ImVec2(min.x, min.y), ImVec2(max.x, min.y + 2.0f), IM_COL32(180, 196, 214, 210), 0.0f);

	const float controlStartX = max.x - ControlWidth * 3.0f;

	const ImVec2 logoMin(min.x + 12.0f, min.y + 6.0f);
	DrawWhipBrandMark(drawList, logoMin);

	std::string title = "Whip Editor";
	if (projectLoaded && Project::GetActive())
		title += "  /  " + Project::GetActive()->GetConfig().m_Name;
	else
		title += "  /  Hub";

	const float titleX = logoMin.x + 40.0f;
	drawList->AddText(ImVec2(titleX, min.y + 5.0f), IM_COL32(240, 244, 248, 245), title.c_str());
	drawList->AddText(ImVec2(titleX + ImGui::CalcTextSize(title.c_str()).x + 10.0f, min.y + 5.0f), IM_COL32(132, 150, 166, 210), projectLoaded ? "Editor" : "Project Launcher");

	Window& window = Application::Get().GetWindow();
	ImGui::SetCursorScreenPos(ImVec2(controlStartX, min.y));
	if (DrawShellWindowControlButton("##ShellMinimize", ShellWindowControl::Minimize, ImVec2(ControlWidth, TitlebarHeight)))
		window.Minimize();
	ImGui::SameLine(0.0f, 0.0f);
	if (DrawShellWindowControlButton("##ShellMaximize", window.IsMaximized() ? ShellWindowControl::Restore : ShellWindowControl::Maximize, ImVec2(ControlWidth, TitlebarHeight)))
	{
		if (window.IsMaximized())
			window.Restore();
		else
			window.Maximize();
	}
	ImGui::SameLine(0.0f, 0.0f);
	if (DrawShellWindowControlButton("##ShellClose", ShellWindowControl::Close, ImVec2(ControlWidth, TitlebarHeight)))
		Application::Get().Close();

	ImGui::EndChild();
}

void EditorLayer::DrawEditorMenuBar(bool projectLoaded)
{
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.040f, 0.055f, 0.070f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.040f, 0.055f, 0.070f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::BeginChild("##EditorShellMenuBar", ImVec2(0.0f, 28.0f), false, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleVar();

	if (ImGui::BeginMenuBar())
	{
		auto drawMenuAction = [this](UI::EditorShortcutAction action, const char* label = nullptr)
			{
				const std::string shortcutId = EditorActionShortcutId(action);
				std::string shortcut = m_ShortcutManager.GetShortcutLabel(shortcutId);
				const bool available = IsEditorActionAvailable(action);
				ImGui::BeginDisabled(!available);
				bool clicked = ImGui::MenuItem(label ? label : UI::UISettings::GetActionDisplayName(action), shortcut.c_str());
				ImGui::EndDisabled();
				m_ShortcutManager.DrawShortcutTooltip(shortcutId, available ? "Run command" : "Command is unavailable in the current editor state.");
				if (clicked)
					ExecuteEditorAction(action);
			};

		if (ImGui::BeginMenu("File"))
		{
			drawMenuAction(UI::EditorShortcutAction::OpenProject);
			drawMenuAction(UI::EditorShortcutAction::SaveProject);
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::NewScene);
			drawMenuAction(UI::EditorShortcutAction::SaveScene);
			drawMenuAction(UI::EditorShortcutAction::SaveSceneAs, "Save Scene As...");
			drawMenuAction(UI::EditorShortcutAction::CloseScene);
			ImGui::Separator();
			if (ImGui::MenuItem("Restart"))
				Application::Get().SubmitToNextTick([]() { Application::Get().Restart(); });
			if (ImGui::MenuItem("Exit"))
				Application::Get().Close();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			drawMenuAction(UI::EditorShortcutAction::OpenCommandPalette);
			drawMenuAction(UI::EditorShortcutAction::OpenSettings, "Settings");
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::Undo);
			drawMenuAction(UI::EditorShortcutAction::Redo);
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::SelectAll);
			drawMenuAction(UI::EditorShortcutAction::Copy);
			drawMenuAction(UI::EditorShortcutAction::Paste);
			drawMenuAction(UI::EditorShortcutAction::Cut);
			drawMenuAction(UI::EditorShortcutAction::DuplicateEntity);
			drawMenuAction(UI::EditorShortcutAction::DeleteEntity);
			ImGui::Separator();
			ImGui::BeginDisabled(!projectLoaded);
			if (ImGui::MenuItem("Show Animation Editor"))
				m_AnimationEditorPanel.Open();
			ImGui::EndDisabled();
			if (ImGui::MenuItem("Show Test Popup"))
				m_PopupHandler.SetShowState(true);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Script"))
		{
			drawMenuAction(UI::EditorShortcutAction::ReloadScripts, "Reload Assembly");
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Project"))
		{
			drawMenuAction(UI::EditorShortcutAction::OpenSettings, "Settings");
			ImGui::Separator();
			drawMenuAction(UI::EditorShortcutAction::OpenProject);
			drawMenuAction(UI::EditorShortcutAction::SaveProject);
			ImGui::Separator();
			ImGui::BeginDisabled(!projectLoaded);
			if (ImGui::MenuItem("Build & Export"))
				m_ExportPanel.Open();
			ImGui::EndDisabled();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("AI"))
		{
			const std::string assistantShortcut = m_ShortcutManager.GetShortcutLabel("assistant.focus_prompt");
			if (ImGui::MenuItem("Whip Assistant", assistantShortcut.c_str(), m_AssistantPanel.IsOpen()))
			{
				m_AssistantPanel.SetOpen(true);
				m_AssistantPanel.FocusPrompt();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Window"))
		{
			RebuildEditorPanelRegistry();
			m_PanelManager.DrawAddPanelMenu(projectLoaded);
			ImGui::Separator();
			ImGui::BeginDisabled(!projectLoaded);
			ImGui::BeginDisabled(!m_AssetEditorPanel.HasOpenEditors());
			if (ImGui::MenuItem("Close Asset Editors"))
				m_AssetEditorPanel.CloseAll();
			ImGui::EndDisabled();
			ImGui::EndDisabled();
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	ImGui::EndChild();
	ImGui::PopStyleColor(2);
}

void EditorLayer::OpenCommandPalette()
{
	m_CommandPaletteOpen = true;
	m_CommandPaletteFocusSearch = true;
	m_CommandPaletteFilter[0] = '\0';
}

void EditorLayer::DrawCommandPalette()
{
	if (!m_CommandPaletteOpen)
		return;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.22f), ImGuiCond_Appearing, ImVec2(0.5f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(680.0f, 460.0f), ImGuiCond_Appearing);

	bool open = true;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
	if (ImGui::Begin("Command Palette", &open, flags))
	{
		if (m_CommandPaletteFocusSearch)
		{
			ImGui::SetKeyboardFocusHere();
			m_CommandPaletteFocusSearch = false;
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##CommandPaletteSearch", "Search commands...", m_CommandPaletteFilter, sizeof(m_CommandPaletteFilter));
		ImGui::SetNextItemWidth(180.0f);
		if (ImGui::BeginCombo("##CommandPaletteScope", m_CommandPaletteScopeFilter < 0 ? "All Scopes" : EditorShortcutManager::GetScopeName(static_cast<EditorShortcutScope>(m_CommandPaletteScopeFilter))))
		{
			if (ImGui::Selectable("All Scopes", m_CommandPaletteScopeFilter < 0))
				m_CommandPaletteScopeFilter = -1;
			for (EditorShortcutScope scope : {
				EditorShortcutScope::Global,
				EditorShortcutScope::Viewport,
				EditorShortcutScope::SceneHierarchy,
				EditorShortcutScope::ContentBrowser,
				EditorShortcutScope::AssetEditor,
				EditorShortcutScope::AnimationEditor,
				EditorShortcutScope::Console,
				EditorShortcutScope::Assistant,
				EditorShortcutScope::Statistics,
				EditorShortcutScope::ProjectHub })
			{
				const int scopeIndex = static_cast<int>(scope);
				if (ImGui::Selectable(EditorShortcutManager::GetScopeName(scope), m_CommandPaletteScopeFilter == scopeIndex))
					m_CommandPaletteScopeFilter = scopeIndex;
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Runnable only", &m_CommandPaletteAvailableOnly);
		ImGui::SameLine();
		ImGui::TextDisabled("Enter runs first result");
		ImGui::Spacing();
		ImGui::Separator();

		std::string firstAvailableShortcutId;

		if (ImGui::BeginChild("##CommandPaletteResults", ImVec2(0.0f, 0.0f), false))
		{
			bool hasVisibleCommand = false;
			if (ImGui::BeginTable("##CommandPaletteTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
			{
				ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 130.0f);
				ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthFixed, 150.0f);
				ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 150.0f);

				for (const EditorShortcut& shortcut : m_ShortcutManager.GetShortcuts())
				{
					if (shortcut.m_Options.m_HiddenFromCommandPalette || !ShortcutMatchesCommandFilter(shortcut, m_CommandPaletteFilter))
						continue;
					if (m_CommandPaletteScopeFilter >= 0 && static_cast<int>(shortcut.m_Scope) != m_CommandPaletteScopeFilter)
						continue;

					const bool available = m_ShortcutManager.IsShortcutAvailable(shortcut.m_Id, false);
					if (m_CommandPaletteAvailableOnly && !available)
						continue;

					hasVisibleCommand = true;
					if (available && firstAvailableShortcutId.empty())
						firstAvailableShortcutId = shortcut.m_Id;

					ImGui::PushID(shortcut.m_Id.c_str());
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::BeginDisabled(!available);
					if (ImGui::Selectable(shortcut.m_DisplayName.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, 30.0f)))
					{
						if (m_ShortcutManager.ExecuteShortcut(shortcut.m_Id, false, true, true) && shortcut.m_Id != "global.open_command_palette")
							m_CommandPaletteOpen = false;
					}
					m_ShortcutManager.DrawShortcutTooltip(shortcut.m_Id, available ? "Run command" : "This command is not currently runnable.");
					ImGui::TableNextColumn();
					ImGui::TextDisabled("%s", shortcut.m_Category.c_str());
					ImGui::TableNextColumn();
					ImGui::TextDisabled("%s", EditorShortcutManager::GetScopeName(shortcut.m_Scope));
					ImGui::TableNextColumn();
					const std::string shortcutLabel = EditorShortcutManager::ShortcutLabel(shortcut.m_Binding);
					if (m_ShortcutManager.HasConflict(shortcut.m_Id))
						ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "%s", m_ShortcutManager.GetConflictDescription(shortcut.m_Id).c_str());
					else if (!available)
						ImGui::TextDisabled("Unavailable");
					else
						ImGui::TextDisabled("%s", shortcutLabel.c_str());
					ImGui::EndDisabled();
					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			if (!hasVisibleCommand)
				ImGui::TextDisabled("No commands found.");
		}
		ImGui::EndChild();

		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
			m_CommandPaletteOpen = false;
		if (!firstAvailableShortcutId.empty() && ImGui::IsKeyPressed(ImGuiKey_Enter))
		{
			if (m_ShortcutManager.ExecuteShortcut(firstAvailableShortcutId, false, true, true) && firstAvailableShortcutId != "global.open_command_palette")
				m_CommandPaletteOpen = false;
		}
	}
	ImGui::End();

	if (!open)
		m_CommandPaletteOpen = false;
}

Assistant::ContextSnapshot EditorLayer::BuildAssistantContextSnapshot() const
{
	Assistant::ContextSnapshot context;
	if (Ref<Project> activeProject = Project::GetActive())
	{
		context.m_HasProject = true;
		context.m_ProjectName = activeProject->GetConfig().m_Name;
		AppendAssistantAssetContext(activeProject, context);
	}

	if (m_SceneManager.EditorScene())
	{
		context.m_HasScene = true;
		context.m_ScenePath = m_SceneManager.EditorScenePath().empty() ? "Unsaved Scene" : m_SceneManager.EditorScenePath().generic_string();
		AppendAssistantSceneContext(m_SceneManager.EditorScene(), context);
	}

	if (Entity selected = m_SceneHierarchyPanel.GetSelectedEntity(); selected)
	{
		context.m_HasSelection = true;
		context.m_SelectedEntity = static_cast<uint64_t>(selected.GetUUID());
		context.m_SelectedEntityName = selected.GetName();

		AppendAssistantComponentNames(selected, context.m_SelectedComponents);
		if (selected.HasComponent<ScriptComponent>())
		{
			const auto& script = selected.GetComponent<ScriptComponent>();
			if (!script.m_ClassName.empty())
			{
				context.m_HasSelectedScript = true;
				context.m_SelectedScriptClass = script.m_ClassName;
				const std::filesystem::path scriptPath = FindScriptSourcePath(script.m_ClassName);
				if (!scriptPath.empty())
				{
					context.m_SelectedScriptPath = MakeAssistantScriptPath(scriptPath);
					std::string source;
					if (ReadTextFile(scriptPath, source))
					{
						context.m_HasSelectedScriptSource = true;
						context.m_SelectedScriptSource = std::move(source);
					}
				}
			}
		}
	}

	context.m_RecentConsole = ConsolePanel::GetRecentMessages(8);
	return context;
}

bool EditorLayer::ApplyAssistantProposal(const Assistant::ToolProposal& proposal)
{
	if (!HasProjectLoaded())
		return false;

	if (proposal.m_Kind == Assistant::ToolKind::EditScript)
	{
		const std::filesystem::path scriptPath = ResolveAssistantScriptEditPath(proposal.m_ScriptPath);
		if (scriptPath.empty() || proposal.m_ScriptContent.empty())
			return false;

		m_ScriptManager.StopSourceWatcher();
		std::ofstream output(scriptPath, std::ios::binary | std::ios::trunc);
		if (!output)
		{
			m_ScriptManager.StartSourceWatcher();
			return false;
		}

		output << proposal.m_ScriptContent;
		output.close();
		if (!output)
		{
			m_ScriptManager.StartSourceWatcher();
			return false;
		}

		WHP_EDITOR_INFO(std::string("[Whip Assistant] Applied script edit: ") + scriptPath.string());
		const bool sceneEditable = m_SceneManager.State() == SceneState::Edit;
		m_ScriptManager.ReloadAssembly(true, sceneEditable);
		if (!sceneEditable)
			m_ScriptManager.StartSourceWatcher();
		return true;
	}

	if (m_SceneManager.State() != SceneState::Edit || !m_SceneManager.EditorScene())
		return false;

	auto findTarget = [this, &proposal]() -> Entity
	{
		if (proposal.m_TargetEntity != 0)
			if (Entity entity = m_SceneManager.EditorScene()->FindEntityByUUID(UUID(proposal.m_TargetEntity)))
				return entity;
		return m_SceneHierarchyPanel.GetSelectedEntity();
	};

	switch (proposal.m_Kind)
	{
	case Assistant::ToolKind::CreateEntity:
	{
		m_HistoryManager.CaptureSceneHistory();
		Entity entity = m_SceneManager.EditorScene()->CreateEntity(proposal.m_EntityName.empty() ? "AI Entity" : proposal.m_EntityName);
		if (proposal.m_HasTransform && entity.HasComponent<TransformComponent>())
		{
			auto& transform = entity.GetComponent<TransformComponent>();
			transform.m_Translation = proposal.m_Translation;
			transform.m_Rotation = proposal.m_Rotation;
			transform.m_Scale = proposal.m_Scale;
		}
		const std::string lowerName = LowerCopy(entity.GetName());
		if (lowerName.find("camera") != std::string::npos && !entity.HasComponent<CameraComponent>())
			entity.AddComponent<CameraComponent>();
		m_SceneHierarchyPanel.SetSelectedEntity(entity);
		m_SceneManager.MarkDirty();
		return true;
	}
	case Assistant::ToolKind::AddComponent:
	{
		Entity target = findTarget();
		if (!target)
			return false;

		bool added = false;
		auto addComponent = [this, &target]<typename T>()
		{
			if (target.HasComponent<T>())
				return false;
			m_HistoryManager.CaptureSceneHistory();
			target.AddComponent<T>();
			return true;
		};
		if (proposal.m_ComponentName == "Sprite Renderer" && !target.HasComponent<SpriteRendererComponent>())
			added = addComponent.operator()<SpriteRendererComponent>();
		else if (proposal.m_ComponentName == "Circle Renderer" && !target.HasComponent<CircleRendererComponent>())
			added = addComponent.operator()<CircleRendererComponent>();
		else if (proposal.m_ComponentName == "Text Renderer" && !target.HasComponent<TextComponent>())
			added = addComponent.operator()<TextComponent>();
		else if (proposal.m_ComponentName == "Camera" && !target.HasComponent<CameraComponent>())
			added = addComponent.operator()<CameraComponent>();
		else if (proposal.m_ComponentName == "Script" && !target.HasComponent<ScriptComponent>())
			added = addComponent.operator()<ScriptComponent>();
		else if (proposal.m_ComponentName == "Animator" && !target.HasComponent<AnimatorComponent>())
			added = addComponent.operator()<AnimatorComponent>();
		else if (proposal.m_ComponentName == "Rigidbody2D" && !target.HasComponent<Rigidbody2DComponent>())
			added = addComponent.operator()<Rigidbody2DComponent>();
		else if (proposal.m_ComponentName == "BoxCollider2D" && !target.HasComponent<BoxCollider2DComponent>())
			added = addComponent.operator()<BoxCollider2DComponent>();
		else if (proposal.m_ComponentName == "CircleCollider2D" && !target.HasComponent<CircleCollider2DComponent>())
			added = addComponent.operator()<CircleCollider2DComponent>();
		else if (proposal.m_ComponentName == "Audio" && !target.HasComponent<AudioComponent>())
			added = addComponent.operator()<AudioComponent>();

		if (!added)
			return false;
		m_SceneHierarchyPanel.SetSelectedEntity(target);
		m_SceneManager.MarkDirty();
		return true;
	}
	case Assistant::ToolKind::SetTransform:
	{
		Entity target = findTarget();
		if (!target || !target.HasComponent<TransformComponent>() || !proposal.m_HasTransform)
			return false;
		m_HistoryManager.CaptureSceneHistory();
		auto& transform = target.GetComponent<TransformComponent>();
		transform.m_Translation = proposal.m_Translation;
		transform.m_Rotation = proposal.m_Rotation;
		transform.m_Scale = proposal.m_Scale;
		m_SceneHierarchyPanel.SetSelectedEntity(target);
		m_SceneManager.MarkDirty();
		return true;
	}
	case Assistant::ToolKind::EditComponent:
	{
		Entity target = findTarget();
		if (!target || proposal.m_ComponentName.empty() || proposal.m_ComponentFields.empty())
			return false;

		m_HistoryManager.CaptureSceneHistory();
		bool changed = false;
		for (const Assistant::ComponentFieldEdit& edit : proposal.m_ComponentFields)
			changed |= ApplyAssistantComponentField(target, proposal.m_ComponentName, edit);

		if (!changed)
			return false;

		m_SceneHierarchyPanel.SetSelectedEntity(target);
		m_SceneManager.MarkDirty();
		return true;
	}
	case Assistant::ToolKind::AssetOperation:
	{
		Entity target = findTarget();
		if (!target || proposal.m_AssetOperation != "assign_asset")
			return false;

		m_HistoryManager.CaptureSceneHistory();
		if (!ApplyAssistantAssetOperation(target, proposal))
			return false;

		m_SceneHierarchyPanel.SetSelectedEntity(target);
		m_SceneManager.MarkDirty();
		return true;
	}
	case Assistant::ToolKind::CreateSpriteLevel:
	{
		Entity lastCreated;
		m_HistoryManager.CaptureSceneHistory();
		if (!ApplyAssistantSpriteLevel(proposal, m_SceneManager.EditorScene(), lastCreated))
			return false;

		if (lastCreated)
			m_SceneHierarchyPanel.SetSelectedEntity(lastCreated);
		m_SceneManager.MarkDirty();
		return true;
	}
	case Assistant::ToolKind::EditScript:
	case Assistant::ToolKind::None:
		return false;
	}
	return false;
}

bool EditorLayer::ExecuteEditorAction(UI::EditorShortcutAction action)
{
	if (!IsEditorActionAvailable(action))
		return false;

	switch (action)
	{
	case UI::EditorShortcutAction::OpenCommandPalette:
		OpenCommandPalette();
		return true;
	case UI::EditorShortcutAction::OpenSettings:
		m_UIProject.Show(UI::UIProject::UISettings, [this]() { m_ProjectManager.FinishProjectSettings(); });
		return true;
	case UI::EditorShortcutAction::OpenProject:
		m_ProjectManager.OpenProject();
		return true;
	case UI::EditorShortcutAction::NewScene:
		m_SceneManager.NewScene();
		return true;
	case UI::EditorShortcutAction::SaveScene:
		m_SceneManager.SaveScene();
		return true;
	case UI::EditorShortcutAction::SaveSceneAs:
		m_SceneManager.SaveSceneAs();
		return true;
	case UI::EditorShortcutAction::SaveProject:
		m_ProjectManager.SaveProject();
		return true;
	case UI::EditorShortcutAction::CloseScene:
		m_SceneManager.CloseScene();
		return true;
	case UI::EditorShortcutAction::ReloadScripts:
		m_ScriptManager.ReloadAssembly(true, m_SceneManager.State() == SceneState::Edit);
		return true;
	case UI::EditorShortcutAction::DuplicateEntity:
		m_HistoryManager.DuplicateSelection();
		return true;
	case UI::EditorShortcutAction::DeleteEntity:
		m_HistoryManager.DeleteSelection();
		return true;
	case UI::EditorShortcutAction::Undo:
		m_HistoryManager.UndoScene();
		return true;
	case UI::EditorShortcutAction::Redo:
		m_HistoryManager.RedoScene();
		return true;
	case UI::EditorShortcutAction::SelectAll:
		m_HistoryManager.SelectAll();
		return true;
	case UI::EditorShortcutAction::Copy:
		m_HistoryManager.CopySelection();
		return true;
	case UI::EditorShortcutAction::Paste:
		m_HistoryManager.PasteSelection();
		return true;
	case UI::EditorShortcutAction::Cut:
		m_HistoryManager.CutSelection();
		return true;
	case UI::EditorShortcutAction::Play:
		if (m_SceneManager.State() == SceneState::Edit)
			m_SceneManager.OnScenePlay();
		else if (m_SceneManager.State() == SceneState::Play)
			m_SceneManager.OnSceneStop();
		return true;
	case UI::EditorShortcutAction::Simulate:
		if (m_SceneManager.State() == SceneState::Edit)
			m_SceneManager.OnSceneSimulate();
		else if (m_SceneManager.State() == SceneState::Simulate)
			m_SceneManager.OnSceneStop();
		return true;
	case UI::EditorShortcutAction::Stop:
		m_SceneManager.OnSceneStop();
		return true;
	case UI::EditorShortcutAction::Pause:
		m_SceneManager.ActiveScene()->SetPaused(!m_SceneManager.ActiveScene()->IsPaused());
		return true;
	case UI::EditorShortcutAction::GizmoNone:
		m_GizmoType = -1;
		return true;
	case UI::EditorShortcutAction::GizmoTranslate:
		m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
		return true;
	case UI::EditorShortcutAction::GizmoRotate:
		m_GizmoType = ImGuizmo::OPERATION::ROTATE;
		return true;
	case UI::EditorShortcutAction::GizmoScale:
		m_GizmoType = ImGuizmo::OPERATION::SCALE;
		return true;
	case UI::EditorShortcutAction::Count:
		return false;
	}
	return false;
}

bool EditorLayer::IsEditorActionAvailable(UI::EditorShortcutAction action) const
{
	const bool projectLoaded = HasProjectLoaded();
	const bool editMode = m_SceneManager.State() == SceneState::Edit;
	const bool hasSelection = (bool)m_SceneHierarchyPanel.GetSelectedEntity();

	switch (action)
	{
	case UI::EditorShortcutAction::OpenProject:
	case UI::EditorShortcutAction::OpenCommandPalette:
		return true;
	case UI::EditorShortcutAction::OpenSettings:
		return projectLoaded;
	case UI::EditorShortcutAction::NewScene:
	case UI::EditorShortcutAction::SaveScene:
	case UI::EditorShortcutAction::SaveSceneAs:
	case UI::EditorShortcutAction::SaveProject:
	case UI::EditorShortcutAction::CloseScene:
	case UI::EditorShortcutAction::ReloadScripts:
	case UI::EditorShortcutAction::SelectAll:
		return projectLoaded && editMode;
	case UI::EditorShortcutAction::DuplicateEntity:
	case UI::EditorShortcutAction::DeleteEntity:
	case UI::EditorShortcutAction::Copy:
	case UI::EditorShortcutAction::Cut:
		return projectLoaded && editMode && hasSelection;
	case UI::EditorShortcutAction::Paste:
		return projectLoaded && editMode && m_HistoryManager.HasClipboard();
	case UI::EditorShortcutAction::Undo:
		return projectLoaded && editMode && m_HistoryManager.CanUndo();
	case UI::EditorShortcutAction::Redo:
		return projectLoaded && editMode && m_HistoryManager.CanRedo();
	case UI::EditorShortcutAction::Play:
		return projectLoaded && m_SceneManager.State() != SceneState::Simulate;
	case UI::EditorShortcutAction::Simulate:
		return projectLoaded && m_SceneManager.State() != SceneState::Play;
	case UI::EditorShortcutAction::Stop:
		return m_SceneManager.State() == SceneState::Play || m_SceneManager.State() == SceneState::Simulate;
	case UI::EditorShortcutAction::Pause:
		return projectLoaded && m_SceneManager.State() != SceneState::Edit;
	case UI::EditorShortcutAction::GizmoNone:
	case UI::EditorShortcutAction::GizmoTranslate:
	case UI::EditorShortcutAction::GizmoRotate:
	case UI::EditorShortcutAction::GizmoScale:
		return projectLoaded && editMode && !m_GizmoUsing;
	case UI::EditorShortcutAction::Count:
		return false;
	}
	return false;
}

void EditorLayer::RegisterEditorShortcuts()
{
	m_ShortcutManager.Clear();

	for (size_t i = 0; i < UI::UISettings::ActionCount; ++i)
	{
		const UI::EditorShortcutAction action = static_cast<UI::EditorShortcutAction>(i);
		EditorShortcutOptions options;
		options.m_AllowWhenActiveWidget =
			action == UI::EditorShortcutAction::OpenCommandPalette ||
			action == UI::EditorShortcutAction::Play ||
			action == UI::EditorShortcutAction::Simulate ||
			action == UI::EditorShortcutAction::Stop ||
			action == UI::EditorShortcutAction::Pause;
		options.m_AllowWhenTextInput = action == UI::EditorShortcutAction::OpenCommandPalette;

		m_ShortcutManager.Add(
			EditorShortcutScope::Global,
			EditorActionShortcutId(action),
			UI::UISettings::GetActionDisplayName(action),
			UI::UISettings::GetActionCategory(action),
			m_UISettings.GetShortcutBinding(action),
			[this, action]() { return ExecuteEditorAction(action); },
			[this, action]() { return IsEditorActionAvailable(action); },
			{},
			options);
	}

	m_SceneHierarchyPanel.RegisterShortcuts(m_ShortcutManager);
	m_AnimationEditorPanel.RegisterShortcuts(m_ShortcutManager);
	m_AssetEditorPanel.RegisterShortcuts(m_ShortcutManager);
	m_AssistantPanel.RegisterShortcuts(m_ShortcutManager);
	m_ExportPanel.RegisterShortcuts(m_ShortcutManager);
	m_ProjectHealthPanel.RegisterShortcuts(m_ShortcutManager);
	m_ShortcutManager.Add(
		EditorShortcutScope::Viewport,
		"viewport.toggle_physics_colliders",
		"Toggle Physics Colliders",
		"Viewport",
		{
			.m_Key = Key::C,
			.m_Ctrl = true,
			.m_Shift = true,
			.m_Alt = false
		},
		[this]()
		{
			m_UISettings.SetShowPhysicsColliders(!m_UISettings.GetShowPhysicsColliders());
			return true;
		},
		[this]() { return HasProjectLoaded(); },
		[this]() { return m_ViewportFocused; });
	m_ShortcutManager.Add(
		EditorShortcutScope::Viewport,
		"viewport.toggle_grid",
		"Toggle Viewport Grid",
		"Viewport",
		{
			.m_Key = Key::G,
			.m_Ctrl = true,
			.m_Shift = false,
			.m_Alt = false
		},
		[this]()
		{
			m_UISettings.SetShowEditorGrid(!m_UISettings.GetShowEditorGrid());
			return true;
		},
		[this]() { return HasProjectLoaded(); },
		[this]() { return m_ViewportFocused; });
	m_ShortcutManager.Add(
		EditorShortcutScope::Viewport,
		"viewport.toggle_cursor_mode",
		"Toggle Game View Cursor Capture",
		"Viewport",
		{
			.m_Key = Key::M,
			.m_Ctrl = true,
			.m_Shift = true,
			.m_Alt = false
		},
		[this]() { return ToggleViewportCursorMode(); },
		[this]() { return HasProjectLoaded() && (m_SceneManager.State() == SceneState::Play || m_SceneManager.State() == SceneState::Simulate); },
		[this]() { return m_GameViewportFocused; });
	auto addConsoleShortcut = [this](const char* id, const char* displayName, const UI::ShortcutBinding& binding, std::function<bool()> callback)
	{
		m_ShortcutManager.Add(
			EditorShortcutScope::Console,
			std::string("console.") + id,
			displayName,
			"Console",
			binding,
			std::move(callback),
			[]() { return ConsolePanel::IsOpen(); },
			[]() { return ConsolePanel::IsShortcutContextActive(); });
	};
	addConsoleShortcut("clear", "Clear Console", { Key::L, true, false, false }, []() { ConsolePanel::Clear(); return true; });
	addConsoleShortcut("copy_visible", "Copy Visible Console Logs", { Key::C, true, true, false }, []() { ConsolePanel::CopyVisible(); return true; });
	addConsoleShortcut("focus_search", "Focus Console Search", { Key::F, true, false, false }, []() { ConsolePanel::FocusSearch(); return true; });
	addConsoleShortcut("clear_filters", "Clear Console Filters", { Key::Backspace, true, false, false }, []() { ConsolePanel::ClearFilters(); return true; });
	addConsoleShortcut("toggle_autoscroll", "Toggle Console Auto-scroll", { Key::A, true, true, false }, []() { ConsolePanel::ToggleAutoScroll(); return true; });
	addConsoleShortcut("show_all", "Console Show All Levels", { Key::D1, true, false, false }, []() { ConsolePanel::ShowAllLevels(); return true; });
	addConsoleShortcut("show_warn_errors", "Console Show Warnings And Errors", { Key::D2, true, false, false }, []() { ConsolePanel::ShowWarningsAndErrors(); return true; });
	addConsoleShortcut("show_errors", "Console Show Errors Only", { Key::D3, true, false, false }, []() { ConsolePanel::ShowErrorsOnly(); return true; });
	if (m_ContentBrowserPanel)
		m_ContentBrowserPanel->RegisterShortcuts(m_ShortcutManager);
}

void EditorLayer::RebuildEditorPanelRegistry()
{
	m_PanelManager.Clear();
	if (m_StatisticsPanelAdapter)
		m_PanelManager.AddPanel(*m_StatisticsPanelAdapter);
	m_PanelManager.AddPanel(m_SceneHierarchyPanel);
	m_PanelManager.AddPanel(m_AnimationEditorPanel);
	m_PanelManager.AddPanel(m_AssetEditorPanel);
	m_PanelManager.AddPanel(m_AssistantPanel);
	m_PanelManager.AddPanel(m_ExportPanel);
	m_PanelManager.AddPanel(m_ProjectHealthPanel);
	if (m_ConsolePanelAdapter)
		m_PanelManager.AddPanel(*m_ConsolePanelAdapter);
	if (m_ContentBrowserPanel)
		m_PanelManager.AddPanel(*m_ContentBrowserPanel);
}

void EditorLayer::DrawEditorGrid()
{
	if (!m_UISettings.GetShowEditorGrid())
		return;

	if (m_ViewportSize.x <= 0.0f || m_ViewportSize.y <= 0.0f)
		return;

	const float aspectRatio = m_ViewportSize.x / m_ViewportSize.y;
	const float distance = glm::max(m_EditorCamera.GetDistance(), 1.0f);
	const float visibleHeight = distance * 1.15f;
	const float visibleWidth = visibleHeight * aspectRatio;
	const glm::vec3 center = m_EditorCamera.GetPosition() + m_EditorCamera.GetForwardDirection() * distance;

	float gridStep = 1.0f;
	const float visibleSpan = glm::max(visibleWidth, visibleHeight);
	while ((visibleSpan / gridStep) > 240.0f)
		gridStep *= 2.0f;

	const int minX = static_cast<int>(std::floor((center.x - visibleWidth * 0.5f) / gridStep)) - 2;
	const int maxX = static_cast<int>(std::ceil((center.x + visibleWidth * 0.5f) / gridStep)) + 2;
	const int minY = static_cast<int>(std::floor((center.y - visibleHeight * 0.5f) / gridStep)) - 2;
	const int maxY = static_cast<int>(std::ceil((center.y + visibleHeight * 0.5f) / gridStep)) + 2;

	constexpr glm::vec4 gridColor{ 0.26f, 0.29f, 0.30f, 0.34f };
	constexpr glm::vec4 majorGridColor{ 0.37f, 0.41f, 0.41f, 0.45f };
	constexpr glm::vec4 xAxisColor{ 0.86f, 0.34f, 0.30f, 0.74f };
	constexpr glm::vec4 yAxisColor{ 0.30f, 0.66f, 0.46f, 0.74f };
	const float minZ = -0.02f;
	auto isMajorGridLine = [](float value)
	{
		return std::fmod(std::abs(value), 10.0f) < 0.0001f;
	};

	Renderer2D::BeginScene(m_EditorCamera);
	Renderer2D::SetLineWidth(1.0f);

	for (int x = minX; x <= maxX; ++x)
	{
		const float worldX = static_cast<float>(x) * gridStep;
		const bool isAxis = std::abs(worldX) < 0.0001f;
		const bool isMajor = isMajorGridLine(worldX);
		const glm::vec4& color = isAxis ? yAxisColor : (isMajor ? majorGridColor : gridColor);
		Renderer2D::DrawLine(
			{ worldX, static_cast<float>(minY) * gridStep, minZ },
			{ worldX, static_cast<float>(maxY) * gridStep, minZ },
			color);
	}

	for (int y = minY; y <= maxY; ++y)
	{
		const float worldY = static_cast<float>(y) * gridStep;
		const bool isAxis = std::abs(worldY) < 0.0001f;
		const bool isMajor = isMajorGridLine(worldY);
		const glm::vec4& color = isAxis ? xAxisColor : (isMajor ? majorGridColor : gridColor);
		Renderer2D::DrawLine(
			{ static_cast<float>(minX) * gridStep, worldY, minZ },
			{ static_cast<float>(maxX) * gridStep, worldY, minZ },
			color);
	}

	Renderer2D::EndScene();
}

void EditorLayer::OnOverlayRender()
{
	WHP_PROFILE_FUNCTION();
	Renderer2D::BeginScene(m_EditorCamera);

	if (m_UISettings.GetShowPhysicsColliders())
	{
		// Box Colliders
		{
			auto view = m_SceneManager.ActiveScene()->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
			for (auto Entity : view)
			{
				auto [tc, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(Entity);

				glm::vec3 translation = tc.m_Translation + glm::vec3(bc2d.m_Offset, 0.001f);
				glm::vec3 scale = tc.m_Scale * glm::vec3(bc2d.m_Size * 2.0f, 1.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), tc.m_Translation)
					* glm::rotate(glm::mat4(1.0f), tc.m_Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
					* glm::translate(glm::mat4(1.0f), glm::vec3(bc2d.m_Offset, 0.001f))
					* glm::scale(glm::mat4(1.0f), scale);

				Renderer2D::DrawRect(transform, glm::vec4(0, 1, 0, 1));
			}
		}

		// Circle Colliders
		{
			auto view = m_SceneManager.ActiveScene()->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
			for (auto Entity : view)
			{
				auto [tc, cc2d] = view.get<TransformComponent, CircleCollider2DComponent>(Entity);

				glm::vec3 translation = tc.m_Translation + glm::vec3(cc2d.m_Offset, 0.001f);
				glm::vec3 scale = tc.m_Scale * glm::vec3(cc2d.m_Radius * 2.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
					* glm::scale(glm::mat4(1.0f), scale);

				Renderer2D::DrawCircle(transform, glm::vec4(0, 1, 0, 1), 0.02f);
			}
		}
	}

	for (Entity selectedEntity : m_SceneHierarchyPanel.GetSelectedEntities())
	{
		if (selectedEntity.HasComponent<UITransformComponent>())
			continue;

		TransformComponent transform = selectedEntity.GetComponent<TransformComponent>();
		if (selectedEntity.HasComponent<TextComponent>() && !selectedEntity.HasComponent<SpriteRendererComponent>() && !selectedEntity.HasComponent<CircleRendererComponent>())
		{
			selectedEntity.GetComponent<TextComponent>();
		}
		else
			Renderer2D::DrawRect(transform.GetTransform(), glm::vec4(0.9f, 0.4f, 0.1f, 1.0f));
	}

	Renderer2D::EndScene();

	if (m_SceneManager.State() != SceneState::Play)
		m_SceneManager.ActiveScene()->RenderUIOverlayDebug(m_SceneHierarchyPanel.GetSelectedEntityIds());
}

bool EditorLayer::HasProjectLoaded() const
{
	return Project::GetActive() != nullptr;
}

void EditorLayer::UpdateViewportCursorMode()
{
	const bool runtimeViewport = m_SceneManager.State() == SceneState::Play || m_SceneManager.State() == SceneState::Simulate;
	if (!runtimeViewport)
	{
		Input::SetCursorMode(CursorMode::Normal);
		Input::SetCursorModeOverride(true, CursorMode::Normal);
		return;
	}

	const bool useGameCursor = m_ViewportCursorMode == ViewportCursorMode::Game && m_GameViewportHovered && m_GameViewportFocused;
	Input::SetCursorModeOverride(!useGameCursor, CursorMode::Normal);
}

bool EditorLayer::ToggleViewportCursorMode()
{
	m_ViewportCursorMode = m_ViewportCursorMode == ViewportCursorMode::Editor ? ViewportCursorMode::Game : ViewportCursorMode::Editor;
	UpdateViewportCursorMode();
	return true;
}

void EditorLayer::UIToolbar()
{
	bool toolbarEnabled = (bool)m_SceneHierarchyPanel.GetContext();

	ImVec4 tintColor = ImVec4(1, 1, 1, 1);
	if (!toolbarEnabled)
		tintColor.w = 0.5f;

	const SceneState sceneState = m_SceneManager.State();
	bool hasPlayButton = sceneState == SceneState::Edit|| sceneState == SceneState::Play;
	bool hasSimulateButton = sceneState == SceneState::Edit || sceneState == SceneState::Simulate;
	bool hasPauseButton = sceneState != SceneState::Edit;
	bool isPaused = hasPauseButton && m_SceneManager.ActiveScene()->IsPaused();
	bool hasStepButton = hasPauseButton && isPaused;

	const float buttonSize = 36.0f;
	const float iconSize = 18.0f;
	const float padding = 6.0f;
	const float spacing = 5.0f;
	const int buttonCount = (hasPlayButton ? 1 : 0) + (hasSimulateButton ? 1 : 0) + (hasPauseButton ? 1 : 0) + (hasStepButton ? 1 : 0);
	const float panelWidth = padding * 2.0f + buttonSize * buttonCount + spacing * glm::max(buttonCount - 1, 0);
	const float panelHeight = buttonSize + padding * 2.0f;

	ImVec2 viewportMin = ImVec2(m_ViewportBounds[0].x, m_ViewportBounds[0].y);
	ImVec2 viewportMax = ImVec2(m_ViewportBounds[1].x, m_ViewportBounds[1].y);
	ImVec2 panelPos = ImVec2(viewportMin.x + ((viewportMax.x - viewportMin.x) - panelWidth) * 0.5f, viewportMin.y + 12.0f);
	ImVec2 panelEnd = ImVec2(panelPos.x + panelWidth, panelPos.y + panelHeight);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(ImVec2(panelPos.x + 2.0f, panelPos.y + 3.0f), ImVec2(panelEnd.x + 2.0f, panelEnd.y + 3.0f), IM_COL32(0, 0, 0, 76), 7.0f);
	drawList->AddRectFilled(panelPos, panelEnd, IM_COL32(24, 22, 19, 238), 7.0f);
	drawList->AddRect(panelPos, panelEnd, IM_COL32(76, 64, 48, 210), 7.0f);

	ImGui::SetCursorScreenPos(ImVec2(panelPos.x + padding, panelPos.y + padding));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, 0.0f));

	auto drawIconButton = [&](const char* id, Icon iconType, ImU32 accent, const char* tooltip, UI::EditorShortcutAction action = UI::EditorShortcutAction::Count) -> bool
		{
			Ref<Texture2D> iconTexture = IconManager::Get().GetIcon(iconType);
			ImGui::InvisibleButton(id, ImVec2(buttonSize, buttonSize));
			const bool clicked = ImGui::IsItemClicked() && toolbarEnabled;
			const bool hovered = ImGui::IsItemHovered();
			const bool active = ImGui::IsItemActive();
			ImVec2 min = ImGui::GetItemRectMin();
			ImVec2 max = ImGui::GetItemRectMax();
			ImU32 buttonColor = active ? ColorU32(0.33f, 0.22f, 0.12f, 0.95f) : hovered ? ColorU32(0.18f, 0.15f, 0.12f, 0.92f) : ColorU32(0.10f, 0.09f, 0.08f, 0.88f);
			drawList->AddRectFilled(min, max, buttonColor, 5.0f);
			if (hovered)
				drawList->AddRect(min, max, accent, 5.0f);

			ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
			ImVec2 iconMin(center.x - iconSize * 0.5f, center.y - iconSize * 0.5f);
			ImVec2 iconMax(center.x + iconSize * 0.5f, center.y + iconSize * 0.5f);
			ImU32 tint = toolbarEnabled ? IM_COL32(240, 232, 216, 255) : IM_COL32(148, 140, 128, 190);
			drawList->AddImage(UI::ToImGuiTextureId(iconTexture->GetRendererId()), iconMin, iconMax, ImVec2(0, 1), ImVec2(1, 0), tint);
			if (hovered && tooltip)
			{
				if (action != UI::EditorShortcutAction::Count)
					m_ShortcutManager.DrawShortcutTooltip(EditorActionShortcutId(action), tooltip);
				else
					ImGui::SetTooltip("%s", tooltip);
			}
			return clicked;
		};

	if(hasPlayButton)
	{
		Icon playIcon = sceneState == SceneState::Play ? Icon::Stop : Icon::Play;
		if (drawIconButton("##ViewportToolbarPlay", playIcon, ColorU32(0.58f, 0.70f, 0.42f, tintColor.w), sceneState == SceneState::Play ? "Stop" : "Play", sceneState == SceneState::Play ? UI::EditorShortcutAction::Stop : UI::EditorShortcutAction::Play))
		{
			if (sceneState == SceneState::Edit || sceneState == SceneState::Simulate)
				m_SceneManager.OnScenePlay();
			else if (sceneState == SceneState::Play)
				m_SceneManager.OnSceneStop();
		}
	}
	if(hasSimulateButton)
	{
		if(hasPlayButton)
			ImGui::SameLine();
		Icon simulateIcon = sceneState == SceneState::Simulate ? Icon::Stop : Icon::Simulate;
		if (drawIconButton("##ViewportToolbarSimulate", simulateIcon, ColorU32(0.66f, 0.55f, 0.42f, tintColor.w), sceneState == SceneState::Simulate ? "Stop simulation" : "Simulate", sceneState == SceneState::Simulate ? UI::EditorShortcutAction::Stop : UI::EditorShortcutAction::Simulate))
		{
			if (sceneState == SceneState::Edit || sceneState == SceneState::Play)
				m_SceneManager.OnSceneSimulate();
			else if (sceneState == SceneState::Simulate)
				m_SceneManager.OnSceneStop();
		}
	}
	if (hasPauseButton)
	{
		ImGui::SameLine();
		if (drawIconButton("##ViewportToolbarPause", Icon::Pause, ColorU32(0.86f, 0.64f, 0.32f, tintColor.w), isPaused ? "Resume" : "Pause", UI::EditorShortcutAction::Pause))
			m_SceneManager.ActiveScene()->SetPaused(!isPaused);

		if (isPaused)
		{
			ImGui::SameLine();
			if (drawIconButton("##ViewportToolbarStepForward", Icon::StepForward, ColorU32(0.86f, 0.64f, 0.32f, tintColor.w), "Step"))
				m_SceneManager.ActiveScene()->Step(m_UISettings.GetStepFrame());
		}
	}
	ImGui::PopStyleVar();

	const EditorScriptManager::Status& scriptStatus = m_ScriptManager.GetStatus();
	if (HasProjectLoaded() && !scriptStatus.m_Message.empty())
	{
		const ImVec2 textSize = ImGui::CalcTextSize(scriptStatus.m_Message.c_str());
		const float statusPaddingX = 10.0f;
		const float statusHeight = 24.0f;
		const float statusWidth = glm::min(textSize.x + statusPaddingX * 2.0f, 260.0f);
		ImVec2 statusPos(panelEnd.x + 10.0f, panelPos.y + (panelHeight - statusHeight) * 0.5f);
		if (statusPos.x + statusWidth > viewportMax.x - 10.0f)
			statusPos = ImVec2(panelPos.x - statusWidth - 10.0f, statusPos.y);

		if (statusPos.x > viewportMin.x + 10.0f)
		{
			const ImU32 statusFill = scriptStatus.m_Failure ? IM_COL32(84, 34, 32, 230) :
				scriptStatus.m_Warning ? IM_COL32(78, 58, 28, 230) : IM_COL32(34, 62, 48, 220);
			const ImU32 statusBorder = scriptStatus.m_Failure ? IM_COL32(214, 94, 84, 230) :
				scriptStatus.m_Warning ? IM_COL32(226, 174, 74, 230) : IM_COL32(112, 184, 136, 220);
			ImVec2 statusEnd(statusPos.x + statusWidth, statusPos.y + statusHeight);
			drawList->AddRectFilled(statusPos, statusEnd, statusFill, 5.0f);
			drawList->AddRect(statusPos, statusEnd, statusBorder, 5.0f);
			drawList->AddText(ImVec2(statusPos.x + statusPaddingX, statusPos.y + 4.0f), IM_COL32(238, 232, 220, 255), scriptStatus.m_Message.c_str());
			if (ImGui::IsMouseHoveringRect(statusPos, statusEnd))
				ImGui::SetTooltip("%s", scriptStatus.m_Message.c_str());
		}
	}
}

_WHIP_END
