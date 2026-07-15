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

struct UICanvasComponent
{
	enum class ScaleMode : uint8_t
	{
		ConstantPixelSize = 0,
		ScaleWithScreenSize
	};

	ScaleMode m_ScaleMode = ScaleMode::ScaleWithScreenSize;
	glm::vec2 m_ReferenceResolution{ 1920.0f, 1080.0f };
	float m_MatchWidthOrHeight = 0.5f;
	float m_ScaleFactor = 1.0f;
	bool m_Visible = true;
	bool m_ShowInEditor = true;
	bool m_ShowSafeAreaInEditor = false;
	glm::vec4 m_SafeAreaInsets{ 0.0f, 0.0f, 0.0f, 0.0f }; // left, top, right, bottom in normalized viewport space

	static constexpr std::string_view ScriptStructName = "UICanvasComponent";
};

struct UIPanelComponent
{
	glm::vec4 m_Color{ 0.05f, 0.08f, 0.10f, 0.86f };
	glm::vec4 m_BorderColor{ 0.22f, 0.35f, 0.44f, 0.65f };
	float m_Radius = 8.0f;
	float m_BorderThickness = 0.0f;
	bool m_RaycastTarget = false;

	static constexpr std::string_view ScriptStructName = "UIPanelComponent";
};

struct UIImageComponent
{
	glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	AssetHandle m_Texture = 0;
	int32_t m_TextureSpriteIndex = -1;
	bool m_RaycastTarget = true;

	static constexpr std::string_view ScriptStructName = "UIImageComponent";
};

enum class UITextHorizontalAlignment : uint8_t
{
	Left = 0,
	Center,
	Right
};

enum class UITextVerticalAlignment : uint8_t
{
	Top = 0,
	Center,
	Bottom
};

struct UITextComponent
{
	std::string m_TextString = "Text";
	AssetHandle m_Font = 0;
	glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	float m_FontSize = 24.0f;
	float m_Kerning = 0.0f;
	float m_LineSpacing = 0.0f;
	UITextHorizontalAlignment m_HorizontalAlignment = UITextHorizontalAlignment::Center;
	UITextVerticalAlignment m_VerticalAlignment = UITextVerticalAlignment::Center;

	static constexpr std::string_view ScriptStructName = "UITextComponent";
};

struct UIPointerEventState
{
	std::string m_OnPointerEnterCallback;
	std::string m_OnPointerExitCallback;
	std::string m_OnPointerDownCallback;
	std::string m_OnPointerUpCallback;
	std::string m_OnPointerDragCallback;
	bool m_PointerInside = false;
	bool m_Dragging = false;
};

struct UIButtonComponent
{
	std::string m_Text = "Button";
	AssetHandle m_Font = 0;
	glm::vec4 m_NormalColor{ 0.12f, 0.16f, 0.20f, 1.0f };
	glm::vec4 m_HoveredColor{ 0.20f, 0.32f, 0.44f, 1.0f };
	glm::vec4 m_PressedColor{ 0.10f, 0.22f, 0.34f, 1.0f };
	glm::vec4 m_DisabledColor{ 0.10f, 0.11f, 0.12f, 0.65f };
	glm::vec4 m_FocusColor{ 0.42f, 0.68f, 0.94f, 1.0f };
	glm::vec4 m_TextColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	float m_FontSize = 20.0f;
	float m_Radius = 8.0f;
	float m_BorderThickness = 1.0f;
	glm::vec4 m_BorderColor{ 0.28f, 0.42f, 0.52f, 0.72f };
	UITextHorizontalAlignment m_TextHorizontalAlignment = UITextHorizontalAlignment::Center;
	UITextVerticalAlignment m_TextVerticalAlignment = UITextVerticalAlignment::Center;
	bool m_Interactable = true;
	bool m_RaycastTarget = true;
	bool m_NavigationEnabled = true;
	bool m_Hovered = false;
	bool m_Pressed = false;
	bool m_Focused = false;
	bool m_ClickedThisFrame = false;
	bool m_SubmittedThisFrame = false;
	UIPointerEventState m_PointerEvents;
	std::string m_OnClickCallback;

	static constexpr std::string_view ScriptStructName = "UIButtonComponent";
};

struct UIToggleComponent
{
	std::string m_Label = "Toggle";
	AssetHandle m_Font = 0;
	glm::vec4 m_BoxColor{ 0.10f, 0.14f, 0.17f, 1.0f };
	glm::vec4 m_CheckColor{ 0.32f, 0.64f, 0.92f, 1.0f };
	glm::vec4 m_TextColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	glm::vec4 m_HoveredColor{ 0.18f, 0.28f, 0.36f, 1.0f };
	float m_FontSize = 20.0f;
	float m_BoxRadius = 5.0f;
	bool m_Checked = false;
	bool m_Interactable = true;
	bool m_RaycastTarget = true;
	bool m_NavigationEnabled = true;
	bool m_Hovered = false;
	bool m_Pressed = false;
	bool m_Focused = false;
	bool m_ChangedThisFrame = false;
	UIPointerEventState m_PointerEvents;
	std::string m_OnValueChangedCallback;

