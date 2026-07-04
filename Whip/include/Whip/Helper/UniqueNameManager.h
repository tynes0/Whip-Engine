#pragma once
#include <Whip/Core/Core.h>
#include <Whip/Core/Memory/AllocatorRegistry.h>

#include <string>

_WHIP_START

class UniqueNameManager 
{
public:
	std::string AddName(const std::string& name);
	bool RemoveName(const std::string& name);
private:
	memory::UnorderedMap<std::string, memory::Set<int>> m_NameMap;
};

_WHIP_END
