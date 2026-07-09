#include "WhipPch.h"
#include <Whip/Scene/SceneSerializer.h>

#include <Whip/Scene/Entity.h>
#include <Whip/Scene/Components.h>

#include <Whip/Core/UUID.h>
#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/Memory.h>
#include <Whip/Core/MouseButtonCodes.h>
#include <Whip/Project/Project.h>
#include <Whip/Scripting/ScriptEngine.h>
#include <Whip/Asset/AssetManager.h>
#include <Whip/Audio/AudioSource.h>

#include <coco.h>

#include <fstream>
#include <memory>

#ifndef YAML_CPP_STATIC_DEFINE
#define YAML_CPP_STATIC_DEFINE
#endif
#include <yaml-cpp/yaml.h>

#define WRITE_SCRIPT_FIELD(fieldType, type)                                    \
			case ScriptFieldType::fieldType:                                 \
			{                                                                  \
				whip::Utils::WriteScriptFieldData<type>(out, scriptField);  \
				break;                                                         \
			}

#define READ_SCRIPT_FIELD(fieldType, type)                                            \
	case ScriptFieldType::fieldType:                                                \
	{                                                                                  \
		whip::Utils::ReadScriptFieldData<type>(scriptField["data"], fieldInstance); \
		break;                                                                         \
	}

namespace YAML 
{
	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};

	template<>
	struct convert<whip::UUID>
	{
		static Node encode(const whip::UUID& uuid)
		{
			Node node;
			node.push_back((uint64_t)uuid);
			return node;
		}

		static bool decode(const Node& node, whip::UUID& uuid)
		{
			uuid = node.as<uint64_t>();
			return true;
		}
	};

	template<>
	struct convert<whip::UUID32>
	{
		static Node encode(const whip::UUID32& uuid)
		{
			Node node;
			node.push_back((uint32_t)uuid);
			return node;
		}

		static bool decode(const Node& node, whip::UUID32& uuid)
		{
			uuid = node.as<uint32_t>();
			return true;
		}
	};
}

_WHIP_START

namespace
{
	bool HasYamlNode(const YAML::Node& node)
	{
		return node.IsDefined() && !node.IsNull();
	}

	YAML::Node FindYamlValue(const YAML::Node& mapNode, const char* key)
	{
		if (!mapNode.IsDefined() || !mapNode.IsMap())
			return {};

		for (const auto& entry : mapNode)
		{
			try
			{
				if (entry.first.as<std::string>() == key)
					return entry.second;
			}
			catch (const YAML::Exception&)
			{
			}
		}

		return {};
	}

	YAML::Node FindYamlValue(const YAML::Node& mapNode, const char* firstKey, const char* secondKey)
	{
		YAML::Node value = FindYamlValue(mapNode, firstKey);
		if (!HasYamlNode(value))
			value = FindYamlValue(mapNode, secondKey);
		return value;
	}
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
{
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
	return out;
}

namespace Utils
{
	template <typename T>
	void WriteScriptFieldData(YAML::Emitter& out, ScriptFieldInstance& fieldInstance)
	{
		if (!fieldInstance.m_Field.m_IsArray)
		{
			out << fieldInstance.GetValue<T>();
			return;
		}

		const size_t size = fieldInstance.GetArraySize<T>();
		T* values = fieldInstance.GetValueArray<T>();

		out << YAML::BeginSeq;
		for (size_t i = 0; i < size; ++i)
			out << values[i];
		out << YAML::EndSeq;
	}

	template <>
	void WriteScriptFieldData<uint64_t>(YAML::Emitter& out, ScriptFieldInstance& fieldInstance)
	{
		if (!fieldInstance.m_Field.m_IsArray)
		{
			out << (fieldInstance.CanGetValue<uint64_t>() ? fieldInstance.GetValue<uint64_t>() : 0);
			return;
		}

		const size_t size = fieldInstance.GetArraySize<uint64_t>();
		uint64_t* values = fieldInstance.GetValueArray<uint64_t>();

		out << YAML::BeginSeq;
		for (size_t i = 0; i < size; ++i)
			out << values[i];
		out << YAML::EndSeq;
	}

	template <>
	void WriteScriptFieldData<std::string>(YAML::Emitter& out, ScriptFieldInstance& fieldInstance)
	{
		if (fieldInstance.m_Field.m_IsArray)
		{
			WHP_CORE_WARN("[SceneSerializer] String arrays are not supported for script fields yet: {0}", fieldInstance.m_Field.m_Name);
			out << YAML::BeginSeq << YAML::EndSeq;
			return;
		}

		out << fieldInstance.GetStringValue();
	}

	template <typename T>
	void ReadScriptFieldData(const YAML::Node& dataNode, ScriptFieldInstance& fieldInstance)
	{
		if (!fieldInstance.m_Field.m_IsArray)
		{
			fieldInstance.SetValue(dataNode.as<T>());
			return;
		}

		if (!dataNode || !dataNode.IsSequence())
		{
			WHP_CORE_WARN("[SceneSerializer] Script field array '{0}' has invalid serialized data. Ignoring override.", fieldInstance.m_Field.m_Name);
			return;
		}

		const size_t size = dataNode ? dataNode.size() : 0;
		auto values = MakeScopeArrayTagged<T>(memory::MemoryTag::Scene, size);
		for (size_t i = 0; i < size; ++i)
			values[i] = dataNode[i].as<T>();

		fieldInstance.SetValueArray<T>(values.get(), size);
	}

	template <>
	void ReadScriptFieldData<std::string>(const YAML::Node& dataNode, ScriptFieldInstance& fieldInstance)
	{
		if (fieldInstance.m_Field.m_IsArray)
		{
			WHP_CORE_WARN("[SceneSerializer] String arrays are not supported for script fields yet: {0}", fieldInstance.m_Field.m_Name);
			fieldInstance.SetValueArray<char>(nullptr, 0);
			return;
		}

		fieldInstance.SetStringValue(dataNode ? dataNode.as<std::string>() : std::string());
	}

