#pragma once

#include "Whip/Core/Core.h"
#include "TypeTraits.h"
#include <xutility>

_WHIP_START

struct wrap_base
{
	wrap_base() {}
	virtual ~wrap_base() {}
};

template <class _Ty>
struct wrap : public wrap_base
{
	wrap() {}

	wrap(const _Ty& value) : m_value(value) {}

	virtual ~wrap() override {}

	_Ty m_value;
};

template<class _Ty>
WHP_NODISCARD constexpr _Ty* addressof(_Ty& _Val) noexcept
{
	return __builtin_addressof(_Val);
}

template <class _Ty>
WHP_NODISCARD constexpr remove_reference_t<_Ty>&& move(_Ty&& _Arg) noexcept
{
	return static_cast<remove_reference_t<_Ty>&&>(_Arg);
}

template <class _Ptrty>
WHP_NODISCARD constexpr auto unfancy(_Ptrty _Ptr) noexcept
{
	return addressof(*_Ptr);
}

template <class _Ty>
WHP_NODISCARD constexpr _Ty* unfancy(_Ty* _Ptr) noexcept
{
	return _Ptr;
}

template <class _Ty>
struct pointer_traits 
{
	using pointer = _Ty;
	using element_type = typename std::_Get_element_type<_Ty>::type;
	using difference_type = typename std::_Get_ptr_difference_type<_Ty>::type;

	template <class _Other>
	using rebind = typename std::_Get_rebind_alias<_Ty, _Other>::type;

	using _Reftype = conditional_t<is_void_v<element_type>, char, element_type>&;

	_NODISCARD static _CONSTEXPR20 pointer pointer_to(_Reftype _Val) 
		noexcept(noexcept(_Ty::pointer_to(_Val)))
	{
		return _Ty::pointer_to(_Val);
	}
};

template <class _Ty>
struct pointer_traits<_Ty*> 
{
	using pointer = _Ty*;
	using element_type = _Ty;
	using difference_type = ptrdiff_t;

	template <class _Other>
	using rebind = _Other*;

	using _Reftype = conditional_t<is_void_v<_Ty>, char, _Ty>&;

	_NODISCARD static _CONSTEXPR20 pointer pointer_to(_Reftype _Val) noexcept 
	{
		return addressof(_Val);
	}
};

template <class _Ty, class = void>
inline constexpr bool has_to_address = false;

template <class _Ty>
inline constexpr bool has_to_address<_Ty, void_t<decltype(pointer_traits<_Ty>::to_address(_STD declval<const _Ty&>()))>> = true;

template <class _Ty>
WHP_NODISCARD constexpr _Ty* to_address(_Ty* const _Val) noexcept 
{
	static_assert(!std::is_function_v<_Ty>, "N4810 20.10.4 [pointer.conversion]/2: The program is ill-formed if T is a function type.");
	return _Val;
}

template <class _Ptr>
WHP_NODISCARD constexpr auto to_address(const _Ptr& _Val) noexcept 
{
	if constexpr (has_to_address<_Ptr>) 
	{
		return pointer_traits<_Ptr>::to_address(_Val);
	}
	else 
	{
		return _WHIP to_address(_Val.operator->());
	}
}

template <class _Ty>
struct _Is_character : false_type {}; // by default, not a character type

template <>
struct _Is_character<char> : true_type {}; // chars are characters

template <>
struct _Is_character<signed char> : true_type {}; // signed chars are also characters

template <>
struct _Is_character<unsigned char> : true_type {}; // unsigned chars are also characters

#ifdef __cpp_char8_t
template <>
struct _Is_character<char8_t> : true_type {}; // UTF-8 code units are sort-of characters
#endif // __cpp_char8_t

template <class _Ty>
struct _Is_character_or_bool : _Is_character<_Ty>::type {};

template <>
struct _Is_character_or_bool<bool> : true_type {};

template <class _Ty>
struct _Is_character_or_byte_or_bool : _Is_character_or_bool<_Ty>::type {};

#ifdef __cpp_lib_byte
template <>
struct _Is_character_or_byte_or_bool<std::byte> : true_type {};
#endif // __cpp_lib_byte

template <class _FwdIt, class _Ty>
WHP_INLINE constexpr bool fill_memset_is_safe = std::conjunction_v<std::is_scalar<_Ty>, 
	_Is_character_or_byte_or_bool<std::_Unwrap_enum_t<remove_reference_t<std::_Iter_ref_t<_FwdIt>>>>,
	std::negation<std::is_volatile<remove_reference_t<std::_Iter_ref_t<_FwdIt>>>>, 
	std::is_assignable<std::_Iter_ref_t<_FwdIt>, const _Ty&>>;

