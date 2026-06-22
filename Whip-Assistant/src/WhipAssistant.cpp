#include <Whip-Assistant/WhipAssistant.h>

#include <Whip-Assistant/Providers/GeminiProvider.h>
#include <Whip-Assistant/Providers/IAssistantProvider.h>
#include <Whip-Assistant/Providers/OpenAIProvider.h>

#include "Providers/AssistantProviderUtils.h"

#include <algorithm>
#include <cctype>
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
		switch (kind)
		{
		case ToolKind::CreateEntity: return "Create Entity";
		case ToolKind::AddComponent: return "Add Component";
		case ToolKind::SetTransform: return "Set Transform";
		case ToolKind::None:
		default: return "None";
		}
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
