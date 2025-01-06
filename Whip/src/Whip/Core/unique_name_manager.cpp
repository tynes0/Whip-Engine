#include "whippch.h"
#include "unique_name_manager.h"

#include <regex>

_WHIP_START

std::string unique_name_manager::add_name(const std::string& name)
{
	std::string checked_name = !name.empty() ? name : "(1)";
	std::string base_name;
	int count = 0;

	std::regex re(R"(^(.*?)(\((\d+)\))?$)");
	std::smatch match;

	if (std::regex_match(checked_name, match, re))
	{
		base_name = match[1];
		std::string suffix = match[3];
		if (!suffix.empty())
			count = std::stoi(suffix);
	}

	while (name_map[base_name].count(count))
		++count;

	name_map[base_name].insert(count);
	return count == 0 ? base_name : base_name + "(" + std::to_string(count) + ")";
}

bool unique_name_manager::remove_name(const std::string& name)
{
	std::string base_name;
	int count = 0;

	std::regex re(R"(^(.*?)(\((\d+)\))?$)");
	std::smatch match;

	if (std::regex_match(name, match, re)) 
	{
		base_name = match[1];
		std::string suffix = match[3];
		if (!suffix.empty())
			count = std::stoi(suffix);
	}

	if (name_map.count(base_name) && name_map[base_name].count(count)) 
	{
		name_map[base_name].erase(count);
		if (name_map[base_name].empty()) 
			name_map.erase(base_name);
		return true;
	}

	return false;
}

_WHIP_END
