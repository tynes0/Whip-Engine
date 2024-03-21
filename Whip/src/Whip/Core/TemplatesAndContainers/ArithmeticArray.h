#pragma once

#include <stdexcept>
#include <cstring>

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/TemplatesAndContainers/TypeTraits.h>    
#include <Whip/Core/TemplatesAndContainers/Utility.h>
#include <Whip/Core/TemplatesAndContainers/Iterator.h>
#include <Whip/Core/TemplatesAndContainers/Concepts.h>

#if !defined(__cpp_concepts)
#define WHP_REQUIRE_ARITHMETIC(T) requires whip::arithmetic<T>
#define WHP_ENABLE_IF_ARITHMETIC(T)
#else // !(__cpp_concepts)
#define WHP_REQUIRE_ARITHMETIC(T)
#define WHP_ENABLE_IF_ARITHMETIC(T) , whip::enable_if_t<std::is_arithmetic_v<T>, int> = 0
#endif // __cpp_concepts

_WHIP_START

template <class _Ty WHP_ENABLE_IF_ARITHMETIC(_Ty)>
WHP_REQUIRE_ARITHMETIC(_Ty)
class arithmetic_array_iterator : public iterator_base<_Ty>
{
public:
	using m_base		= iterator_base<_Ty>;
    using value_type	= _Ty;
    using pointer		= _Ty*;
    using reference		= _Ty&;
    using size_type		= typename m_base::size_type;
    using diff_type		= typename m_base::diff_type;

    template <class _Ty>
    friend class const_arithmetic_array_iterator;

    arithmetic_array_iterator(pointer ptr = nullptr, size_type offset = 0) : m_ptr(ptr), m_offset(offset) {}

    arithmetic_array_iterator(const arithmetic_array_iterator& other) : m_ptr(other.m_ptr), m_offset(other.m_offset) {}

    ~arithmetic_array_iterator() {}

    reference operator*() const
    {
        return *(m_ptr + m_offset);
    }

    arithmetic_array_iterator& operator++()
    {
        ++m_offset;
        return *this;
    }

    arithmetic_array_iterator operator++(int)
    {
        arithmetic_array_iterator temp(*this);
        ++(*this);
        return temp;
    }

    arithmetic_array_iterator& operator--()
    {
        --m_offset;
        return *this;
    }

    arithmetic_array_iterator operator--(int)
    {
        arithmetic_array_iterator temp(*this);
        --(*this);
        return temp;
    }

