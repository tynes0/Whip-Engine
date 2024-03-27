#pragma once

#include "TypeTraits.h"
#include "Utility.h"
#include "Tag.h"

// TODO: improve

#pragma warning(push)
#pragma warning(disable : 5053)

_WHIP_START

#define WHP_PAIR_ENABLE_IDX(I) whip::enable_if_t<I == 0 || I == 1, int> = 0
#define WHP_TRIO_ENABLE_IDX(I) whip::enable_if_t<I == 0 || I == 1 || I == 2, int> = 0

template <class _Ty1, class _Ty2 = _Ty1>
struct pair
{
	using first_type = _Ty1;
	using second_type = _Ty2;
	first_type first;
	second_type second;

	constexpr static bool nothrow_swappable = (is_nothrow_swappable_v<_Ty1> && is_nothrow_swappable_v<_Ty2>);

	template <class _Uty1 = _Ty1, class _Uty2 = _Ty2>
	constexpr explicit(!conjunction_v<std::_Is_implicitly_default_constructible<_Uty1>, std::_Is_implicitly_default_constructible<_Uty2>>)
		pair() noexcept : first(), second() {}

	template <class _Uty1 = _Ty1, class _Uty2 = _Ty2>
	constexpr explicit(!conjunction_v<is_convertible<const _Uty1&, _Uty1>, is_convertible<const _Uty2&, _Uty2>>)
		pair(const _Ty1& first_in, const _Ty2& second_in) noexcept : first(first_in), second(second_in) {}

	template <class _Other1, class _Other2>
	constexpr explicit(!conjunction_v<is_convertible<const _Other1&, _Ty1>, is_convertible<const _Other2&, _Ty2>>)
		pair(const pair<_Other1, _Other2>& other) noexcept : first(other.first), second(other.second) {}

	pair(const pair&) = default;
	pair(pair&&) = default;

	pair& operator=(const volatile pair&) = delete;

	constexpr pair& operator=(const pair& _Right)
	{
		first = _Right.first;
		second = _Right.second;
		return *this;
	}

	constexpr bool operator==(const pair& _Right) const 
	{
		return first == _Right.first && second == _Right.second;
	}

	constexpr bool operator!=(const pair& _Right) const 
	{
		return !(*this == _Right);
	}

#if _HAS_CXX20

	template <size_t I, WHP_PAIR_ENABLE_IDX(I)>
	constexpr decltype(auto) operator[](tag<I>) &
	{
		if constexpr (I == 0)
			return first;
		else // I == 1
			return second;
	}

	template <size_t I, WHP_PAIR_ENABLE_IDX(I)>
	constexpr decltype(auto) operator[](tag<I>) const&
	{
		if constexpr (I == 0)
			return first;
		else // I == 1
			return second;
	}

	template <size_t I, WHP_PAIR_ENABLE_IDX(I)>
	constexpr decltype(auto) operator[](tag<I>) &&
	{
		if constexpr (I == 0)
			return static_cast<pair<_Ty1, _Ty2>&&>(*this).first;
		else // I == 1
			return static_cast<pair<_Ty1, _Ty2>&&>(*this).second;
	}

#endif // _HAS_CXX20

	constexpr void swap(pair<_Ty1, _Ty2>& _Other)
	{
		if (this != addressof(_Other))
		{
			swap_adl(first, _Other.first);
			swap_adl(second, _Other.second);
		}
	}
};

struct zero_then_variadic_args_t
{
	explicit zero_then_variadic_args_t() = default;
};

struct one_then_variadic_args_t
{
	explicit one_then_variadic_args_t() = default;
};

template <class _Ty1, class _Ty2, bool = is_empty_v<_Ty1> && !std::is_final_v<_Ty1>>
class compressed_pair final : private _Ty1
{
public:
	_Ty2 second;

	using m_base = _Ty1;

	template <class... Other2>
	constexpr explicit compressed_pair(zero_then_variadic_args_t, Other2&&... value2) noexcept(conjunction_v<is_nothrow_default_constructible<_Ty1>, is_nothrow_constructible<_Ty2, Other2...>>)
		: _Ty1(), second(forward<Other2>(value2)...) {}

	template <class Other1, class... Other2>
	constexpr explicit compressed_pair(one_then_variadic_args_t, Other1&& value1, Other2&&... value2) noexcept(conjunction_v<is_nothrow_constructible<_Ty1, Other1>, is_nothrow_constructible<_Ty2, Other2...>>)
		: _Ty1(forward<Other1>(value1)), second(forward<Other2>(value2)...) {}

	constexpr _Ty1& get_first() noexcept
	{
		return *this;
	}