	static constexpr std::string_view ScriptStructName = "UIToggleComponent";
};

struct UISliderComponent
{
	float m_Value = 0.5f;
	float m_MinValue = 0.0f;
	float m_MaxValue = 1.0f;
	glm::vec4 m_BackgroundColor{ 0.08f, 0.11f, 0.14f, 1.0f };
	glm::vec4 m_FillColor{ 0.30f, 0.58f, 0.88f, 1.0f };
	glm::vec4 m_HandleColor{ 0.92f, 0.96f, 1.0f, 1.0f };
	float m_TrackRadius = 6.0f;
	float m_HandleRadius = 16.0f;
	bool m_Interactable = true;
	bool m_RaycastTarget = true;
	bool m_Hovered = false;
	bool m_Pressed = false;
	bool m_Focused = false;
	bool m_ChangedThisFrame = false;
	UIPointerEventState m_PointerEvents;
	std::string m_OnValueChangedCallback;

	static constexpr std::string_view ScriptStructName = "UISliderComponent";
};

struct UIInputFieldComponent
{
	std::string m_Text;
	std::string m_Placeholder = "Enter text";
	AssetHandle m_Font = 0;
	glm::vec4 m_BackgroundColor{ 0.08f, 0.11f, 0.14f, 1.0f };
	glm::vec4 m_FocusedColor{ 0.11f, 0.18f, 0.24f, 1.0f };
	glm::vec4 m_TextColor{ 0.94f, 0.96f, 0.98f, 1.0f };
	glm::vec4 m_PlaceholderColor{ 0.52f, 0.60f, 0.68f, 1.0f };
	glm::vec4 m_SelectionColor{ 0.30f, 0.56f, 0.86f, 0.48f };
	glm::vec4 m_CaretColor{ 0.88f, 0.95f, 1.0f, 1.0f };
	float m_FontSize = 20.0f;
	float m_Radius = 7.0f;
	float m_BorderThickness = 1.0f;
	glm::vec4 m_BorderColor{ 0.22f, 0.34f, 0.42f, 0.82f };
	int32_t m_MaxCharacters = 128;
	int32_t m_CaretIndex = 0;
	int32_t m_SelectionAnchor = 0;
	bool m_Selecting = false;
	bool m_Interactable = true;
	bool m_RaycastTarget = true;
	bool m_Hovered = false;
	bool m_Focused = false;
	bool m_SubmittedThisFrame = false;
	bool m_ChangedThisFrame = false;
	UIPointerEventState m_PointerEvents;
	std::string m_OnSubmitCallback;
	std::string m_OnValueChangedCallback;

	static constexpr std::string_view ScriptStructName = "UIInputFieldComponent";
};

struct UIStackLayoutComponent
{
	enum class Axis : uint8_t { Horizontal = 0, Vertical };
	enum class Alignment : uint8_t { Start = 0, Center, End, Stretch };

	Axis m_Axis = Axis::Vertical;
	Alignment m_Alignment = Alignment::Center;
	glm::vec4 m_Padding{ 12.0f, 12.0f, 12.0f, 12.0f }; // left, top, right, bottom
	float m_Spacing = 8.0f;
	glm::vec2 m_ChildSize{ 180.0f, 48.0f };
	bool m_ControlChildWidth = true;
	bool m_ControlChildHeight = true;
	bool m_Reverse = false;

	static constexpr std::string_view ScriptStructName = "UIStackLayoutComponent";
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
	UITransformComponent, UICanvasComponent, UIPanelComponent, UIImageComponent, UITextComponent, UIButtonComponent, UIToggleComponent, UISliderComponent, UIInputFieldComponent, UIStackLayoutComponent,
	AnimatorComponent, Rigidbody2DComponent, BoxCollider2DComponent, CircleCollider2DComponent, AudioComponent>;

using AllComponentsNoIDNoTag = ComponentGroup<TransformComponent, SpriteRendererComponent,
	CircleRendererComponent, TextComponent, CameraComponent, ScriptComponent,
	UITransformComponent, UICanvasComponent, UIPanelComponent, UIImageComponent, UITextComponent, UIButtonComponent, UIToggleComponent, UISliderComponent, UIInputFieldComponent, UIStackLayoutComponent, AnimatorComponent, Rigidbody2DComponent,
	BoxCollider2DComponent, CircleCollider2DComponent, AudioComponent, HierarchyComponent, PrefabComponent>;

using AllComponents = ComponentGroup<TransformComponent, SpriteRendererComponent,
	CircleRendererComponent, TextComponent, CameraComponent, ScriptComponent,
	UITransformComponent, UICanvasComponent, UIPanelComponent, UIImageComponent, UITextComponent, UIButtonComponent, UIToggleComponent, UISliderComponent, UIInputFieldComponent, UIStackLayoutComponent, AnimatorComponent, Rigidbody2DComponent,
	BoxCollider2DComponent, CircleCollider2DComponent, AudioComponent, IDComponent, TagComponent, HierarchyComponent, PrefabComponent>;

_WHIP_END
