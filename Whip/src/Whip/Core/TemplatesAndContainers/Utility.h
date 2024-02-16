#pragma once

#include "Whip/Core/Core.h"
#include "TypeTraits.h"

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

template <class _Ty>
constexpr void swap_in_range(_Ty& left, _Ty& right, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        swap(*(left + i), *(right + i));
    }
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