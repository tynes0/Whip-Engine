#pragma once
#include <Whip/Core/Core.h>

#include <unordered_map>
#include <set>
#include <string>

_WHIP_START

class UniqueNameManager 
{
public:
	std::string AddName(const std::string& name);
	bool RemoveName(const std::string& name);
private:
	std::unordered_map<std::string, std::set<int>> m_NameMap;
};

_WHIP_END
