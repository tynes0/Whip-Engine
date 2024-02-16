#pragma once

#include <type_traits>

#include "Whip/Core/Core.h"
#include "Iterator.h"
#include "TypeTraits.h"
#include "Utility.h"

// TODO: FIX ALL OF THE CLASS 

_WHIP_START

template <class _Ty, enable_if_t<std::is_arithmetic_v<_Ty>, int> = 0>
class range_iterator : public iterator_base<const _Ty>
{
public:
    using value_type = _Ty;

    range_iterator(value_type _value, value_type step) : m_value(_value), m_step(step) {}

    value_type operator*()
    {
        return m_value;
    }

    range_iterator& operator++()
    {
        m_value = m_value + m_step;
        return *this;
    }

    range_iterator operator++(int)
    {
        range_iterator temp = *this;
        m_value = m_value + m_step;
        return temp;
    }

    range_iterator& operator--()
    {
        m_value = m_value - m_step;
        return *this;
    }

    range_iterator operator--(int)
    {
        range_iterator temp = *this;
        m_value = m_value - m_step;
        return temp;
    }

    range_iterator& operator=(const range_iterator& other) noexcept
    {
        m_value = other.m_value;
        m_step = other.m_step;
        return *this;
    }

    range_iterator& operator=(range_iterator&& other) noexcept
    {
        m_value = move(other.m_value);
        m_step  = move(other.m_step);
        return *this;
    }

    bool operator==(const range_iterator& other)
    {
        return m_value == other.m_value;
    }

    bool operator!=(const range_iterator& other)
    {
        return !this->operator==(other);
    }

private:
    value_type m_value;
    value_type m_step;
};

template <class _Ty, enable_if_t<std::is_arithmetic_v<_Ty>, int> = 0>
class range
{
public:
    using range_type    = _Ty;
    using step_type     = conditional_t<std::is_integral_v<_Ty>, long long, double>;
    using size_type     = step_type;
    using iterator      = range_iterator<_Ty>;

    constexpr range(_Ty end)
    {
        reset(0, end);
    }

    constexpr range(_Ty start, _Ty end, step_type step = 1)
    {
        reset(start, end, step);
    }

    constexpr void reset(_Ty start, _Ty end, step_type step = 1) noexcept
    {
        m_start = start;
        m_end   = end;

        // Users can make mistakes, and this provides an error -> now, if users makes mistakes, program will understand the mistake and will fix it.
        if (step > 0 && m_start < m_end)
            m_step = step;
        else if (step > 0 && m_start > m_end)
            m_step = step * -1;
        else if (step < 0 && m_start > m_end)
            m_step = step;
        else if (step < 0 && m_start < m_end)
            m_step = step * -1;

        m_real_end = (step > 0) ? (m_start + static_cast<_Ty>(size()) * static_cast<_Ty>(abs(m_step))) : (m_start - static_cast<_Ty>(size()) * static_cast<_Ty>(abs(m_step)));
    }

    WHP_NODISCARD constexpr iterator begin() noexcept
    {
        return iterator(m_start, static_cast<_Ty>(m_step));
    }

    WHP_NODISCARD constexpr iterator begin() const noexcept
    {
        return iterator(m_start, static_cast<_Ty>(m_step));
    }

    WHP_NODISCARD constexpr iterator end() noexcept
    {
        return iterator(m_real_end, static_cast<_Ty>(m_real_end));
    }

    WHP_NODISCARD constexpr iterator end() const noexcept
    {
        return iterator(m_real_end, static_cast<_Ty>(m_real_end));
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
            result = abs((m_end - m_start) / abs(m_step));
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
    _Ty m_real_end;
    step_type m_step;
};

using irange    = range<int>;
using lrange    = range<long>;
using llrange   = range<long long>;
using drange    = range<double>;
using frange    = range<float>;
using uirange   = range<unsigned int>;
using srange    = range<size_t>;

_WHIP_END