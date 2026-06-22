#include <WhipPch.h>
#include <Whip-Editor/Assistant/WhipAssistant.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winhttp.h>
#endif

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

		std::string EscapeJson(std::string_view value)
		{
			std::string result;
			result.reserve(value.size() + 16);
			for (char character : value)
			{
				switch (character)
				{
				case '\\': result += "\\\\"; break;
				case '"': result += "\\\""; break;
				case '\b': result += "\\b"; break;
				case '\f': result += "\\f"; break;
				case '\n': result += "\\n"; break;
				case '\r': result += "\\r"; break;
				case '\t': result += "\\t"; break;
				default:
					if (static_cast<unsigned char>(character) < 0x20)
						result += ' ';
					else
						result += character;
					break;
				}
			}
			return result;
		}

		bool ReadJsonString(const std::string& text, size_t quotePosition, std::string& value)
		{
			if (quotePosition == std::string::npos || quotePosition >= text.size() || text[quotePosition] != '"')
				return false;

			value.clear();
			for (size_t i = quotePosition + 1; i < text.size(); ++i)
			{
				const char character = text[i];
				if (character == '"')
					return true;
				if (character != '\\')
				{
					value += character;
					continue;
				}

				if (++i >= text.size())
					return false;

				switch (text[i])
				{
				case '"': value += '"'; break;
				case '\\': value += '\\'; break;
				case '/': value += '/'; break;
				case 'b': value += '\b'; break;
				case 'f': value += '\f'; break;
				case 'n': value += '\n'; break;
				case 'r': value += '\r'; break;
				case 't': value += '\t'; break;
				case 'u':
					value += '?';
					i += std::min<size_t>(4, text.size() - i - 1);
					break;
				default:
					value += text[i];
					break;
				}
			}
			return false;
		}

		bool ExtractJsonStringAfterKey(const std::string& response, std::string_view key, std::string& value)
		{
			const std::string quotedKey = "\"" + std::string(key) + "\"";
			size_t keyPos = response.find(quotedKey);
			while (keyPos != std::string::npos)
			{
				const size_t colon = response.find(':', keyPos + quotedKey.size());
				if (colon == std::string::npos)
					return false;

				size_t quote = response.find('"', colon + 1);
				if (quote != std::string::npos && ReadJsonString(response, quote, value))
					return true;

				keyPos = response.find(quotedKey, keyPos + quotedKey.size());
			}

			return false;
		}

		std::string ExtractResponseText(const std::string& response)
		{
			std::string text;
			if (ExtractJsonStringAfterKey(response, "output_text", text))
				return text;
			if (ExtractJsonStringAfterKey(response, "text", text))
				return text;
			return {};
		}

