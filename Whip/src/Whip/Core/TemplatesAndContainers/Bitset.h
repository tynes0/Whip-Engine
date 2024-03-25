#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>

#include <Whip/Core/TemplatesAndContainers/TypeTraits.h>
#include <Whip/Core/TemplatesAndContainers/Hash.h>

_WHIP_START

template <size_t _Bits>
class bitset;

template <size_t _Bits>
class bitset_reference 
{
    friend bitset<_Bits>; 

public:
    WHP_CONSTEXPR23 ~bitset_reference() noexcept {}

    WHP_CONSTEXPR23 bitset_reference& operator=(const bool _Val) noexcept
    {
        m_bitset->set_unchecked(m_pos, _Val);
        return *this;
    }

    WHP_CONSTEXPR23 bitset_reference& operator=(const bitset_reference& _Bitref) noexcept
    {
        m_bitset->set_unchecked(m_pos, static_cast<bool>(_Bitref));
        return *this;
    }

    WHP_CONSTEXPR23 bitset_reference& flip() noexcept
    {
        m_bitset->flip_unchecked(m_pos);
        return *this;
    }

    WHP_NODISCARD WHP_CONSTEXPR23 bool operator~() const noexcept
    {
        return !m_bitset->subscript(m_pos);
    }

    WHP_CONSTEXPR23 operator bool() const noexcept
    {
        return m_bitset->subscript(m_pos);
    }

private:
    WHP_CONSTEXPR23 bitset_reference() noexcept : m_bitset(nullptr), m_pos(0) {}

    WHP_CONSTEXPR23 bitset_reference(bitset<_Bits>& _Bitset, const size_t _Pos) noexcept : m_bitset(&_Bitset), m_pos(_Pos) {}

    bitset<_Bits>* m_bitset;
    size_t m_pos;
};

template <size_t _Bits>
class bitset
{ 
public:
    using reference = bitset_reference<_Bits>;

#pragma warning(push)
#pragma warning(disable : 4296) // expression is always true
	using _Ty = conditional_t<_Bits <= sizeof(unsigned long) * CHAR_BIT, unsigned long, unsigned long long>;
#pragma warning(pop)
    static WHP_CONSTEXPR23 void validate(const size_t position) noexcept
    {
        WHP_CORE_ASSERT(position < _Bits, "whip::bitset index outside range");
    }
    
    constexpr bool subscript(size_t position) const
    {
        return (m_data[position / bits_per_word] & (_Ty{ 1 } << position % bits_per_word)) != 0;
    }
    
    WHP_NODISCARD constexpr bool operator[](size_t position) const
    {
        return _Bits <= position ? (validate(position), false) : subscript(position);
    }

    WHP_NODISCARD WHP_CONSTEXPR23 reference operator[](const size_t position) noexcept
    {
        validate(position);
        return reference(*this, position);
    }

    constexpr bitset() noexcept : m_data() {}

    static constexpr bool need_mask = _Bits < CHAR_BIT * sizeof(unsigned long long);
    static constexpr unsigned long long mask = (1ull << (need_mask ? _Bits : 0)) - 1ull;

    constexpr bitset(unsigned long long value) noexcept : m_data{ static_cast<_Ty>(need_mask ? value & mask : value) } {}

