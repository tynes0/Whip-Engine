#pragma once

#include "SceneCamera.h"
#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>
#include <Whip/Utils/Utility.h>
#include <Whip/Helper/UniqueNameManager.h>
#include <Whip/Render/Font.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <entity/entity.hpp>
#include <glm/gtx/quaternion.hpp>

_WHIP_START

struct IDComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
	UUID m_ID;

	IDComponent() = default;
	IDComponent(const IDComponent&) = default;
	IDComponent& operator=(const IDComponent&) = default;
	IDComponent(UUID uuid) : m_ID(uuid) {}

	static constexpr std::string_view ScriptStructName = "IDComponent";
};

struct TagComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
	std::string m_Tag;

	TagComponent() = default;
	TagComponent(const TagComponent&) = default;
	TagComponent& operator=(const TagComponent&) = default;
	TagComponent(std::string tag) : m_Tag(std::move(tag)) {}

	static constexpr std::string_view ScriptStructName = "TagComponent";
};

struct HierarchyComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
	UUID m_Parent = 0;
	std::vector<UUID> m_Children;
	bool m_IsGroup = false;

	HierarchyComponent() = default;
	HierarchyComponent(const HierarchyComponent&) = default;
	HierarchyComponent& operator=(const HierarchyComponent&) = default;

	static constexpr std::string_view ScriptStructName = "HierarchyComponent";
};

struct PrefabComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
	AssetHandle m_Source = 0;
	UUID m_SourceEntity = 0;
	bool m_Root = false;

	PrefabComponent() = default;
	PrefabComponent(const PrefabComponent&) = default;
	PrefabComponent& operator=(const PrefabComponent&) = default;

	bool Valid() const { return m_Source != 0; }

	static constexpr std::string_view ScriptStructName = "PrefabComponent";
};

struct TransformComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
    glm::vec3 m_Translation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 m_Rotation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 m_Scale = { 1.0f, 1.0f, 1.0f };

    TransformComponent() = default;
    TransformComponent(const TransformComponent&) = default;
    TransformComponent& operator=(const TransformComponent&) = default;
    TransformComponent(const glm::vec3& translation) : m_Translation(translation) {}

    glm::mat4 GetTransform() const
    {
        glm::mat4 rotation = glm::toMat4(glm::quat(m_Rotation));
        return glm::translate(glm::mat4(1.0f), m_Translation) * rotation * glm::scale(glm::mat4(1.0f), m_Scale);
    }

	static constexpr std::string_view ScriptStructName = "TransformComponent";
};

struct SpriteRendererComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
    glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	AssetHandle m_Texture = 0;
	int32_t m_TextureSpriteIndex = -1;
	float m_TilingFactor = 1.0f;

    SpriteRendererComponent() = default;
    SpriteRendererComponent(const SpriteRendererComponent&) = default;
    SpriteRendererComponent& operator=(const SpriteRendererComponent&) = default;
    SpriteRendererComponent(const glm::vec4& color) : m_Color(color) {}

    operator glm::vec4& () { return m_Color; }
    operator const glm::vec4& () const { return m_Color; }

	static constexpr std::string_view ScriptStructName = "SpriteRendererComponent";
};

struct CircleRendererComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
	glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	float m_Thickness = 1.0f;
	float m_Fade = 0.005f;

	CircleRendererComponent() = default;
	CircleRendererComponent(const CircleRendererComponent&) = default;
	CircleRendererComponent& operator=(const CircleRendererComponent&) = default;

	static constexpr std::string_view ScriptStructName = "CircleRendererComponent";
};

struct TextComponent
{
	std::string m_TextString;
	AssetHandle m_Font = 0;
	glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	float m_Kerning = 0.0f;
	float m_LineSpacing = 0.0f;

	static constexpr std::string_view ScriptStructName = "TextComponent";
};

struct UITransformComponent
{
	glm::vec2 m_AnchorMin{ 0.5f, 0.5f };
	glm::vec2 m_AnchorMax{ 0.5f, 0.5f };
	glm::vec2 m_Pivot{ 0.5f, 0.5f };
	glm::vec2 m_AnchoredPosition{ 0.0f, 0.0f };
	glm::vec2 m_Size{ 180.0f, 48.0f };
	glm::vec2 m_Scale{ 1.0f, 1.0f };
	float m_Rotation = 0.0f;
	int32_t m_SortOrder = 0;
	bool m_Visible = true;

	static constexpr std::string_view ScriptStructName = "UITransformComponent";
};

struct UIImageComponent
{
	glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	AssetHandle m_Texture = 0;
	int32_t m_TextureSpriteIndex = -1;
	bool m_RaycastTarget = true;

	static constexpr std::string_view ScriptStructName = "UIImageComponent";
};

struct UITextComponent
{
	std::string m_TextString = "Text";
	AssetHandle m_Font = 0;
	glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	float m_FontSize = 24.0f;
	float m_Kerning = 0.0f;
	float m_LineSpacing = 0.0f;

	static constexpr std::string_view ScriptStructName = "UITextComponent";
};

struct UIButtonComponent
{
	std::string m_Text = "Button";
	AssetHandle m_Font = 0;
	glm::vec4 m_NormalColor{ 0.12f, 0.16f, 0.20f, 1.0f };
	glm::vec4 m_HoveredColor{ 0.20f, 0.32f, 0.44f, 1.0f };
	glm::vec4 m_PressedColor{ 0.10f, 0.22f, 0.34f, 1.0f };
	glm::vec4 m_DisabledColor{ 0.10f, 0.11f, 0.12f, 0.65f };
	glm::vec4 m_TextColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	float m_FontSize = 20.0f;
	bool m_Interactable = true;
	bool m_RaycastTarget = true;
	bool m_Hovered = false;
	bool m_Pressed = false;
	bool m_ClickedThisFrame = false;

