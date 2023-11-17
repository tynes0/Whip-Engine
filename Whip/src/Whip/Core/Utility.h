#pragma once

#include "Core.h"
#include "TypeTraits.h"
#include "Iterator.h"

_WHIP_START

#pragma warning(push)
#pragma warning(disable : 4624)
template <class _Ty>
struct wrap {
	_Ty m_value;
};
#pragma warning(pop)

template<class _Ty>
WHP_NODISCARD constexpr _Ty* addressof(_Ty& _Val) noexcept
{
	return __builtin_addressof(_Val);
}

template <class _Ty>
WHP_NODISCARD constexpr remove_reference_t<_Ty>&& move(_Ty&& _Arg) noexcept {
	return static_cast<remove_reference_t<_Ty>&&>(_Arg);
}

template <class _Ptrty>
WHP_NODISCARD constexpr auto unfancy(_Ptrty _Ptr) noexcept {
	return addressof(*_Ptr);
}

template <class _Ty>
WHP_NODISCARD constexpr _Ty* unfancy(_Ty* _Ptr) noexcept {
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
WHP_NODISCARD constexpr _Ty&& forward(remove_reference_t<_Ty>& _Arg) noexcept {
	return static_cast<_Ty&&>(_Arg);
}

template <class _Ty>
WHP_NODISCARD constexpr _Ty&& forward(remove_reference_t<_Ty>&& _Arg) noexcept {
	static_assert(!is_lvalue_reference_v<_Ty>, "bad forward call");
	return static_cast<_Ty&&>(_Arg);
}

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
struct external_operator_left_hand_side {
	_FunTy fun;
	T& value;
};

template <typename T, typename _FunTy>
inline external_operator_left_hand_side<T, _FunTy> operator <(T& left_hand_side, const external_operator_wrapper<_FunTy>& right_hand_side) {
	return { right_hand_side.fun, left_hand_side };
}

template <typename T, typename _FunTy>
inline external_operator_left_hand_side<T const, _FunTy> operator <(T const& left_hand_side, external_operator_wrapper<_FunTy> right_hand_side) {
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

template <class _Ty, enable_if_t<std::is_arithmetic_v<_Ty>, int> = 0>
class range_iterator : public whip_iterator_base<const _Ty>
{
public:
	using value_type = _Ty;

	range_iterator(value_type _value, value_type end, value_type step) : m_value(_value), m_end(end), m_step(step) {}

	value_type operator*()
	{
		return m_value;
	}

	range_iterator& operator++()
	{
		m_value = (m_value < m_end) ? m_value + m_step : m_value - m_step;
		return *this;
	}

	range_iterator& operator--()
	{
        m_value = (m_value < m_end) ? m_value - m_step : m_value + m_step;
		return *this;
	}

	range_iterator& operator=(const range_iterator& other) noexcept
	{
		m_value = other.m_value;
		m_end	= other.m_end;
		m_step	= other.m_step;
		return *this;
	}

	range_iterator& operator=(range_iterator&& other) noexcept
	{
		m_value	= move(other.m_value);
		m_end	= move(other.m_end);
		m_step	= move(other.m_step);
		return *this;
	}

    bool operator==(const range_iterator& other)
    {
        return (m_step == other.m_step && m_value == other.m_value && m_end == other.m_end);
    }

	bool operator!=(const range_iterator& other)
	{
        return !this->operator==(other);
	}

private:
	value_type m_value;
	value_type m_step;
	value_type m_end;
};

template <class _Ty, std::enable_if_t<std::is_arithmetic_v<_Ty>, int> = 0>
class range
{
public:
    using range_type    = _Ty;
    using size_type     = size_t;
    using step_type     = make_signed_t<_Ty>;
    using iterator      = range_iterator<_Ty>;

    constexpr range(_Ty end)
    {
        reset(static_cast<_Ty>(0), end);
    }

    constexpr range(_Ty start, _Ty end, step_type step = static_cast<step_type>(1))
    {
        reset(start, end, step);
    }

    constexpr void reset(_Ty start, _Ty end, step_type step = static_cast<step_type>(1)) noexcept
    {
        m_start = start;
        m_end = end;
        m_step = abs(step);
    }

    WHP_NODISCARD constexpr iterator begin() noexcept
    {
        return iterator(m_start, m_end, m_step);
    }

    WHP_NODISCARD constexpr iterator begin() const noexcept
    {
        return iterator(m_start, m_end, m_step);
    }

    WHP_NODISCARD constexpr iterator end() noexcept
    {
        return iterator(m_end, m_end, m_step);
    }

    WHP_NODISCARD constexpr iterator end() const noexcept
    {
        return iterator(m_end, m_end, m_step);
    }

    WHP_NODISCARD constexpr range step(step_type new_step) const noexcept
    {
        return range(m_start, m_end, new_step);
    }

    WHP_NODISCARD constexpr range reverse() const noexcept
    {
        if (m_step < 0)
        {
            return range(m_end + 1, m_start + 1, -m_step);
        }
        return range(m_end - 1, m_start - 1, -m_step);
    }

    WHP_NODISCARD constexpr _Ty nth_step(size_type n) const noexcept
    {
        return m_start + (m_step * (n - 1));
    }

    WHP_NODISCARD constexpr size_type size() const noexcept
    {
        size_type result = 0;
        if (m_start == m_end) return result;
        if constexpr (std::is_integral_v<_Ty>)
        {
            result = abs((m_end - m_start) / m_step);
            if (m_step != 1) result += 1;
            return result;
        }
        else if constexpr (std::is_floating_point_v<_Ty>)
        {
            result = ceil(abs((m_end - m_start) / m_step));
            return result;
        }
        return result;
    }

private:
    _Ty m_start;
    _Ty m_end;
    step_type m_step;
};

using irange	= range<int>;
using lrange	= range<long>;
using llrange	= range<long long>;
using drange	= range<double>;
using frange	= range<float>;
using srange	= range<size_t>;
using uirange	= range<unsigned int>;

_WHIP_END

#include "Pair.h"