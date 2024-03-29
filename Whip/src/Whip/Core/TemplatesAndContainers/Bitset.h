#pragma once
#ifndef _WHIP_BITSET_
#define _WHIP_BITSET_

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>

#include <Whip/Core/TemplatesAndContainers/TypeTraits.h>
#include <Whip/Core/TemplatesAndContainers/Hash.h>
#include <Whip/Core/TemplatesAndContainers/Iterator.h>
#include <Whip/Core/TemplatesAndContainers/Allocator.h>

#include <iostream>

#pragma warning(push)
#pragma warning(disable : _WHP_DISABLED_WARNINGS)

_WHIP_START

template <size_t _Bits>
class bitset;
template <size_t _Bits>
class bitset_iterator;
template <size_t _Bits>
class bitset_const_iterator;

template <size_t _Bits>
class bitset_reference 
{
    friend bitset<_Bits>; 
    friend bitset_iterator<_Bits>;
    friend bitset_const_iterator<_Bits>;

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
class bitset_iterator : public whip::iterator_base<bool>
{
public:
    using m_base        = iterator_base<bool>;
    using value_type    = bool;
    using pointer       = bitset_iterator;
    using reference     = bitset_reference<_Bits>;
    using size_type     = typename m_base::size_type;
    using diff_type     = typename m_base::diff_type;

    WHP_CONSTEXPR17 bitset_iterator(bitset<_Bits>* data = nullptr, size_type offset = 0) : m_ptr(data), m_offset(offset) {}
     
    WHP_CONSTEXPR17 bitset_iterator(const bitset_iterator& other) : m_ptr(other.m_ptr), m_offset(other.m_offset) {}
     
    WHP_CONSTEXPR17 ~bitset_iterator() {}

    WHP_CONSTEXPR17 reference operator*() const
    {
        return reference(DREF(m_ptr), m_offset);
    }

    WHP_CONSTEXPR17 bitset_iterator& operator++()
    {
        ++m_offset;
        return *this;
    }

    WHP_CONSTEXPR17 bitset_iterator operator++(int)
    {
        bitset_iterator temp(*this);
        ++(*this);
        return temp;
    }

    WHP_CONSTEXPR17 bitset_iterator& operator--()
    {
        --m_offset;
        return *this;
    }

    WHP_CONSTEXPR17 bitset_iterator operator--(int)
    {
        bitset_iterator temp(*this);
        --(*this);
        return temp;
    }

    WHP_CONSTEXPR17 bool operator==(const bitset_iterator& other) const
    {
        return ((m_ptr == other.m_ptr) && (m_offset == other.m_offset));
    }

    WHP_CONSTEXPR17 bool operator==(bitset_iterator&& other) const
    {
        bool res = ((m_ptr == other.m_ptr) && (m_offset == other.m_offset));
        other.m_ptr = nullptr;
        return res;
    }

    WHP_CONSTEXPR17 bool operator!=(const bitset_iterator& other) const
    {
        return !(this->operator==(move(other)));
    }
    /////////////////////////////////////////////////////////
    ///// We need this compare operators in here because ////
    //////// bitset_iteretor is not unwrappable and /////////
    /////////// verify_range uses this operators ////////////
    /////////////////////////////////////////////////////////

    WHP_CONSTEXPR17 bool operator>(const bitset_iterator& other) const
    {
        if (m_ptr != other.m_ptr)
            return false; // maybe true? how can we understand this? maybe we can compare pointers
        return m_offset > other.m_offset;
    }

    WHP_CONSTEXPR17 bool operator<(const bitset_iterator& other) const
    {
        if (m_ptr != other.m_ptr)
            return false; // maybe true? how can we understand this? maybe we can compare pointers
        return m_offset < other.m_offset;
    }

    /////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////

    WHP_CONSTEXPR17 bitset_iterator operator+(size_type n) const
    {
        return bitset_iterator(m_ptr, m_offset + n);
    }

    WHP_CONSTEXPR17 bitset_iterator operator-(size_type n) const
    {
        return bitset_iterator(m_ptr, m_offset - n);
    }

    constexpr void reset(pointer ptr = nullptr, size_type offset = 0) noexcept
    {
        m_ptr = ptr.m_ptr;
        m_offset = offset;
    }

    // bitset_iterator is not unwrappable
    constexpr pointer unwrapped() noexcept
    {
        return *this;
    }

