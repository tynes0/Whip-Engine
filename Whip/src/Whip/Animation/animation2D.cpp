#include <whippch.h>
#include <Whip/Scene/components.h>

#include "animation2D.h"
#include "animation_manager.h"

#define YAML_CPP_STATIC_DEFINE
#include <yaml-cpp/yaml.h>

namespace YAML
{

	template<>
	struct convert<whip::asset_handle>
	{
		static Node encode(const whip::asset_handle& handle)
		{
			Node node;
			node.push_back((uint64_t)handle);
			return node;
		}

		static bool decode(const Node& node, whip::asset_handle& handle)
		{
			handle = node.as<uint64_t>();
			return true;
		}
	};
}

_WHIP_START

animation2D::animation2D(asset_handle handle_in) : asset(handle_in), m_frames() { }

animation2D::~animation2D() 
{
	//animation_manager::get_animation_name_manager().remove_name(m_name);
}

ref<animation2D> animation2D::copy(ref<animation2D> anim)
{
	auto new_animation = make_ref<animation2D>();
	new_animation->m_frames = anim->m_frames;
	new_animation->m_loop = anim->m_loop;
	new_animation->m_name = anim->m_name;
	return new_animation;
}

void animation2D::set_frames(const std::vector<animation_frame>& frames, bool loop)
{
	m_frames = frames;
	m_loop = loop;
}

void animation2D::add_frame(const animation_frame& frame)
{
	m_frames.push_back(frame);
}

void animation2D::remove_frame(size_t index)
{
	if (index < m_frames.size())
		m_frames.erase(m_frames.begin() + index);
}

void animation2D::bound_with_entity(entity target_entity)
{
	if (!target_entity.has_component<sprite_renderer_component>())
	{
		WHP_CORE_ERROR("[animation2D] Target entity does not have a sprite_renderer_component!");
		return;
	}

	auto& sprite_renderer = target_entity.get_component<sprite_renderer_component>();
	m_original_texture = sprite_renderer.texture;

	m_target_entity = target_entity;
}

void animation2D::unbound_from_entity()
{
	m_target_entity = {};
	m_original_texture = {};
}

void animation2D::apply_frame(asset_handle texture)
{
	if (!m_target_entity)
	{
		WHP_CORE_ERROR("[animation2D] No entity bound!");
		return;
	}

	auto& sprite_renderer = m_target_entity.get_component<sprite_renderer_component>();
	sprite_renderer.texture = texture;
}

void animation2D::play()
{
	if (m_is_playing)
		return;

	if (!m_target_entity)
	{
		WHP_CORE_ERROR("[animation2D] No entity bound to this animation!");
		return;
	}

	m_is_playing = true;
	m_is_paused = false;
	m_current_frame = 0;
	m_elapsed_time = 0.0f;

	if (!m_frames.empty())
		apply_frame(m_frames[m_current_frame].texture);
}

void animation2D::stop()
{
	if (!m_is_playing)
		return;

	m_is_playing = false;
	m_is_paused = false;
	m_current_frame = 0;
	m_elapsed_time = 0.0f;

	apply_frame(m_original_texture);
}

void animation2D::pause()
{
	if (!m_is_playing || m_is_paused)
		return;

	m_is_paused = true;
}

void animation2D::resume()
{
	if (!m_is_playing || !m_is_paused)
		return;

	m_is_paused = false;
}

void animation2D::set_name(const std::string& new_name)
{
	animation_manager::get_animation_name_manager().remove_name(m_name);
	m_name = animation_manager::get_animation_name_manager().add_name(new_name);
}

void animation2D::serialize(const std::filesystem::path& filepath)
{
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "name" << YAML::Value << m_name;
	out << YAML::Key << "loop" << YAML::Value << m_loop;
	out << YAML::Key << "frames" << YAML::Value << YAML::BeginSeq;

	for (const auto& frame : m_frames)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "handle" << YAML::Value << frame.texture;
		out << YAML::Key << "duration" << YAML::Value << frame.duration;
		out << YAML::EndMap;
	}

	out << YAML::EndSeq;
	out << YAML::EndMap;

	std::ofstream fout(filepath);
	fout << out.c_str();
}

bool animation2D::deserialize(const std::filesystem::path& filepath)
{
	YAML::Node data;
	try
	{
		data = YAML::LoadFile(filepath.string());
	}
	catch (YAML::Exception& e)
	{
		WHP_CORE_ERROR("[animation2D] Failed to load .wanim file '{0}' -> {1}", filepath.string(), e.what());
		return false;
	}

	m_name = animation_manager::get_animation_name_manager().add_name(data["name"].as<std::string>());
	m_loop = data["loop"].as<bool>();
	m_frames.clear();

	const auto& frames_node = data["frames"];
	for (const auto& frame_node : frames_node)
	{
		animation_frame frame;
		frame.texture = frame_node["handle"].as<uint64_t>();
		frame.duration = frame_node["duration"].as<float>();
		m_frames.push_back(frame);
	}
	return true;
}

_WHIP_END
