#include <Whip-Assistant/Providers/IAssistantProvider.h>

#include <Whip-Assistant/Providers/GeminiProvider.h>
#include <Whip-Assistant/Providers/OpenAIProvider.h>

_WHIP_START

namespace Assistant
{
	AssistantProviderPtr CreateAssistantProvider(ProviderKind provider)
	{
		switch (provider)
		{
		case ProviderKind::OpenAI:
			return std::make_unique<OpenAIProvider>();
		case ProviderKind::Gemini:
			return std::make_unique<GeminiProvider>();
		case ProviderKind::Offline:
		default:
			return {};
		}
	}
}

_WHIP_END