    constexpr const pointer unwrapped() const noexcept
    {
        return *this;
    }

private:
    bitset<_Bits>* m_ptr;
    size_type m_offset;
};

template <size_t _Bits>
class bitset
{ 
public:
    using reference                 = bitset_reference<_Bits>;
    using iterator                  = bitset_iterator<_Bits>;
    using const_iterator            = bitset_iterator<_Bits>;
    using reverse_iterator          = _WHIP reverse_iterator<bitset_iterator<_Bits>>;
    using const_reverse_iterator    = _WHIP reverse_iterator<bitset_iterator<_Bits>>;

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

    WHP_CONSTEXPR23 bitset& operator&=(const bitset& right) noexcept
    {
        for (size_t wpos = 0; wpos <= words; ++wpos)
            m_data[wpos] &= right.m_data[wpos];
        return *this;
    }

    WHP_CONSTEXPR23 bitset& operator|=(const bitset& right) noexcept
    {
        for (size_t wpos = 0; wpos <= words; ++wpos)
            m_data[wpos] |= right.m_data[wpos]; return *this;
    }

    WHP_CONSTEXPR23 bitset& operator^=(const bitset& right) noexcept 
    {
        for (size_t wpos = 0; wpos <= words; ++wpos)
            m_data[wpos] ^= right.m_data[wpos]; 
        return *this;
    }

    WHP_CONSTEXPR23 bitset& operator<<=(size_t position) noexcept
    {
        const auto word_shift = static_cast<ptrdiff_t> (position / bits_per_word);
        if (word_shift != 0)
            for (ptrdiff_t wpos = words; 0 <= wpos; --wpos)
                m_data[wpos] = word_shift <= wpos ? m_data[wpos - word_shift] : 0;

        if ((position %= bits_per_word) != 0)
        {
            for (ptrdiff_t wpos = words; 0 < wpos; --wpos)
                m_data[wpos] = (m_data[wpos] << position) | (m_data[wpos - 1] >> (bits_per_word - position));

            m_data[0] <<= position;
        }
        trim();
        return *this;
    }

    WHP_CONSTEXPR23 bitset& operator>>=(size_t position) noexcept
    {
        const auto word_shift = static_cast<ptrdiff_t> (position / bits_per_word);
        if (word_shift != 0)
            for (ptrdiff_t wpos = 0; wpos <= words; ++wpos)
                m_data[wpos] = word_shift <= words - wpos ? m_data[wpos + word_shift] : 0;

        if ((position %= bits_per_word) != 0)
        {
            for (ptrdiff_t wpos = 0; wpos < words; ++wpos)
                m_data[wpos] = (m_data[wpos] >> position) | (m_data[wpos + 1] << (bits_per_word - position));

            m_data[0] >>= position;
        }
        return *this;
    }

    WHP_CONSTEXPR23 bitset& set() noexcept
    {
#if _WHP_HAS_CPP_VERSION(23)
        if (_WHIP is_constant_evaluated())
        {
            for (auto& item : m_data)
                item = static_cast<_Ty>(-1);
        }
        else
#endif
        {
            ::memset(&m_data, 0xFF, sizeof(m_data));
        }
        trim();
        return *this;
    }

    WHP_CONSTEXPR23 bitset& set(const size_t position, bool value = true)
    {
        if (_Bits <= position)
            throw_oran();
        return set_unchecked(position, value);
    }

    WHP_CONSTEXPR23 bitset& reset() noexcept
    {
#if _WHP_HAS_CPP_VERSION(23)
        if (_WHIP is_constant_evaluated())
        {
            for (auto& item : m_data)
                item = 0;
        }
        else
#endif
        {
            ::memset(&m_data, 0, sizeof(m_data));
        }
        return *this;
    }

    WHP_CONSTEXPR23 bitset& reset(const size_t position)
    {
        return set(position, false);
    }

    WHP_NODISCARD WHP_CONSTEXPR23 bitset operator~() const noexcept
    {
        bitset temp = *this;
        temp.flip();
        return temp;
    }

    WHP_CONSTEXPR23 bitset& flip() noexcept
    {
        for (size_t wpos = 0; wpos <= words; ++wpos)
            m_data[wpos] = ~m_data[wpos];
        trim();
        return *this;
    }

    WHP_CONSTEXPR23 bitset& flip(const size_t position)
    {
        if (_Bits <= position)
            throw_oran();
        return flip_unchecked(position);
    }

