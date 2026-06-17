#include "WhipPch.h"
#include <Whip/Helper/UniqueNameManager.h>

#include <regex>

_WHIP_START

std::string UniqueNameManager::AddName(const std::string& name)
{
	std::string checkedName = !name.empty() ? name : "(1)";
	std::string baseName;
	int count = 0;

	std::regex re(R"(^(.*?)(\((\d+)\))?$)");
	std::smatch match;

	if (std::regex_match(checkedName, match, re))
	{
		baseName = match[1];
		std::string suffix = match[3];
		if (!suffix.empty())
			count = std::stoi(suffix);
	}

	while (m_NameMap[baseName].contains(count))
		++count;

	m_NameMap[baseName].insert(count);
	return count == 0 ? baseName : baseName + "(" + std::to_string(count) + ")";
}

bool UniqueNameManager::RemoveName(const std::string& name)
{
	std::string baseName;
	int count = 0;

	std::regex re(R"(^(.*?)(\((\d+)\))?$)");
	std::smatch match;

	if (std::regex_match(name, match, re))
	{
		baseName = match[1];
		std::string suffix = match[3];
		if (!suffix.empty())
			count = std::stoi(suffix);
	}

	if (m_NameMap.contains(baseName) && m_NameMap[baseName].contains(count))
	{
		m_NameMap[baseName].erase(count);
		if (m_NameMap[baseName].empty())
			m_NameMap.erase(baseName);
		return true;
	}

	return false;
}

_WHIP_END
