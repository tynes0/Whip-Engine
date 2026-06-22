#pragma once

#include <Whip-Assistant/WhipAssistant.h>

#include <memory>
#include <string>

_WHIP_START

namespace Assistant
{
	class IAssistantProvider
	{
	public:
		virtual ~IAssistantProvider() = default;

		virtual ProviderKind GetKind() const = 0;
		virtual bool HasCredentials(const Settings& settings) const = 0;
		virtual Response RequestResponse(const Settings& settings, const ContextSnapshot& context, const std::string& prompt) = 0;
	};

	using AssistantProviderPtr = std::unique_ptr<IAssistantProvider>;

	AssistantProviderPtr CreateAssistantProvider(ProviderKind provider);
}

_WHIP_END
