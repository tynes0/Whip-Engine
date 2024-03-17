#pragma once

#include "Whip/Core/Core.h"
#include "Utility.h"
#include "Algorithms.h"
#include "Iterator.h"
#include "Whip/Core/Log.h"

_WHIP_START

template <class _Ty>
class array_iterator : public iterator_base<_Ty>
{
public:
    using m_base        = iterator_base<_Ty>;
    using value_type    = _Ty;
    using pointer       = _Ty*;
    using reference     = _Ty&;
    using size_type     = typename m_base::size_type;
    using diff_type     = typename m_base::diff_type;

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
class const_array_iterator : public iterator_base<const _Ty>
{
public:
    using m_base        = iterator_base<const _Ty>;
    using value_type    = const _Ty;
    using pointer       = const _Ty* const;
    using reference     = const _Ty&;
    using size_type     = typename m_base::size_type;
    using diff_type     = typename m_base::diff_type;


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

	bool operator==(const const_array_iterator& other)
	{
		return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
	}

	bool operator==(const_array_iterator&& other)
	{
		return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
	}

	bool operator!=(const const_array_iterator& other)
	{
		return !(this->operator==(move(other)));
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

	pointer unwrapped()
	{
		return (m_ptr + m_offset);
	}

private:
    pointer m_ptr;
    size_type m_offset;
};


template <class _Ty, size_t _Size>
class array
{
public:
	using value_type				= _Ty;
	using size_type					= size_t;
	using difference_type			= ptrdiff_t;
	using pointer					= _Ty*;
	using const_pointer				= const _Ty*;
	using reference					= _Ty&;
	using const_reference			= const _Ty&;
	using iterator					= array_iterator<_Ty>;
	using const_iterator			= const_array_iterator<_Ty>;
	using reverse_iterator			= _WHIP reverse_iterator<array_iterator<_Ty>>;
	using const_reverse_iterator	= _WHIP reverse_iterator<const_array_iterator<_Ty>>;

	constexpr reference operator[](size_type index) noexcept
	{
		WHP_ASSERT(index < _Size, "array subscript out of range");
		return *(m_data + index);
	}

	constexpr const_reference operator[](size_type index) const noexcept
	{
		WHP_ASSERT(index < _Size, "array subscript out of range");
		return *(m_data + index);
	}

	constexpr size_type size() const noexcept
	{
		return _Size;
	}

	constexpr size_type max_size() const noexcept
	{
		return _Size;
	}

	constexpr iterator begin() noexcept
	{
		return iterator(m_data);
	}

	constexpr iterator end() noexcept
	{
		return iterator(m_data, _Size);
	}

	constexpr const_iterator begin() const noexcept
	{
		return const_iterator(m_data);
	}

	constexpr const_iterator end() const noexcept
	{
		return const_iterator(m_data, _Size);
	}

	constexpr const_iterator cbegin() const noexcept
	{
		return const_iterator(m_data);
	}

	constexpr const_iterator cend() const noexcept
	{
		return const_iterator(m_data, _Size);
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

	constexpr pointer data() noexcept
	{
		return m_data;
	}

	constexpr const_pointer data() const noexcept
	{
		return m_data;
	}

	constexpr reference at(size_t index)
	{
		if (index >= _Size) throw_oran();
		return *(m_data + index);
	}

	constexpr const_reference at(size_t index) const
	{
		if (index >= _Size) throw_oran();
		return *(m_data + index);
	}

	constexpr reference front() noexcept
	{
		return m_data[0];
	}

	constexpr const_reference front() const noexcept
	{
		return m_data[0];
	}

	constexpr reference back() noexcept
	{
		return m_data[_Size - 1];
	}

	constexpr const_reference back() const noexcept
	{
		return m_data[_Size - 1];
	}

	constexpr bool empty() const noexcept
	{
		return false;
	}

	constexpr void fill(const_reference _Val) noexcept
	{
		for (auto i = begin(); i != end(); ++i)
		{
			*i = _Val;
		}
	}

	constexpr void swap(array& other) noexcept
	{
		if (addressof(other) != this)
		{
			swap_in_range(m_data, other.m_data, _Size);
		}
	}

	constexpr void copy(const array& other) noexcept
	{
		if (addressof(other) != this)
		{
			for (auto i = 0; i < _Size; ++i)
			{
				*(m_data + i) = *(other.m_data + i);
			}
		}
	}

	constexpr pointer unchecked_begin() noexcept
	{
		return m_data;
	}

	constexpr const_pointer unchecked_begin() const noexcept
	{
		return m_data;
	}

	constexpr pointer unchecked_end() noexcept
	{
		return m_data + _Size;
	}

	constexpr const_pointer unchecked_end() const noexcept
	{
		return m_data + _Size;
	}

	_Ty m_data[_Size];
private:
	void throw_oran()
	{
		std::_Xout_of_range("array subscript out of range");
	}
};

template <class _Ty>
class array<_Ty, 0>
{
public:
	using value_type				= _Ty;
	using size_type					= size_t;
	using difference_type			= ptrdiff_t;
	using pointer					= _Ty*;
	using const_pointer				= const _Ty*;
	using reference					= _Ty&;
	using const_reference			= const _Ty&;
	using iterator					= array_iterator<_Ty>;
	using const_iterator			= const_array_iterator<_Ty>;
	using reverse_iterator			= _WHIP reverse_iterator<array_iterator<_Ty>>;
	using const_reverse_iterator	= _WHIP reverse_iterator<const_array_iterator<_Ty>>;

