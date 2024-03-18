#pragma once

#include "Whip/Core/Core.h"
#include "Utility.h"
#include "MathDef.h"

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

template <class _Iter>
constexpr void swap_in_range(_Iter left, _Iter right, size_t size)
{
	verify_range(left, left + size);
	verify_range(right, right + size);
	auto lufirst		= get_unwrapped(left);
	const auto lulast	= get_unwrapped(left + size);
	auto rufirst		= get_unwrapped(right);
	const auto rulast	= get_unwrapped(right + size);
	for (; lufirst != lulast && rufirst != rulast; ++lufirst, ++rufirst)
		swap(*lufirst, *rufirst);
}

template <class _Iter, class _Ty>
constexpr void fill(_Iter first, _Iter last, const _Ty& val)
{
	verify_range(first, last);
	auto ufirst			= get_unwrapped(first);
	const auto ulast	= get_unwrapped(last);

	if (is_all_bits_zero(val)) 
		fill_zero_memset(ufirst, static_cast<size_t>(ulast - ufirst));
	else
		for (; ufirst != ulast; ++ufirst)
			*ufirst = val;
}

template <class _Iter1, class _Iter2, class _Pr>
WHP_NODISCARD WHP_CONSTEXPR _Iter1 find_first_of(_Iter1 first1, const _Iter1 last1, const _Iter2 first2, const _Iter2 last2, _Pr pred) noexcept
{
	verify_range(first1, last1);
	verify_range(first2, last2);
	auto ufirst1		= get_unwrapped(first1);
	const auto ulast1	= get_unwrapped(last1);
	const auto ufirst2	= get_unwrapped(first2);
	const auto ulast2	= get_unwrapped(last2);
	for (; ufirst1 != ulast1; ++ufirst1)
	{
		for (auto umid2 = ufirst2; umid2 != ulast2; ++umid2)
		{
			if (pred(*ufirst1, *umid2))
			{
				reset_wrapped(first1, ufirst1);
				return first1;
			}
		}
	}
	reset_wrapped(first1, ufirst1);
	return first1;
}

template <class _Iter1, class _Iter2>
WHP_NODISCARD WHP_CONSTEXPR _Iter1 find_first_of(const _Iter1 first1, const _Iter1 last1, const _Iter2 first2, const _Iter2 last2)
{
	return _WHIP find_first_of(first1, last1, first2, last2, equal_to<>{});
}


_WHIP_END