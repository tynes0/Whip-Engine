#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>

#include "scene.h"

_WHIP_START

class scene_serializer
{
public:
	scene_serializer(const ref<scene>& scene);

	void serialize(const std::filesystem::path& filepath);
	bool deserialize(const std::filesystem::path& filepath);

	void serialize_runtime(const std::filesystem::path& filepath);
	bool deserialize_runtime(const std::filesystem::path& filepath);
private:
	ref<scene> m_scene;
};

_WHIP_END