    WHP_NODISCARD WHP_CONSTEXPR23 unsigned long to_ulong() const noexcept(_Bits <= 32)
    {
        constexpr bool bits_zero    = _Bits == 0;
        constexpr bool bits_small   = _Bits <= 32;
        constexpr bool bits_large   = _Bits > 64;
        if constexpr (bits_zero)
            return 0;
        else if constexpr (bits_small)
            return static_cast<unsigned long>(m_data[0]);
        else
        {
            if constexpr (bits_large)
                for (size_t idx = 1; idx <= words; ++idx)
                    if (m_data[idx] != 0)
                        throw_oflow();
            if (m_data[0] > ULONG_MAX)
                throw_oflow();
            return static_cast<unsigned long>(m_data[0]);
        }
    }

    WHP_NODISCARD WHP_CONSTEXPR23 unsigned long long to_ullong() const noexcept(_Bits <= 64)
    {
        constexpr bool bits_zero = _Bits == 0;
        constexpr bool bits_large = _Bits > 64;
        if constexpr (bits_zero)
            return 0;
        else
        {
            if constexpr (bits_large)
                for (size_t idx = 1; idx <= words; ++idx)
                    if (m_data[idx] != 0)
                        throw_oflow();
            return m_data[0];
        }
    }

    template <class _Elem = char, class _Traits = std::char_traits<_Elem>, class _Alloc = allocator<_Elem>>
    WHP_NODISCARD WHP_CONSTEXPR23 std::basic_string<_Elem, _Traits, _Alloc> to_string(const _Elem elem0 = static_cast<_Elem>('0'), const _Elem elem1 = static_cast<_Elem>('1')) const
    {
        std::basic_string<_Elem, _Traits, _Alloc> str;
        str.reserve(_Bits);
        for (auto pos = _Bits; 0 < pos;)
            str.push_back(subscript(--pos) ? elem1 : elem0);
        return str;
    }

    // todo (maybe): count method

    WHP_NODISCARD constexpr size_t size() const noexcept
    {
        return _Bits;
    }

    WHP_NODISCARD WHP_CONSTEXPR23 bool operator==(const bitset& right) const noexcept
    {
#if _WHP_HAS_CPP_VERSION(23)
        if (_WHIP is_constant_evaluated())
        {
            for (size_t i = 0; i <= words; ++i)
                if (m_data[i] != right.m_data[i])
                    return false;
            return true;
        }
        else
#endif // _WHP_HAS_CPP_VERSION(23)
        {
            return ::memcmp(&m_data[0], &right.m_data[0], sizeof(m_data)) == 0;
        }
    }

#if !_WHP_HAS_CPP_VERSION(20)
    WHP_NODISCARD bool operator!=(const bitset& right) const noexcept 
    {
        return !(*this == right);
    }
#endif // !_WHP_HAS_CPP_VERSION(23)

    WHP_NODISCARD WHP_CONSTEXPR23 bool test(const size_t position) const
    {
        if (_Bits <= position)
            throw_oran();
        return subscript(position);
    }

    WHP_NODISCARD WHP_CONSTEXPR23 bool any() const noexcept
    {
        for (size_t wpos = 0; wpos <= words; ++wpos)
            if (m_data[wpos] != 0)
                return true;
        return false;
    }

    WHP_NODISCARD WHP_CONSTEXPR23 bool none() const noexcept
    {
        return !any();
    }

    WHP_NODISCARD WHP_CONSTEXPR23 bool all() const noexcept
    {
        constexpr bool zero_length = _Bits == 0;
        if constexpr (zero_length)
            return true;
        constexpr bool no_padding = _Bits % bits_per_word == 0;
        for (size_t wpos = 0; wpos < words + no_padding; ++wpos)
            if (m_data[wpos] != ~static_cast<_Ty>(0))
                return false;
        return no_padding || m_data[words] == (static_cast<_Ty>(1) << (_Bits % bits_per_word)) - 1;
    }

    WHP_NODISCARD WHP_CONSTEXPR23 bitset operator<<(const size_t position) const noexcept
    {
        bitset temp = *this;
        temp <<= position;
        return temp;
    }

    WHP_NODISCARD WHP_CONSTEXPR23 bitset operator>>(const size_t position) const noexcept
    {
        bitset temp = *this;
        temp >>= position;
        return temp;
    }

    WHP_NODISCARD _Ty get_word(size_t wpos) const noexcept
    {
        return m_data[wpos];
    }

    WHP_NODISCARD WHP_CONSTEXPR23 iterator begin() noexcept
    {
        return iterator(this, 0);
    }

