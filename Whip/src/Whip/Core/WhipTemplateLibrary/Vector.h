#pragma once
#ifndef _WHIP_VECTOR_
#define _WHIP_VECTOR_

#include "Whip/Core/Core.h"
#include "Whip/Core/Log.h"

#include "Iterator.h"
#include "Pair.h"
#include "Utility.h"
#include "MemoryUtil.h"
#include "Algorithms.h"

#include <cstring>
#include <initializer_list>

#pragma warning(push)
#pragma warning(disable : _WHP_DISABLED_WARNINGS)

_WHIP_START

template <class _Ty>
class vector_iterator : public iterator_base<_Ty>
{
public:
    using m_base        = iterator_base<_Ty>;
    using value_type    = _Ty;
    using pointer       = _Ty*;
    using reference     = _Ty&;
    using size_type     = typename m_base::size_type;
    using diff_type     = typename m_base::diff_type;

    template <class _Ty>
    friend class const_vector_iterator;

    WHP_CONSTEXPR17 vector_iterator(pointer ptr = nullptr, size_type offset = 0) : m_ptr(ptr), m_offset(offset) {}

    WHP_CONSTEXPR17 vector_iterator(const vector_iterator& other) : m_ptr(other.m_ptr), m_offset(other.m_offset) {}

    WHP_CONSTEXPR17 ~vector_iterator() {}

    WHP_CONSTEXPR17 reference operator*() const
    {
        return *(m_ptr + m_offset);
    }

    WHP_CONSTEXPR17 vector_iterator& operator++()
    {
        ++m_offset;
        return *this;
    }

    WHP_CONSTEXPR17 vector_iterator operator++(int)
    {
        vector_iterator temp(*this);
        ++(*this);
        return temp;
    }

    WHP_CONSTEXPR17 vector_iterator& operator--()
    {
        --m_offset;
        return *this;
    }

    WHP_CONSTEXPR17 vector_iterator operator--(int)
    {
        vector_iterator temp(*this);
        --(*this);
        return temp;
    }

