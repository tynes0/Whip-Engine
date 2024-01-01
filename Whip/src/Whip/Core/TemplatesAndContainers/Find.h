#pragma once

#include "Whip/Core/Core.h"
#include "Whip/Core/TemplatesAndContainers/Iterator.h"
#include "Whip/Core/TemplatesAndContainers/TypeTraits.h"
#include "Whip/Core/TemplatesAndContainers/ExternalOperatorWrapper.h"
#include "Whip/Core/TemplatesAndContainers/Array.h"


_WHIP_START

template <class _Iter, class _Ty, enable_if_t<is_whip_iterator_v<_Iter>, int> = 0>
WHP_NODISCARD _Iter find(const _Iter& first, const _Iter& last, const _Ty& value)
{
	_Iter it = first;
	for (; it != last && (*it) != value; ++it);
	if (*it != value)
		return last;
	return it;
}

_WHIP_END