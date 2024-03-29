#pragma once
#ifndef _whip_is_in_defined_

#include "Whip/Core/Core.h"
#include "ExternalOperatorWrapper.h"


#include <string>

#pragma warning(push)
#pragma warning(disable : _WHP_DISABLED_WARNINGS)

_WHIP_START

static bool in_impl(char ch, const std::string& str)
{
	if (str.find(ch) != std::string::npos)
		return true;
	return false;
}

static WHP_INLINE constexpr auto in_op = make_external_operator(in_impl);

#ifndef is_in
#define is_in <_WHIP in_op>
#define _whip_is_in_defined_ 1
#endif // !in

_WHIP_END

#pragma warning(pop)
#endif // !_whip_is_in_defined_