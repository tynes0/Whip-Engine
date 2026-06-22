#pragma once

#include <Whip-Assistant/WhipAssistant.h>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

_WHIP_START

namespace Assistant::ProviderUtils
{
	struct HttpResponse
	{
		bool m_Success = false;
		uint32_t m_StatusCode = 0;
		std::string m_Body;
		std::string m_Error;
	};

	std::string LowerCopy(std::string value);
	bool Contains(std::string_view haystack, std::string_view needle);
	bool ContainsAny(std::string_view haystack, std::initializer_list<std::string_view> needles);
	std::string PickEntityName(const std::string& prompt);
	std::string GetLocalEditorTime();
	std::string BuildWhipScriptingGuide();
	std::string BuildSystemInstructions();
	std::string EscapeJson(std::string_view value);
	std::wstring ToWide(std::string_view value);
	std::string ExtractOpenAIText(const std::string& response);
	std::string ExtractGeminiText(const std::string& response);
	HttpResponse PostJson(std::wstring host, std::wstring path, std::wstring headers, const std::string& body);
}

_WHIP_END
