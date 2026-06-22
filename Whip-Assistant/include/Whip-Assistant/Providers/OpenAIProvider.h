#pragma once

#include <Whip-Assistant/Providers/IAssistantProvider.h>

_WHIP_START

namespace Assistant
{
	class OpenAIProvider final : public IAssistantProvider
	{
	public:
		ProviderKind GetKind() const override { return ProviderKind::OpenAI; }
		bool HasCredentials(const Settings& settings) const override;
		Response RequestResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt) override;
	};
}

_WHIP_END
