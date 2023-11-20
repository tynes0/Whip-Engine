#pragma once

#include "Whip/Core/Core.h"
#include "Whip/Core/TypeTraits.h"

_WHIP_START

#ifdef _WIN64
typedef long long wptrdiff_t;
#else
typedef int wptrdiff_t;
#endif

template <class _Ty, class _Ptr = _Ty*, class _Ref = _Ty&, class _SizeT = size_t, class _Diff = wptrdiff_t>
struct iterator_base
{
	using value_type	= _Ty;
	using pointer		= _Ptr;
	using reference		= _Ref;
	using size_type		= _SizeT;
	using diff_type		= _Diff;
};

template <class _WhipIter, class _Ty>
inline constexpr bool is_const_whip_iterator_v = is_base_of_v<iterator_base<const _Ty>, _WhipIter>;

template <class _WhipIter, class _Ty>
inline constexpr bool is_non_const_whip_iterator_v = is_base_of_v<iterator_base<remove_const_t<_Ty>>, _WhipIter>;

template <class _WhipIter, class _Ty>
inline constexpr bool is_whip_iterator_v = is_const_whip_iterator_v<_WhipIter, _Ty> || is_non_const_whip_iterator_v<_WhipIter, _Ty>;



_WHIP_END