#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Render/Texture.h>
#include <Whip/Asset/texture_importer.h>

#include <string>
#include <filesystem>
#include <unordered_map>

_WHIP_START

template <class KeyType = std::string>
class texture_manager
{
public:
	texture_manager() {}

	ref<texture2D> add(const KeyType& key, const std::filesystem::path& filepath, flip_direction_t flip_direction = flip_direction_none)
	{
		m_texture_map[key] = texture_importer::load_texture2D(filepath, flip_direction);
	}

	ref<texture2D> add(const KeyType& key, ref<texture2D> tex)
	{
		m_texture_map[key] = tex;
	}

	bool remove(const KeyType& key)
	{
		auto it = m_texture_map.find(key);
		if (it == m_texture_map.end())
			return false;
		m_texture_map.erase(it);
		return true;
	}

	ref<texture2D> get(const KeyType& key)
	{
		auto it = m_texture_map.find(key);
		if (it == m_texture_map.end())
			return nullptr;
		return m_texture_map[key];
	}

	void clear() 
	{ 
		m_texture_map.clear(); 
	}

	bool exist(const KeyType& name) const 
	{ 
		return m_texture_map.find(name) != m_texture_map.end(); 
	}

	bool empty() const 
	{ 
		return m_texture_map.empty(); 
	}

	size_t size() const 
	{ 
		return m_texture_map.size(); 
	}
private:
	std::unordered_map<KeyType, ref<texture2D>> m_texture_map;
};

_WHIP_END