	constexpr const _Ty1& get_first() const noexcept
	{
		return *this;
	}
};

template <class _Ty1, class _Ty2>
class compressed_pair<_Ty1, _Ty2, false> final
{
public:
	_Ty1 first;
	_Ty2 second;

	template <class... Other2>
	constexpr explicit compressed_pair(zero_then_variadic_args_t, Other2&&... value2) noexcept(conjunction_v<is_nothrow_default_constructible<_Ty1>, is_nothrow_constructible<_Ty2, Other2...>>)
		: first(), second(forward<Other2>(value2)...) {}

	template <class Other1, class... Other2>
	constexpr explicit compressed_pair(one_then_variadic_args_t, Other1&& value1, Other2&&... value2) noexcept(conjunction_v<is_nothrow_constructible<_Ty1, Other1>, is_nothrow_constructible<_Ty2, Other2...>>)
		: first(forward<Other1>(value1)), second(forward<Other2>(value2)...) {}

	constexpr _Ty1& get_first() noexcept
	{
		return first;
	}

	constexpr const _Ty1& get_first() const noexcept
	{
		return first;
	}
};

template <class _Ty1, class _Ty2 = _Ty1, class _Ty3 = _Ty1>
struct trio
{
	using first_type = _Ty1;
	using second_type = _Ty2;
	using third_type = _Ty3;

	constexpr static bool nothrow_swappable = (is_nothrow_swappable_v<_Ty1> && is_nothrow_swappable_v<_Ty2> && is_nothrow_swappable_v<_Ty3>);

	first_type first;
	second_type second;
	third_type third;

	template <class _Uty1 = _Ty1, class _Uty2 = _Ty2, class _Uty3 = _Ty3>
	constexpr explicit(!conjunction_v<std::_Is_implicitly_default_constructible<_Uty1>, std::_Is_implicitly_default_constructible<_Uty2>, std::_Is_implicitly_default_constructible<_Uty3>>)
		trio() noexcept : first(), second(), third() {}

	template <class _Uty1 = _Ty1, class _Uty2 = _Ty2, class _Uty3 = _Ty3>
	constexpr explicit(!conjunction_v<is_convertible<const _Uty1&, _Uty1>, is_convertible<const _Uty2&, _Uty2>, is_convertible<const _Uty3&, _Uty3>>)
		trio(const _Ty1& first_in, const _Ty2& second_in, const _Ty3& third_in) noexcept : first(first_in), second(second_in), third(third_in) {}

	template <class _Other1, class _Other2, class _Other3>
	constexpr explicit(!conjunction_v<is_convertible<const _Other1&, _Ty1>, is_convertible<const _Other2&, _Ty2>, is_convertible<const _Other3&, _Ty3>>)
		trio(const trio<_Other1, _Other2, _Other3>& other) noexcept : first(other.first), second(other.second), third(other.third) {}

	trio(const trio&) = default;
	trio(trio&&) = default;

	trio& operator=(const volatile trio&) = delete;

	constexpr trio& operator=(const trio& _Right)
	{
		first = _Right.first;
		second = _Right.second;
		third = _Right.third;
		return *this;
	}

	bool operator==(const trio& _Right) const
	{
		return first == _Right.first && second == _Right.second && third == _Right.third;
	}

	bool operator!=(const trio& _Right) const 
	{
		return !(*this == _Right);
	}

#if _HAS_CXX20

	template <size_t I, WHP_TRIO_ENABLE_IDX(I)>
	constexpr decltype(auto) operator[](tag<I>)&
	{
		if constexpr (I == 0)
			return first;
		else if constexpr (I == 1)
			return second;
		else // I == 2
			return third;
	}

	template <size_t I, WHP_TRIO_ENABLE_IDX(I)>
	constexpr decltype(auto) operator[](tag<I>) const&
	{
		if constexpr (I == 0)
			return first;
		else if constexpr (I == 1)
			return second;
		else // I == 2
			return third;
	}

	template <size_t I, WHP_TRIO_ENABLE_IDX(I)>
	constexpr decltype(auto) operator[](tag<I>)&&
	{
		if constexpr (I == 0)
			return static_cast<trio<_Ty1, _Ty2, _Ty3>&&>(*this).first;
		else if constexpr (I == 1)
			return static_cast<trio<_Ty1, _Ty2, _Ty3>&&>(*this).second;
		else // I == 2
			return static_cast<trio<_Ty1, _Ty2, _Ty3>&&>(*this).third;
	}

#endif // _HAS_CXX20

