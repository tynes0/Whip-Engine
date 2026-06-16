#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>

#include "Scene.h"
#include "Entity.h"

_WHIP_START

class SceneSerializer
{
public:
	SceneSerializer(const Ref<Scene>& scene);

	void Serialize(const std::filesystem::path& filepath);
	bool Deserialize(const std::filesystem::path& filepath);
	bool SerializeEntityTemplate(Entity entityIn, const std::filesystem::path& filepath);
	Entity DeserializeEntityTemplate(const std::filesystem::path& filepath, AssetHandle sourceHandle = 0);

	void SerializeRuntime(const std::filesystem::path& filepath);
	bool DeserializeRuntime(const std::filesystem::path& filepath);
private:
	Ref<Scene> m_Scene;
};

_WHIP_END
