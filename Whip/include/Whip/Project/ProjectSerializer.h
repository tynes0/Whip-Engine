#pragma once


#include <Whip/Core/Core.h>
#include "Project.h"

_WHIP_START
class ProjectSerializer
{
public:
	ProjectSerializer(Ref<Project> project);

	bool Serialize(const std::filesystem::path& filepath);
	bool Deserialize(const std::filesystem::path& filepath);
private:
	Ref<Project> m_Project;
};

_WHIP_END
