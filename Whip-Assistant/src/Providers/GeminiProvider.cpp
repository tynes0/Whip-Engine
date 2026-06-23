#include <Whip-Assistant/Providers/GeminiProvider.h>

#include "AssistantProviderUtils.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <sstream>
#include <variant>
#include <vector>

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

		bool WantsVisualAssetReasoning(const std::string& prompt)
		{
			const std::string lower = ProviderUtils::LowerCopy(prompt);
			return ProviderUtils::ContainsAny(lower, {
				"level", "sahne", "tasarla", "dizayn", "sprite", "spritesheet", "sprite sheet",
				"atlas", "tileset", "tile", "texture", "harita", "platform", "zemin"
			});
		}

		std::vector<const ContextSnapshot::AssetSummary*> CollectTextureImageAttachments(const ContextSnapshot& context, const std::string& prompt)
		{
			std::vector<const ContextSnapshot::AssetSummary*> attachments;
			if (!WantsVisualAssetReasoning(prompt))
				return attachments;

			const std::string lowerPrompt = ProviderUtils::LowerCopy(prompt);
			struct Candidate
			{
				const ContextSnapshot::AssetSummary* m_Asset = nullptr;
				int m_Score = 0;
			};

			std::vector<Candidate> candidates;
			const bool promptMentionsTileset = ProviderUtils::ContainsAny(lowerPrompt, { "tileset", "tilesets", "tile set", "tile" });
			const bool promptMentionsTexturesFolder = ProviderUtils::ContainsAny(lowerPrompt, { "texture", "textures", "dokular", "atlas", "sprite" });
			const bool promptMentionsLevelDesign = ProviderUtils::ContainsAny(lowerPrompt, { "level", "seviye", "sahne", "harita", "platform", "tasarla", "dizayn" });
			for (const ContextSnapshot::AssetSummary& asset : context.m_ProjectAssets)
			{
				if (asset.m_Type != AssetType::Texture2D || asset.m_AbsolutePath.empty())
					continue;
				std::error_code error;
				if (!std::filesystem::exists(asset.m_AbsolutePath, error) || error)
					continue;

				int score = asset.m_SpriteCount > 0 ? 30 : 5;
				const std::string path = ProviderUtils::LowerCopy(asset.m_Path);
				const std::string name = ProviderUtils::LowerCopy(asset.m_Name);
				if (!name.empty() && ProviderUtils::Contains(lowerPrompt, name))
					score += 80;
				if (!path.empty() && ProviderUtils::Contains(lowerPrompt, path))
					score += 70;
				if (ProviderUtils::ContainsAny(name, { "sprite", "atlas", "tileset", "tile", "sheet" }))
					score += 35;
				if (ProviderUtils::ContainsAny(path, { "sprite", "atlas", "tileset", "tile", "sheet" }))
					score += 20;
				if (promptMentionsTileset && ProviderUtils::ContainsAny(path + " " + name, { "tileset", "tilesets", "tile", "tiles" }))
					score += 70;
				if (promptMentionsTexturesFolder && ProviderUtils::ContainsAny(path, { "texture", "textures" }))
					score += 20;
				if (promptMentionsLevelDesign && asset.m_SpriteCount > 1)
					score += 25;

				candidates.push_back({ &asset, score });
			}

			std::ranges::sort(candidates, [](const Candidate& left, const Candidate& right)
			{
				if (left.m_Score != right.m_Score)
					return left.m_Score > right.m_Score;
				return left.m_Asset && right.m_Asset && left.m_Asset->m_Path < right.m_Asset->m_Path;
			});

			constexpr size_t MaxAttachments = 6;
			for (const Candidate& candidate : candidates)
			{
				if (!candidate.m_Asset || attachments.size() >= MaxAttachments)
					break;
				attachments.push_back(candidate.m_Asset);
			}
			return attachments;
		}
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
		const std::vector<const ContextSnapshot::AssetSummary*> imageAttachments = settings.m_SendAssetImages ? CollectTextureImageAttachments(context, prompt) : std::vector<const ContextSnapshot::AssetSummary*>{};
		const float temperature = WantsVisualAssetReasoning(prompt) ? 0.45f : 0.25f;
		std::string input = BuildContextPrompt(context, settings) + "\n\nUser request:\n" + prompt;
		if (!imageAttachments.empty())
		{
			input += "\n\nAttached texture atlas images:";
			for (size_t i = 0; i < imageAttachments.size(); ++i)
			{
				const ContextSnapshot::AssetSummary& asset = *imageAttachments[i];
				input += "\n- Image " + std::to_string(i + 1) + ": handle " + std::to_string(asset.m_Handle) + ", path " + asset.m_Path + ", spriteCount " + std::to_string(asset.m_SpriteCount);
			}
			input += "\nUse these images to infer sprite roles visually, but emit placements using the listed assetHandle and sprite indices/names.";
		}

#if WHP_ENABLE_GEMINI_CPP
		try
		{
			GeminiCPP::Client client(settings.m_GeminiApiKey);
			GeminiCPP::RequestBuilder request = client.request();
			request
				.model(model)
				.systemInstruction(ProviderUtils::BuildSystemInstructions())
				.text(input)
				.temperature(temperature);

			for (const ContextSnapshot::AssetSummary* asset : imageAttachments)
				request.image(asset->m_AbsolutePath);

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
			"\"}]}],\"generationConfig\":{\"temperature\":" + std::to_string(temperature) + "}}";
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