	constexpr void swap(trio<_Ty1, _Ty2, _Ty3>& _Other)
	{
		if (this != addressof(_Other))
		{
			swap_adl(first, _Other.first);
			swap_adl(second, _Other.second);
			swap_adl(third, _Other.third);
		}
	}
};

#if _HAS_CXX20

template <size_t I, class A, class B, WHP_PAIR_ENABLE_IDX(I)>
WHP_INLINE constexpr decltype(auto) get(whip::pair<A, B>& pr)
{
	return pr[I];
}

template <size_t I, class A, class B, WHP_PAIR_ENABLE_IDX(I)>
WHP_INLINE constexpr decltype(auto) get(const whip::pair<A, B>& pr)
{
	return pr[I];
}

template <size_t I, class A, class B, WHP_PAIR_ENABLE_IDX(I)>
WHP_INLINE constexpr decltype(auto) get(whip::pair<A, B>&& pr)
{
	return static_cast<whip::pair<A, B>&&>(pr)[I];
}

template <class F, class A, class B>
WHP_INLINE constexpr decltype(auto) apply(F&& func, whip::pair<A, B>& pr)
{
	return static_cast<F&&>(func)(pr.first, pr.second);
}

template <class F, class A, class B>

WHP_INLINE constexpr decltype(auto) apply(F&& func, const whip::pair<A, B>& pr)
{
	return static_cast<F&&>(func)(pr.first, pr.second);
}

template <class F, class A, class B>
WHP_INLINE constexpr decltype(auto) apply(F&& func, whip::pair<A, B>&& pr)
{
	using P = whip::pair<A, B> &&;
	return static_cast<F&&>(func)(static_cast<P>(pr).first, static_cast<P>(pr).second);
}


template <class A, class B>
WHP_INLINE void swap(pair<A, B>& a, pair<A, B>& b) noexcept(pair<A, B>::nothrow_swappable)
{
	a.swap(b);
}

template <class A, class B>
struct tuple_size<pair<A, B>> : integral_constant<size_t, 2> {};

template <class A, class B>
struct tuple_element<0, pair<A, B>>
{
	using type = A;
};

template <class A, class B>
struct tuple_element<1, pair<A, B>>
{
	using type = B;
};


template <size_t I, class A, class B, class C, WHP_TRIO_ENABLE_IDX(I)>
WHP_INLINE constexpr decltype(auto) get(whip::trio<A, B, C>& tr)
{
	return tr[I];
}

template <size_t I, class A, class B, class C, WHP_TRIO_ENABLE_IDX(I)>
WHP_INLINE constexpr decltype(auto) get(const whip::trio<A, B, C>& tr)
{
	return tr[I];
}

template <size_t I, class A, class B, class C, WHP_TRIO_ENABLE_IDX(I)>
WHP_INLINE constexpr decltype(auto) get(whip::trio<A, B, C>&& tr)
{
	return static_cast<whip::trio<A, B, C>&&>(tr)[I];
}

template <class F, class A, class B, class C>
WHP_INLINE constexpr decltype(auto) apply(F&& func, whip::trio<A, B, C>& tr)
{
	return static_cast<F&&>(func)(tr.first, tr.second, tr.third);
}

template <class F, class A, class B, class C>

WHP_INLINE constexpr decltype(auto) apply(F&& func, const whip::trio<A, B, C>& tr)
{
	return static_cast<F&&>(func)(tr.first, tr.second, tr.third);
}

template <class F, class A, class B, class C>
WHP_INLINE constexpr decltype(auto) apply(F&& func, whip::trio<A, B, C>&& tr)
{
	using TR = whip::trio<A, B, C>&&;
	return static_cast<F&&>(func)(static_cast<TR>(tr).first, static_cast<TR>(tr).second, static_cast<TR>(tr).third);
}


template <class A, class B, class C>
WHP_INLINE void swap(trio<A, B, C>& a, trio<A, B, C>& b) noexcept(trio<A, B, C>::nothrow_swappable)
{
	a.swap(b);
}

template <class A, class B, class C>
struct tuple_size<trio<A, B, C>> : integral_constant<size_t, 3> {};

template <class A, class B, class C>
struct tuple_element<0, trio<A, B, C>>
{
	using type = A;
};

template <class A, class B, class C>
struct tuple_element<1, trio<A, B, C>>
{
	using type = B;
};

template <class A, class B, class C>
struct tuple_element<2, trio<A, B, C>>
{
	using type = C;
};

#endif //_HAS_CXX20

#undef WHP_PAIR_ENABLE_IDX
#undef WHP_TRIO_ENABLE_IDX

_WHIP_END

#pragma warning(pop)