#pragma once

#include "Core.h"
#include "Utility.h"
#include "Iterator.h"

_WHIP_START

template <class _Ty>
class array_iterator : public iterator_base<_Ty>
{
public:
    using m_base        = iterator_base<_Ty>;
    using value_type    = _Ty;
    using pointer       = _Ty*;
    using reference     = _Ty&;
    using size_type     = m_base::size_type;
    using diff_type     = m_base::diff_type;

    template <class _Ty>
    friend class const_array_iterator;


    array_iterator(pointer ptr = nullptr, size_type offset = 0) : m_ptr(ptr), m_offset(offset) {}

    array_iterator(const array_iterator& other) : m_ptr(other.m_ptr), m_offset(other.m_offset) {}

    ~array_iterator() {}

    reference operator*() const
    {
        return *(m_ptr + m_offset);
    }

    array_iterator& operator++()
    {
        ++m_offset;
        return *this;
    }

    array_iterator operator++(int)
    {
        array_iterator temp(*this);
        ++(*this);
        return temp;
    }

    array_iterator& operator--()
    {
        --m_offset;
        return *this;
    }

    array_iterator operator--(int)
    {
        array_iterator temp(*this);
        --(*this);
        return temp;
    }

    bool operator==(const array_iterator& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator==(array_iterator&& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator!=(const array_iterator& other)
    {
        return !(this->operator==(move(other)));
    }

    array_iterator operator+(size_type n) const
    {
        return array_iterator(m_ptr, m_offset + n);
    }

    size_type operator+(const array_iterator& other) const
    {
        pointer other_ptr = other.m_ptr + other.m_offset;
        pointer ptr = m_ptr + m_offset;
        return ptr + other_ptr;
    }

    array_iterator operator-(size_type n) const
    {
        return array_iterator(m_ptr, m_offset - n);
    }

    size_type operator-(const array_iterator& other) const
    {
        pointer other_ptr = other.m_ptr + other.m_offset;
        pointer ptr = m_ptr + m_offset;
        return ptr - other_ptr;
    }

    array_iterator begin() noexcept
    {
        return array_iterator(m_ptr);
    }

    array_iterator end() noexcept
    {
        return array_iterator(m_ptr + m_offset);
    }

private:
    constexpr void reset(pointer ptr = nullptr, size_type offset = 0) noexcept
    {
        m_ptr = ptr;
        m_offset = offset;
    }

private:
    pointer m_ptr;
    size_type m_offset;
};

template <class _Ty>
class const_array_iterator : public iterator_base<const _Ty>
{
public:
    using m_base        = iterator_base<const _Ty>;
    using value_type    = const _Ty;
    using pointer       = const _Ty* const;
    using reference     = const _Ty&;
    using size_type     = m_base::size_type;
    using diff_type     = m_base::diff_type;


    const_array_iterator(pointer ptr = nullptr, size_type offset = 0)
        : m_ptr(ptr), m_offset(offset)
    {}

    const_array_iterator(const array_iterator<_Ty>& other)
        : m_ptr(other.m_ptr), m_offset(other.m_offset)
    {}

    ~const_array_iterator() {}

    reference operator*() const
    {
        return *(m_ptr + m_offset);
    }

    const_array_iterator& operator++()
    {
        ++m_offset;
        return *this;
    }

    const_array_iterator operator++(int)
    {
        const_array_iterator temp(*this);
        ++(*this);
        return temp;
    }

    const_array_iterator& operator--()
    {
        --m_offset;
        return *this;
    }

    const_array_iterator operator--(int)
    {
        const_array_iterator temp(*this);
        --(*this);
        return temp;
    }

    bool operator==(const const_array_iterator& other) const
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator!=(const const_array_iterator& other) const
    {
        return !(*this == other);
    }

    const_array_iterator operator+(size_type n) const
    {
        return const_array_iterator(m_ptr, m_offset + n);
    }

    const_array_iterator operator-(size_type n) const
    {
        return const_array_iterator(m_ptr, m_offset - n);
    }

    size_type operator-(const const_array_iterator& other) const
    {
        pointer other_ptr = other.m_ptr + other.m_offset;
        pointer ptr = m_ptr + m_offset;
        return ptr - other_ptr;
    }

    const_array_iterator begin() noexcept
    {
        return const_array_iterator(m_ptr);
    }

    const_array_iterator end() noexcept
    {
        return const_array_iterator(m_ptr + m_offset);
    }

private:
    pointer m_ptr;
    size_type m_offset;
};


_WHIP_END