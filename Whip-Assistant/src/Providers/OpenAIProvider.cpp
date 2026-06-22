#include <Whip-Assistant/Providers/OpenAIProvider.h>

#include "AssistantProviderUtils.h"

_WHIP_START

namespace Assistant
{
	bool OpenAIProvider::HasCredentials(const Settings& settings) const
	{
		return !settings.m_OpenAIApiKey.empty();
	}

	Response OpenAIProvider::RequestResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt)
	{
		Response result;
		if (!settings.m_Enabled)
		{
			result.m_Error = "Whip Assistant is disabled in settings.";
			return result;
		}
		if (!HasCredentials(settings))
		{
			result.m_Error = "OpenAI API key is empty.";
			return result;
		}

		const std::string input = BuildContextPrompt(context, settings) + "\n\nUser request:\n" + prompt;
		const std::string model = settings.m_OpenAIModel.empty() ? "gpt-5.5" : settings.m_OpenAIModel;
		const std::string body =
			"{\"model\":\"" + ProviderUtils::EscapeJson(model) +
			"\",\"instructions\":\"" + ProviderUtils::EscapeJson(ProviderUtils::BuildSystemInstructions()) +
			"\",\"input\":\"" + ProviderUtils::EscapeJson(input) +
			"\",\"store\":false}";

#ifdef _WIN32
		const std::wstring headers =
			L"Content-Type: application/json\r\nAuthorization: Bearer " + ProviderUtils::ToWide(settings.m_OpenAIApiKey) + L"\r\n";
		const ProviderUtils::HttpResponse http = ProviderUtils::PostJson(L"api.openai.com", L"/v1/responses", headers, body);
		if (!http.m_Success)
		{
			result.m_Error = http.m_Error;
			return result;
		}

		result.m_Text = ProviderUtils::ExtractOpenAIText(http.m_Body);
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
}

_WHIP_END
