#include "whippch.h"
#include "scene_serializer.h"

#include "entity.h"
#include "components.h"

#include <Whip/Core/UUID.h>
#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/MouseButtonCodes.h>
#include <Whip/Project/project.h>
#include <Whip/Scripting/script_engine.h>
#include <Whip/Asset/asset_manager.h>
#include <Whip/Audio/audio_source.h>

#include <coco.h>

#include <fstream>

#define YAML_CPP_STATIC_DEFINE
#include <yaml-cpp/yaml.h>

#define WRITE_SCRIPT_FIELD(field_type, type)            \
			case script_field_type::field_type:			\
			{											\
				out << sc_field.get_value<type>();		\
				break;									\
			}

#define READ_SCRIPT_FIELD(field_type, type)				\
	case script_field_type::field_type:					\
	{													\
		type data = sc_field["data"].as<type>();		\
		field_instance.set_value(data);					\
		break;											\
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

namespace utils
{

	static std::string rigidbody2D_body_type_to_string(rigidbody2D_component::body_type type)
	{
		switch (type)
		{
		case rigidbody2D_component::body_type::bt_static:    return "Static";
		case rigidbody2D_component::body_type::bt_dynamic:   return "Dynamic";
		case rigidbody2D_component::body_type::bt_kinematic: return "Kinematic";
		}

		WHP_CORE_ASSERT(false, "Unknown body type");
		return {};
	}

	static rigidbody2D_component::body_type rigidbody2D_body_type_from_string(const std::string& bodyTypeString)
	{
		if (bodyTypeString == "Static")    return rigidbody2D_component::body_type::bt_static;
		if (bodyTypeString == "Dynamic")   return rigidbody2D_component::body_type::bt_dynamic;
		if (bodyTypeString == "Kinematic") return rigidbody2D_component::body_type::bt_kinematic;

		WHP_CORE_ASSERT(false, "Unknown body type");
		return rigidbody2D_component::body_type::bt_static;
	}

	static void serialize_entity(YAML::Emitter& out, entity entity_in)
	{
		WHP_CORE_ASSERT(entity_in.has_component<ID_component>(), "entity has no ID!");
		out << YAML::BeginMap; // Entity
		out << YAML::Key << "entity" << YAML::Value << entity_in.get_UUID();

		if (entity_in.has_component<tag_component>())
		{
			out << YAML::Key << "tag_component";
			out << YAML::BeginMap; // tag_component

			auto& tag = entity_in.get_component<tag_component>().tag;
			out << YAML::Key << "tag" << YAML::Value << tag;

			out << YAML::EndMap; // tag_component
		}

		if (entity_in.has_component<transform_component>())
		{
			out << YAML::Key << "transform_component";
			out << YAML::BeginMap; // transform_component

			auto& tc = entity_in.get_component<transform_component>();
			out << YAML::Key << "translation" << YAML::Value << tc.translation;
			out << YAML::Key << "rotation" << YAML::Value << tc.rotation;
			out << YAML::Key << "scale" << YAML::Value << tc.scale;

			out << YAML::EndMap; // transform_component
		}

		if (entity_in.has_component<camera_component>())
		{
			out << YAML::Key << "camera_component";
			out << YAML::BeginMap; // camera_component

			auto& camera_comp = entity_in.get_component<camera_component>();
			auto& camera = camera_comp.camera;

			out << YAML::Key << "camera" << YAML::Value;
			out << YAML::BeginMap; // Camera
			out << YAML::Key << "projection_type" << YAML::Value << (int)camera.get_projection_type();
			out << YAML::Key << "perspective_FOV" << YAML::Value << camera.get_perspective_vertical_FOV();
			out << YAML::Key << "perspective_near" << YAML::Value << camera.get_perspective_near_clip();
			out << YAML::Key << "perspective_far" << YAML::Value << camera.get_perspective_far_clip();
			out << YAML::Key << "orthographic_size" << YAML::Value << camera.get_orthographic_size();
			out << YAML::Key << "orthographic_near" << YAML::Value << camera.get_orthographic_near_clip();
			out << YAML::Key << "orthographic_far" << YAML::Value << camera.get_orthographic_far_clip();
			out << YAML::EndMap; // Camera

			out << YAML::Key << "primary" << YAML::Value << camera_comp.primary;
			out << YAML::Key << "fixed_aspect_ratio" << YAML::Value << camera_comp.fixed_aspect_ratio;

			out << YAML::EndMap; // camera_component
		}

		if (entity_in.has_component<script_component>())
		{
			auto& script_comp = entity_in.get_component<script_component>();

			out << YAML::Key << "script_component";
			out << YAML::BeginMap; // script_component
			out << YAML::Key << "class_name" << YAML::Value << script_comp.class_name;
			ref<script_class> entity_class = script_engine::get_entity_class(script_comp.class_name);
			const auto& fields = entity_class->get_fields();
			if (fields.size() > 0)
			{
				out << YAML::Key << "script_fields" << YAML::Value;
				auto& entity_fields = script_engine::get_script_field_map(entity_in);
				out << YAML::BeginSeq;
				for (const auto& [name, field] : fields)
				{
					if (entity_fields.find(name) == entity_fields.end())
						continue;

					out << YAML::BeginMap; // script_fields
					out << YAML::Key << "name" << YAML::Value << name;
					out << YAML::Key << "type" << YAML::Value << utils::script_field_type_to_string(field.type);

					out << YAML::Key << "data" << YAML::Value;
					script_field_instance& sc_field = entity_fields.at(name);

					switch (field.type)
					{
						WRITE_SCRIPT_FIELD(Float, float);
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
						WRITE_SCRIPT_FIELD(KeyCode, key_code);
						WRITE_SCRIPT_FIELD(MouseCode, mouse_code);
						WRITE_SCRIPT_FIELD(Vector2, glm::vec2);
						WRITE_SCRIPT_FIELD(Vector3, glm::vec3);
						WRITE_SCRIPT_FIELD(Vector4, glm::vec4);
						WRITE_SCRIPT_FIELD(Entity, UUID);
					}
					out << YAML::EndMap; // script_fields
				}
				out << YAML::EndSeq;
			}
			out << YAML::EndMap; // script_component
		}

		if (entity_in.has_component<sprite_renderer_component>())
		{
			out << YAML::Key << "sprite_renderer_component";
			out << YAML::BeginMap; // sprite_renderer_component

			auto& sprite_renderer_comp = entity_in.get_component<sprite_renderer_component>();
			out << YAML::Key << "color" << YAML::Value << sprite_renderer_comp.color;
			out << YAML::Key << "texture_handle" << YAML::Value << sprite_renderer_comp.texture;

			out << YAML::Key << "tiling_factor" << YAML::Value << sprite_renderer_comp.tiling_factor;

			out << YAML::EndMap; // sprite_renderer_component
		}

		if (entity_in.has_component<circle_renderer_component>())
		{
			out << YAML::Key << "circle_renderer_component";
			out << YAML::BeginMap; // CircleRendererComponent
		
			auto& circle_renderer_comp = entity_in.get_component<circle_renderer_component>();
			out << YAML::Key << "color" << YAML::Value << circle_renderer_comp.color;
			out << YAML::Key << "thickness" << YAML::Value << circle_renderer_comp.thickness;
			out << YAML::Key << "fade" << YAML::Value << circle_renderer_comp.fade;
		
			out << YAML::EndMap; // CircleRendererComponent
		}

		if (entity_in.has_component<text_component>())
		{
			out << YAML::Key << "text_component";
			out << YAML::BeginMap; // text_component

			auto& text_comp = entity_in.get_component<text_component>();
			out << YAML::Key << "text_string" << YAML::Value << text_comp.text_string;
			// TODO: text_component.font_asset
			out << YAML::Key << "color" << YAML::Value << text_comp.color;
			out << YAML::Key << "kerning" << YAML::Value << text_comp.kerning;
			out << YAML::Key << "line_spacing" << YAML::Value << text_comp.line_spacing;

			out << YAML::EndMap; // text_component
		}

		if (entity_in.has_component<rigidbody2D_component>())
		{
			out << YAML::Key << "rigidbody2D_component";
			out << YAML::BeginMap; // rigidbody2D_component

			auto& rb2dComponent = entity_in.get_component<rigidbody2D_component>();
			out << YAML::Key << "body_type" << YAML::Value << rigidbody2D_body_type_to_string(rb2dComponent.type);
			out << YAML::Key << "fixed_rotation" << YAML::Value << rb2dComponent.fixed_rotation;
			out << YAML::Key << "gravity_scale" << YAML::Value << rb2dComponent.gravity_scale;

			out << YAML::EndMap; // rigidbody2D_component
		}

		if (entity_in.has_component<box_collider2D_component>())
		{
			out << YAML::Key << "box_collider2D_component";
			out << YAML::BeginMap; // box_collider2D_component

			auto& bc2dComponent = entity_in.get_component<box_collider2D_component>();
			out << YAML::Key << "offset" << YAML::Value << bc2dComponent.offset;
			out << YAML::Key << "size" << YAML::Value << bc2dComponent.size;
			out << YAML::Key << "density" << YAML::Value << bc2dComponent.density;
			out << YAML::Key << "friction" << YAML::Value << bc2dComponent.friction;
			out << YAML::Key << "restitution" << YAML::Value << bc2dComponent.restitution;
			out << YAML::Key << "restitution_threshold" << YAML::Value << bc2dComponent.restitution_threshold;
			out << YAML::Key << "sensor" << YAML::Value << bc2dComponent.sensor;
			out << YAML::Key << "tag" << YAML::Value << bc2dComponent.tag;

			out << YAML::EndMap; // box_collider2D_component
		}

		if (entity_in.has_component<circle_collider2D_component>())
		{
			out << YAML::Key << "circle_collider2D_component";
			out << YAML::BeginMap; // circle_collider2D_component

			auto& cc2dComponent = entity_in.get_component<circle_collider2D_component>();
			out << YAML::Key << "offset" << YAML::Value << cc2dComponent.offset;
			out << YAML::Key << "radius" << YAML::Value << cc2dComponent.radius;
			out << YAML::Key << "density" << YAML::Value << cc2dComponent.density;
			out << YAML::Key << "friction" << YAML::Value << cc2dComponent.friction;
			out << YAML::Key << "restitution" << YAML::Value << cc2dComponent.restitution;
			out << YAML::Key << "restitution_threshold" << YAML::Value << cc2dComponent.restitution_threshold;
			out << YAML::Key << "sensor" << YAML::Value << cc2dComponent.sensor;
			out << YAML::Key << "tag" << YAML::Value << cc2dComponent.tag;

			out << YAML::EndMap; // circle_collider2D_component
		}

		if (entity_in.has_component<audio_component>())
		{
			out << YAML::Key << "audio_component";
			out << YAML::BeginMap; // audio_component
			auto& a_component = entity_in.get_component<audio_component>();
			if (!a_component.audio_datas.empty())
			{
				out << YAML::Key << "audio_datas" << YAML::Value;
				out << YAML::BeginSeq;
				for (const auto& data : a_component.audio_datas)
				{
					out << YAML::BeginMap; // data
					out << YAML::Key << "audio" << YAML::Value << data.audio;
					out << YAML::Key << "tag" << YAML::Value << data.tag;
					out << YAML::Key << "translation" << YAML::Value << data.translation;
					out << YAML::Key << "spitial" << YAML::Value << data.spitial;
					out << YAML::Key << "loop" << YAML::Value << data.loop;
					out << YAML::Key << "gain" << YAML::Value << data.gain;
					out << YAML::Key << "pitch" << YAML::Value << data.pitch;
					out << YAML::Key << "clip_start" << YAML::Value << data.clip_start;
					out << YAML::Key << "clip_end" << YAML::Value << data.clip_end;
					out << YAML::Key << "full_clip_length" << YAML::Value << data.full_clip_length;
					out << YAML::Key << "ID" << YAML::Value << data.ID;
					out << YAML::EndMap; // data
				}
				out << YAML::EndSeq;
			}
			out << YAML::EndMap; // audio_component
		}
		out << YAML::EndMap; // Entity
	}
}

scene_serializer::scene_serializer(const ref<scene>& scene) : m_scene(scene) {}

void scene_serializer::serialize(const std::filesystem::path& filepath)
{
	WHP_CORE_DEBUG("[Scene serializer] Scene serializing...");
	coco::timer<coco::time_units::milliseconds> coco_timer;
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "scene" << YAML::Value << filepath.stem().string();
	out << YAML::Key << "entities" << YAML::Value << YAML::BeginSeq;

	auto group = m_scene->m_registry.group<>(entt::get<ID_component>);

	for (auto entityID : group)
	{
		entity ent = { entityID, m_scene.get() };
		if (!ent)
			return;

		utils::serialize_entity(out, ent);
	}

	out << YAML::EndSeq;
	out << YAML::EndMap;

	std::ofstream fout(filepath);
	fout << out.c_str();
	coco_timer.stop();
	WHP_CORE_DEBUG("[Scene serializer] Scene serialized in {0} millisecond.", coco_timer.get_time());
}

void scene_serializer::serialize_runtime(const std::filesystem::path& filepath)
{
	WHP_CORE_ASSERT(false, "Not Implamented!"); // Not implemented
}

bool scene_serializer::deserialize(const std::filesystem::path& filepath)
{
	WHP_CORE_DEBUG("[Scene Serializer] Scene deserializing...");
	coco::timer<coco::time_units::milliseconds> coco_timer;
	YAML::Node data;
	try
	{
		data = YAML::LoadFile(filepath.string());
	}
	catch (YAML::Exception e)
	{
		WHP_CORE_ERROR("[Scene Serializer] Failed to load .whip file '{0}' -> {1}", filepath.string(), e.what());
		return false;
	}
	if (!data["scene"])
		return false;

	std::string scene_name = data["scene"].as<std::string>();

	auto entities = data["entities"];
	if (entities)
	{
		for (auto ent : entities)
		{
			uint64_t uuid = ent["entity"].as<uint64_t>();

			std::string name;
			auto tag_comp = ent["tag_component"];
			if (tag_comp)
				name = tag_comp["tag"].as<std::string>();

			entity deserialized_entity = m_scene->create_entity_with_UUID(uuid, name);

			auto transform_comp = ent["transform_component"];
			if (transform_comp)
			{
				// Entities always have transforms
				auto& tc = deserialized_entity.get_component<transform_component>();
				tc.translation = transform_comp["translation"].as<glm::vec3>();
				tc.rotation = transform_comp["rotation"].as<glm::vec3>();
				tc.scale = transform_comp["scale"].as<glm::vec3>();
			}

			auto camera_comp = ent["camera_component"];
			if (camera_comp)
			{
				auto& cc = deserialized_entity.add_component<camera_component>();

				auto camera_props = camera_comp["camera"];
				cc.camera.set_projection_type((scene_camera::projection_type)camera_props["projection_type"].as<int>());

				cc.camera.set_perspective_vertical_FOV(camera_props["perspective_FOV"].as<float>());
				cc.camera.set_perspective_near_clip(camera_props["perspective_near"].as<float>());
				cc.camera.set_perspective_far_clip(camera_props["perspective_far"].as<float>());

				cc.camera.set_orthographic_size(camera_props["orthographic_size"].as<float>());
				cc.camera.set_orthographic_near_clip(camera_props["orthographic_near"].as<float>());
				cc.camera.set_orthographic_far_clip(camera_props["orthographic_far"].as<float>());

				cc.primary = camera_comp["primary"].as<bool>();
				cc.fixed_aspect_ratio = camera_comp["fixed_aspect_ratio"].as<bool>();
			}

			auto script_comp = ent["script_component"];
			if (script_comp)
			{
				auto& sc = deserialized_entity.add_component<script_component>();
				sc.class_name = script_comp["class_name"].as<std::string>();

				auto sc_fields = script_comp["script_fields"];
				if (sc_fields)
				{
					ref<script_class> entity_class = script_engine::get_entity_class(sc.class_name);
					if(entity_class)
					{
						const auto& fields = entity_class->get_fields();
						auto& entity_fields = script_engine::get_script_field_map(deserialized_entity);

						for (auto sc_field : sc_fields)
						{
							std::string name = sc_field["name"].as<std::string>();
							std::string type_string = sc_field["type"].as<std::string>();
							script_field_type type = utils::script_field_type_from_string(type_string);

							script_field_instance& field_instance = entity_fields[name];


							if (fields.find(name) == fields.end())
							{
								WHP_CORE_WARN("Entity {0} has no script field called {1}", uuid, name);
								continue;
							}

							field_instance.field = fields.at(name);

							switch (type)
							{
								READ_SCRIPT_FIELD(Float, float);
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
								READ_SCRIPT_FIELD(KeyCode, key_code);
								READ_SCRIPT_FIELD(MouseCode, mouse_code);
								READ_SCRIPT_FIELD(Vector2, glm::vec2);
								READ_SCRIPT_FIELD(Vector3, glm::vec3);
								READ_SCRIPT_FIELD(Vector4, glm::vec4);
								READ_SCRIPT_FIELD(Entity, UUID);
							}
						}
					}
				}
			}

			auto sprite_renderer_comp = ent["sprite_renderer_component"];
			if (sprite_renderer_comp)
			{
				auto& src = deserialized_entity.add_component<sprite_renderer_component>();
				src.color = sprite_renderer_comp["color"].as<glm::vec4>();

				if (sprite_renderer_comp["texture_handle"])
					src.texture = sprite_renderer_comp["texture_handle"].as<asset_handle>();

				if (sprite_renderer_comp["tiling_factor"])
					src.tiling_factor = sprite_renderer_comp["tiling_factor"].as<float>();
			}

			auto circle_renderer_comp = ent["circle_renderer_component"];
			if (circle_renderer_comp)
			{
				auto& crc = deserialized_entity.add_component<circle_renderer_component>();
				crc.color = circle_renderer_comp["color"].as<glm::vec4>();
				crc.thickness = circle_renderer_comp["thickness"].as<float>();
				crc.fade = circle_renderer_comp["fade"].as<float>();
			}

			auto text_comp = ent["text_component"];
			if (text_comp)
			{
				auto& tc = deserialized_entity.add_component<text_component>();
				tc.text_string = text_comp["text_string"].as<std::string>();
				// tc.font_asset // TODO
				tc.color = text_comp["color"].as<glm::vec4>();
				tc.kerning = text_comp["kerning"].as<float>();
				tc.line_spacing = text_comp["line_spacing"].as<float>();
			}

			auto rigidbody2D_comp = ent["rigidbody2D_component"];
			if (rigidbody2D_comp)
			{
				auto& rb2d = deserialized_entity.add_component<rigidbody2D_component>();
				rb2d.type = utils::rigidbody2D_body_type_from_string(rigidbody2D_comp["body_type"].as<std::string>());
				rb2d.fixed_rotation = rigidbody2D_comp["fixed_rotation"].as<bool>();
				rb2d.gravity_scale = rigidbody2D_comp["gravity_scale"].as<float>();
			}

			auto box_collider2D_comp = ent["box_collider2D_component"];
			if (box_collider2D_comp)
			{
				auto& bc2d = deserialized_entity.add_component<box_collider2D_component>();
				bc2d.offset = box_collider2D_comp["offset"].as<glm::vec2>();
				bc2d.size = box_collider2D_comp["size"].as<glm::vec2>();
				bc2d.density = box_collider2D_comp["density"].as<float>();
				bc2d.friction = box_collider2D_comp["friction"].as<float>();
				bc2d.restitution = box_collider2D_comp["restitution"].as<float>();
				bc2d.restitution_threshold = box_collider2D_comp["restitution_threshold"].as<float>();
				bc2d.sensor = box_collider2D_comp["sensor"].as<bool>();
				bc2d.tag = box_collider2D_comp["tag"].as<std::string>();
			}

			auto circle_collider2d_comp = ent["circle_collider2D_component"];
			if (circle_collider2d_comp)
			{
				auto& cc2d = deserialized_entity.add_component<circle_collider2D_component>();
				cc2d.offset = circle_collider2d_comp["offset"].as<glm::vec2>();
				cc2d.radius = circle_collider2d_comp["radius"].as<float>();
				cc2d.density = circle_collider2d_comp["density"].as<float>();
				cc2d.friction = circle_collider2d_comp["friction"].as<float>();
				cc2d.restitution = circle_collider2d_comp["restitution"].as<float>();
				cc2d.restitution_threshold = circle_collider2d_comp["restitution_threshold"].as<float>();
				cc2d.sensor = circle_collider2d_comp["sensor"].as<bool>();
				cc2d.tag = circle_collider2d_comp["tag"].as<std::string>();
			}

			auto audio_comp = ent["audio_component"];
			if (audio_comp)
			{
				auto& ac = deserialized_entity.add_component<audio_component>();

				auto audio_datas = audio_comp["audio_datas"];
				ac.audio_datas.reserve(audio_datas.size());
				if (audio_datas)
				{
					for(auto _audio_data : audio_datas)
					{
						audio_component::audio_data comp_audio_data;
						comp_audio_data.audio = _audio_data["audio"].as<uint64_t>();
						comp_audio_data.tag = _audio_data["tag"].as<std::string>();
						comp_audio_data.translation = _audio_data["translation"].as<glm::vec3>();
						comp_audio_data.spitial = _audio_data["spitial"].as<bool>();
						comp_audio_data.loop = _audio_data["loop"].as<bool>();
						comp_audio_data.gain = _audio_data["gain"].as<float>();
						comp_audio_data.pitch = _audio_data["pitch"].as<float>();
						comp_audio_data.clip_start = _audio_data["clip_start"].as<float>();
						comp_audio_data.clip_end = _audio_data["clip_end"].as<float>();
						comp_audio_data.full_clip_length = _audio_data["full_clip_length"].as<float>();
						comp_audio_data.ID = _audio_data["ID"].as<uint32_t>();
						ac.audio_datas.push_back(comp_audio_data);
						ac.un_manager.add_name(comp_audio_data.tag);
					}
				}
			}
		}
	}
	coco_timer.stop();
	WHP_CORE_DEBUG("[Scene Serializer] Scene '{0}' deserialized in {1} milliseconds.", scene_name, coco_timer.get_time());
	return true;
}

bool scene_serializer::deserialize_runtime(const std::filesystem::path& filepath)
{
	// Not implemented
	WHP_CORE_ASSERT(false, "");
	return false;
}

_WHIP_END
