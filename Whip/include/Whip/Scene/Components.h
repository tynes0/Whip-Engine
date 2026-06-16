#pragma once

#include "SceneCamera.h"
#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>
#include <Whip/Utils/Utility.h>
#include <Whip/Core/Memory.h>
#include <Whip/Helper/UniqueNameManager.h>
#include <Whip/Render/Texture.h>
#include <Whip/Render/Font.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

_WHIP_START

struct IDComponent
{
	UUID m_ID;

	IDComponent() = default;
	IDComponent(const IDComponent&) = default;
	IDComponent& operator=(const IDComponent&) = default;
	IDComponent(UUID uuid) : m_ID(uuid) {}

	static constexpr const char* ScriptStructName = "IDComponent";
};

struct TagComponent
{
	std::string m_Tag;

	TagComponent() = default;
	TagComponent(const TagComponent&) = default;
	TagComponent& operator=(const TagComponent&) = default;
	TagComponent(const std::string& tag) : m_Tag(tag) {}

	static constexpr const char* ScriptStructName = "TagComponent";
};

struct HierarchyComponent
{
	UUID m_Parent = 0;
	std::vector<UUID> m_Children;
	bool m_IsGroup = false;

	HierarchyComponent() = default;
	HierarchyComponent(const HierarchyComponent&) = default;
	HierarchyComponent& operator=(const HierarchyComponent&) = default;

	static constexpr const char* ScriptStructName = "HierarchyComponent";
};

struct PrefabComponent
{
	AssetHandle m_Source = 0;
	UUID m_SourceEntity = 0;
	bool m_Root = false;

	PrefabComponent() = default;
	PrefabComponent(const PrefabComponent&) = default;
	PrefabComponent& operator=(const PrefabComponent&) = default;

	bool Valid() const { return m_Source != 0; }

	static constexpr const char* ScriptStructName = "PrefabComponent";
};

struct TransformComponent
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

	static constexpr const char* ScriptStructName = "TransformComponent";
};

struct SpriteRendererComponent
{
    glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	AssetHandle m_Texture = 0;
	float m_TilingFactor = 1.0f;

    SpriteRendererComponent() = default;
    SpriteRendererComponent(const SpriteRendererComponent&) = default;
    SpriteRendererComponent& operator=(const SpriteRendererComponent&) = default;
    SpriteRendererComponent(const glm::vec4& color) : m_Color(color) {}

    operator glm::vec4& () { return m_Color; }
    operator const glm::vec4& () const { return m_Color; }

	static constexpr const char* ScriptStructName = "SpriteRendererComponent";
};

struct CircleRendererComponent
{
	glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	float m_Thickness = 1.0f;
	float m_Fade = 0.005f;

	CircleRendererComponent() = default;
	CircleRendererComponent(const CircleRendererComponent&) = default;
	CircleRendererComponent& operator=(const CircleRendererComponent&) = default;

	static constexpr const char* ScriptStructName = "CircleRendererComponent";
};

struct TextComponent
{
	std::string m_TextString;
	AssetHandle m_Font = 0;
	glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	float m_Kerning = 0.0f;
	float m_LineSpacing = 0.0f;

	static constexpr const char* ScriptStructName = "TextComponent";
};

struct CameraComponent
{
    SceneCamera m_Camera;
    bool m_Primary = true;
    bool m_FixedAspectRatio = false;

    CameraComponent() = default;
    CameraComponent(const CameraComponent&) = default;
    CameraComponent& operator=(const CameraComponent&) = default;

	static constexpr const char* ScriptStructName = "CameraComponent";
};

struct ScriptComponent
{
	std::string m_ClassName;

	ScriptComponent() = default;
	ScriptComponent(const ScriptComponent&) = default;
	ScriptComponent& operator=(const ScriptComponent&) = default;

	static constexpr const char* ScriptStructName = "ScriptComponent";
};

class ScriptableEntity;

struct NativeScriptComponent
{
    ScriptableEntity* m_Instance = nullptr;

	ScriptableEntity* (*m_InstantiateScript)() = {};
	void (*m_DestroyScript)(NativeScriptComponent*) = {};

    template <class T>
    void Bind()
    {
        m_InstantiateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
        m_DestroyScript = [](NativeScriptComponent* nsc)
        {
            delete nsc->m_Instance;
            nsc->m_Instance = nullptr;
        };
    }

	static constexpr const char* ScriptStructName = "NativeScriptComponent";
};

struct Rigidbody2DComponent
{
	enum class BodyType { Static = 0, Dynamic, Kinematic };
	BodyType m_Type = BodyType::Static;
	bool m_FixedRotation = false;
	float m_GravityScale = 1.0f;

	void* m_RuntimeBody = nullptr;

	Rigidbody2DComponent() = default;
	Rigidbody2DComponent(const Rigidbody2DComponent&) = default;
	Rigidbody2DComponent& operator=(const Rigidbody2DComponent&) = default;

	static constexpr const char* ScriptStructName = "Rigidbody2DComponent";
};

struct BoxCollider2DComponent
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

	static constexpr const char* ScriptStructName = "BoxCollider2DComponent";
};

struct CircleCollider2DComponent
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

	static constexpr const char* ScriptStructName = "CircleCollider2DComponent";
};

struct AudioComponent
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

	static constexpr const char* ScriptStructName = "AudioComponent";
};

template<typename...>
struct ComponentGroup {};

using AllComponentsNoIDNoTagNoScript = ComponentGroup<TransformComponent,
	SpriteRendererComponent, CircleRendererComponent, TextComponent, CameraComponent,
	Rigidbody2DComponent, BoxCollider2DComponent, CircleCollider2DComponent, AudioComponent>;

using AllComponentsNoIDNoTag = ComponentGroup<TransformComponent, SpriteRendererComponent,
	CircleRendererComponent, TextComponent, CameraComponent, ScriptComponent, NativeScriptComponent,
	Rigidbody2DComponent, BoxCollider2DComponent, CircleCollider2DComponent, AudioComponent, HierarchyComponent, PrefabComponent>;

using AllComponents = ComponentGroup<TransformComponent, SpriteRendererComponent,
	CircleRendererComponent, TextComponent, CameraComponent, ScriptComponent, NativeScriptComponent,
	Rigidbody2DComponent, BoxCollider2DComponent, CircleCollider2DComponent, AudioComponent, IDComponent, TagComponent, HierarchyComponent, PrefabComponent>;

_WHIP_END