	static constexpr std::string_view ScriptStructName = "UIButtonComponent";
};

struct CameraComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
    SceneCamera m_Camera;
    bool m_Primary = true;
    bool m_FixedAspectRatio = false;

    CameraComponent() = default;
    CameraComponent(const CameraComponent&) = default;
    CameraComponent& operator=(const CameraComponent&) = default;

	static constexpr std::string_view ScriptStructName = "CameraComponent";
};

struct ScriptComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
	std::string m_ClassName;

	ScriptComponent() = default;
	ScriptComponent(const ScriptComponent&) = default;
	ScriptComponent& operator=(const ScriptComponent&) = default;

	static constexpr std::string_view ScriptStructName = "ScriptComponent";
};

struct AnimatorComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
	AssetHandle m_Controller = 0;
	std::string m_InitialState;
	bool m_PlayOnStart = true;
	float m_Speed = 1.0f;

	AnimatorComponent() = default;
	AnimatorComponent(const AnimatorComponent&) = default;
	AnimatorComponent& operator=(const AnimatorComponent&) = default;

	static constexpr std::string_view ScriptStructName = "AnimatorComponent";
};

struct BodyUserData
{
	entt::entity m_EntityID;
};

struct Rigidbody2DComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
	enum class BodyType : uint8_t { Static = 0, Dynamic, Kinematic };
	BodyType m_Type = BodyType::Static;
	bool m_FixedRotation = false;
	float m_GravityScale = 1.0f;

	void* m_RuntimeBody = nullptr;

	std::shared_ptr<BodyUserData> m_UserData = nullptr;

	Rigidbody2DComponent() = default;
	Rigidbody2DComponent(const Rigidbody2DComponent&) = default;
	Rigidbody2DComponent& operator=(const Rigidbody2DComponent&) = default;

	static constexpr std::string_view ScriptStructName = "Rigidbody2DComponent";
};

struct BoxCollider2DComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
	glm::vec2 m_Offset = { 0.0f, 0.0f };
	glm::vec2 m_Size = { 0.5f, 0.5f };

	float m_Density = 1.0f;
	float m_Friction = 0.5f;
	float m_Restitution = 0.0f;
	float m_RestitutionThreshold = 0.5f;

	bool m_Sensor = false;

	void* m_RuntimeFixture = nullptr;

	std::string m_Tag;

	BoxCollider2DComponent() = default;
	BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
	BoxCollider2DComponent& operator=(const BoxCollider2DComponent&) = default;

	static constexpr std::string_view ScriptStructName = "BoxCollider2DComponent";
};

struct CircleCollider2DComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
	glm::vec2 m_Offset = { 0.0f, 0.0f };
	float m_Radius = 0.5f;

	float m_Density = 1.0f;
	float m_Friction = 0.5f;
	float m_Restitution = 0.0f;
	float m_RestitutionThreshold = 0.5f;

	bool m_Sensor = false;

	void* m_RuntimeFixture = nullptr;

	std::string m_Tag;

	CircleCollider2DComponent() = default;
	CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
	CircleCollider2DComponent& operator=(const CircleCollider2DComponent&) = default;

	static constexpr std::string_view ScriptStructName = "CircleCollider2DComponent";
};

struct AudioComponent // NOLINT(cppcoreguidelines-special-member-functions)
{
	struct AudioData
	{
		static constexpr const char* DefaultTag = "Empty";

		AssetHandle m_Audio = 0;
		std::string m_Tag = DefaultTag;
		glm::vec3 m_Translation = { 0.0f, 0.0f, 0.0f };
		bool m_Spatial = false;
		bool m_Loop = false;
		bool m_LoadedInRuntime = false;
		float m_Gain = 1.0f;
		float m_Pitch = 1.0f;

		float m_FullClipLength = 0.0f;
		float m_ClipStart = 0.0f;
		float m_ClipEnd = 0.0f;
		UUID32 m_ID = 0;
	};

	UniqueNameManager m_UniqueNameManager;

	std::vector<AudioData> m_AudioDatas;
	size_t m_SelectedAudioIndex = npos<size_t>;

	AudioComponent() = default;
	AudioComponent(const AudioComponent&) = default;
	AudioComponent& operator=(const AudioComponent&) = default;

	static constexpr std::string_view ScriptStructName = "AudioComponent";
};

template<typename...>
struct ComponentGroup {};

using AllComponentsNoIDNoTagNoScript = ComponentGroup<TransformComponent,
	SpriteRendererComponent, CircleRendererComponent, TextComponent, CameraComponent,
	UITransformComponent, UIImageComponent, UITextComponent, UIButtonComponent,
	AnimatorComponent, Rigidbody2DComponent, BoxCollider2DComponent, CircleCollider2DComponent, AudioComponent>;

using AllComponentsNoIDNoTag = ComponentGroup<TransformComponent, SpriteRendererComponent,
	CircleRendererComponent, TextComponent, CameraComponent, ScriptComponent,
	UITransformComponent, UIImageComponent, UITextComponent, UIButtonComponent, AnimatorComponent, Rigidbody2DComponent,
	BoxCollider2DComponent, CircleCollider2DComponent, AudioComponent, HierarchyComponent, PrefabComponent>;

using AllComponents = ComponentGroup<TransformComponent, SpriteRendererComponent,
	CircleRendererComponent, TextComponent, CameraComponent, ScriptComponent,
	UITransformComponent, UIImageComponent, UITextComponent, UIButtonComponent, AnimatorComponent, Rigidbody2DComponent,
	BoxCollider2DComponent, CircleCollider2DComponent, AudioComponent, IDComponent, TagComponent, HierarchyComponent, PrefabComponent>;

_WHIP_END