    bool operator==(const arithmetic_array_iterator& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator==(arithmetic_array_iterator&& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator!=(const arithmetic_array_iterator& other)
    {
        return !(this->operator==(move(other)));
    }

    arithmetic_array_iterator operator+(size_type n) const
    {
        return arithmetic_array_iterator(m_ptr, m_offset + n);
    }

    size_type operator+(const arithmetic_array_iterator& other) const
    {
        pointer other_ptr = other.m_ptr + other.m_offset;
        pointer ptr = m_ptr + m_offset;
        return ptr + other_ptr;
    }

    arithmetic_array_iterator operator-(size_type n) const
    {
        return arithmetic_array_iterator(m_ptr, m_offset - n);
    }

    size_type operator-(const arithmetic_array_iterator& other) const
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

    const pointer unwrapped() const
    {
        return (m_ptr + m_offset);
    }

private:
    pointer m_ptr;
    size_type m_offset;
};

template <class _Ty WHP_ENABLE_IF_ARITHMETIC(_Ty)>
WHP_REQUIRE_ARITHMETIC(_Ty)
class const_arithmetic_array_iterator : public iterator_base<const _Ty>
{
public:
    using m_base		= iterator_base<const _Ty>;
    using value_type	= const _Ty;
    using pointer		= const _Ty* const;
    using reference		= const _Ty&;
    using size_type		= typename m_base::size_type;
    using diff_type		= typename m_base::diff_type;


    const_arithmetic_array_iterator(pointer ptr = nullptr, size_type offset = 0)
        : m_ptr(ptr), m_offset(offset)
    {}

    const_arithmetic_array_iterator(const arithmetic_array_iterator<_Ty>& other)
        : m_ptr(other.m_ptr), m_offset(other.m_offset)
    {}

    ~const_arithmetic_array_iterator() {}

    reference operator*() const
    {
        return *(m_ptr + m_offset);
    }

    const_arithmetic_array_iterator& operator++()
    {
        ++m_offset;
        return *this;
    }

    const_arithmetic_array_iterator operator++(int)
    {
        const_arithmetic_array_iterator temp(*this);
        ++(*this);
        return temp;
    }

    const_arithmetic_array_iterator& operator--()
    {
        --m_offset;
        return *this;
    }

    const_arithmetic_array_iterator operator--(int)
    {
        const_arithmetic_array_iterator temp(*this);
        --(*this);
        return temp;
    }

    bool operator==(const const_arithmetic_array_iterator& other) const
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator==(const_arithmetic_array_iterator&& other) const
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator!=(const const_arithmetic_array_iterator& other) const
    {
        return !(this->operator==(move(other)));
    }

    const_arithmetic_array_iterator operator+(size_type n) const
    {
        return const_arithmetic_array_iterator(m_ptr, m_offset + n);
    }

    const_arithmetic_array_iterator operator-(size_type n) const
    {
        return const_arithmetic_array_iterator(m_ptr, m_offset - n);
    }

    size_type operator-(const const_arithmetic_array_iterator& other) const
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

    pointer unwrapped() const
    {
        return (m_ptr + m_offset);
    }

private:
    pointer m_ptr;
    size_type m_offset;
};

template <class _Ty>
WHP_REQUIRE_ARITHMETIC(_Ty)
struct pointer_traits<arithmetic_array_iterator<_Ty>>
{
	using pointer = arithmetic_array_iterator<_Ty>;
    using element_type = const _Ty;
    using difference_type = ptrdiff_t;

    WHP_NODISCARD static constexpr element_type* to_address(const pointer iter) noexcept
    {
        return iter.unwrapped();
    }
};

template <class _Ty>
WHP_REQUIRE_ARITHMETIC(_Ty)
struct pointer_traits<const_arithmetic_array_iterator<_Ty>>
{
    using pointer = const_arithmetic_array_iterator<_Ty>;
    using element_type = const _Ty;
    using difference_type = ptrdiff_t;

    WHP_NODISCARD static constexpr element_type* to_address(const pointer iter) noexcept
    {
        return iter.unwrapped();
    }
};

namespace detail_arithmetic_array
{
	template <class _Ty1, class _Ty2 WHP_ENABLE_IF_ARITHMETIC(_Ty1)>
	WHP_REQUIRE_ARITHMETIC(_Ty)
	class arithmetic_array_base {};
}


template <class _Ty, size_t _Size>
WHP_REQUIRE_ARITHMETIC(_Ty)
class arithmetic_array : private detail_arithmetic_array::arithmetic_array_base<_Ty, whip::integral_constant<size_t, _Size>>
{
public:
	using value_type				= _Ty;
	using size_type					= size_t;
	using difference_type			= ptrdiff_t;
	using pointer					= _Ty*;
	using const_pointer				= const _Ty*;
	using reference					= _Ty&;
	using const_reference			= const _Ty&;
	using iterator					= arithmetic_array_iterator<_Ty>;
	using const_iterator			= const_arithmetic_array_iterator<_Ty>;
	using reverse_iterator			= _WHIP reverse_iterator<arithmetic_array_iterator<_Ty>>;
	using const_reverse_iterator	= _WHIP reverse_iterator<const_arithmetic_array_iterator<_Ty>>;

	constexpr reference operator[](size_type index) noexcept
	{
		WHP_CORE_ASSERT(index < _Size, "arithmetic_array subscript out of range");
		return *(m_data + index);
	}

	constexpr const_reference operator[](size_type index) const noexcept
	{
		WHP_CORE_ASSERT(index < _Size, "arithmetic_array subscript out of range");
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

	constexpr void swap(arithmetic_array& right) noexcept
	{
		if (addressof(right) != this)
		{
			_WHIP swap(m_data, right.m_data);
		}
	}

	constexpr void copy(const arithmetic_array& other) noexcept
	{
		if (addressof(other) != this)
		{
			::memcpy(m_data, other.m_data, _Size * sizeof(_Ty));
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
		std::_Xout_of_range("arithmetic_array subscript out of range");
	}
};

template <class _Ty>
WHP_REQUIRE_ARITHMETIC(_Ty)
class arithmetic_array<_Ty, 0>
{
public:
	using value_type				= _Ty;
	using size_type					= size_t;
	using difference_type			= ptrdiff_t;
	using pointer					= _Ty*;
	using const_pointer				= const _Ty*;
	using reference					= _Ty&;
	using const_reference			= const _Ty&;
	using iterator					= arithmetic_array_iterator<_Ty>;
	using const_iterator			= const_arithmetic_array_iterator<_Ty>;
	using reverse_iterator			= _WHIP reverse_iterator<arithmetic_array_iterator<_Ty>>;
	using const_reverse_iterator	= _WHIP reverse_iterator<const_arithmetic_array_iterator<_Ty>>;

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

	constexpr void swap(arithmetic_array& other) noexcept {}

	constexpr void copy(const arithmetic_array& other) noexcept {}

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
constexpr void swap(arithmetic_array<_Ty, _Size>& lhs, arithmetic_array<_Ty, _Size>& rhs)
{
	lhs.swap(rhs);
}

template <class _Ty, size_t _Size>
struct tuple_size<arithmetic_array<_Ty, _Size>> : integral_constant<size_t, _Size> {};

template <size_t _Idx, class _Ty, size_t _Size>
struct tuple_element<_Idx, arithmetic_array<_Ty, _Size>>
{
	static_assert(_Idx < _Size, "arithmetic_array index out of bounds");
	using type = _Ty;
};

_WHIP_END

#undef WHP_REQUIRE_ARITHMETIC
#undef WHP_ENABLE_IF_ARITHMETIC