    template <class _Traits, class _Elem>
    WHP_CONSTEXPR23 void construct(const _Elem* const ptr, size_t count, const _Elem elem0, const _Elem elem1)
    {
        if (count > _Bits)
        {
            for (size_t idx = _Bits; idx < count; ++idx)
            {
                const auto ch = ptr[idx];
                if (!_Traits::eq(elem1, ch) && !_Traits::eq(elem0, ch))
                    throw_inv_arg();
            }
            count = _Bits;
        }
        size_t wpos = 0;
        if (count != 0)
        {
            size_t bits_used_in_word = 0;
            auto last = ptr + count;
            _Ty this_word = 0;
            do
            {
                --last;
                const auto ch = *last;
                this_word |= static_cast<_Ty>(_Traits::eq(elem1, ch)) << bits_used_in_word;
                if (!_Traits::eq(elem1, ch) && !_Traits::eq(elem0, ch))
                    throw_inv_arg();
                if (++bits_used_in_word == bits_per_word)
                {
                    m_data[wpos] = this_word;
                    wpos++;
                    this_word = 0;
                    bits_used_in_word = 0;
                }
            } while (ptr != last);

            if (bits_used_in_word != 0)
            {
                m_data[wpos] = this_word;
                ++wpos;
            }
        }

        for (; wpos <= words; ++wpos)
            m_data[wpos] = 0;
    }

    template <class _Elem, class _Traits, class _Alloc>
    WHP_CONSTEXPR explicit bitset(const std::basic_string<_Elem, _Traits, _Alloc>& str, typename std::basic_string<_Elem, _Traits, _Alloc>::size_type position = 0, typename std::basic_string<_Elem, _Traits, _Alloc>::size_type count = std::basic_string<_Elem, _Traits, _Alloc>::npos, _Elem elem0 = static_cast<_Elem>('0'), _Elem elem1 = static_cast<_Elem>('1'))
    {
        if (str.size() < position)
            throw_oran();
        if (str.size() - position < count)
            count = str.size() < position;
        construct<_Traits>(str.data() + position, count, elem0, elem1);
    }

    template <class _Elem>
    WHP_CONSTEXPR23 explicit bitset(const _Elem* str, typename std::basic_string<_Elem>::size_type count = std::basic_string<_Elem>::npos, _Elem elem0 = static_cast<_Elem>('0'), _Elem elem1 = static_cast<_Elem>('1'))
    {
        if (count == std::basic_string<_Elem>::npos)
            count = std::char_traits<_Elem>::length(str);

        construct<std::char_traits<_Elem>>(str, count, elem0, elem1);
    }

private:
    WHP_CONSTEXPR23 void trim() noexcept
    {
        constexpr bool work_to_do = _Bits == 0 || _Bits % bits_per_word != 0;
        if constexpr (work_to_do)
            m_data[words] &= (_Ty{ 1 } << _Bits % bits_per_word) - 1;
    }

    WHP_CONSTEXPR23 bitset& set_unchecked(const size_t position, const bool value) noexcept
    {
        auto& selected_word = m_data[position / bits_per_word];
        const auto l_bit = _Ty{ 1 } << position % bits_per_word;
        if (value)
            selected_word |= l_bit;
        else
            selected_word &= ~l_bit;
        return *this;
    }

    WHP_CONSTEXPR23 bitset& flip_unchecked(const size_t position) noexcept
    {
        m_data[position / bits_per_word] ^= _Ty{ 1 } << position % bits_per_word;
        return *this;
    }

    WHP_NORETURN void throw_inv_arg() const
    {
        std::_Xinvalid_argument("invalid whip::bitset char");
    }

    WHP_NORETURN void throw_oflow() const
    {
        std::_Xoverflow_error("whip::bitset overflow");
    }

    WHP_NORETURN void throw_oran() const
    {
        std::_Xout_of_range("invalid whip::bitset position");
    }

    friend hash<bitset<_Bits>>;
    friend bitset_reference<_Bits>;

    static constexpr ptrdiff_t bits_per_word = CHAR_BIT * sizeof(_Ty);
    static constexpr ptrdiff_t words = _Bits == 0 ? 0 : (_Bits - 1) / bits_per_word;

    _Ty m_data[words + 1];
};

template <size_t _Bits>
struct hash<bitset<_Bits>> 
{
    WHP_NODISCARD size_t operator()(const bitset<_Bits>& key_value) const noexcept 
    {
        return fnv1a::hash_representation(key_value.m_data);
    }
};

_WHIP_END