#include <Whip-Assistant/Providers/GeminiProvider.h>

#include "AssistantProviderUtils.h"

#include <exception>
#include <sstream>
#include <variant>

#if WHP_ENABLE_GEMINI_CPP
#include <gemini/client.h>
#include <gemini/request_builder.h>
#endif

_WHIP_START

namespace Assistant
{
	namespace
	{
#if WHP_ENABLE_GEMINI_CPP
		std::string BuildGeminiGroundingSummary(const GeminiCPP::GenerationResult& generation)
		{
			if (!generation.groundingMetadata.has_value())
				return {};

			std::ostringstream stream;
			size_t sourceCount = 0;
			for (const GeminiCPP::GroundingChunk& chunk : generation.groundingMetadata->groundingChunks)
			{
				const auto* web = std::get_if<GeminiCPP::Web>(&chunk.chunk);
				if (!web || web->uri.empty())
					continue;

				if (sourceCount == 0)
					stream << "\n\nSources:";

				stream << "\n- ";
				if (!web->title.empty())
					stream << web->title << ": ";
				stream << web->uri;

				if (++sourceCount >= 5)
					break;
			}

			return stream.str();
		}
#endif
	}

	bool GeminiProvider::HasCredentials(const Settings& settings) const
	{
		return !settings.m_GeminiApiKey.empty();
	}

	Response GeminiProvider::RequestResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt)
	{
		Response result;
		if (!settings.m_Enabled)
		{
			result.m_Error = "Whip Assistant is disabled in settings.";
			return result;
		}
		if (!HasCredentials(settings))
		{
			result.m_Error = "Gemini API key is empty.";
			return result;
		}

		const std::string model = settings.m_GeminiModel.empty() ? "gemini-2.0-flash" : settings.m_GeminiModel;
		const std::string input = BuildContextPrompt(context, settings) + "\n\nUser request:\n" + prompt;

#if WHP_ENABLE_GEMINI_CPP
		try
		{
			GeminiCPP::Client client(settings.m_GeminiApiKey);
			GeminiCPP::RequestBuilder request = client.request();
			request
				.model(model)
				.systemInstruction(ProviderUtils::BuildSystemInstructions())
				.text(input)
				.temperature(0.25f);

			if (settings.m_GeminiUseGoogleSearch)
				request.googleSearch();

			const GeminiCPP::GenerationResult generation = request.generate();
			if (!generation.success)
			{
				result.m_Error = generation.errorMessage.empty() ? "Gemini SDK request failed." : generation.errorMessage;
				return result;
			}

			result.m_Text = generation.text();
			if (result.m_Text.empty())
				result.m_Text = "Gemini responded, but no text output was found in the SDK response.";

			const std::string groundingSummary = BuildGeminiGroundingSummary(generation);
			if (!groundingSummary.empty())
				result.m_Text += groundingSummary;

			result.m_Success = true;
		}
		catch (const std::exception& exception)
		{
			result.m_Error = std::string("Gemini SDK request failed: ") + exception.what();
			return result;
		}
#else
		const std::string body =
			"{\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"" + ProviderUtils::EscapeJson(ProviderUtils::BuildSystemInstructions() + "\n\n" + input) +
			"\"}]}],\"generationConfig\":{\"temperature\":0.25}}";
#ifdef _WIN32
		std::string normalizedModel = model;
		if (!normalizedModel.starts_with("models/"))
			normalizedModel = "models/" + normalizedModel;
		std::wstring path = L"/v1beta/";
		path += ProviderUtils::ToWide(normalizedModel);
		path += L":generateContent";
		const std::wstring headers =
			L"Content-Type: application/json\r\nx-goog-api-key: " + ProviderUtils::ToWide(settings.m_GeminiApiKey) + L"\r\n";
		const ProviderUtils::HttpResponse http = ProviderUtils::PostJson(L"generativelanguage.googleapis.com", path, headers, body);
		if (!http.m_Success)
		{
			result.m_Error = http.m_Error;
			return result;
		}
		result.m_Text = ProviderUtils::ExtractGeminiText(http.m_Body);
		if (result.m_Text.empty())
			result.m_Text = "Gemini responded, but no text output was found in the response.";
		result.m_Success = true;
#else
		result.m_Error = "Gemini responses are currently implemented for Windows editor builds.";
#endif
#endif

		if (result.m_Success)
		{
			result.m_Proposals = ParseToolProposals(context, result.m_Text);
			if (!result.m_Proposals.empty())
			{
				result.m_Text = StripToolProposalBlocks(result.m_Text);
				if (result.m_Text.empty())
					result.m_Text = "I prepared reviewable editor proposal(s).";
			}
		}
		return result;
	}
}

_WHIP_END
