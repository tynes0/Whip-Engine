#pragma once

#include "Whip/Core/Core.h"
#include "TypeTraits.h"
#include "Utility.h"

_WHIP_START

template <class _Ty, class _Ptr = _Ty*, class _Ref = _Ty&, class _SizeT = size_t, class _Diff = ptrdiff_t>
struct iterator_base
{
	using value_type	= _Ty;
	using pointer		= _Ptr;
	using reference		= _Ref;
	using size_type		= _SizeT;
	using diff_type		= _Diff;
};

template <class _WhipIter>
WHP_INLINE constexpr bool is_const_whip_iterator_v = is_base_of_v<iterator_base<const remove_const_t<typename _WhipIter::value_type>>, _WhipIter>;

template <class _WhipIter>
WHP_INLINE constexpr bool is_non_const_whip_iterator_v = is_base_of_v<iterator_base<remove_const_t<typename _WhipIter::value_type>>, _WhipIter>;

template <class _WhipIter>
WHP_INLINE constexpr bool is_whip_iterator_v = is_const_whip_iterator_v<_WhipIter> || is_non_const_whip_iterator_v<_WhipIter>;

template <class _Ty>
class basic_iterator : public iterator_base<_Ty>
{
public:
    using m_base        = iterator_base<_Ty>;
    using value_type    = _Ty;
    using pointer       = _Ty*;
    using reference     = _Ty&;
    using size_type     = typename m_base::size_type;
    using diff_type     = typename m_base::diff_type;

    template <class _Ty>
    friend class const_basic_iterator;

    basic_iterator(pointer ptr = nullptr, size_type offset = 0) : m_ptr(ptr), m_offset(offset) {}

    basic_iterator(const basic_iterator& other) : m_ptr(other.m_ptr), m_offset(other.m_offset) {}

    ~basic_iterator() {}

    reference operator*() const
    {
        return *(m_ptr + m_offset);
    }

    basic_iterator& operator++()
    {
        ++m_offset;
        return *this;
    }

    basic_iterator operator++(int)
    {
        basic_iterator temp(*this);
        ++(*this);
        return temp;
    }

    basic_iterator& operator--()
    {
        --m_offset;
        return *this;
    }

    basic_iterator operator--(int)
    {
        basic_iterator temp(*this);
        --(*this);
        return temp;
    }