	static std::string Rigidbody2DBodyTypeToString(Rigidbody2DComponent::BodyType type)
	{
		switch (type)
		{
		case Rigidbody2DComponent::BodyType::Static:    return "Static";
		case Rigidbody2DComponent::BodyType::Dynamic:   return "Dynamic";
		case Rigidbody2DComponent::BodyType::Kinematic: return "Kinematic";
		}

		WHP_CORE_ASSERT(false, "Unknown body type");
		return {};
	}

	static Rigidbody2DComponent::BodyType Rigidbody2DBodyTypeFromString(const std::string& bodyTypeString)
	{
		if (bodyTypeString == "Static")    return Rigidbody2DComponent::BodyType::Static;
		if (bodyTypeString == "Dynamic")   return Rigidbody2DComponent::BodyType::Dynamic;
		if (bodyTypeString == "Kinematic") return Rigidbody2DComponent::BodyType::Kinematic;

		WHP_CORE_ASSERT(false, "Unknown body type");
		return Rigidbody2DComponent::BodyType::Static;
	}

	static void SerializeEntity(YAML::Emitter& out, Entity entityIn)
	{
		WHP_CORE_ASSERT(entityIn.HasComponent<IDComponent>(), "entity has no ID!");
		out << YAML::BeginMap; // Entity
		out << YAML::Key << "entity" << YAML::Value << entityIn.GetUUID();

		if (entityIn.HasComponent<TagComponent>())
		{
			out << YAML::Key << "tag_component";
			out << YAML::BeginMap; // tag_component

			auto& tag = entityIn.GetComponent<TagComponent>().m_Tag;
			out << YAML::Key << "tag" << YAML::Value << tag;

			out << YAML::EndMap; // tag_component
		}

		if (entityIn.HasComponent<HierarchyComponent>())
		{
			auto& hierarchy = entityIn.GetComponent<HierarchyComponent>();
			if (hierarchy.m_Parent != 0 || !hierarchy.m_Children.empty() || hierarchy.m_IsGroup)
			{
				out << YAML::Key << "hierarchy_component";
				out << YAML::BeginMap; // hierarchy_component
				out << YAML::Key << "parent" << YAML::Value << (uint64_t)hierarchy.m_Parent;
				out << YAML::Key << "is_group" << YAML::Value << hierarchy.m_IsGroup;
				out << YAML::Key << "children" << YAML::Value;
				out << YAML::BeginSeq;
				for (UUID child : hierarchy.m_Children)
					out << (uint64_t)child;
				out << YAML::EndSeq;
				out << YAML::EndMap; // hierarchy_component
			}
		}

		if (entityIn.HasComponent<PrefabComponent>())
		{
			auto& prefab = entityIn.GetComponent<PrefabComponent>();
			if (prefab.m_Source != 0)
			{
				out << YAML::Key << "prefab_component";
				out << YAML::BeginMap; // prefab_component
				out << YAML::Key << "source" << YAML::Value << (uint64_t)prefab.m_Source;
				out << YAML::Key << "source_entity" << YAML::Value << (uint64_t)prefab.m_SourceEntity;
				out << YAML::Key << "root" << YAML::Value << prefab.m_Root;
				out << YAML::EndMap; // prefab_component
			}
		}

		if (entityIn.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "transform_component";
			out << YAML::BeginMap; // transform_component

			auto& transform = entityIn.GetComponent<TransformComponent>();
			out << YAML::Key << "translation" << YAML::Value << transform.m_Translation;
			out << YAML::Key << "rotation" << YAML::Value << transform.m_Rotation;
			out << YAML::Key << "scale" << YAML::Value << transform.m_Scale;

			out << YAML::EndMap; // transform_component
		}

		if (entityIn.HasComponent<CameraComponent>())
		{
			out << YAML::Key << "camera_component";
			out << YAML::BeginMap; // camera_component

			auto& cameraComponent = entityIn.GetComponent<CameraComponent>();
			auto& camera = cameraComponent.m_Camera;

			out << YAML::Key << "Camera" << YAML::Value;
			out << YAML::BeginMap; // Camera
			out << YAML::Key << "projection_type" << YAML::Value << (int)camera.GetProjectionType();
			out << YAML::Key << "perspective_FOV" << YAML::Value << camera.GetPerspectiveVerticalFOV();
			out << YAML::Key << "perspective_near" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "perspective_far" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "orthographic_size" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "orthographic_near" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "orthographic_far" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::EndMap; // Camera

			out << YAML::Key << "primary" << YAML::Value << cameraComponent.m_Primary;
			out << YAML::Key << "fixed_aspect_ratio" << YAML::Value << cameraComponent.m_FixedAspectRatio;

			out << YAML::EndMap; // camera_component
		}

		if (entityIn.HasComponent<ScriptComponent>())
		{
			auto& scriptComponent = entityIn.GetComponent<ScriptComponent>();

			out << YAML::Key << "script_component";
			out << YAML::BeginMap; // script_component
			out << YAML::Key << "class_name" << YAML::Value << scriptComponent.m_ClassName;
			Ref<ScriptClass> entityClass = ScriptEngine::GetEntityClass(scriptComponent.m_ClassName);
			if (entityClass)
			{
				const auto& fields = entityClass->GetFields();
				if (fields.size() > 0)
				{
					out << YAML::Key << "ScriptFields" << YAML::Value;
					auto& entityFields = ScriptEngine::GetScriptFieldMap(entityIn);
					out << YAML::BeginSeq;
					for (const auto& [name, field] : fields)
					{
						if (entityFields.find(name) == entityFields.end())
							continue;

						out << YAML::BeginMap; // ScriptFields
						out << YAML::Key << "name" << YAML::Value << name;
						out << YAML::Key << "type" << YAML::Value << frenum::to_string(field.m_Type);

						out << YAML::Key << "data" << YAML::Value;
						ScriptFieldInstance& scriptField = entityFields.at(name);

						switch (field.m_Type)
						{
							WRITE_SCRIPT_FIELD(Float, float);
							WRITE_SCRIPT_FIELD(String, std::string);
							WRITE_SCRIPT_FIELD(Double, double);
							WRITE_SCRIPT_FIELD(Bool, bool);
							WRITE_SCRIPT_FIELD(Char, char);
							WRITE_SCRIPT_FIELD(SByte, int8_t);
							WRITE_SCRIPT_FIELD(Short, int16_t);
							WRITE_SCRIPT_FIELD(Int, int32_t);
							WRITE_SCRIPT_FIELD(Long, int64_t);
							WRITE_SCRIPT_FIELD(Byte, uint8_t);
							WRITE_SCRIPT_FIELD(UShort, uint16_t);
							WRITE_SCRIPT_FIELD(UInt, uint32_t);
							WRITE_SCRIPT_FIELD(ULong, uint64_t);
							WRITE_SCRIPT_FIELD(KeyCode, KeyCode);
							WRITE_SCRIPT_FIELD(MouseCode, MouseCode);
							WRITE_SCRIPT_FIELD(Vector2, glm::vec2);
							WRITE_SCRIPT_FIELD(Vector3, glm::vec3);
							WRITE_SCRIPT_FIELD(Vector4, glm::vec4);
							WRITE_SCRIPT_FIELD(Entity, UUID);
							WRITE_SCRIPT_FIELD(Scene, uint64_t);
						}
						out << YAML::EndMap; // ScriptFields
					}
					out << YAML::EndSeq;
				}
			}
			else if (!scriptComponent.m_ClassName.empty())
				WHP_CORE_WARN("[Scene Serializer] Script class '{0}' is not loaded. Serializing class name without field overrides.", scriptComponent.m_ClassName);
			out << YAML::EndMap; // script_component
		}

		if (entityIn.HasComponent<AnimatorComponent>())
		{
			auto& animatorComponent = entityIn.GetComponent<AnimatorComponent>();

			out << YAML::Key << "animator_component";
			out << YAML::BeginMap; // animator_component
			out << YAML::Key << "controller" << YAML::Value << (uint64_t)animatorComponent.m_Controller;
			out << YAML::Key << "initial_state" << YAML::Value << animatorComponent.m_InitialState;
			out << YAML::Key << "play_on_start" << YAML::Value << animatorComponent.m_PlayOnStart;
			out << YAML::Key << "speed" << YAML::Value << animatorComponent.m_Speed;
			out << YAML::EndMap; // animator_component
		}

		if (entityIn.HasComponent<SpriteRendererComponent>())
		{
			out << YAML::Key << "sprite_renderer_component";
			out << YAML::BeginMap; // sprite_renderer_component

			auto& spriteRendererComponent = entityIn.GetComponent<SpriteRendererComponent>();
			out << YAML::Key << "color" << YAML::Value << spriteRendererComponent.m_Color;
			out << YAML::Key << "texture_handle" << YAML::Value << spriteRendererComponent.m_Texture;
			out << YAML::Key << "texture_sprite_index" << YAML::Value << spriteRendererComponent.m_TextureSpriteIndex;

			out << YAML::Key << "tiling_factor" << YAML::Value << spriteRendererComponent.m_TilingFactor;

			out << YAML::EndMap; // sprite_renderer_component
		}

		if (entityIn.HasComponent<CircleRendererComponent>())
		{
			out << YAML::Key << "circle_renderer_component";
			out << YAML::BeginMap; // CircleRendererComponent
		
			auto& circleRendererComponent = entityIn.GetComponent<CircleRendererComponent>();
			out << YAML::Key << "color" << YAML::Value << circleRendererComponent.m_Color;
			out << YAML::Key << "thickness" << YAML::Value << circleRendererComponent.m_Thickness;
			out << YAML::Key << "fade" << YAML::Value << circleRendererComponent.m_Fade;
		
			out << YAML::EndMap; // CircleRendererComponent
		}

		if (entityIn.HasComponent<TextComponent>())
		{
			out << YAML::Key << "text_component";
			out << YAML::BeginMap; // text_component

			auto& textComponent = entityIn.GetComponent<TextComponent>();
			out << YAML::Key << "text_string" << YAML::Value << textComponent.m_TextString;
			out << YAML::Key << "Font" << YAML::Value << (uint64_t)textComponent.m_Font;
			out << YAML::Key << "color" << YAML::Value << textComponent.m_Color;
			out << YAML::Key << "kerning" << YAML::Value << textComponent.m_Kerning;
			out << YAML::Key << "line_spacing" << YAML::Value << textComponent.m_LineSpacing;

			out << YAML::EndMap; // text_component
		}

		if (entityIn.HasComponent<UITransformComponent>())
		{
			out << YAML::Key << "ui_transform_component";
			out << YAML::BeginMap; // ui_transform_component

			auto& uiTransform = entityIn.GetComponent<UITransformComponent>();
			out << YAML::Key << "anchor_min" << YAML::Value << uiTransform.m_AnchorMin;
			out << YAML::Key << "anchor_max" << YAML::Value << uiTransform.m_AnchorMax;
			out << YAML::Key << "pivot" << YAML::Value << uiTransform.m_Pivot;
			out << YAML::Key << "anchored_position" << YAML::Value << uiTransform.m_AnchoredPosition;
			out << YAML::Key << "size" << YAML::Value << uiTransform.m_Size;
			out << YAML::Key << "scale" << YAML::Value << uiTransform.m_Scale;
			out << YAML::Key << "rotation" << YAML::Value << uiTransform.m_Rotation;
			out << YAML::Key << "sort_order" << YAML::Value << uiTransform.m_SortOrder;
			out << YAML::Key << "visible" << YAML::Value << uiTransform.m_Visible;

			out << YAML::EndMap; // ui_transform_component
		}

		if (entityIn.HasComponent<UIImageComponent>())
		{
			out << YAML::Key << "ui_image_component";
			out << YAML::BeginMap; // ui_image_component

			auto& image = entityIn.GetComponent<UIImageComponent>();
			out << YAML::Key << "color" << YAML::Value << image.m_Color;
			out << YAML::Key << "texture_handle" << YAML::Value << image.m_Texture;
			out << YAML::Key << "texture_sprite_index" << YAML::Value << image.m_TextureSpriteIndex;
			out << YAML::Key << "raycast_target" << YAML::Value << image.m_RaycastTarget;

			out << YAML::EndMap; // ui_image_component
		}

		if (entityIn.HasComponent<UITextComponent>())
		{
			out << YAML::Key << "ui_text_component";
			out << YAML::BeginMap; // ui_text_component

			auto& text = entityIn.GetComponent<UITextComponent>();
			out << YAML::Key << "text_string" << YAML::Value << text.m_TextString;
			out << YAML::Key << "font" << YAML::Value << (uint64_t)text.m_Font;
			out << YAML::Key << "color" << YAML::Value << text.m_Color;
			out << YAML::Key << "font_size" << YAML::Value << text.m_FontSize;
			out << YAML::Key << "kerning" << YAML::Value << text.m_Kerning;
			out << YAML::Key << "line_spacing" << YAML::Value << text.m_LineSpacing;

			out << YAML::EndMap; // ui_text_component
		}

		if (entityIn.HasComponent<UIButtonComponent>())
		{
			out << YAML::Key << "ui_button_component";
			out << YAML::BeginMap; // ui_button_component

			auto& button = entityIn.GetComponent<UIButtonComponent>();
			out << YAML::Key << "text" << YAML::Value << button.m_Text;
			out << YAML::Key << "font" << YAML::Value << (uint64_t)button.m_Font;
			out << YAML::Key << "normal_color" << YAML::Value << button.m_NormalColor;
			out << YAML::Key << "hovered_color" << YAML::Value << button.m_HoveredColor;
			out << YAML::Key << "pressed_color" << YAML::Value << button.m_PressedColor;
			out << YAML::Key << "disabled_color" << YAML::Value << button.m_DisabledColor;
			out << YAML::Key << "text_color" << YAML::Value << button.m_TextColor;
			out << YAML::Key << "font_size" << YAML::Value << button.m_FontSize;
			out << YAML::Key << "interactable" << YAML::Value << button.m_Interactable;
			out << YAML::Key << "raycast_target" << YAML::Value << button.m_RaycastTarget;

			out << YAML::EndMap; // ui_button_component
		}

		if (entityIn.HasComponent<UIStackLayoutComponent>())
		{
			out << YAML::Key << "ui_stack_layout_component";
			out << YAML::BeginMap; // ui_stack_layout_component

			auto& layout = entityIn.GetComponent<UIStackLayoutComponent>();
			out << YAML::Key << "axis" << YAML::Value << static_cast<int>(layout.m_Axis);
			out << YAML::Key << "alignment" << YAML::Value << static_cast<int>(layout.m_Alignment);
			out << YAML::Key << "padding" << YAML::Value << layout.m_Padding;
			out << YAML::Key << "spacing" << YAML::Value << layout.m_Spacing;
			out << YAML::Key << "child_size" << YAML::Value << layout.m_ChildSize;
			out << YAML::Key << "control_child_width" << YAML::Value << layout.m_ControlChildWidth;
			out << YAML::Key << "control_child_height" << YAML::Value << layout.m_ControlChildHeight;
			out << YAML::Key << "reverse" << YAML::Value << layout.m_Reverse;

			out << YAML::EndMap; // ui_stack_layout_component
		}

		if (entityIn.HasComponent<Rigidbody2DComponent>())
		{
			out << YAML::Key << "rigidbody2D_component";
			out << YAML::BeginMap; // rigidbody2D_component

			auto& rb2dComponent = entityIn.GetComponent<Rigidbody2DComponent>();
			out << YAML::Key << "body_type" << YAML::Value << Rigidbody2DBodyTypeToString(rb2dComponent.m_Type);
			out << YAML::Key << "fixed_rotation" << YAML::Value << rb2dComponent.m_FixedRotation;
			out << YAML::Key << "gravity_scale" << YAML::Value << rb2dComponent.m_GravityScale;

			out << YAML::EndMap; // rigidbody2D_component
		}

		if (entityIn.HasComponent<BoxCollider2DComponent>())
		{
			out << YAML::Key << "box_collider2D_component";
			out << YAML::BeginMap; // box_collider2D_component

			auto& bc2dComponent = entityIn.GetComponent<BoxCollider2DComponent>();
			out << YAML::Key << "offset" << YAML::Value << bc2dComponent.m_Offset;
			out << YAML::Key << "size" << YAML::Value << bc2dComponent.m_Size;
			out << YAML::Key << "density" << YAML::Value << bc2dComponent.m_Density;
			out << YAML::Key << "friction" << YAML::Value << bc2dComponent.m_Friction;
			out << YAML::Key << "restitution" << YAML::Value << bc2dComponent.m_Restitution;
			out << YAML::Key << "restitution_threshold" << YAML::Value << bc2dComponent.m_RestitutionThreshold;
			out << YAML::Key << "sensor" << YAML::Value << bc2dComponent.m_Sensor;
			out << YAML::Key << "tag" << YAML::Value << bc2dComponent.m_Tag;

			out << YAML::EndMap; // box_collider2D_component
		}

		if (entityIn.HasComponent<CircleCollider2DComponent>())
		{
			out << YAML::Key << "circle_collider2D_component";
			out << YAML::BeginMap; // circle_collider2D_component

			auto& cc2dComponent = entityIn.GetComponent<CircleCollider2DComponent>();
			out << YAML::Key << "offset" << YAML::Value << cc2dComponent.m_Offset;
			out << YAML::Key << "radius" << YAML::Value << cc2dComponent.m_Radius;
			out << YAML::Key << "density" << YAML::Value << cc2dComponent.m_Density;
			out << YAML::Key << "friction" << YAML::Value << cc2dComponent.m_Friction;
			out << YAML::Key << "restitution" << YAML::Value << cc2dComponent.m_Restitution;
			out << YAML::Key << "restitution_threshold" << YAML::Value << cc2dComponent.m_RestitutionThreshold;
			out << YAML::Key << "sensor" << YAML::Value << cc2dComponent.m_Sensor;
			out << YAML::Key << "tag" << YAML::Value << cc2dComponent.m_Tag;

			out << YAML::EndMap; // circle_collider2D_component
		}

		if (entityIn.HasComponent<AudioComponent>())
		{
			out << YAML::Key << "audio_component";
			out << YAML::BeginMap; // audio_component
			auto& audioComponent = entityIn.GetComponent<AudioComponent>();
			if (!audioComponent.m_AudioDatas.empty())
			{
				out << YAML::Key << "audio_datas" << YAML::Value;
				out << YAML::BeginSeq;
				for (const auto& data : audioComponent.m_AudioDatas)
				{
					out << YAML::BeginMap; // data
					out << YAML::Key << "audio" << YAML::Value << data.m_Audio;
					out << YAML::Key << "tag" << YAML::Value << data.m_Tag;
					out << YAML::Key << "translation" << YAML::Value << data.m_Translation;
					out << YAML::Key << "spitial" << YAML::Value << data.m_Spatial;
					out << YAML::Key << "loop" << YAML::Value << data.m_Loop;
					out << YAML::Key << "gain" << YAML::Value << data.m_Gain;
					out << YAML::Key << "pitch" << YAML::Value << data.m_Pitch;
					out << YAML::Key << "clip_start" << YAML::Value << data.m_ClipStart;
					out << YAML::Key << "clip_end" << YAML::Value << data.m_ClipEnd;
					out << YAML::Key << "full_clip_length" << YAML::Value << data.m_FullClipLength;
					out << YAML::Key << "ID" << YAML::Value << data.m_ID;
					out << YAML::EndMap; // data
				}
				out << YAML::EndSeq;
			}
			out << YAML::EndMap; // audio_component
		}
		out << YAML::EndMap; // Entity
	}
}

SceneSerializer::SceneSerializer(const Ref<Scene>& scene) : m_Scene(scene) {}

void SceneSerializer::Serialize(const std::filesystem::path& filepath)
{
	WHP_CORE_DEBUG("[Scene serializer] Scene serializing...");
	coco::timer<coco::time_units::milliseconds> cocoTimer;
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "scene" << YAML::Value << filepath.stem().string();
	out << YAML::Key << "entities" << YAML::Value << YAML::BeginSeq;

	auto group = m_Scene->m_Registry.group<>(entt::get<IDComponent>);

	for (auto entityID : group)
	{
		Entity ent = { entityID, m_Scene.get() };
		if (!ent)
			return;

		Utils::SerializeEntity(out, ent);
	}

	out << YAML::EndSeq;
	out << YAML::EndMap;

	std::error_code error;
	if (!filepath.parent_path().empty())
	{
		std::filesystem::create_directories(filepath.parent_path(), error);
		if (error)
		{
			WHP_CORE_ERROR("[Scene serializer] Could not create scene directory '{0}': {1}", filepath.parent_path().string(), error.message());
			return;
		}
	}

	std::ofstream fout(filepath);
	if (!fout)
	{
		WHP_CORE_ERROR("[Scene serializer] Could not open scene file for writing: {0}", filepath.string());
		return;
	}

	fout << out.c_str();
	cocoTimer.stop();
	WHP_CORE_DEBUG("[Scene serializer] Scene serialized in {0} millisecond.", cocoTimer.get_time());
}

bool SceneSerializer::SerializeEntityTemplate(Entity entityIn, const std::filesystem::path& filepath)
{
	if (!entityIn)
		return false;

	Ref<Scene> templateScene = MakeRef<Scene>();
	Entity templateRoot = templateScene->InstantiateEntityTemplate(entityIn);
	if (!templateRoot)
		return false;

	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "scene" << YAML::Value << filepath.stem().string();
	out << YAML::Key << "template_type" << YAML::Value << "entity";
	out << YAML::Key << "entities" << YAML::Value << YAML::BeginSeq;

	auto view = templateScene->m_Registry.view<IDComponent>();
	for (auto entityId : view)
	{
		Entity templateEntity{ entityId, templateScene.get() };
		Utils::SerializeEntity(out, templateEntity);
	}

	out << YAML::EndSeq;
	out << YAML::EndMap;

	std::ofstream file(filepath);
	if (!file)
		return false;

	file << out.c_str();
	return true;
}

Entity SceneSerializer::DeserializeEntityTemplate(const std::filesystem::path& filepath, AssetHandle sourceHandle)
{
	Ref<Scene> templateScene = MakeRef<Scene>();
	SceneSerializer templateSerializer(templateScene);
	if (!templateSerializer.Deserialize(filepath))
		return {};

	auto view = templateScene->m_Registry.view<IDComponent>();
	Entity fallbackSource;
	for (auto entityId : view)
	{
		Entity source{ entityId, templateScene.get() };
		if (!fallbackSource)
			fallbackSource = source;

		if (source.HasComponent<HierarchyComponent>() && source.GetComponent<HierarchyComponent>().m_Parent == 0)
			return m_Scene->InstantiateEntityTemplate(source, sourceHandle);
	}

	return fallbackSource ? m_Scene->InstantiateEntityTemplate(fallbackSource, sourceHandle) : Entity{};
}

void SceneSerializer::SerializeRuntime(const std::filesystem::path& filepath)
{
	WHP_CORE_ASSERT(false, "Not Implamented!"); // Not implemented
}

bool SceneSerializer::Deserialize(const std::filesystem::path& filepath)
{
	WHP_CORE_DEBUG("[Scene Serializer] Scene deserializing...");
	coco::timer<coco::time_units::milliseconds> cocoTimer;
	YAML::Node data;
	try
	{
		data = YAML::LoadFile(filepath.string());
	}
	catch (YAML::Exception e)
	{
		WHP_CORE_ERROR("[Scene Serializer] Failed to load .wscene file '{0}' -> {1}", filepath.string(), e.what());
		return false;
	}
	std::string sceneName;
	try
	{
	auto sceneNode = FindYamlValue(data, "scene", "Scene");
	if (!HasYamlNode(sceneNode))
		return false;

	sceneName = sceneNode.as<std::string>();

	auto entities = FindYamlValue(data, "entities", "Entities");
	if (HasYamlNode(entities))
	{
		for (auto entityNode : entities)
		{
			uint64_t uuid = entityNode["entity"].as<uint64_t>();

			std::string name;
			auto tagComponent = entityNode["tag_component"];
			if (tagComponent)
				name = tagComponent["tag"].as<std::string>();

			Entity deserializedEntity = m_Scene->CreateEntityWithUUID(uuid, name);

			auto hierarchyComponent = entityNode["hierarchy_component"];
			if (hierarchyComponent)
			{
				auto& hierarchy = deserializedEntity.GetComponent<HierarchyComponent>();
				if (hierarchyComponent["parent"])
					hierarchy.m_Parent = hierarchyComponent["parent"].as<uint64_t>();
				if (hierarchyComponent["is_group"])
					hierarchy.m_IsGroup = hierarchyComponent["is_group"].as<bool>();
				if (hierarchyComponent["children"])
				{
					for (auto child : hierarchyComponent["children"])
						hierarchy.m_Children.push_back(child.as<uint64_t>());
				}
			}

			auto prefabComponent = entityNode["prefab_component"];
			if (prefabComponent)
			{
				auto& prefab = deserializedEntity.AddComponent<PrefabComponent>();
				if (prefabComponent["source"])
					prefab.m_Source = prefabComponent["source"].as<uint64_t>();
				if (prefabComponent["source_entity"])
					prefab.m_SourceEntity = prefabComponent["source_entity"].as<uint64_t>();
				if (prefabComponent["root"])
					prefab.m_Root = prefabComponent["root"].as<bool>();
			}

			auto transformComponent = entityNode["transform_component"];
			if (transformComponent)
			{
				// Entities always have transforms
				auto& transform = deserializedEntity.GetComponent<TransformComponent>();
				transform.m_Translation = transformComponent["translation"].as<glm::vec3>();
				transform.m_Rotation = transformComponent["rotation"].as<glm::vec3>();
				transform.m_Scale = transformComponent["scale"].as<glm::vec3>();
			}

			auto cameraComponent = entityNode["camera_component"];
			if (cameraComponent)
			{
				auto& camera = deserializedEntity.AddComponent<CameraComponent>();

				auto cameraProperties = FindYamlValue(cameraComponent, "camera", "Camera");
				if (!HasYamlNode(cameraProperties))
				{
					WHP_CORE_WARN("Entity {0} camera component has no camera data. Using default camera.", uuid);
				}
				else
				{
					camera.m_Camera.SetProjectionType((SceneCamera::ProjectionType)cameraProperties["projection_type"].as<int>((int)SceneCamera::ProjectionType::Orthographic));

					camera.m_Camera.SetPerspectiveVerticalFOV(cameraProperties["perspective_FOV"].as<float>(camera.m_Camera.GetPerspectiveVerticalFOV()));
					camera.m_Camera.SetPerspectiveNearClip(cameraProperties["perspective_near"].as<float>(camera.m_Camera.GetPerspectiveNearClip()));
					camera.m_Camera.SetPerspectiveFarClip(cameraProperties["perspective_far"].as<float>(camera.m_Camera.GetPerspectiveFarClip()));

					camera.m_Camera.SetOrthographicSize(cameraProperties["orthographic_size"].as<float>(camera.m_Camera.GetOrthographicSize()));
					camera.m_Camera.SetOrthographicNearClip(cameraProperties["orthographic_near"].as<float>(camera.m_Camera.GetOrthographicNearClip()));
					camera.m_Camera.SetOrthographicFarClip(cameraProperties["orthographic_far"].as<float>(camera.m_Camera.GetOrthographicFarClip()));
				}

				camera.m_Primary = cameraComponent["primary"].as<bool>(camera.m_Primary);
				camera.m_FixedAspectRatio = cameraComponent["fixed_aspect_ratio"].as<bool>(camera.m_FixedAspectRatio);
			}

			auto scriptComponent = entityNode["script_component"];
			if (scriptComponent)
			{
				auto& script = deserializedEntity.AddComponent<ScriptComponent>();
				script.m_ClassName = scriptComponent["class_name"].as<std::string>();

				auto scriptFields = FindYamlValue(scriptComponent, "script_fields", "ScriptFields");
				if (HasYamlNode(scriptFields))
				{
					Ref<ScriptClass> entityClass = ScriptEngine::GetEntityClass(script.m_ClassName);
					if(entityClass)
					{
						const auto& fields = entityClass->GetFields();
						auto& entityFields = ScriptEngine::GetScriptFieldMap(deserializedEntity);

						for (auto scriptField : scriptFields)
						{
							std::string name = scriptField["name"].as<std::string>();
							std::string typeString = scriptField["type"].as<std::string>();
							auto type = frenum::cast<ScriptFieldType>(typeString);

							if (fields.find(name) == fields.end())
							{
								WHP_CORE_WARN("Entity {0} has no script field called {1}", uuid, name);
								continue;
							}

							if (!type.has_value())
							{
								WHP_CORE_WARN("Entity {0} script field {1} has invalid type {2}", uuid, name, typeString);
								continue;
							}

							const ScriptField& field = fields.at(name);
							const YAML::Node dataNode = scriptField["data"];
							if (field.m_IsArray && (!dataNode || !dataNode.IsSequence()))
							{
								WHP_CORE_WARN("Entity {0} script field array {1} has invalid data. Using script default.", uuid, name);
								continue;
							}

							ScriptFieldInstance& fieldInstance = entityFields[name];
							fieldInstance.m_Field = field;

							switch (*type)
							{
								READ_SCRIPT_FIELD(Float, float);
								READ_SCRIPT_FIELD(String, std::string);
								READ_SCRIPT_FIELD(Double, double);
								READ_SCRIPT_FIELD(Bool, bool);
								READ_SCRIPT_FIELD(Char, char);
								READ_SCRIPT_FIELD(SByte, int8_t);
								READ_SCRIPT_FIELD(Short, int16_t);
								READ_SCRIPT_FIELD(Int, int32_t);
								READ_SCRIPT_FIELD(Long, int64_t);
								READ_SCRIPT_FIELD(Byte, uint8_t);
								READ_SCRIPT_FIELD(UShort, uint16_t);
								READ_SCRIPT_FIELD(UInt, uint32_t);
								READ_SCRIPT_FIELD(ULong, uint64_t);
								READ_SCRIPT_FIELD(KeyCode, KeyCode);
								READ_SCRIPT_FIELD(MouseCode, MouseCode);
								READ_SCRIPT_FIELD(Vector2, glm::vec2);
								READ_SCRIPT_FIELD(Vector3, glm::vec3);
								READ_SCRIPT_FIELD(Vector4, glm::vec4);
								READ_SCRIPT_FIELD(Entity, UUID);
								READ_SCRIPT_FIELD(Scene, uint64_t);
							}
						}
					}
				}
			}

			auto animatorComponent = entityNode["animator_component"];
			if (animatorComponent)
			{
				auto& animator = deserializedEntity.AddComponent<AnimatorComponent>();
				animator.m_Controller = animatorComponent["controller"].as<uint64_t>(0);
				animator.m_InitialState = animatorComponent["initial_state"].as<std::string>("");
				animator.m_PlayOnStart = animatorComponent["play_on_start"].as<bool>(animator.m_PlayOnStart);
				animator.m_Speed = animatorComponent["speed"].as<float>(animator.m_Speed);
			}

			auto spriteRendererComponent = entityNode["sprite_renderer_component"];
			if (spriteRendererComponent)
			{
				auto& sprite = deserializedEntity.AddComponent<SpriteRendererComponent>();
				sprite.m_Color = spriteRendererComponent["color"].as<glm::vec4>();

				if (spriteRendererComponent["texture_handle"])
					sprite.m_Texture = spriteRendererComponent["texture_handle"].as<AssetHandle>();

				if (spriteRendererComponent["texture_sprite_index"])
					sprite.m_TextureSpriteIndex = spriteRendererComponent["texture_sprite_index"].as<int32_t>(-1);

				if (spriteRendererComponent["tiling_factor"])
					sprite.m_TilingFactor = spriteRendererComponent["tiling_factor"].as<float>();
			}

			auto circleRendererComponent = entityNode["circle_renderer_component"];
			if (circleRendererComponent)
			{
				auto& circleRenderer = deserializedEntity.AddComponent<CircleRendererComponent>();
				circleRenderer.m_Color = circleRendererComponent["color"].as<glm::vec4>();
				circleRenderer.m_Thickness = circleRendererComponent["thickness"].as<float>();
				circleRenderer.m_Fade = circleRendererComponent["fade"].as<float>();
			}

			auto textComponent = entityNode["text_component"];
			if (textComponent)
			{
				auto& text = deserializedEntity.AddComponent<TextComponent>();
				text.m_TextString = textComponent["text_string"].as<std::string>();
				auto font = FindYamlValue(textComponent, "font", "Font");
				if (HasYamlNode(font))
					text.m_Font = font.as<uint64_t>();
				text.m_Color = textComponent["color"].as<glm::vec4>();
				text.m_Kerning = textComponent["kerning"].as<float>();
				text.m_LineSpacing = textComponent["line_spacing"].as<float>();
			}

			auto uiTransformComponent = entityNode["ui_transform_component"];
			if (uiTransformComponent)
			{
				auto& uiTransform = deserializedEntity.AddComponent<UITransformComponent>();
				uiTransform.m_AnchorMin = uiTransformComponent["anchor_min"].as<glm::vec2>(uiTransform.m_AnchorMin);
				uiTransform.m_AnchorMax = uiTransformComponent["anchor_max"].as<glm::vec2>(uiTransform.m_AnchorMax);
				uiTransform.m_Pivot = uiTransformComponent["pivot"].as<glm::vec2>(uiTransform.m_Pivot);
				uiTransform.m_AnchoredPosition = uiTransformComponent["anchored_position"].as<glm::vec2>(uiTransform.m_AnchoredPosition);
				uiTransform.m_Size = uiTransformComponent["size"].as<glm::vec2>(uiTransform.m_Size);
				uiTransform.m_Scale = uiTransformComponent["scale"].as<glm::vec2>(uiTransform.m_Scale);
				uiTransform.m_Rotation = uiTransformComponent["rotation"].as<float>(uiTransform.m_Rotation);
				uiTransform.m_SortOrder = uiTransformComponent["sort_order"].as<int32_t>(uiTransform.m_SortOrder);
				uiTransform.m_Visible = uiTransformComponent["visible"].as<bool>(uiTransform.m_Visible);
			}

			auto uiImageComponent = entityNode["ui_image_component"];
			if (uiImageComponent)
			{
				auto& image = deserializedEntity.AddComponent<UIImageComponent>();
				image.m_Color = uiImageComponent["color"].as<glm::vec4>(image.m_Color);
				image.m_Texture = uiImageComponent["texture_handle"].as<AssetHandle>(image.m_Texture);
				image.m_TextureSpriteIndex = uiImageComponent["texture_sprite_index"].as<int32_t>(image.m_TextureSpriteIndex);
				image.m_RaycastTarget = uiImageComponent["raycast_target"].as<bool>(image.m_RaycastTarget);
			}

			auto uiTextComponent = entityNode["ui_text_component"];
			if (uiTextComponent)
			{
				auto& text = deserializedEntity.AddComponent<UITextComponent>();
				text.m_TextString = uiTextComponent["text_string"].as<std::string>(text.m_TextString);
				text.m_Font = uiTextComponent["font"].as<uint64_t>(text.m_Font);
				text.m_Color = uiTextComponent["color"].as<glm::vec4>(text.m_Color);
				text.m_FontSize = uiTextComponent["font_size"].as<float>(text.m_FontSize);
				text.m_Kerning = uiTextComponent["kerning"].as<float>(text.m_Kerning);
				text.m_LineSpacing = uiTextComponent["line_spacing"].as<float>(text.m_LineSpacing);
			}

			auto uiButtonComponent = entityNode["ui_button_component"];
			if (uiButtonComponent)
			{
				auto& button = deserializedEntity.AddComponent<UIButtonComponent>();
				button.m_Text = uiButtonComponent["text"].as<std::string>(button.m_Text);
				button.m_Font = uiButtonComponent["font"].as<uint64_t>(button.m_Font);
				button.m_NormalColor = uiButtonComponent["normal_color"].as<glm::vec4>(button.m_NormalColor);
				button.m_HoveredColor = uiButtonComponent["hovered_color"].as<glm::vec4>(button.m_HoveredColor);
				button.m_PressedColor = uiButtonComponent["pressed_color"].as<glm::vec4>(button.m_PressedColor);
				button.m_DisabledColor = uiButtonComponent["disabled_color"].as<glm::vec4>(button.m_DisabledColor);
				button.m_TextColor = uiButtonComponent["text_color"].as<glm::vec4>(button.m_TextColor);
				button.m_FontSize = uiButtonComponent["font_size"].as<float>(button.m_FontSize);
				button.m_Interactable = uiButtonComponent["interactable"].as<bool>(button.m_Interactable);
				button.m_RaycastTarget = uiButtonComponent["raycast_target"].as<bool>(button.m_RaycastTarget);
			}

			auto uiStackLayoutComponent = entityNode["ui_stack_layout_component"];
			if (uiStackLayoutComponent)
			{
				auto& layout = deserializedEntity.AddComponent<UIStackLayoutComponent>();
				layout.m_Axis = static_cast<UIStackLayoutComponent::Axis>(uiStackLayoutComponent["axis"].as<int>(static_cast<int>(layout.m_Axis)));
				layout.m_Alignment = static_cast<UIStackLayoutComponent::Alignment>(uiStackLayoutComponent["alignment"].as<int>(static_cast<int>(layout.m_Alignment)));
				layout.m_Padding = uiStackLayoutComponent["padding"].as<glm::vec4>(layout.m_Padding);
				layout.m_Spacing = uiStackLayoutComponent["spacing"].as<float>(layout.m_Spacing);
				layout.m_ChildSize = uiStackLayoutComponent["child_size"].as<glm::vec2>(layout.m_ChildSize);
				layout.m_ControlChildWidth = uiStackLayoutComponent["control_child_width"].as<bool>(layout.m_ControlChildWidth);
				layout.m_ControlChildHeight = uiStackLayoutComponent["control_child_height"].as<bool>(layout.m_ControlChildHeight);
				layout.m_Reverse = uiStackLayoutComponent["reverse"].as<bool>(layout.m_Reverse);
			}

			auto rigidbody2DComponent = entityNode["rigidbody2D_component"];
			if (rigidbody2DComponent)
			{
				auto& rb2d = deserializedEntity.AddComponent<Rigidbody2DComponent>();
				rb2d.m_Type = Utils::Rigidbody2DBodyTypeFromString(rigidbody2DComponent["body_type"].as<std::string>());
				rb2d.m_FixedRotation = rigidbody2DComponent["fixed_rotation"].as<bool>();
				rb2d.m_GravityScale = rigidbody2DComponent["gravity_scale"].as<float>();
			}

			auto boxCollider2DComponent = entityNode["box_collider2D_component"];
			if (boxCollider2DComponent)
			{
				auto& bc2d = deserializedEntity.AddComponent<BoxCollider2DComponent>();
				bc2d.m_Offset = boxCollider2DComponent["offset"].as<glm::vec2>();
				bc2d.m_Size = boxCollider2DComponent["size"].as<glm::vec2>();
				bc2d.m_Density = boxCollider2DComponent["density"].as<float>();
				bc2d.m_Friction = boxCollider2DComponent["friction"].as<float>();
				bc2d.m_Restitution = boxCollider2DComponent["restitution"].as<float>();
				bc2d.m_RestitutionThreshold = boxCollider2DComponent["restitution_threshold"].as<float>();
				bc2d.m_Sensor = boxCollider2DComponent["sensor"].as<bool>();
				bc2d.m_Tag = boxCollider2DComponent["tag"].as<std::string>();
			}

			auto circleCollider2DComponent = entityNode["circle_collider2D_component"];
			if (circleCollider2DComponent)
			{
				auto& cc2d = deserializedEntity.AddComponent<CircleCollider2DComponent>();
				cc2d.m_Offset = circleCollider2DComponent["offset"].as<glm::vec2>();
				cc2d.m_Radius = circleCollider2DComponent["radius"].as<float>();
				cc2d.m_Density = circleCollider2DComponent["density"].as<float>();
				cc2d.m_Friction = circleCollider2DComponent["friction"].as<float>();
				cc2d.m_Restitution = circleCollider2DComponent["restitution"].as<float>();
				cc2d.m_RestitutionThreshold = circleCollider2DComponent["restitution_threshold"].as<float>();
				cc2d.m_Sensor = circleCollider2DComponent["sensor"].as<bool>();
				cc2d.m_Tag = circleCollider2DComponent["tag"].as<std::string>();
			}

			auto audioComponent = entityNode["audio_component"];
			if (audioComponent)
			{
				auto& audio = deserializedEntity.AddComponent<AudioComponent>();

				auto audioDatas = audioComponent["audio_datas"];
				audio.m_AudioDatas.reserve(audioDatas.size());
				if (audioDatas)
				{
					for(auto audioDataNode : audioDatas)
					{
						AudioComponent::AudioData componentAudioData;
						componentAudioData.m_Audio = audioDataNode["audio"].as<uint64_t>();
						componentAudioData.m_Tag = audioDataNode["tag"].as<std::string>();
						componentAudioData.m_Translation = audioDataNode["translation"].as<glm::vec3>();
						componentAudioData.m_Spatial = audioDataNode["spitial"].as<bool>();
						componentAudioData.m_Loop = audioDataNode["loop"].as<bool>();
						componentAudioData.m_Gain = audioDataNode["gain"].as<float>();
						componentAudioData.m_Pitch = audioDataNode["pitch"].as<float>();
						componentAudioData.m_ClipStart = audioDataNode["clip_start"].as<float>();
						componentAudioData.m_ClipEnd = audioDataNode["clip_end"].as<float>();
						componentAudioData.m_FullClipLength = audioDataNode["full_clip_length"].as<float>();
						componentAudioData.m_ID = audioDataNode["ID"].as<uint32_t>();
						audio.m_AudioDatas.push_back(componentAudioData);
						audio.m_UniqueNameManager.AddName(componentAudioData.m_Tag);
					}
				}
			}
		}
	}
	}
	catch (const YAML::Exception& e)
	{
		WHP_CORE_ERROR("[Scene Serializer] Failed to deserialize .wscene file '{0}' -> {1}", filepath.string(), e.what());
		return false;
	}
	cocoTimer.stop();
	WHP_CORE_DEBUG("[Scene Serializer] Scene '{0}' deserialized in {1} milliseconds.", sceneName, cocoTimer.get_time());
	return true;
}

bool SceneSerializer::DeserializeRuntime(const std::filesystem::path& filepath)
{
	// Not implemented
	WHP_CORE_ASSERT(false, "");
	return false;
}

_WHIP_END
