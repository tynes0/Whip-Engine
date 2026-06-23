#pragma once

#include <Whip/Core/Core.h>

#include <string>
#include <vector>

_WHIP_START

namespace Assistant
{
	struct KnowledgeMember
	{
		std::string m_Name;
		std::string m_Type;
		std::string m_Description;
	};

	struct KnowledgeType
	{
		std::string m_Name;
		std::string m_Category;
		std::string m_Description;
		std::vector<KnowledgeMember> m_Properties;
		std::vector<KnowledgeMember> m_Methods;
	};

	struct KnowledgeCallback
	{
		std::string m_Signature;
		std::string m_Description;
	};

	struct KnowledgeTool
	{
		std::string m_Name;
		std::string m_Status;
		std::string m_Description;
	};

	struct EngineKnowledgeManifest
	{
		std::string m_Name;
		std::string m_Version;
		std::vector<KnowledgeCallback> m_ScriptCallbacks;
		std::vector<KnowledgeType> m_ScriptTypes;
		std::vector<KnowledgeTool> m_AssistantTools;
		std::vector<std::string> m_ForbiddenApis;
		std::vector<std::string> m_CodeEditRules;
	};

	const EngineKnowledgeManifest& GetEngineKnowledgeManifest();
	std::string BuildEngineKnowledgePrompt();
	std::string BuildEngineKnowledgeJson();
}

_WHIP_END