    WHP_CONSTEXPR17 bool operator==(const vector_iterator& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    WHP_CONSTEXPR17 bool operator==(vector_iterator&& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    WHP_CONSTEXPR17 bool operator!=(const vector_iterator& other)
    {
        return !(this->operator==(move(other)));
    }

    WHP_CONSTEXPR17 vector_iterator operator+(size_type n) const
    {
        return vector_iterator(m_ptr, m_offset + n);
    }

    WHP_CONSTEXPR17 size_type operator+(const vector_iterator& other) const
    {
        pointer other_ptr = other.m_ptr + other.m_offset;
        pointer ptr = m_ptr + m_offset;
        return ptr + other_ptr;
    }

    WHP_CONSTEXPR17 vector_iterator operator-(size_type n) const
    {
        return vector_iterator(m_ptr, m_offset - n);
    }

    WHP_CONSTEXPR17 size_type operator-(const vector_iterator& other) const
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

    constexpr const pointer unwrapped() const
    {
        return (m_ptr + m_offset);
    }

    constexpr pointer unwrapped()
    {
        return (m_ptr + m_offset);
    }

private:
    pointer m_ptr;
    size_type m_offset;
};

template <class _Ty>
class const_vector_iterator : public iterator_base<const _Ty>
{
public:
    using m_base        = iterator_base<const _Ty>;
    using value_type    = const _Ty;
    using pointer       = const _Ty* const;
    using reference     = const _Ty&;
    using size_type     = typename m_base::size_type;
    using diff_type     = typename m_base::diff_type;


    WHP_CONSTEXPR17 const_vector_iterator(pointer ptr = nullptr, size_type offset = 0)
        : m_ptr(ptr), m_offset(offset)
    {}

    WHP_CONSTEXPR17 const_vector_iterator(const vector_iterator<_Ty>& other)
        : m_ptr(other.m_ptr), m_offset(other.m_offset)
    {}

    WHP_CONSTEXPR17 ~const_vector_iterator() {}

    WHP_CONSTEXPR17 reference operator*() const
    {
        return *(m_ptr + m_offset);
    }

    WHP_CONSTEXPR17 const_vector_iterator& operator++()
    {
        ++m_offset;
        return *this;
    }

    WHP_CONSTEXPR17 const_vector_iterator operator++(int)
    {
        const_vector_iterator temp(*this);
        ++(*this);
        return temp;
    }

    WHP_CONSTEXPR17 const_vector_iterator& operator--()
    {
        --m_offset;
        return *this;
    }

    WHP_CONSTEXPR17 const_vector_iterator operator--(int)
    {
        const_vector_iterator temp(*this);
        --(*this);
        return temp;
    }

    WHP_CONSTEXPR17 bool operator==(const const_vector_iterator& other) const
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    WHP_CONSTEXPR17 bool operator==(const_vector_iterator&& other) const
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    WHP_CONSTEXPR17 bool operator!=(const const_vector_iterator& other) const
    {
        return !(this->operator==(move(other)));
    }

    WHP_CONSTEXPR17 const_vector_iterator operator+(size_type n) const
    {
        return const_vector_iterator(m_ptr, m_offset + n);
    }

    WHP_CONSTEXPR17 const_vector_iterator operator-(size_type n) const
    {
        return const_vector_iterator(m_ptr, m_offset - n);
    }

    WHP_CONSTEXPR17 size_type operator-(const const_vector_iterator& other) const
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

    constexpr pointer unwrapped() const
    {
        return (m_ptr + m_offset);
    }

private:
    pointer m_ptr;
    size_type m_offset;
};

template <class _Ty>
struct pointer_traits<vector_iterator<_Ty>>
{
    using pointer = vector_iterator<_Ty>;
    using element_type = const _Ty;
    using difference_type = ptrdiff_t;

    WHP_NODISCARD static constexpr element_type* to_address(const pointer iter) noexcept
    {
        return iter.unwrapped();
    }
};

template <class _Ty>
struct pointer_traits<const_vector_iterator<_Ty>>
{
    using pointer = const_vector_iterator<_Ty>;
    using element_type = const _Ty;
    using difference_type = ptrdiff_t;

    WHP_NODISCARD static constexpr element_type* to_address(const pointer iter) noexcept
    {
        return iter.unwrapped();
    }
};

template <class _Ty>
class vector
{
public:
    using value_type                = _Ty;
    using size_type                 = size_t;
    using difference_type           = ptrdiff_t;
    using pointer                   = _Ty*;
    using const_pointer             = const _Ty*;
    using reference                 = _Ty&;
    using const_reference           = const _Ty&;
    using iterator                  = vector_iterator<_Ty>;
    using const_iterator            = const_vector_iterator<_Ty>;
    using reverse_iterator          = _WHIP reverse_iterator<vector_iterator<_Ty>>;
    using const_reverse_iterator    = _WHIP reverse_iterator<const_vector_iterator<_Ty>>;

    WHP_CONSTEXPR vector() = default;

    WHP_CONSTEXPR vector(size_t size)
    {
        resize(size);
    }

    WHP_CONSTEXPR vector(const vector& other) : m_capacities(other.m_capacities)
    {
        allocator<_Ty> al;
        m_data = al.allocate(other.m_capacities.first);
#if _WHP_HAS_CPP_VERSION(17)
        if constexpr (is_trivial_v<_Ty>)
#else // _WHP_HAS_CPP_VERSION(17)
        if (is_trivial_v<_Ty>)
#endif // _WHP_HAS_CPP_VERSION(17)
            std::memcpy(m_data, other.m_data, other.m_capacities.second * sizeof(_Ty));
        else
            memory::copy_range(other.begin().unwrapped(), other.end().unwrapped(), m_data);
    }

    WHP_CONSTEXPR vector(vector&& other) noexcept : m_data(other.m_data), m_capacities(other.m_capacities)
    {
        other.m_data = nullptr;
    }

    WHP_CONSTEXPR vector(const std::initializer_list<_Ty>& ilist) : m_capacities({ ilist.size(), ilist.size() })
    {
        allocator<_Ty> al;
        m_data = al.allocate(ilist.size());
        if constexpr (is_trivial_v<_Ty>)
            std::memcpy(m_data, ilist.begin(), ilist.size() * sizeof(_Ty));
        else
            memory::copy_range(ilist.begin(), ilist.end(), m_data);
    }

    WHP_CONSTEXPR vector& operator=(const vector& right)
    {
        m_capacities = right.m_capacities;
        allocator<_Ty> al;
        m_data = al.allocate(right.m_capacities.first);

        if constexpr (is_trivial_v<_Ty>)
            std::memcpy(m_data, right.m_data, right.m_capacities.second * sizeof(_Ty));
        else
            memory::copy_range(right.begin().unwrapped(), right.end().unwrapped(), m_data);
        return *this;
    }

    WHP_CONSTEXPR vector& operator=(vector&& right) noexcept
    {
        m_data = right.m_data;
        m_capacities = right.m_capacities;
        right.m_data = nullptr;
        return *this;
    }

    WHP_CONSTEXPR ~vector()
    {
#if _WHP_HAS_CPP_VERSION(17)
        if constexpr (is_trivial_v<_Ty>)
#else // _WHP_HAS_CPP_VERSION(17)
        if (is_trivial_v<_Ty>)
#endif // _WHP_HAS_CPP_VERSION(17)
            memory::destruct_range(begin().unwrapped(), end().unwrapped());
        allocator<_Ty>{}.deallocate(m_data, 1);
    }

    WHP_CONSTEXPR _Ty& operator[](size_t idx)
    {
        WHP_CORE_ASSERT(idx < m_capacities.second,"Index out of the range. (vector)");
        return m_data[idx];
    }

    WHP_CONSTEXPR const _Ty& operator[](size_t idx) const
    {
        WHP_CORE_ASSERT(idx < m_capacities.second, "Index out of the range. (vector)");
        return m_data[idx];
    }

    WHP_CONSTEXPR _Ty& front()
    {
        WHP_CORE_ASSERT(0 < m_capacities.second, "Container is empty. (vector)");
        return m_data[0];
    }

    WHP_CONSTEXPR const _Ty& front() const
    {
        WHP_CORE_ASSERT(0 < m_capacities.second, "Container is empty. (vector)");
        return m_data[0];
    }

    WHP_CONSTEXPR _Ty& back()
    {
        WHP_CORE_ASSERT(0 < m_capacities.second, "Container is empty. (vector)");
        return m_data[m_capacities.second - 1];
    }

    WHP_CONSTEXPR const _Ty& back() const
    {
        WHP_CORE_ASSERT(0 < m_capacities.second, "Container is empty. (vector)");
        return m_data[m_capacities.second - 1];
    }

    WHP_CONSTEXPR _Ty* data()
    {
        return m_data;
    }

    WHP_CONSTEXPR const _Ty* data() const
    {
        return m_data;
    }

    WHP_CONSTEXPR bool empty() const noexcept
    {
        return m_capacities.second == 0;
    }

    WHP_CONSTEXPR size_type size() const noexcept
    {
        return m_capacities.second;
    }

    WHP_CONSTEXPR size_type capacity() const noexcept
    {
        return m_capacities.first;
    }

    WHP_CONSTEXPR void reserve(size_t new_cap)
    {
        WHP_CORE_ASSERT(new_cap >= m_capacities.first, "Capacity is already greater than the passed value. (vector)");
        if (new_cap == m_capacities.first)
        {
            WHP_CORE_WARN("Capacity is already equal to the passed value. (vector)");
            return;
        }
        if constexpr (is_trivial_v<_Ty>)
        {
            m_data = reinterpret_cast<_Ty*>(std::realloc(m_data, sizeof(_Ty) * new_cap));
            WHP_CORE_ASSERT(m_data != nullptr, "Reallocation failed.");
        }
        else
        {
            auto ufirst = begin().unwrapped();
            auto ulast  = end().unwrapped();
            allocator<_Ty> al;
            _Ty* new_data = al.allocate(new_cap);
            memory::copy_range(ufirst, ulast, new_data);
            memory::destruct_range(ufirst, ulast);
            allocator<_Ty>{}.deallocate(m_data, 1);
            m_data = new_data;
        }
        m_capacities.first = new_cap;
    }

    WHP_CONSTEXPR void shrink_to_fit()
    {
        if (m_capacities.first > m_capacities.second)
        {
            if constexpr (is_trivial_v<_Ty>)
            {
                m_data = reinterpret_cast<_Ty*>(std::realloc(m_data, sizeof(_Ty) * m_capacities.second));
            }
            else
            {
                auto ufirst = begin().unwrapped();
                auto ulast  = end().unwrapped();
                allocator<_Ty> al;
                _Ty* new_data = al.allocate(m_capacities.second);
                memory::copy_range(ufirst, ulast, new_data);
                memory::destruct_range(ufirst, ulast);
                allocator<_Ty>{}.deallocate(m_data, 1);
                m_data = new_data;
            }
            m_capacities.first = m_capacities.second;
        }
    }

    WHP_CONSTEXPR void clear() noexcept
    {
        if constexpr (!is_trivial_v<_Ty>)
            memory::destruct_range(begin().unwrapped(), end().unwrapped());
        m_capacities.second = 0;
    }

    WHP_CONSTEXPR void erase(iterator position)
    {
        auto ufirst = begin().unwrapped();
        auto ulast  = end().unwrapped();
        auto upos   = position.unwrapped();

        WHP_CORE_ASSERT(upos >= ufirst && upos < ulast, "Position is not within the vector.");
        if (upos + 1 < ulast)
            std::memmove(upos, upos + 1, ulast - upos - 1 * sizeof(_Ty));
        if constexpr (!is_trivial_v<_Ty>)
            allocator<_Ty>{}.destroy(ulast);
        m_capacities.second--;
    }

    WHP_CONSTEXPR void erase(iterator start, iterator last)
    {
        auto ufirst     = begin().unwrapped();
        auto ulast      = end().unwrapped();
        auto ufirst2    = start.unwrapped();
        auto ulast2     = last.unwrapped();

        WHP_CORE_ASSERT(ufirst2 >= ufirst && ufirst2 < ulast  && ulast2 <= ulast, "Invalid range.");
        size_type distance = last - start;
        if (static_cast<size_t>(ulast - ufirst2) > 0)
            std::memmove(ufirst2, ufirst2 + distance, static_cast<size_t>(m_capacities.second - distance) * sizeof(_Ty));
        for (size_type i = 0; i < distance; i++)
            if constexpr (!is_trivial_v<_Ty>)
                allocator<_Ty>{}.destroy(ulast - i - 1);
        m_capacities.second -= distance;
    }

    WHP_CONSTEXPR void erase(size_t offset)
    {
        WHP_CORE_ASSERT(offset < m_capacities.second, "Passed value is equal or greater than the size. (vector)");
        this->erase(begin() + offset);
    }

    WHP_CONSTEXPR iterator insert(iterator position, const _Ty& value)
    {
        ptrdiff_t diff = position.unwrapped() - begin().unwrapped();

        if (m_capacities.first == m_capacities.second)
            reserve(m_capacities.first * m_grow_factor + 1);

        auto ufirst = begin().unwrapped();
        auto ulast  = end().unwrapped();
        auto upos   = begin().unwrapped() + diff;

        WHP_CORE_ASSERT(upos >= ufirst && upos <= ulast, "Position is not within the vector.");

        std::memmove(upos + 1, upos, (ulast - upos) * sizeof(_Ty));

        if constexpr (!is_trivial_v<_Ty>)
            allocator<_Ty>{}.construct(upos, value);
        else
            *upos = value;

        m_capacities.second++;

        return iterator(upos, m_capacities.second);
    }

    WHP_CONSTEXPR void insert(iterator position, size_t count, const _Ty& value)
    {
        ptrdiff_t diff = position.unwrapped() - begin().unwrapped();

        if (m_capacities.first == m_capacities.second)
            reserve(m_capacities.first * m_grow_factor + 1);

        auto ufirst = begin().unwrapped();
        auto ulast = end().unwrapped();
        auto upos = begin().unwrapped() + diff;

        WHP_CORE_ASSERT(upos >= ufirst && upos <= ulast, "Position is not within the vector.");

        size_t distance = ulast - upos;
        if (distance > 0)
            std::memmove(upos + count, upos, distance * sizeof(_Ty));

        for (size_t i = 0; i < count; i++)
        {
            if constexpr (!is_trivial_v<_Ty>)
                allocator<_Ty>{}.construct(upos + i, value);
            else
                *(upos + i) = value;
        }

        m_capacities.second += count;
    }

    template <class _InputIter>
    WHP_CONSTEXPR void insert(iterator position, _InputIter first, _InputIter last)
    {
        ptrdiff_t diff = position.unwrapped() - begin().unwrapped();

        if (first == last)
            return;

        size_t count = distance(first, last);
        if (m_capacities.first == m_capacities.second)
            reserve(m_capacities.first * m_grow_factor + count);

        auto ufirst = begin().unwrapped();
        auto ulast  = end().unwrapped();
        auto upos = begin().unwrapped() + diff;

        WHP_CORE_ASSERT(upos >= ufirst && upos <= ulast, "Position is not within the vector.");

        size_t distance = ulast - upos;
        if (distance > 0)
            std::memmove(upos + count, upos, distance * sizeof(_Ty));

        for (size_t i = 0; first != last; ++first, ++i)
        {
            if constexpr (!is_trivial_v<_Ty>)
                allocator<_Ty>{}.construct(upos + i, *first);
            else
                *(upos + i) = *first;
        }

        m_capacities.second += count;
    }

    WHP_CONSTEXPR void push_back(const _Ty& value)
    {
        if (m_capacities.first == m_capacities.second)
            reserve(m_capacities.first * m_grow_factor + 1);
        if constexpr (is_trivial_v<_Ty>)
            m_data[m_capacities.second] = value;
        else
            allocator<_Ty>{}.construct(m_data + m_capacities.second, value);
        m_capacities.second++;
    }

    WHP_CONSTEXPR void push_back(_Ty&& value)
    {
        if (m_capacities.first == m_capacities.second)
            reserve(m_capacities.first * m_grow_factor + 1);
        if constexpr (is_trivial_v<_Ty>)
            m_data[m_capacities.second] = value;
        else
            allocator<_Ty>{}.construct(m_data + m_capacities.second, whip::move(value));
        m_capacities.second++;
    }

    template <class... _Args>
    WHP_CONSTEXPR iterator emplace(iterator position, _Args&&... args)
    {
        static_assert(!is_trivial_v<_Ty>, "Use insert() instead of emplace() with trivial types.");
        ptrdiff_t diff = position.unwrapped() - begin().unwrapped();

        if (m_capacities.first == m_capacities.second)
            reserve(m_capacities.first * m_grow_factor + 1);

        auto ufirst = begin().unwrapped();
        auto ulast = end().unwrapped();
        auto upos = begin().unwrapped() + diff;

        WHP_CORE_ASSERT(upos >= ufirst && upos <= ulast, "Position is not within the vector.");

        size_type index = upos - ufirst;

        if (index < m_capacities.second)
            std::move_backward(m_data + index, m_data + m_capacities.second, m_data + m_capacities.first);

        allocator<_Ty>{}.construct(m_data + index, _WHIP forward<_Args>(args)...);

        m_capacities.second++;

        return iterator(m_data + index, m_capacities.second);
    }
    
    template <class... _Args>
    WHP_CONSTEXPR void emplace_back(_Args&&... args)
    {
        static_assert(!is_trivial_v<_Ty>, "Use push_back() instead of emplace_back() with trivial types.");

        if (m_capacities.first == m_capacities.second)
            reserve(m_capacities.first * m_grow_factor + 1);
        allocator<_Ty>{}.construct(m_data + m_capacities.second, whip::forward<_Args>(args)...);
        m_capacities.second++;
    }

    WHP_CONSTEXPR void pop_back()
    {
        WHP_CORE_ASSERT(m_capacities.second > 0, "Container is empty. (vector)");
        if constexpr (is_trivial_v<_Ty>)
            allocator<_Ty>{}.destroy(&m_data[m_capacities.second - 1]);
        m_capacities.second--;
    }

    WHP_CONSTEXPR void resize(size_t new_size)
    {
        if (new_size == m_capacities.second)
        {
            WHP_CORE_WARN("Size is already equal to the passed value. (vector)");
            return;
        }

        if (new_size > m_capacities.first)
            reserve(new_size);

        if constexpr (is_trivial_v<_Ty>)
        {
            if (new_size > m_capacities.second)
                memory::construct_range(m_data + m_capacities.second, m_data + new_size);
            else
                memory::destruct_range(m_data + new_size, m_data + m_capacities.second);
        }
        m_capacities.second = new_size;
    }

    WHP_CONSTEXPR bool operator==(const vector& right) const noexcept
    {
        if (m_capacities.second != right.m_capacities.second)
            return false;
        if (m_capacities.second == 0)
            return true;
        auto ufirst1 = begin().unwrapped();
        auto ufirst2 = right.begin().unwrapped();
        const auto ulast = end().unwrapped();
        for (; ufirst1 != ulast; ++ufirst1, ++ufirst2)
            if (DREF(ufirst1) != DREF(ufirst2))
                return false;
        return true;
    }

    WHP_CONSTEXPR bool operator!=(const vector& right) const noexcept
    {
        return !this->operator==(right);
    }

    WHP_CONSTEXPR void swap(vector& right) noexcept
    {
        if (addressof(right) != this)
        {
            whip::swap_nt(m_data, right.m_data);
            whip::swap_nt(m_grow_factor, right.m_grow_factor);
            m_capacities.swap(right.m_capacities);
        }
    }

    // max grow_factor is equal to 10
    WHP_CONSTEXPR void set_grow_factor(size_type grow_factor = 2) noexcept
    {
        constexpr size_type max_grow_factor = 10;
        if (grow_factor < max_grow_factor)
            m_grow_factor = grow_factor;
        else
        {
            WHP_CORE_WARN("Maximum growth factor has been exceeded. Growth factor set equal to maximum value ({0}). (vector)", max_grow_factor);
            m_grow_factor = max_grow_factor;
        }
    }

    WHP_CONSTEXPR size_type get_grow_factor() const noexcept
    {
        m_grow_factor;
    }

    WHP_CONSTEXPR iterator begin() noexcept
    {
        return iterator(m_data);
    }

    WHP_CONSTEXPR iterator end() noexcept
    {
        return iterator(m_data, m_capacities.second);
    }

    WHP_CONSTEXPR const_iterator begin() const noexcept
    {
        return const_iterator(m_data);
    }

    WHP_CONSTEXPR const_iterator end() const noexcept
    {
        return const_iterator(m_data, m_capacities.second);
    }

    WHP_CONSTEXPR const_iterator cbegin() const noexcept
    {
        return const_iterator(m_data);
    }

    WHP_CONSTEXPR const_iterator cend() const noexcept
    {
        return const_iterator(m_data, m_capacities.second);
    }

    WHP_CONSTEXPR reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }

    WHP_CONSTEXPR reverse_iterator rend() noexcept
    {
        return reverse_iterator(begin());
    }

    WHP_CONSTEXPR const_reverse_iterator rbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }

    WHP_CONSTEXPR const_reverse_iterator rend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }

    WHP_CONSTEXPR const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }

    WHP_CONSTEXPR const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }

private:
    _Ty* m_data = nullptr;
    pair<size_t> m_capacities = { 0 , 0 }; // first -> capacity, second -> size
    size_type m_grow_factor = 2;
};

template <class _Ty>
WHP_INLINE WHP_CONSTEXPR void swap(vector<_Ty>& lhs, vector < _Ty>& rhs) noexcept
{
    lhs.swap(rhs);
}

_WHIP_END

#pragma warning(pop)

#endif // !_WHIP_VECTOR_