#ifdef _WIN32
		std::wstring ToWide(std::string_view value)
		{
			if (value.empty())
				return {};

			const int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
			if (required <= 0)
				return {};

			std::wstring result(static_cast<size_t>(required), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required);
			return result;
		}

		struct WinHttpHandle
		{
			HINTERNET m_Handle = nullptr;
			~WinHttpHandle()
			{
				if (m_Handle)
					WinHttpCloseHandle(m_Handle);
			}
			operator HINTERNET() const { return m_Handle; }
		};

		Response SendResponsesRequest(const Settings& settings, const std::string& body)
		{
			Response result;
			WinHttpHandle session{ WinHttpOpen(L"Whip Assistant/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0) };
			if (!session)
			{
				result.m_Error = "Could not open WinHTTP session.";
				return result;
			}

			WinHttpHandle connection{ WinHttpConnect(session, L"api.openai.com", INTERNET_DEFAULT_HTTPS_PORT, 0) };
			if (!connection)
			{
				result.m_Error = "Could not connect to api.openai.com.";
				return result;
			}

			WinHttpHandle request{ WinHttpOpenRequest(connection, L"POST", L"/v1/responses", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) };
			if (!request)
			{
				result.m_Error = "Could not create HTTP request.";
				return result;
			}

			const std::wstring headers =
				L"Content-Type: application/json\r\nAuthorization: Bearer " + ToWide(settings.m_ApiKey) + L"\r\n";

			const BOOL sent = WinHttpSendRequest(
				request,
				headers.c_str(),
				static_cast<DWORD>(headers.size()),
				const_cast<char*>(body.data()),
				static_cast<DWORD>(body.size()),
				static_cast<DWORD>(body.size()),
				0);

			if (!sent || !WinHttpReceiveResponse(request, nullptr))
			{
				result.m_Error = "OpenAI request failed before a response was received.";
				return result;
			}

			DWORD statusCode = 0;
			DWORD statusCodeSize = sizeof(statusCode);
			WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

			std::string responseText;
			for (;;)
			{
				DWORD available = 0;
				if (!WinHttpQueryDataAvailable(request, &available) || available == 0)
					break;

				std::string chunk;
				chunk.resize(available);
				DWORD read = 0;
				if (!WinHttpReadData(request, chunk.data(), available, &read))
					break;
				chunk.resize(read);
				responseText += chunk;
			}

			if (statusCode < 200 || statusCode >= 300)
			{
				result.m_Error = "OpenAI request returned HTTP " + std::to_string(statusCode) + ".";
				if (!responseText.empty())
					result.m_Error += " " + responseText.substr(0, 320);
				return result;
			}

			result.m_Text = ExtractResponseText(responseText);
			if (result.m_Text.empty())
				result.m_Text = "OpenAI responded, but no text output was found in the response.";
			result.m_Success = true;
			return result;
		}
#endif
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

	std::string BuildContextPrompt(const ContextSnapshot& context, const Settings& settings)
	{
		std::ostringstream stream;
		stream << "Whip Editor context:\n";
		stream << "- Project: " << (context.m_HasProject ? context.m_ProjectName : "none") << '\n';
		stream << "- Scene: " << (context.m_HasScene ? context.m_ScenePath : "none") << '\n';

		if (settings.m_SendSceneContext && context.m_HasSelection)
		{
			stream << "- Selected entity: " << context.m_SelectedEntityName << " (" << context.m_SelectedEntity << ")\n";
			stream << "- Components:";
			for (const std::string& component : context.m_SelectedComponents)
				stream << ' ' << component;
			stream << '\n';
		}

		if (settings.m_SendConsoleContext && !context.m_RecentConsole.empty())
		{
			stream << "- Recent console:\n";
			for (const std::string& line : context.m_RecentConsole)
				stream << "  " << line << '\n';
		}

		stream << "\nAnswer as a concise game-engine assistant. When scene changes are needed, describe them as reviewable steps instead of pretending they are already applied.";
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

	Response RequestOpenAIResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt)
	{
		Response result;
		if (!settings.m_Enabled)
		{
			result.m_Error = "Whip Assistant is disabled in settings.";
			return result;
		}
		if (!settings.m_UseOnlineResponses)
		{
			result.m_Error = "Online responses are disabled in settings.";
			return result;
		}
		if (settings.m_ApiKey.empty())
		{
			result.m_Error = "OpenAI API key is empty.";
			return result;
		}

		const std::string instructions =
			"You are Whip Assistant inside the Whip game engine editor. "
			"Help design, debug, and change games safely. Be concise. "
			"Do not claim file or scene changes were applied unless the editor explicitly applies a proposal.";

		const std::string input = BuildContextPrompt(context, settings) + "\n\nUser request:\n" + prompt;
		const std::string model = settings.m_Model.empty() ? "gpt-5.5" : settings.m_Model;
		const std::string body =
			"{\"model\":\"" + EscapeJson(model) +
			"\",\"instructions\":\"" + EscapeJson(instructions) +
			"\",\"input\":\"" + EscapeJson(input) +
			"\",\"store\":false}";

#ifdef _WIN32
		result = SendResponsesRequest(settings, body);
#else
		result.m_Error = "Online responses are currently implemented for Windows editor builds.";
#endif
		if (result.m_Success)
			result.m_Proposals = BuildLocalProposals(context, prompt);
		return result;
	}
}

_WHIP_END
