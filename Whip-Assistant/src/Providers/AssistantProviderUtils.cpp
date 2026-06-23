#include "AssistantProviderUtils.h"

#include <Whip-Assistant/EngineKnowledgeManifest.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winhttp.h>
#endif

_WHIP_START

namespace Assistant::ProviderUtils
{
	namespace
	{
#ifdef _WIN32
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
#endif
	}

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

	std::string GetLocalEditorTime()
	{
		const auto now = std::chrono::system_clock::now();
		const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
		std::tm localTime{};
#ifdef _WIN32
		localtime_s(&localTime, &nowTime);
#else
		localtime_r(&nowTime, &localTime);
#endif

		std::ostringstream stream;
		stream << std::put_time(&localTime, "%Y-%m-%d %A %H:%M:%S");
		return stream.str();
	}

	std::string BuildWhipScriptingGuide()
	{
		return BuildEngineKnowledgePrompt();
	}

	std::string BuildSystemInstructions()
	{
		return
			"You are Whip Assistant inside the Whip game engine editor. "
			"Be concise, practical, and action-oriented. Use available tools when current real-world data is needed. "
			"Scene edits must be described as reviewable steps unless the editor explicitly applies a proposal. "
			"Never invent engine APIs. Prefer exact Whip API names from the guide below.\n\n" +
			BuildWhipScriptingGuide();
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

	std::wstring ToWide(std::string_view value)
	{
		if (value.empty())
			return {};

#ifdef _WIN32
		const int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
		if (required <= 0)
			return {};

		std::wstring result(static_cast<size_t>(required), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required);
		return result;
#else
		return std::wstring(value.begin(), value.end());
#endif
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

	HttpResponse PostJson(std::wstring host, std::wstring path, std::wstring headers, const std::string& body)
	{
		HttpResponse result;
#ifdef _WIN32
		WinHttpHandle session{ WinHttpOpen(L"Whip Assistant/0.3", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0) };
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
#else
		(void)host;
		(void)path;
		(void)headers;
		(void)body;
		result.m_Error = "Provider HTTP requests are currently implemented for Windows editor builds.";
#endif
		return result;
	}
}

_WHIP_END
