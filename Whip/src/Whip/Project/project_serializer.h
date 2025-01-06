#pragma once


#include <Whip/Core/Core.h>
#include "project.h"

_WHIP_START
class project_serializer
{
public:
	project_serializer(ref<project> proj);

	bool serialize(const std::filesystem::path& filepath);
	bool deserialize(const std::filesystem::path& filepath);
private:
	ref<project> m_project;
};

_WHIP_END
