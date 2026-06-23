#include <Whip-Assistant/WhipAssistant.h>

#include <Whip-Assistant/AssistantToolRegistry.h>
#include <Whip-Assistant/Providers/GeminiProvider.h>
#include <Whip-Assistant/Providers/IAssistantProvider.h>
#include <Whip-Assistant/Providers/OpenAIProvider.h>

#include "Providers/AssistantProviderUtils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <string_view>
#include <utility>

_WHIP_START

namespace Assistant
{
	namespace
	{
		std::string LowerCopy(std::string value)
		{
			std::ranges::transform(value, value.begin(),
				[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
			return value;
		}

		bool Contains(std::string_view haystack, std::string_view needle)
		{
			return haystack.find(needle) != std::string_view::npos;
		}

		bool ContainsAny(std::string_view haystack, std::initializer_list<std::string_view> needles)
		{
			for (std::string_view needle : needles)
				if (Contains(haystack, needle))
					return true;
			return false;
		}

		std::string TrimCopy(std::string_view value)
		{
			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
				value.remove_prefix(1);
			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
				value.remove_suffix(1);
			return std::string(value);
		}

		bool StartsWithNoCase(std::string_view value, std::string_view prefix)
		{
			if (value.size() < prefix.size())
				return false;

			for (size_t i = 0; i < prefix.size(); ++i)
				if (std::tolower(static_cast<unsigned char>(value[i])) != std::tolower(static_cast<unsigned char>(prefix[i])))
					return false;
			return true;
		}

		std::string StripSingleEdgeNewline(std::string value)
		{
			if (value.starts_with("\r\n"))
				value.erase(0, 2);
			else if (value.starts_with('\n') || value.starts_with('\r'))
				value.erase(0, 1);

			if (value.ends_with("\r\n"))
				value.erase(value.size() - 2);
			else if (value.ends_with('\n') || value.ends_with('\r'))
				value.erase(value.size() - 1);
			return value;
		}

		std::string PickEntityName(const std::string& prompt)
		{
			const size_t firstQuote = prompt.find_first_of("\"'");
			if (firstQuote != std::string::npos)
			{
				const size_t secondQuote = prompt.find_first_of("\"'", firstQuote + 1);
				if (secondQuote != std::string::npos && secondQuote > firstQuote + 1)
					return prompt.substr(firstQuote + 1, secondQuote - firstQuote - 1);
			}

			const std::string lower = LowerCopy(prompt);
			if (ContainsAny(lower, { "camera", "kamera" }))
				return "AI Camera";
			if (ContainsAny(lower, { "player", "character", "karakter", "oyuncu" }))
				return "Player";
			if (ContainsAny(lower, { "enemy", "dusman", "dushman" }))
				return "Enemy";
			return "AI Entity";
		}

		void PushAddComponentProposal(std::vector<ToolProposal>& proposals, const ContextSnapshot& context, std::string componentName)
		{
			if (!context.m_HasSelection)
				return;

			ToolProposal proposal;
			proposal.m_Kind = ToolKind::AddComponent;
			proposal.m_Title = "Add " + componentName;
			proposal.m_Description = "Adds " + componentName + " to selected entity '" + context.m_SelectedEntityName + "'.";
			proposal.m_TargetEntity = context.m_SelectedEntity;
			proposal.m_ComponentName = std::move(componentName);
			proposals.push_back(std::move(proposal));
		}
	}

	const char* RoleName(Role role)
	{
		switch (role)
		{
		case Role::User: return "User";
		case Role::Assistant: return "Assistant";
		case Role::System: return "System";
		default: return "Unknown";
		}
	}

	const char* ToolKindName(ToolKind kind)
	{
		if (const ToolDefinition* definition = FindAssistantTool(kind))
			return definition->m_DisplayName.c_str();
		return "None";
	}

	const char* ProviderName(ProviderKind provider)
	{
		switch (provider)
		{
		case ProviderKind::OpenAI: return "openai";
		case ProviderKind::Gemini: return "gemini";
		case ProviderKind::Offline:
		default: return "offline";
		}
	}

	const char* ProviderDisplayName(ProviderKind provider)
	{
		switch (provider)
		{
		case ProviderKind::OpenAI: return "OpenAI";
		case ProviderKind::Gemini: return "Gemini";
		case ProviderKind::Offline:
		default: return "Offline";
		}
	}

	ProviderKind ProviderFromName(const std::string& name)
	{
		const std::string lower = LowerCopy(name);
		if (lower == "openai")
			return ProviderKind::OpenAI;
		if (lower == "gemini")
			return ProviderKind::Gemini;
		return ProviderKind::Offline;
	}

	bool HasProviderCredentials(const Settings& settings)
	{
		if (AssistantProviderPtr provider = CreateAssistantProvider(settings.m_Provider))
			return provider->HasCredentials(settings);
		return false;
	}

	std::string BuildContextPrompt(const ContextSnapshot& context, const Settings& settings)
	{
		std::ostringstream stream;
		stream << "Whip Editor context:\n";
		stream << "- Local editor time: " << ProviderUtils::GetLocalEditorTime() << '\n';
		stream << "- Project: " << (context.m_HasProject ? context.m_ProjectName : "none") << '\n';
		stream << "- Scene: " << (context.m_HasScene ? context.m_ScenePath : "none") << '\n';

		if (settings.m_SendSceneContext && context.m_HasSelection)
		{
			stream << "- Selected entity: " << context.m_SelectedEntityName << " (" << context.m_SelectedEntity << ")\n";
			stream << "- Components:";
			for (const std::string& component : context.m_SelectedComponents)
				stream << ' ' << component;
			stream << '\n';

			if (context.m_HasSelectedScript)
				stream << "- Selected script class: " << context.m_SelectedScriptClass << '\n';

			if (context.m_HasSelectedScriptSource)
			{
				stream << "- Selected script path: " << context.m_SelectedScriptPath << '\n';
				stream << "- Selected script source:\n```csharp\n";
				stream << context.m_SelectedScriptSource;
				if (!context.m_SelectedScriptSource.empty() && context.m_SelectedScriptSource.back() != '\n')
					stream << '\n';
				stream << "```\n";
			}
		}

		if (settings.m_SendConsoleContext && !context.m_RecentConsole.empty())
		{
			stream << "- Recent console:\n";
			for (const std::string& line : context.m_RecentConsole)
				stream << "  " << line << '\n';
		}

		stream << "\n" << ProviderUtils::BuildWhipScriptingGuide();
		stream << "\nAnswer as a concise game-engine assistant. When scene or code changes are needed, describe them as reviewable steps instead of pretending they are already applied.";
		return stream.str();
	}

	std::vector<ToolProposal> BuildLocalProposals(const ContextSnapshot& context, const std::string& prompt)
	{
		std::vector<ToolProposal> proposals;
		if (!context.m_HasScene)
			return proposals;

		const std::string lower = LowerCopy(prompt);
		const bool wantsCreate = ContainsAny(lower, { "create", "add entity", "new entity", "olustur", "ekle", "nesne", "obje" });
		if (wantsCreate)
		{
			ToolProposal proposal;
			proposal.m_Kind = ToolKind::CreateEntity;
			proposal.m_EntityName = PickEntityName(prompt);
			proposal.m_Title = "Create " + proposal.m_EntityName;
			proposal.m_Description = "Creates a new entity in the active edit scene and selects it.";
			if (ContainsAny(lower, { "camera", "kamera" }))
				proposal.m_Translation = { 0.0f, 0.0f, 8.0f };
			proposal.m_HasTransform = ContainsAny(lower, { "camera", "kamera", "at ", "position", "konum" });
			proposals.push_back(std::move(proposal));
		}

		if (ContainsAny(lower, { "sprite", "texture", "2d render", "render" }))
			PushAddComponentProposal(proposals, context, "Sprite Renderer");
		if (ContainsAny(lower, { "circle", "daire" }))
			PushAddComponentProposal(proposals, context, "Circle Renderer");
		if (ContainsAny(lower, { "text", "font", "yazi" }))
			PushAddComponentProposal(proposals, context, "Text Renderer");
		if (ContainsAny(lower, { "camera", "kamera" }))
			PushAddComponentProposal(proposals, context, "Camera");
		if (ContainsAny(lower, { "script", "cs", "mono" }))
			PushAddComponentProposal(proposals, context, "Script");
		if (ContainsAny(lower, { "animator", "animation", "animasyon" }))
			PushAddComponentProposal(proposals, context, "Animator");
		if (ContainsAny(lower, { "rigidbody", "physics", "fizik", "dynamic" }))
			PushAddComponentProposal(proposals, context, "Rigidbody2D");
		if (ContainsAny(lower, { "box collider", "boxcollider", "kutu collider" }))
			PushAddComponentProposal(proposals, context, "BoxCollider2D");
		if (ContainsAny(lower, { "circle collider", "circlecollider", "daire collider" }))
			PushAddComponentProposal(proposals, context, "CircleCollider2D");
		if (ContainsAny(lower, { "audio", "sound", "ses" }))
			PushAddComponentProposal(proposals, context, "Audio");

		return proposals;
	}

	std::vector<ToolProposal> ParseToolProposals(const ContextSnapshot& context, const std::string& responseText)
	{
		std::vector<ToolProposal> proposals;
		constexpr std::string_view fence = "```whip_script_edit";
		constexpr std::string_view beginMarker = "---BEGIN CONTENT---";
		constexpr std::string_view endMarker = "---END CONTENT---";

		size_t searchOffset = 0;
		while (searchOffset < responseText.size())
		{
			const size_t fenceStart = responseText.find(fence, searchOffset);
			if (fenceStart == std::string::npos)
				break;

			const size_t blockStart = responseText.find('\n', fenceStart + fence.size());
			if (blockStart == std::string::npos)
				break;

			const size_t fenceEnd = responseText.find("```", blockStart + 1);
			if (fenceEnd == std::string::npos)
				break;

			const std::string block = responseText.substr(blockStart + 1, fenceEnd - blockStart - 1);
			searchOffset = fenceEnd + 3;

			const size_t contentBegin = block.find(beginMarker);
			if (contentBegin == std::string::npos)
				continue;

			const size_t contentStart = contentBegin + beginMarker.size();
			const size_t contentEnd = block.find(endMarker, contentStart);
			if (contentEnd == std::string::npos)
				continue;

			const std::string header = block.substr(0, contentBegin);
			std::string scriptPath = context.m_SelectedScriptPath;
			std::string summary = "Assistant script edit";

			size_t lineStart = 0;
			while (lineStart < header.size())
			{
				size_t lineEnd = header.find('\n', lineStart);
				if (lineEnd == std::string::npos)
					lineEnd = header.size();

				const std::string line = TrimCopy(std::string_view(header).substr(lineStart, lineEnd - lineStart));
				if (StartsWithNoCase(line, "path:"))
					scriptPath = TrimCopy(std::string_view(line).substr(5));
				else if (StartsWithNoCase(line, "summary:"))
					summary = TrimCopy(std::string_view(line).substr(8));

				lineStart = lineEnd + 1;
			}

			std::string scriptContent = StripSingleEdgeNewline(block.substr(contentStart, contentEnd - contentStart));
			if (scriptPath.empty() || scriptContent.empty())
				continue;

			ToolProposal proposal;
			proposal.m_Kind = ToolKind::EditScript;
			proposal.m_Title = "Edit " + std::filesystem::path(scriptPath).filename().string();
			proposal.m_Description = summary.empty() ? "Replace selected script source with assistant generated code." : summary;
			proposal.m_TargetEntity = context.m_SelectedEntity;
			proposal.m_ScriptPath = std::move(scriptPath);
			proposal.m_ScriptContent = std::move(scriptContent);
			proposals.push_back(std::move(proposal));
		}

		return proposals;
	}

	std::string StripToolProposalBlocks(const std::string& responseText)
	{
		constexpr std::string_view fence = "```whip_script_edit";
		std::string result;
		size_t copyOffset = 0;

		while (copyOffset < responseText.size())
		{
			const size_t fenceStart = responseText.find(fence, copyOffset);
			if (fenceStart == std::string::npos)
			{
				result.append(responseText.substr(copyOffset));
				break;
			}

			result.append(responseText.substr(copyOffset, fenceStart - copyOffset));
			const size_t blockStart = responseText.find('\n', fenceStart + fence.size());
			if (blockStart == std::string::npos)
				break;

			const size_t fenceEnd = responseText.find("```", blockStart + 1);
			if (fenceEnd == std::string::npos)
				break;

			copyOffset = fenceEnd + 3;
		}

		return TrimCopy(result);
	}

	Response RequestResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt)
	{
		if (settings.m_Provider != ProviderKind::Offline)
		{
			if (AssistantProviderPtr provider = CreateAssistantProvider(settings.m_Provider))
				return provider->RequestResponse(settings, context, prompt);
		}

		Response response;
		response.m_Success = true;
		response.m_Text = "Offline provider created local proposals only.";
		response.m_Proposals = BuildLocalProposals(context, prompt);
		return response;
	}

	Response RequestOpenAIResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt)
	{
		OpenAIProvider provider;
		return provider.RequestResponse(settings, context, prompt);
	}

	Response RequestGeminiResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt)
	{
		GeminiProvider provider;
		return provider.RequestResponse(settings, context, prompt);
	}
}

_WHIP_END