    WHP_NODISCARD WHP_CONSTEXPR23 iterator end() noexcept
    {
        return iterator(this, _Bits);
    }

    WHP_NODISCARD WHP_CONSTEXPR23 reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }

    WHP_NODISCARD WHP_CONSTEXPR23 reverse_iterator rend() noexcept
    {
        return reverse_iterator(begin());
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
WHP_NODISCARD WHP_CONSTEXPR23 bitset_iterator<_Bits> begin(bitset<_Bits>& bits) noexcept
{
    return { &bits, 0 };
}

template <size_t _Bits>
WHP_NODISCARD WHP_CONSTEXPR23 bitset_iterator<_Bits> end(bitset<_Bits>& bits) noexcept
{
    return { &bits, _Bits };
}

template <size_t _Bits>
WHP_NODISCARD WHP_CONSTEXPR23 bitset<_Bits> operator&(const bitset<_Bits>& left, const bitset<_Bits>& right)
{
    bitset<_Bits> ans = left;
    ans &= right;
    return ans;
}

template <size_t _Bits>
WHP_NODISCARD WHP_CONSTEXPR23 bitset<_Bits> operator|(const bitset<_Bits>& left, const bitset<_Bits>& right)
{
    bitset<_Bits> ans = left;
    ans |= right;
    return ans;
}

template <size_t _Bits>
WHP_NODISCARD WHP_CONSTEXPR23 bitset<_Bits> operator^(const bitset<_Bits>&left, const bitset<_Bits>& right)
{
    bitset<_Bits> ans = left;
    ans ^= right;
    return ans;
}

template <class _Elem, class _Traits, size_t _Bits>
std::basic_ostream<_Elem, _Traits>& operator<<(std::basic_ostream<_Elem, _Traits>& ostr, const bitset<_Bits>& right)
{
    using _Ctype = typename std::basic_ostream<_Elem, _Traits>::_Ctype;
    const _Ctype& ctype_fac = std::use_facet<_Ctype>(ostr.getloc());
    const _Elem elem0 = ctype_fac.widen('0');
    const _Elem elem1 = ctype_fac.widen('1');
    return ostr << right.template to_string<_Elem, _Traits, allocator<_Elem>>(elem0, elem1);
}

// todo: fix this shit !
template <class _Elem, class _Traits, size_t _Bits>
std::basic_istream<_Elem, _Traits>& operator>>(std::basic_istream<_Elem, _Traits>& istr, bitset<_Bits>& right)
{
    using _Istr_t = std::basic_istream<_Elem, _Traits>;
    using _Ctype = typename _Istr_t::_Ctype;
    const _Ctype& ctype_fac = std::use_facet<_Ctype>(istr.getloc());
    const _Elem elem0 = ctype_fac.widen('0');
    const _Elem elem1 = ctype_fac.widen('1');
    typename _Istr_t::iostate state = _Istr_t::goodbit;
    bool changed = false;
    std::string str;
    const typename _Istr_t::sentry _Ok(istr);

    if (_Ok) 
    {
        try
        {
            typename _Traits::int_type meta = istr.rdbuf()->sgetc();
            for (size_t count = right.size(); 0 < count; meta = istr.rdbuf()->snextc(), (void) --count)
            {
                _Elem ch;
                if (_Traits::eq_int_type(_Traits::eof(), meta))
                {
                    state |= _Istr_t::eofbit;
                    break;
                }
                else if ((ch = _Traits::to_char_type(meta)) != elem0 && ch != elem1)
                    break;
                else if (str.max_size() <= str.size())
                {
                    state |= _Istr_t::failbit;
                    break;
                }
                else
                {
                    str.push_back('0' + (ch == elem1));
                    changed = true;
                }
            }
        }
        catch (...)
        {
            istr.setstate(_Istr_t::badbit, true);
        }
    }

    constexpr bool _Has_bits = _Bits > 0;

    if constexpr (_Has_bits)
        if (!changed)
            state |= _Istr_t::failbit;

    istr.setstate(state);
    right = bitset<_Bits>(str);
    return istr;
}

template <size_t _Bits>
struct hash<bitset<_Bits>> 
{
    WHP_NODISCARD size_t operator()(const bitset<_Bits>& key_value) const noexcept 
    {
        return fnv1a::hash_representation(key_value.m_data);
    }
};

_WHIP_END

#pragma warning(pop)

#endif // !_WHIP_BITSET_