template <class _FwdIt, class _Ty>
WHP_INLINE constexpr bool fill_zero_memset_is_safe = std::conjunction_v<std::is_scalar<_Ty>, 
	std::is_scalar<std::_Iter_value_t<_FwdIt>>, std::negation<std::is_member_pointer<std::_Iter_value_t<_FwdIt>>>,
	std::negation<std::is_volatile<remove_reference_t<std::_Iter_ref_t<_FwdIt>>>>, 
	std::is_assignable<std::_Iter_ref_t<_FwdIt>, const _Ty&>>;

template <class _CtgIt, class _Ty>
void fill_memset(_CtgIt _Dest, const _Ty _Val, const size_t _Count) 
{
	std::_Iter_value_t<_CtgIt> _Dest_val = _Val;
	::memset(to_address(_Dest), static_cast<unsigned char>(_Dest_val), _Count);
}

template <class _CtgIt>
void fill_zero_memset(_CtgIt _Dest, const size_t _Count) 
{
	::memset(to_address(_Dest), 0, _Count * sizeof(std::_Iter_value_t<_CtgIt>));
}

template <class _Ty>
WHP_NODISCARD bool is_all_bits_zero(const _Ty& _Val) 
{
	WHP_CORE_ASSERT(std::is_scalar_v<_Ty> && !std::is_member_pointer_v<_Ty>, "value is not scalar or member pointer.");
	if constexpr (is_same_v<_Ty, nullptr_t>) 
	{
		return true;
	}
	else 
	{
		constexpr _Ty zero{};
		return ::memcmp(&_Val, &zero, sizeof(_Ty)) == 0;
	}
}

template <class _Ty>
constexpr void swap(_Ty& _Left, _Ty& _Right)
{
	_Ty temp	= move(_Left);
	_Left		= move(_Right);
	_Right		= move(temp);
}

template <class _Ty>
constexpr void swap_nt(_Ty& left, _Ty& right) noexcept(std::_Is_nothrow_swappable<_Ty>::value)
{
	swap(left, right);
}

template <class _Ty>
constexpr void swap_adl(_Ty _Left, _Ty _Right)
{
	swap<_Ty>(_Left, _Right);
}

template <class _Ty, class _Other = _Ty>
constexpr _Ty exchange(_Ty& val, _Other&& new_val) noexcept(conjunction_v<is_nothrow_constructible<_Ty>, is_nothrow_assignable<_Ty&, _Other>>)
{
	_Ty old_val = static_cast<_Ty&&>(val);
	val = static_cast<_Other&&>(new_val);
	return old_val;
}

template <class _Ty, size_t _Size>
WHP_NODISCARD constexpr _Ty* begin(_Ty(&_Array)[_Size]) noexcept
{
	return _Array;
}

template <class _Ty, size_t _Size>
WHP_NODISCARD constexpr _Ty* end(_Ty(&_Array)[_Size]) noexcept
{
	return _Array + _Size;
}

WHP_NODISCARD constexpr float calculate_aspect_ratio(float x, float y)
{
	return (x / y);
}

template <class _Ty, enable_if_t<std::is_arithmetic_v<_Ty>, int> = 0>
constexpr _Ty wabs(const _Ty& _val) noexcept
{
	return (_val < 0) ? (-_val) : _val;
}

template <class _Ty, enable_if_t<std::is_floating_point_v<_Ty>, int> = 0>
constexpr _Ty wceil(const _Ty& _val) noexcept
{
	_Ty result = static_cast<_Ty>(static_cast<long long>(_val));
	if (_val > result)
	{
		result += 1;
	}
	return result;
}

template <class _Ty>
WHP_NODISCARD constexpr _Ty&& forward(remove_reference_t<_Ty>& _Arg) noexcept 
{
	return static_cast<_Ty&&>(_Arg);
}

template <class _Ty>
WHP_NODISCARD constexpr _Ty&& forward(remove_reference_t<_Ty>&& _Arg) noexcept 
{
	static_assert(!is_lvalue_reference_v<_Ty>, "bad forward call");
	return static_cast<_Ty&&>(_Arg);
}

// !!!!!!!WARNING!!!!! NOT TOTALY SAFE! Just use for whip iterator or normal pointer
template <class _Iter>
WHP_NODISCARD decltype(auto) get_unwrapped(_Iter&& it)
{
	if constexpr (is_pointer_v<std::decay_t<_Iter>>)
	{
		return it + 0;
	}
	else // whip iterator -> i hope 
	{
		return it.unwrapped();
	}
}

template <class _Iter>
void verify_range(_Iter first, _Iter last)
{
	WHP_ASSERT(get_unwrapped(first) < get_unwrapped(last), "range is not verified");
}

_WHIP_END

#include "Pair.h"
#include "ExternalOperatorWrapper.h"