    bool operator==(const basic_iterator& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator==(basic_iterator&& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator!=(const basic_iterator& other)
    {
        return !(this->operator==(move(other)));
    }

    basic_iterator operator+(size_type n) const
    {
        return basic_iterator(m_ptr, m_offset + n);
    }

    size_type operator+(const basic_iterator& other) const
    {
        pointer other_ptr = other.m_ptr + other.m_offset;
        pointer ptr = m_ptr + m_offset;
        return ptr + other_ptr;
    }

    basic_iterator operator-(size_type n) const
    {
        return basic_iterator(m_ptr, m_offset - n);
    }

    size_type operator-(const basic_iterator& other) const
    {
        pointer other_ptr = other.m_ptr + other.m_offset;
        pointer ptr = m_ptr + m_offset;
        return ptr - other_ptr;
    }

    constexpr void reset(pointer ptr = nullptr, size_type offset = 0) noexcept
    {
        m_ptr = ptr;
        m_offset = offset;
    }

    pointer unwrapped()
    {
        return (m_ptr + m_offset);
    }

private:
    pointer m_ptr;
    size_type m_offset;
};

template <class _Ty>
class const_basic_iterator : public iterator_base<const _Ty>
{
public:
    using m_base        = iterator_base<const _Ty>;
    using value_type    = const _Ty;
    using pointer       = const _Ty* const;
    using reference     = const _Ty&;
    using size_type     = typename m_base::size_type;
    using diff_type     = typename m_base::diff_type;


    const_basic_iterator(pointer ptr = nullptr, size_type offset = 0)
        : m_ptr(ptr), m_offset(offset)
    {}

    const_basic_iterator(const basic_iterator<_Ty>& other)
        : m_ptr(other.m_ptr), m_offset(other.m_offset)
    {}

    ~const_basic_iterator() {}

    reference operator*() const
    {
        return *(m_ptr + m_offset);
    }

    const_basic_iterator& operator++()
    {
        ++m_offset;
        return *this;
    }

    const_basic_iterator operator++(int)
    {
        const_basic_iterator temp(*this);
        ++(*this);
        return temp;
    }

    const_basic_iterator& operator--()
    {
        --m_offset;
        return *this;
    }

    const_basic_iterator operator--(int)
    {
        const_basic_iterator temp(*this);
        --(*this);
        return temp;
    }

    bool operator==(const const_basic_iterator& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator==(const_basic_iterator&& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator!=(const const_basic_iterator& other)
    {
        return !(this->operator==(move(other)));
    }

    const_basic_iterator operator+(size_type n) const
    {
        return const_basic_iterator(m_ptr, m_offset + n);
    }

    const_basic_iterator operator-(size_type n) const
    {
        return const_basic_iterator(m_ptr, m_offset - n);
    }

    size_type operator-(const const_basic_iterator& other) const
    {
        pointer other_ptr = other.m_ptr + other.m_offset;
        pointer ptr = m_ptr + m_offset;
        return ptr - other_ptr;
    }

    pointer unwrapped()
    {
        return (m_ptr + m_offset);
    }

private:
    pointer m_ptr;
    size_type m_offset;
};

template <class _Iter>
constexpr decltype(auto) make_basic_iterator(_Iter iter, bool is_end = false)
{
    basic_iterator result = (!is_end) ? &DREF(iter) : ((&(DREF(iter - 1))) + 1);
    return result;
}

template <class _Iter, enable_if_t<is_whip_iterator_v<_Iter>, int> = 0>
class reverse_iterator : public iterator_base<typename _Iter::value_type>
{
public:
	using m_base		= _Iter;
	using value_type	= typename m_base::value_type;
	using pointer		= typename m_base::pointer;
	using reference		= typename m_base::reference;
	using size_type		= typename m_base::size_type;
	using diff_type		= typename m_base::diff_type;

	reverse_iterator() = default;

	reverse_iterator(const _Iter& right) : current_iterator(move(right)) {}

	reverse_iterator(const reverse_iterator& right) : current_iterator(right.current_iterator) {}

	reverse_iterator& operator=(const reverse_iterator& right)
	{
		current_iterator = right.current_iterator;
		return *this;
	}

	WHP_NODISCARD _Iter base() const
	{
		return current_iterator;
	}

	WHP_NODISCARD reference operator*() const 
	{
		_Iter temp = current_iterator;
		return *(--temp);
	}

	// !!!!!!!!! DANGER !!!!!!!!! todo: we should improve
	WHP_NODISCARD pointer operator->() const
	{
		_Iter temp = current_iterator;
		--temp;
		return temp.operator->();
	}

	reverse_iterator& operator++()
	{
		--current_iterator;
		return *this;
	}

	reverse_iterator operator++(int)
	{
		_Iter temp = current_iterator;
		current_iterator--;
		return temp;
	}

	reverse_iterator& operator--()
	{
		++current_iterator;
		return *this;
	}

	reverse_iterator operator--(int)
	{
		_Iter temp = current_iterator;
		current_iterator++;
		return temp;
	}

	reverse_iterator operator+(diff_type n) const
	{
		return reverse_iterator(current_iterator - n);
	}

	reverse_iterator operator-(diff_type n) const
	{
		return reverse_iterator(current_iterator + n);
	}

	bool operator==(const reverse_iterator& other)
	{
		return (this->current_iterator == other.current_iterator);
	}

	bool operator==(reverse_iterator&& other)
	{
		return (this->current_iterator == other.current_iterator);
	}

	bool operator!=(const reverse_iterator& other)
	{
		return !(this->operator==(move(other)));
	}

    pointer unwrapped()
    {
        _Iter temp = current_iterator;
        return (--temp).unwrapped();
    }

private:
	_Iter current_iterator{};
};

template <class _Ty = int>
class iterate
{
public:
    using value_type                = _Ty;
    using pointer                   = _Ty*;
    using iterator                  = basic_iterator<_Ty>;
    using const_iterator            = const_basic_iterator<_Ty>;
    using reverse_iterator          = _WHIP reverse_iterator<basic_iterator<_Ty>>;
    using const_reverse_iterator    = _WHIP reverse_iterator<const_basic_iterator<_Ty>>;

    template <class _Ty2>
    using rebind = iterate<_Ty2>;

    iterate() : m_begin(nullptr), m_end(nullptr) {}

    template <class _Iter>
    iterate(_Iter first, _Iter last) : m_begin(get_unwrapped(first)), m_end(get_unwrapped(last)) {}

    iterate(const pair<_Ty*>& pointers) : m_begin(pointers.first), m_end(pointers.second) {}

    iterate(const iterate& right) : m_begin(right.m_begin), m_end(right.m_end) {}

    iterate& operator=(const iterate& right)
    {
        m_begin = right.m_begin;
        m_end   = right.m_end;
        return *this;
    }

    constexpr iterator begin() noexcept
    {
        return iterator(m_begin);
    }

    constexpr iterator end() noexcept
    {
        return iterator(m_end);
    }

    constexpr const_iterator begin() const noexcept
    {
        return const_iterator(m_begin);
    }

    constexpr const_iterator end() const noexcept
    {
        return const_iterator(m_end);
    }

    constexpr const_iterator cbegin() const noexcept
    {
        return const_iterator(m_begin);
    }

    constexpr const_iterator cend() const noexcept
    {
        return const_iterator(m_end);
    }

    constexpr reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }

    constexpr reverse_iterator rend() noexcept
    {
        return reverse_iterator(begin());
    }

    constexpr const_reverse_iterator rbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }

    constexpr const_reverse_iterator rend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }

    constexpr const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }

    constexpr const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }

private:
    _Ty* m_begin;
    _Ty* m_end;
};


_WHIP_END