#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Render/Texture.h>
#include <Whip/Asset/TextureImporter.h>

#include <string>
#include <filesystem>
#include <unordered_map>

_WHIP_START

template <class KeyType = std::string>
class TextureManager
{
public:
	TextureManager() = default;

	Ref<Texture2D> Add(const KeyType& key, const std::filesystem::path& filepath, FlipDirection flipDirection = FlipDirectionNone)
	{
		return m_TextureMap[key] = TextureImporter::LoadTexture2D(filepath, flipDirection);
	}

	Ref<Texture2D> Add(const KeyType& key, Ref<Texture2D> texture)
	{
		return m_TextureMap[key] = texture;
	}

	bool Remove(const KeyType& key)
	{
		auto it = m_TextureMap.find(key);
		if (it == m_TextureMap.end())
			return false;
		m_TextureMap.erase(it);
		return true;
	}

	Ref<Texture2D> Get(const KeyType& key)
	{
		auto it = m_TextureMap.find(key);
		if (it == m_TextureMap.end())
			return nullptr;
		return m_TextureMap[key];
	}

	void Clear() 
	{ 
		m_TextureMap.clear(); 
	}

	bool Exist(const KeyType& name) const 
	{ 
		return m_TextureMap.find(name) != m_TextureMap.end(); 
	}

	bool Empty() const 
	{ 
		return m_TextureMap.empty(); 
	}

	size_t Size() const 
	{ 
		return m_TextureMap.size(); 
	}
private:
	std::unordered_map<KeyType, Ref<Texture2D>> m_TextureMap;
};

_WHIP_END
