#pragma once

#include "Whip/Core/Core.h"
#include "TypeTraits.h"
#include <xutility>

_WHIP_START

namespace detail_utility
{
	template <typename... _Args> WHP_CONSTEXPR void ignore_unused(const _Args&...) {}
}

constexpr auto is_constant_evaluated(bool default_value = false) noexcept -> bool
{
#if _WHP_TEST_CPP_FT(lib_is_constant_evaluated)
	detail_utility::ignore_unused(default_value);
	return std::is_constant_evaluated();
#else
	return default_value;
#endif
}

template <class _Ty>
struct wrap
{
	_Ty m_value;
};

template <class _Tuple>
struct tuple_size;

template <size_t _Index, class _Tuple>
struct tuple_element;

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
WHP_INLINE constexpr bool has_to_address = false;

template <class _Ty>
WHP_INLINE constexpr bool has_to_address<_Ty, void_t<decltype(pointer_traits<_Ty>::to_address(whip::declval<const _Ty&>()))>> = true;

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

namespace detail_utility
{
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
	WHP_NODISCARD bool is_all_bits_zero(const _Ty & _Val)
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
}

template <class _Ty>
constexpr void swap(_Ty& _Left, _Ty& _Right)
{
	_Ty temp	= move(_Left);
	_Left		= move(_Right);
	_Right		= move(temp);
}

template <class _FwdIt1, class _FwdIt2>
WHP_CONSTEXPR void iter_swap(_FwdIt1 _Left, _FwdIt2 _Right) 
{
	_WHIP swap(*_Left, *_Right);
}

template <class _Ty, size_t _Size, enable_if_t<std::_Is_swappable<_Ty>::value, int> _Enabled>
WHP_CONSTEXPR void swap(_Ty(&_Left)[_Size], _Ty(&_Right)[_Size]) noexcept(std::_Is_nothrow_swappable<_Ty>::value) 
{
	if (&_Left != &_Right) 
	{
		_Ty* _First1 = _Left;
		_Ty* _Last1 = _First1 + _Size;
		_Ty* _First2 = _Right;
		for (; _First1 != _Last1; ++_First1, ++_First2) 
			_WHIP iter_swap(_First1, _First2);
	}
}

template <class _Ty>
WHP_CONSTEXPR void swap_nt(_Ty& left, _Ty& right) noexcept(std::_Is_nothrow_swappable<_Ty>::value)
{
	_WHIP swap(left, right);
}

template <class _Ty>
WHP_CONSTEXPR void swap_adl(_Ty& _Left, _Ty& _Right)
{
	_WHIP swap(_Left, _Right);
}

template <class _Ty, class _Other = _Ty>
WHP_CONSTEXPR _Ty exchange(_Ty& val, _Other&& new_val) noexcept(conjunction_v<is_nothrow_constructible<_Ty>, is_nothrow_assignable<_Ty&, _Other>>)
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

template <class _Iter, class = void>
WHP_INLINE constexpr bool unwrappable_v = false;

template <class _Iter>
WHP_INLINE constexpr bool unwrappable_v<_Iter,void_t<decltype(_WHIP declval<std::_Remove_cvref_t<_Iter>&>().reset(_WHIP declval<_Iter>().unwrapped()))>> = true;

template <class _Iter, class = void>
WHP_INLINE constexpr bool has_nothrow_unwrapped = false;
template <class _Iter>
WHP_INLINE constexpr bool has_nothrow_unwrapped<_Iter, void_t<decltype(_WHIP declval<_Iter>().unwrapped())>> = noexcept(_WHIP declval<_Iter>().unwrapped());

template <class _Iter>
WHP_NODISCARD decltype(auto) get_unwrapped(_Iter&& it)	noexcept(!unwrappable_v<_Iter> || has_nothrow_unwrapped<_Iter>)
{
	if constexpr (is_pointer_v<std::decay_t<_Iter>>)
		return it + 0; // default pointer case
	else if constexpr (unwrappable_v<_Iter>)
		return static_cast<_Iter&&>(it).unwrapped(); // whip iterator case
	else if constexpr (std::_Unwrappable_v<_Iter>)
		return static_cast<_Iter&&>(it)._Unwrapped(); // std iterator case
	else 
		return static_cast<_Iter&&>(it); // default case
}

template <class _Iter, class _UIter, class = void>
WHP_INLINE constexpr bool wrapped_resetable_v = false;

template <class _Iter, class _UIter>
WHP_INLINE constexpr bool wrapped_resetable_v<_Iter, _UIter, void_t<decltype(_WHIP declval<_Iter&>().reset(_STD declval<_UIter>()))>> = true;

template <class _Iter, class _UIter>
constexpr void reset_wrapped(_Iter& it, _UIter&& _UIt)
{
	if constexpr (wrapped_resetable_v<_Iter, _UIter>)
		it.reset(_WHIP forward<_UIter>(_UIt)); // whip iterator case
	else if constexpr (std::_Wrapped_seekable_v<_Iter, _UIter>)
		it._Seek_to(_WHIP forward<_UIter>(_UIt)); // std iterator case
	else
		it = _WHIP forward<_UIter>(_UIt); // default case
}

template <class _Iter>
void verify_range(_Iter first, _Iter last)
{
	WHP_ASSERT(get_unwrapped(first) < get_unwrapped(last), "range is not verified");
}

#define WHP_NODISCARD_EMPTY_NON_MEMBER WHP_NODISCARD_MSG("This function returns a bool indicating whether the container or container-like object is empty and has no other effects. It is not useful to call this function and discard the return value.")

template <class _Container>
WHP_NODISCARD constexpr auto size(const _Container& _Cont) noexcept(noexcept(_Cont.size()))-> decltype(_Cont.size())
{
	return _Cont.size();
}

template <class _Ty, size_t _Size>
WHP_NODISCARD constexpr size_t size(const _Ty(&)[_Size]) noexcept
{
	return _Size;
}

template <class _Container>
WHP_NODISCARD_EMPTY_NON_MEMBER constexpr auto empty(const _Container& _Cont) noexcept(noexcept(_Cont.empty()))-> decltype(_Cont.empty()) 
{
	return _Cont.empty();
}

template <class _Ty, size_t _Size>
WHP_NODISCARD_EMPTY_NON_MEMBER constexpr bool empty(const _Ty(&)[_Size]) noexcept
{
	return false;
}

template <class _Elem>
WHP_NODISCARD_EMPTY_NON_MEMBER constexpr bool empty(std::initializer_list<_Elem> _Ilist) noexcept
{
	return _Ilist.size() == 0;
}

template <class _Container>
WHP_NODISCARD constexpr auto data(_Container& _Cont) noexcept(noexcept(_Cont.data()))-> decltype(_Cont.data())
{
	return _Cont.data();
}

template <class _Container>
WHP_NODISCARD constexpr auto data(const _Container& _Cont) noexcept(noexcept(_Cont.data()))-> decltype(_Cont.data())
{
	return _Cont.data();
}

template <class _Ty, size_t _Size>
_NODISCARD constexpr _Ty* data(_Ty(&_Array)[_Size]) noexcept 
{
	return _Array;
}

template <class _Elem>
WHP_NODISCARD constexpr const _Elem* data(std::initializer_list<_Elem> _Ilist) noexcept
{
	return _Ilist.begin();
}

#undef WHP_NODISCARD_EMPTY_NON_MEMBER

_WHIP_END

#include "ExternalOperatorWrapper.h"