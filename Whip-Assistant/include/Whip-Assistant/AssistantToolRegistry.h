#pragma once

#include <Whip-Assistant/WhipAssistant.h>

#include <Whip/Core/Core.h>

#include <string>
#include <string_view>
#include <vector>

_WHIP_START

namespace Assistant
{
	struct ToolFieldDefinition
	{
		std::string m_Name;
		std::string m_Type;
		bool m_Required = true;
		std::string m_Description;
	};

	struct ToolDefinition
	{
		ToolKind m_Kind = ToolKind::None;
		std::string m_Name;
		std::string m_DisplayName;
		std::string m_Status;
		std::string m_Description;
		std::string m_ResponseFormat;
		bool m_ProviderCallable = false;
		bool m_RequiresApply = true;
		std::vector<ToolFieldDefinition> m_Fields;
	};

	const std::vector<ToolDefinition>& GetAssistantToolRegistry();
	const ToolDefinition* FindAssistantTool(ToolKind kind);
	const ToolDefinition* FindAssistantTool(std::string_view name);
	std::string BuildAssistantToolRegistryPrompt();
	std::string BuildAssistantToolRegistryJson();
}

_WHIP_END