	constexpr reference operator[](size_type index)
	{
		return *data();
	}

	constexpr const_reference operator[](size_type index) const
	{
		return *data();
	}

	constexpr size_type size() const noexcept
	{
		return 0;
	}

	constexpr size_type max_size() const noexcept
	{
		return 0;
	}

	constexpr iterator begin() noexcept
	{
		return iterator{};
	}

	constexpr iterator end() noexcept
	{
		return iterator{};
	}

	constexpr const_iterator begin() const noexcept
	{
		return const_iterator{};
	}

	constexpr const_iterator end() const noexcept
	{
		return const_iterator{};
	}

	constexpr const_iterator cbegin() const noexcept
	{
		return const_iterator{};
	}

	constexpr const_iterator cend() const noexcept
	{
		return const_iterator{};
	}

	constexpr reverse_iterator rbegin() noexcept
	{
		return reverse_iterator{};
	}

	constexpr reverse_iterator rend() noexcept
	{
		return reverse_iterator{};
	}

	constexpr const_reverse_iterator rbegin() const noexcept
	{
		return const_reverse_iterator{};
	}

	constexpr const_reverse_iterator rend() const noexcept
	{
		return const_reverse_iterator{};
	}

	constexpr const_reverse_iterator crbegin() const noexcept
	{
		return const_reverse_iterator{};
	}

	constexpr const_reverse_iterator crend() const noexcept
	{
		return const_reverse_iterator{};
	}

	constexpr pointer data() noexcept
	{
		return nullptr;
	}

	constexpr const_pointer data() const noexcept
	{
		return nullptr;
	}

	constexpr reference at(size_t index) noexcept
	{
		return *data();
	}

	constexpr const_reference at(size_t index) const noexcept
	{
		return *data();
	}

	constexpr reference front() noexcept
	{
		return *data();
	}

	constexpr const_reference front() const noexcept
	{
		return *data();
	}

	constexpr reference back() noexcept
	{
		return *data();
	}

	constexpr const_reference back() const noexcept
	{
		return *data();
	}

	constexpr bool empty() const noexcept
	{
		return true;
	}

	constexpr void fill(const_reference _Val) noexcept {}

	constexpr void swap(array& other) noexcept {}

	constexpr void copy(const array& other) noexcept {}

	constexpr pointer unchecked_begin() noexcept
	{
		return data();
	}

	constexpr const_pointer unchecked_begin() const noexcept
	{
		return data();
	}

	constexpr pointer unchecked_end() noexcept
	{
		return data();
	}

	constexpr const_pointer unchecked_end() const noexcept
	{
		return data();
	}
};

template <class _Ty, size_t _Size>
constexpr void swap(array<_Ty, _Size>& lhs, array<_Ty, _Size>& rhs)
{
	lhs.swap(rhs);
}

template <class _Ty, size_t _Size>
struct tuple_size<array<_Ty, _Size>> : integral_constant<size_t, _Size> {}; 

template <size_t _Idx, class _Ty, size_t _Size>
struct tuple_element<_Idx, array<_Ty, _Size>> 
{
	static_assert(_Idx < _Size, "array index out of bounds");
	using type = _Ty;
};


_WHIP_END