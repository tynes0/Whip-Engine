#pragma once

#include "Whip/Core/Core.h"
#include "Whip/Core/Log.h"
#include "Iterator.h"
#include "Pair.h"
#include "MemoryUtil.h"
#include "Utility.h"

#include <cstring>
#include <initializer_list>

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

    vector_iterator(pointer ptr = nullptr, size_type offset = 0) : m_ptr(ptr), m_offset(offset) {}

    vector_iterator(const vector_iterator& other) : m_ptr(other.m_ptr), m_offset(other.m_offset) {}

    ~vector_iterator() {}

    reference operator*() const
    {
        return *(m_ptr + m_offset);
    }

    vector_iterator& operator++()
    {
        ++m_offset;
        return *this;
    }

    vector_iterator operator++(int)
    {
        vector_iterator temp(*this);
        ++(*this);
        return temp;
    }

    vector_iterator& operator--()
    {
        --m_offset;
        return *this;
    }

    vector_iterator operator--(int)
    {
        vector_iterator temp(*this);
        --(*this);
        return temp;
    }

    bool operator==(const vector_iterator& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator==(vector_iterator&& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator!=(const vector_iterator& other)
    {
        return !(this->operator==(move(other)));
    }

    vector_iterator operator+(size_type n) const
    {
        return vector_iterator(m_ptr, m_offset + n);
    }

    size_type operator+(const vector_iterator& other) const
    {
        pointer other_ptr = other.m_ptr + other.m_offset;
        pointer ptr = m_ptr + m_offset;
        return ptr + other_ptr;
    }

    vector_iterator operator-(size_type n) const
    {
        return vector_iterator(m_ptr, m_offset - n);
    }

    size_type operator-(const vector_iterator& other) const
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
class const_vector_iterator : public iterator_base<const _Ty>
{
public:
    using m_base        = iterator_base<const _Ty>;
    using value_type    = const _Ty;
    using pointer       = const _Ty* const;
    using reference     = const _Ty&;
    using size_type     = typename m_base::size_type;
    using diff_type     = typename m_base::diff_type;


    const_vector_iterator(pointer ptr = nullptr, size_type offset = 0)
        : m_ptr(ptr), m_offset(offset)
    {}

    const_vector_iterator(const vector_iterator<_Ty>& other)
        : m_ptr(other.m_ptr), m_offset(other.m_offset)
    {}

    ~const_vector_iterator() {}

    reference operator*() const
    {
        return *(m_ptr + m_offset);
    }

    const_vector_iterator& operator++()
    {
        ++m_offset;
        return *this;
    }

    const_vector_iterator operator++(int)
    {
        const_vector_iterator temp(*this);
        ++(*this);
        return temp;
    }

    const_vector_iterator& operator--()
    {
        --m_offset;
        return *this;
    }

    const_vector_iterator operator--(int)
    {
        const_vector_iterator temp(*this);
        --(*this);
        return temp;
    }

    bool operator==(const const_vector_iterator& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator==(const_vector_iterator&& other)
    {
        return ((m_ptr + m_offset) == (other.m_ptr + other.m_offset));
    }

    bool operator!=(const const_vector_iterator& other)
    {
        return !(this->operator==(move(other)));
    }

    const_vector_iterator operator+(size_type n) const
    {
        return const_vector_iterator(m_ptr, m_offset + n);
    }

    const_vector_iterator operator-(size_type n) const
    {
        return const_vector_iterator(m_ptr, m_offset - n);
    }

    size_type operator-(const const_vector_iterator& other) const
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

    vector() = default;

    vector(size_t size)
    {
        resize(size);
    }

    vector(const vector& other) : m_capacities(other.m_capacities)
    {
        allocator<_Ty> al;
        m_data = al.allocate(other.m_capacities.first);

        if constexpr (std::is_trivial_v<_Ty>)
            std::memcpy(m_data, other.m_data, other.m_capacities.second);
        else
            memory::copy_range(other.begin().unwrapped(), other.end().unwrapped(), m_data);
    }

    vector(vector&& other) noexcept : m_data(other.m_data), m_capacities(other.m_capacities)
    {
        other.m_data = nullptr;
    }

    vector(const std::initializer_list<_Ty>& ilist)
    {

    }

    vector& operator=(const vector& right)
    {
        m_capacities = right.m_capacities;
        allocator<_Ty> al;
        m_data = al.allocate(right.m_capacities.first);

        if constexpr (std::is_trivial_v<_Ty>)
            std::memcpy(m_data, right.m_data, right.m_capacities.second);
        else
            memory::copy_range(DREF(right.begin()), DREF(right.end()), m_data);
    }

    vector& operator=(vector&& right) noexcept
    {
        m_data = right.m_data;
        m_capacities = right.m_capacities;
        right.m_data = nullptr;
        return *this;
    }

    ~vector()
    {
        if constexpr (!std::is_trivial_v<_Ty>)
            memory::destruct_range(begin().unwrapped(), end().unwrapped());
        allocator<_Ty>{}.deallocate(m_data, 1);
    }

    _Ty& operator[](size_t idx)
    {
        WHP_CORE_ASSERT(idx < m_capacities.second,"Index out of the range. (vector)");
        return m_data[idx];
    }

    const _Ty& operator[](size_t idx) const
    {
        WHP_CORE_ASSERT(idx < m_capacities.second, "Index out of the range. (vector)");
        return m_data[idx];
    }

    _Ty& front()
    {
        WHP_CORE_ASSERT(0 < m_capacities.second, "Container is empty. (vector)");
        return m_data[0];
    }

    const _Ty& front() const
    {
        WHP_CORE_ASSERT(0 < m_capacities.second, "Container is empty. (vector)");
        return m_data[0];
    }

    _Ty& back()
    {
        WHP_CORE_ASSERT(0 < m_capacities.second, "Container is empty. (vector)");
        return m_data[m_capacities.second - 1];
    }

    const _Ty& back() const
    {
        WHP_CORE_ASSERT(0 < m_capacities.second, "Container is empty. (vector)");
        return m_data[m_capacities.second - 1];
    }

    _Ty* data()
    {
        return m_data;
    }

    const _Ty* data() const
    {
        return m_data;
    }

    bool empty() const noexcept
    {
        return m_capacities.second == 0;
    }

    size_type size() const noexcept
    {
        return m_capacities.second;
    }

    size_type capacity() const noexcept
    {
        return m_capacities.first;
    }

    void reserve(size_t new_cap)
    {
        WHP_CORE_ASSERT(new_cap >= m_capacities.first, "Capacity is already greater than the passed value. (vector)");
        if (new_cap == m_capacities.first)
        {
            WHP_CORE_WARN("Capacity is already equal to the passed value. (vector)");
            return;
        }
        if constexpr (std::is_trivial_v<_Ty>)
        {
            m_data = reinterpret_cast<_Ty*>(std::realloc(m_data, sizeof(_Ty) * new_cap));
            WHP_CORE_ASSERT(m_data != nullptr, "Reallocation failed.");
        }
        else
        {
            allocator<_Ty> al;
            _Ty* new_data = al.allocate(new_cap);
            memory::copy_range(begin().unwrapped(), end().unwrapped(), new_data);
            memory::destruct_range(begin().unwrapped(), end().unwrapped());
            allocator<_Ty>{}.deallocate(m_data, 1);
            m_data = new_data;
        }
        m_capacities.first = new_cap;
    }

    void shrink_to_fit()
    {
        if (m_capacities.first > m_capacities.second)
        {
            if constexpr (std::is_trivial_v<_Ty>)
            {
                m_data = reinterpret_cast<_Ty*>(std::realloc(m_data, sizeof(_Ty) * m_capacities.second));
            }
            else
            {
                allocator<_Ty> al;
                _Ty* new_data = al.allocate(m_capacities.second);
                memory::copy_range(begin().unwrapped(), end().unwrapped(), new_data);
                memory::destruct_range(begin().unwrapped(), end().unwrapped());
                allocator<_Ty>{}.deallocate(m_data, 1);
                m_data = new_data;
            }
            m_capacities.first = m_capacities.second;
        }
    }

    void clear() noexcept
    {
        if constexpr (!std::is_trivial_v<_Ty>)
            memory::destruct_range(begin().unwrapped(), end().unwrapped());
        m_capacities.second = 0;
    }

    void erase(size_t offset)
    {
        WHP_CORE_ASSERT(offset < m_capacities.second, "Passed value is equal or greater than the size. (vector)");
        if constexpr (!std::is_trivial_v<_Ty>)
            allocator<_Ty>{}.destroy((begin() + offset).unwrapped());
        (m_data + offset) = (m_data + offset + 1);
    }

    void push_back(const _Ty& value)
    {
        if (m_capacities.first == m_capacities.second)
            reserve(m_capacities.first * m_grow_factor + 1);
        if constexpr (std::is_trivial_v<_Ty>)
            m_data[m_capacities.second] = value;
        else
            allocator<_Ty>{}.construct(m_data + m_capacities.second, value);
        m_capacities.second++;
    }

    void push_back(_Ty&& value)
    {
        if (m_capacities.first == m_capacities.second)
            reserve(m_capacities.first * m_grow_factor + 1);
        if constexpr (std::is_trivial_v<_Ty>)
            m_data[m_capacities.second] = value;
        else
            allocator<_Ty>{}.construct(m_data + m_capacities.second, whip::move(value));
        m_capacities.second++;
    }
    
    template <class... _Args>
    void emplace_back(_Args&&... args)
    {
        static_assert(!std::is_trivial_v<_Ty>, "Use push_back() instead of emplace_back() with trivial types.");

        if (m_capacities.first == m_capacities.second)
            reserve(m_capacities.first * m_grow_factor + 1);
        allocator<_Ty>{}.construct(m_data + m_capacities.second, whip::forward<_Args>(args)...);
        m_capacities.second++;
    }

    void pop_back()
    {
        WHP_CORE_ASSERT(m_capacities.second > 0, "Container is empty. (vector)");
        if constexpr (!std::is_trivial_v<_Ty>)
            allocator<_Ty>{}.destroy(&m_data[m_capacities.second - 1]);
        m_capacities.second--;
    }

    void resize(size_t new_size)
    {
        if (new_size == m_capacities.second)
        {
            WHP_CORE_WARN("Size is already equal to the passed value. (vector)");
            return;
        }

        if (new_size > m_capacities.first)
            reserve(new_size);

        if constexpr (!std::is_trivial_v<_Ty>)
        {
            if (new_size > m_capacities.second)
                memory::construct_range(m_data + m_capacities.second, m_data + new_size);
            else
                memory::destruct_range(m_data + new_size, m_data + m_capacities.second);
        }
        m_capacities.second = new_size;
    }

    // max grow_factor is equal to 10
    void set_grow_factor(size_type grow_factor = 2) noexcept
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

    size_type get_grow_factor() const noexcept
    {
        m_grow_factor;
    }

    iterator begin() noexcept
    {
        return iterator(m_data);
    }

    iterator end() noexcept
    {
        return iterator(m_data, m_capacities.second);
    }

    const_iterator begin() const noexcept
    {
        return const_iterator(m_data);
    }

    const_iterator end() const noexcept
    {
        return const_iterator(m_data, m_capacities.second);
    }

    const_iterator cbegin() const noexcept
    {
        return const_iterator(m_data);
    }

    const_iterator cend() const noexcept
    {
        return const_iterator(m_data, m_capacities.second);
    }

    reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }

    reverse_iterator rend() noexcept
    {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }

    const_reverse_iterator rend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }

    const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }

    const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }

private:
    _Ty* m_data = nullptr;
    pair<size_t> m_capacities = { 0 , 0 }; // first -> capacity, second -> size
    size_type m_grow_factor = 2;
};

_WHIP_END