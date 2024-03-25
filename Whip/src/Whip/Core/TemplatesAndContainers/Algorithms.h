#pragma once

#include "Whip/Core/Core.h"
#include "Whip/Core/Log.h"
#include "Utility.h"
#include "Invoker.h"
#include "MathDef.h"

// todo whip::sort

_WHIP_START

template <class _Iter, class _Fn>
WHP_CONSTEXPR _Fn for_each(_Iter first, _Iter last, _Fn fun)
{
	verify_range(first, last);
	auto ufirst			= get_unwrapped(first);
	const auto ulast	= get_unwrapped(last);
	for (; ufirst != ulast; ++ufirst)
		fun(DREF(ufirst));
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
		swap(DREF(lufirst), DREF(rufirst));
}

template <class _Iter, class _Ty>
constexpr void fill(_Iter first, _Iter last, const _Ty& val)
{
	verify_range(first, last);
	auto ufirst			= get_unwrapped(first);
	const auto ulast	= get_unwrapped(last);

	if (detail_utility::is_all_bits_zero(val))
		detail_utility::fill_zero_memset(ufirst, static_cast<size_t>(ulast - ufirst));
	else
		for (; ufirst != ulast; ++ufirst)
			DREF(ufirst) = val;
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

template <class _Iter, class _Pr>
WHP_NODISCARD WHP_CONSTEXPR bool all_of(_Iter first, _Iter last, _Pr pred)
{
	verify_range(first, last);
	auto ufirst			= get_unwrapped(first);
	const auto ulast	= get_unwrapped(last);
	for (; ufirst != ulast; ++ufirst)
		if (!pred(DREF(ufirst)))
			return false;
	return true;
}

template <class _Iter, class _Pr>
WHP_NODISCARD WHP_CONSTEXPR bool any_of(_Iter first, _Iter last, _Pr pred)
{
	verify_range(first, last);
	auto ufirst			= get_unwrapped(first);
	const auto ulast	= get_unwrapped(last);
	for (; ufirst != ulast; ++ufirst)
		if (pred(DREF(ufirst)))
			return true;
	return false;
}

template <class _Iter, class _Pr>
WHP_NODISCARD WHP_CONSTEXPR bool none_of(_Iter first, _Iter last, _Pr pred)
{
	verify_range(first, last);
	auto ufirst			= get_unwrapped(first);
	const auto ulast	= get_unwrapped(last);
	for (; ufirst != ulast; ++ufirst)
		if (pred(DREF(ufirst)))
			return false;
	return true;
}

template <class _Iter, class _Pr>
WHP_NODISCARD WHP_CONSTEXPR bool all_of_2range(_Iter first1, _Iter first2, size_t n, _Pr pred)
{
	auto ufirst1		= get_unwrapped(first1);
	auto ufirst2		= get_unwrapped(first2);
	const auto ulast	= get_unwrapped(first1 + n);
	for (; ufirst1 != ulast; ++ufirst1, ++ufirst2)
		if (!pred(DREF(ufirst1), DREF(ufirst2)))
			return false;
	return true;
}

template <class _Iter, class _Pr>
WHP_NODISCARD WHP_CONSTEXPR bool any_of_2range(_Iter first1, _Iter first2, size_t n, _Pr pred)
{
	auto ufirst1		= get_unwrapped(first1);
	auto ufirst2		= get_unwrapped(first2);
	const auto ulast	= get_unwrapped(first1 + n);
	for (; ufirst1 != ulast; ++ufirst1, ++ufirst2)
		if (pred(DREF(ufirst1), DREF(ufirst2)))
			return true;
	return false;
}

template <class _Iter, class _Pr>
WHP_NODISCARD WHP_CONSTEXPR bool none_of_2range(_Iter first1, _Iter first2, size_t n, _Pr pred)
{
	auto ufirst1		= get_unwrapped(first1);
	auto ufirst2		= get_unwrapped(first2);
	const auto ulast	= get_unwrapped(first1 + n);
	for (; ufirst1 != ulast; ++ufirst1, ++ufirst2)
		if (pred(DREF(ufirst1), DREF(ufirst2)))
			return false;
	return true;
}


namespace detail_algorithms
{
	// random number generator from uniform random number generator
	template <class _Diff, class _Urng>
	class rng_from_urng
	{
	public:
		using _Ty0 = make_unsigned_t<_Diff>;
		using _Ty1 = detail_invoker::invoke_result_t<_Urng&>;

		using _Udiff = conditional_t<sizeof(_Ty1) < sizeof(_Ty0), _Ty0, _Ty1>;

		explicit rng_from_urng(_Urng& func) : m_ref(func), m_bits(CHAR_BIT * sizeof(_Udiff)), m_bit_mask(static_cast<_Udiff>(-1))
		{
			for (; static_cast<_Udiff>((_Urng::max)() - (_Urng::min)()) < m_bit_mask; m_bit_mask >>= 1)
				--m_bits;
		}

		_Diff operator()(_Diff idx)
		{
			for (;;)
			{
				_Udiff result = 0;
				_Udiff mask = 0;

				while (mask < static_cast<_Udiff>(idx - 1)) 
				{
					result <<= m_bits - 1;
					result <<= 1;
					result |= get_bits();
					mask <<= m_bits - 1;
					mask <<= 1;
					mask |= m_bit_mask;
				}
				if (result / idx < mask / idx || mask % idx == static_cast<_Udiff>(idx - 1))
					return static_cast<_Diff>(result % idx);
			}
		}

		_Udiff get_all_bits() 
		{
			_Udiff result = 0;

			for (size_t number = 0; number < CHAR_BIT * sizeof(_Udiff); number += m_bits) 
			{ 
				result <<= m_bits - 1;
				result <<= 1;
				result |= get_bits();
			}
			return result;
		}

		rng_from_urng(const rng_from_urng&) = delete;
		rng_from_urng& operator=(const rng_from_urng&) = delete;
	private:
		_Udiff get_bits()
		{
			for (;;)
			{
				const _Udiff val = static_cast<_Udiff>(m_ref() - (_Urng::min)());
				if (val <= m_bit_mask)
					return val;
			}
		}

		_Urng& m_ref;
		size_t m_bits;
		_Udiff m_bit_mask;
	};
} // detail_algorithms

template <class _Iter, class _Urng>
void shuffle(_Iter first, _Iter last, _Urng&& func)
{
	using _Urng0 = remove_reference_t<_Urng>;
	using iter_diff_t = typename _Iter::diff_type;
	detail_algorithms::rng_from_urng<iter_diff_t, _Urng0> rng_func(func);
	verify_range(first, last);
	auto ufirst = get_unwrapped(first);
	auto ulast = get_unwrapped(last);
	if (ufirst == ulast)
		return;
	auto utarget = ufirst;
	iter_diff_t target_idx = 1;
	for (; ++utarget != ulast; ++target_idx)
	{
		iter_diff_t offset = rng_func(static_cast<iter_diff_t>(target_idx + 1));
		WHP_CORE_ASSERT(0 <= offset && offset <= target_idx, "random value out of the range");
		if (offset != target_idx)
			_WHIP iter_swap(utarget, ufirst + offset);
	}
}

_WHIP_END