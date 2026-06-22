#include <Whip-Assistant/WhipAssistant.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>
#include <utility>

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

		std::string ExtractOpenAIText(const std::string& response)
		{
			std::string text;
			if (ExtractJsonStringAfterKey(response, "output_text", text))
				return text;
			if (ExtractJsonStringAfterKey(response, "text", text))
				return text;
			return {};
		}

		std::string ExtractGeminiText(const std::string& response)
		{
			std::string text;
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

		struct HttpResponse
		{
			bool m_Success = false;
			DWORD m_StatusCode = 0;
			std::string m_Body;
			std::string m_Error;
		};

		HttpResponse PostJson(std::wstring host, std::wstring path, std::wstring headers, const std::string& body)
		{
			HttpResponse result;
			WinHttpHandle session{ WinHttpOpen(L"Whip Assistant/0.2", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0) };
			if (!session)
			{
				result.m_Error = "Could not open WinHTTP session.";
				return result;
			}

			WinHttpHandle connection{ WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0) };
			if (!connection)
			{
				result.m_Error = "Could not connect to provider host.";
				return result;
			}

			WinHttpHandle request{ WinHttpOpenRequest(connection, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) };
			if (!request)
			{
				result.m_Error = "Could not create HTTP request.";
				return result;
			}

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
				result.m_Error = "Provider request failed before a response was received.";
				return result;
			}

			DWORD statusCode = 0;
			DWORD statusCodeSize = sizeof(statusCode);
			WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
			result.m_StatusCode = statusCode;

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
				result.m_Body += chunk;
			}

			result.m_Success = statusCode >= 200 && statusCode < 300;
			if (!result.m_Success)
			{
				result.m_Error = "Provider request returned HTTP " + std::to_string(statusCode) + ".";
				if (!result.m_Body.empty())
					result.m_Error += " " + result.m_Body.substr(0, 320);
			}
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
		switch (settings.m_Provider)
		{
		case ProviderKind::OpenAI: return !settings.m_OpenAIApiKey.empty();
		case ProviderKind::Gemini: return !settings.m_GeminiApiKey.empty();
		case ProviderKind::Offline:
		default: return false;
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

	Response RequestResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt)
	{
		switch (settings.m_Provider)
		{
		case ProviderKind::OpenAI: return RequestOpenAIResponse(settings, context, prompt);
		case ProviderKind::Gemini: return RequestGeminiResponse(settings, context, prompt);
		case ProviderKind::Offline:
		default:
		{
			Response response;
			response.m_Success = true;
			response.m_Text = "Offline provider created local proposals only.";
			response.m_Proposals = BuildLocalProposals(context, prompt);
			return response;
		}
		}
	}

	Response RequestOpenAIResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt)
	{
		Response result;
		if (!settings.m_Enabled)
		{
			result.m_Error = "Whip Assistant is disabled in settings.";
			return result;
		}
		if (settings.m_OpenAIApiKey.empty())
		{
			result.m_Error = "OpenAI API key is empty.";
			return result;
		}

		const std::string instructions =
			"You are Whip Assistant inside the Whip game engine editor. "
			"Help design, debug, and change games safely. Be concise. "
			"Do not claim file or scene changes were applied unless the editor explicitly applies a proposal.";

		const std::string input = BuildContextPrompt(context, settings) + "\n\nUser request:\n" + prompt;
		const std::string model = settings.m_OpenAIModel.empty() ? "gpt-5.5" : settings.m_OpenAIModel;
		const std::string body =
			"{\"model\":\"" + EscapeJson(model) +
			"\",\"instructions\":\"" + EscapeJson(instructions) +
			"\",\"input\":\"" + EscapeJson(input) +
			"\",\"store\":false}";

#ifdef _WIN32
		const std::wstring headers =
			L"Content-Type: application/json\r\nAuthorization: Bearer " + ToWide(settings.m_OpenAIApiKey) + L"\r\n";
		const HttpResponse http = PostJson(L"api.openai.com", L"/v1/responses", headers, body);
		if (!http.m_Success)
		{
			result.m_Error = http.m_Error;
			return result;
		}
		result.m_Text = ExtractOpenAIText(http.m_Body);
		if (result.m_Text.empty())
			result.m_Text = "OpenAI responded, but no text output was found in the response.";
		result.m_Success = true;
#else
		result.m_Error = "OpenAI responses are currently implemented for Windows editor builds.";
#endif
		if (result.m_Success)
			result.m_Proposals = BuildLocalProposals(context, prompt);
		return result;
	}

	Response RequestGeminiResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt)
	{
		Response result;
		if (!settings.m_Enabled)
		{
			result.m_Error = "Whip Assistant is disabled in settings.";
			return result;
		}
		if (settings.m_GeminiApiKey.empty())
		{
			result.m_Error = "Gemini API key is empty.";
			return result;
		}

		const std::string model = settings.m_GeminiModel.empty() ? "gemini-2.0-flash" : settings.m_GeminiModel;
		const std::string input =
			"You are Whip Assistant inside the Whip game engine editor. Be concise and practical. "
			"Scene edits must be described as reviewable steps.\n\n" +
			BuildContextPrompt(context, settings) + "\n\nUser request:\n" + prompt;
		const std::string body =
			"{\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"" + EscapeJson(input) +
			"\"}]}],\"generationConfig\":{\"temperature\":0.35}}";

#ifdef _WIN32
		std::string normalizedModel = model;
		if (!normalizedModel.starts_with("models/"))
			normalizedModel = "models/" + normalizedModel;
		std::wstring path = L"/v1beta/";
		path += ToWide(normalizedModel);
		path += L":generateContent";
		const std::wstring headers =
			L"Content-Type: application/json\r\nx-goog-api-key: " + ToWide(settings.m_GeminiApiKey) + L"\r\n";
		const HttpResponse http = PostJson(L"generativelanguage.googleapis.com", path, headers, body);
		if (!http.m_Success)
		{
			result.m_Error = http.m_Error;
			return result;
		}
		result.m_Text = ExtractGeminiText(http.m_Body);
		if (result.m_Text.empty())
			result.m_Text = "Gemini responded, but no text output was found in the response.";
		result.m_Success = true;
#else
		result.m_Error = "Gemini responses are currently implemented for Windows editor builds.";
#endif
		if (result.m_Success)
			result.m_Proposals = BuildLocalProposals(context, prompt);
		return result;
	}
}

_WHIP_END
