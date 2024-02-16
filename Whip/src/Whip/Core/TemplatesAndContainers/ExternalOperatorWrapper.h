#pragma once

#include "Whip/Core/Core.h"
#include "TypeTraits.h"

_WHIP_START

template <class _FunTy>
struct external_operator_wrapper
{
	_FunTy fun;
};

template <class _FunTy>
inline constexpr external_operator_wrapper<_FunTy> make_external_operator(_FunTy _Fun)
{
	return { _Fun };
}

template <typename T, typename _FunTy>
struct external_operator_left_hand_side 
{
	_FunTy fun;
	T& value;
};

template <typename T, typename _FunTy>
inline external_operator_left_hand_side<T, _FunTy> operator <(T& left_hand_side, const external_operator_wrapper<_FunTy>& right_hand_side) 
{
	return { right_hand_side.fun, left_hand_side };
}

template <typename T, typename _FunTy>
inline external_operator_left_hand_side<T const, _FunTy> operator <(T const& left_hand_side, external_operator_wrapper<_FunTy> right_hand_side) 
{
	return { right_hand_side.fun, left_hand_side };
}

template <typename _Ty1, typename _Ty2, typename _FunTy>
inline auto operator >(external_operator_left_hand_side<_Ty1, _FunTy> const& left_hand_side, _Ty2 const& right_hand_side)
-> decltype(left_hand_side.fun(declval<_Ty1>(), declval<_Ty2>()))
{
	return left_hand_side.fun(left_hand_side.value, right_hand_side);
}

template <typename _Ty1, typename _Ty2, typename _FunTy>
inline auto operator >=(external_operator_left_hand_side<_Ty1, _FunTy> const& left_hand_side, _Ty2 const& right_hand_side)
-> decltype(left_hand_side.value = left_hand_side.fun(declval<_Ty1>(), declval<_Ty2>()))
{
	return left_hand_side.value = left_hand_side.fun(left_hand_side.value, right_hand_side);
}

_WHIP_END