#pragma once

#include "scene_camera.h"
#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>
#include <Whip/Core/utility.h>
#include <Whip/Core/memory.h>
#include <Whip/Core/unique_name_manager.h>
#include <Whip/Render/Texture.h>
#include <Whip/Render/font.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

_WHIP_START

struct ID_component
{
	UUID ID;

	ID_component() = default;
	ID_component(const ID_component&) = default;
	ID_component& operator=(const ID_component&) = default;
	ID_component(UUID uuid) : ID(uuid) {}

	static constexpr const char* script_struct_name = "IDComponent";
};

struct tag_component
{
	std::string tag;

	tag_component() = default;
	tag_component(const tag_component&) = default;
	tag_component& operator=(const tag_component&) = default;
	tag_component(const std::string& tag) : tag(tag) {}

	static constexpr const char* script_struct_name = "TagComponent";
};

struct transform_component
{
    glm::vec3 translation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

    transform_component() = default;
    transform_component(const transform_component&) = default;
    transform_component& operator=(const transform_component&) = default;
    transform_component(const glm::vec3& translation) : translation(translation) {}

    glm::mat4 get_transform() const
    {
        glm::mat4 rot = glm::toMat4(glm::quat(rotation));
        return glm::translate(glm::mat4(1.0f), translation) * rot * glm::scale(glm::mat4(1.0f), scale);
    }

	static constexpr const char* script_struct_name = "TransformComponent";
};

struct sprite_renderer_component
{
    glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	asset_handle texture = 0;
	float tiling_factor = 1.0f;

    sprite_renderer_component() = default;
    sprite_renderer_component(const sprite_renderer_component&) = default;
    sprite_renderer_component& operator=(const sprite_renderer_component&) = default;
    sprite_renderer_component(const glm::vec4& color) : color(color) {}

    operator glm::vec4& () { return color; }
    operator const glm::vec4& () const { return color; }

	static constexpr const char* script_struct_name = "SpriteRendererComponent";
};

struct circle_renderer_component
{
	glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	float thickness = 1.0f;
	float fade = 0.005f;

	circle_renderer_component() = default;
	circle_renderer_component(const circle_renderer_component&) = default;
	circle_renderer_component& operator=(const circle_renderer_component&) = default;

	static constexpr const char* script_struct_name = "CircleRendererComponent";
};

struct text_component
{
	std::string text_string;
	asset_handle font = 0;
	glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	float kerning = 0.0f;
	float line_spacing = 0.0f;

	static constexpr const char* script_struct_name = "TextComponent";
};

struct camera_component
{
    scene_camera camera;
    bool primary = true;
    bool fixed_aspect_ratio = false;

    camera_component() = default;
    camera_component(const camera_component&) = default;
    camera_component& operator=(const camera_component&) = default;

	static constexpr const char* script_struct_name = "CameraComponent";
};

struct script_component
{
	std::string class_name;

	script_component() = default;
	script_component(const script_component&) = default;
	script_component& operator=(const script_component&) = default;

	static constexpr const char* script_struct_name = "ScriptComponent";
};

class scriptable_entity;

struct native_script_component
{
    scriptable_entity* instance = nullptr;
    
	scriptable_entity* (*instantiate_script)() = {};
	void (*destroy_script)(native_script_component*) = {};

    template <class T>
    void bind()
    {
        instantiate_script = []() { return static_cast<scriptable_entity*>(new T()); };
        destroy_script = [](native_script_component* nsc)
        {
            delete nsc->instance;
            nsc->instance = nullptr;
        };
    }

	static constexpr const char* script_struct_name = "NativeScriptComponent";
};

struct rigidbody2D_component
{
	enum class body_type { bt_static = 0, bt_dynamic, bt_kinematic };
	body_type type = body_type::bt_static;
	bool fixed_rotation = false;
	float gravity_scale = 1.0f;

	void* runtime_body = nullptr;

	rigidbody2D_component() = default;
	rigidbody2D_component(const rigidbody2D_component&) = default;
	rigidbody2D_component& operator=(const rigidbody2D_component&) = default;

	static constexpr const char* script_struct_name = "Rigidbody2DComponent";
};

struct box_collider2D_component
{
	glm::vec2 offset = { 0.0f, 0.0f };
	glm::vec2 size = { 0.5f, 0.5f };

	float density = 1.0f;
	float friction = 0.5f;
	float restitution = 0.0f;
	float restitution_threshold = 0.5f;

	bool sensor = false;

	void* runtime_fixture = nullptr;

	std::string tag;

	box_collider2D_component() = default;
	box_collider2D_component(const box_collider2D_component&) = default;
	box_collider2D_component& operator=(const box_collider2D_component&) = default;

	static constexpr const char* script_struct_name = "BoxCollider2DComponent";
};

struct circle_collider2D_component
{
	glm::vec2 offset = { 0.0f, 0.0f };
	float radius = 0.5f;

	float density = 1.0f;
	float friction = 0.5f;
	float restitution = 0.0f;
	float restitution_threshold = 0.5f;

	bool sensor = false;

	void* runtime_fixture = nullptr;

	std::string tag;

	circle_collider2D_component() = default;
	circle_collider2D_component(const circle_collider2D_component&) = default;
	circle_collider2D_component& operator=(const circle_collider2D_component&) = default;

	static constexpr const char* script_struct_name = "CircleCollider2DComponent";
};

struct audio_component
{
	struct audio_data
	{
		static constexpr const char* default_tag = "Empty";

		asset_handle audio = 0;
		std::string tag = default_tag;
		glm::vec3 translation = { 0.0f, 0.0f, 0.0f };
		bool spitial = false;
		bool loop = false;
		bool loaded_in_runtime = false;
		float gain = 1.0f;
		float pitch = 1.0f;

		float full_clip_length = 0.0f;
		float clip_start = 0.0f;
		float clip_end = 0.0f;
		UUID32 ID = 0;
	};

	unique_name_manager un_manager;

	std::vector<audio_data> audio_datas;
	size_t selected_audio_index = npos<size_t>;

	audio_component() = default;
	audio_component(const audio_component&) = default;
	audio_component& operator=(const audio_component&) = default;

	static constexpr const char* script_struct_name = "AudioComponent";
};

template<typename... Component>
struct component_group {};

using all_components_no_id_no_tag_no_script = component_group<transform_component, 
	sprite_renderer_component, circle_renderer_component, text_component, camera_component,
	rigidbody2D_component, box_collider2D_component, circle_collider2D_component, audio_component>;

using all_components_no_id_no_tag = component_group<transform_component, sprite_renderer_component,
	circle_renderer_component, text_component, camera_component, script_component, native_script_component,
	rigidbody2D_component, box_collider2D_component, circle_collider2D_component, audio_component>;

using all_components = component_group<transform_component, sprite_renderer_component,
	circle_renderer_component, text_component, camera_component, script_component, native_script_component,
	rigidbody2D_component, box_collider2D_component, circle_collider2D_component, audio_component, ID_component, tag_component>;

_WHIP_END
