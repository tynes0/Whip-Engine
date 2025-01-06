#pragma once
#include "Core.h"

#include <unordered_map>
#include <set>
#include <string>

_WHIP_START

class unique_name_manager 
{
public:
	std::string add_name(const std::string& name);
	bool remove_name(const std::string& name);
private:
	std::unordered_map<std::string, std::set<int>> name_map;
};

_WHIP_END
