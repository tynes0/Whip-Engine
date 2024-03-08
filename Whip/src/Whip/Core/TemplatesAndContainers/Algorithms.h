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

_WHIP_END