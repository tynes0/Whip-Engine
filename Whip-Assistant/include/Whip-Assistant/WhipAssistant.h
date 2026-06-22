#pragma once

#include <Whip/Core/Core.h>

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>
#include <vector>

_WHIP_START

namespace Assistant
{
	enum class Role : uint8_t
	{
		User = 0,
		Assistant,
		System
	};

	enum class ToolKind : uint8_t
	{
		None = 0,
		CreateEntity,
		AddComponent,
		SetTransform
	};

	enum class ProviderKind : uint8_t
	{
		Offline = 0,
		OpenAI,
		Gemini
	};

	struct Settings
	{
		bool m_Enabled = true;
		ProviderKind m_Provider = ProviderKind::Offline;
		bool m_SendSceneContext = true;
		bool m_SendConsoleContext = true;
		std::string m_OpenAIModel = "gpt-5.5";
		std::string m_OpenAIApiKey;
		std::string m_GeminiModel = "gemini-2.0-flash";
		std::string m_GeminiApiKey;
		bool m_GeminiUseGoogleSearch = true;
	};

	struct Message
	{
		Role m_Role = Role::Assistant;
		std::string m_Content;
	};

	struct ContextSnapshot
	{
		bool m_HasProject = false;
		std::string m_ProjectName;
		bool m_HasScene = false;
		std::string m_ScenePath;
		bool m_HasSelection = false;
		uint64_t m_SelectedEntity = 0;
		std::string m_SelectedEntityName;
		std::vector<std::string> m_SelectedComponents;
		bool m_HasSelectedScript = false;
		std::string m_SelectedScriptClass;
		std::vector<std::string> m_RecentConsole;
	};

	struct ToolProposal
	{
		ToolKind m_Kind = ToolKind::None;
		std::string m_Title;
		std::string m_Description;
		std::string m_EntityName;
		uint64_t m_TargetEntity = 0;
		std::string m_ComponentName;
		glm::vec3 m_Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 m_Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 m_Scale = { 1.0f, 1.0f, 1.0f };
		bool m_HasTransform = false;
	};

	struct Response
	{
		bool m_Success = false;
		std::string m_Text;
		std::string m_Error;
		std::vector<ToolProposal> m_Proposals;
	};

	const char* RoleName(Role role);
	const char* ToolKindName(ToolKind kind);
	const char* ProviderName(ProviderKind provider);
	const char* ProviderDisplayName(ProviderKind provider);
	ProviderKind ProviderFromName(const std::string& name);
	bool HasProviderCredentials(const Settings& settings);

	std::string BuildContextPrompt(const ContextSnapshot& context, const Settings& settings);
	std::vector<ToolProposal> BuildLocalProposals(const ContextSnapshot& context, const std::string& prompt);
	Response RequestResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt);
	Response RequestOpenAIResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt);
	Response RequestGeminiResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt);
}

_WHIP_END
