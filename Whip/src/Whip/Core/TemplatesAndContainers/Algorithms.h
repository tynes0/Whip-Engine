#pragma once

#include "Whip/Core/Core.h"
#include "Utility.h"

_WHIP_START

template <class _Iter, class _Fn>
WHP_CONSTEXPR _Fn for_each(_Iter first, _Iter last, _Fn fun)
{
	verify_range(first, last);
	auto ufirst			= get_unwrapped(first);
	const auto ulast	= get_unwrapped(last);
	for (; ufirst != ulast; ++ufirst)
		fun(*ufirst);
	return fun;
}

_WHIP_END