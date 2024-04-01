#pragma once
#ifndef _WHIP_ANY_
#define _WHIP_ANY_

#include <typeinfo>
#include <type_traits>

#include "Whip/Core/Core.h"
#include "Utility.h"
#include "TypeTraits.h"

#pragma warning(push)
#pragma warning(disable : _WHP_DISABLED_WARNINGS)

_WHIP_START

class bad_any_cast : public std::bad_cast
{
public:
    const char* what() const override
    {
        return "whip::bad_any_cast: failed conversion using whip::any_cast";
    }
};

namespace detail_any
{
    class placeholder
    {
    public:
        virtual ~placeholder() {}
        virtual const type_info& type() const noexcept = 0;
    };
}

class any
{
public:
    constexpr any() noexcept : m_content(0) {}

    template <class _Ty>
    any(const _Ty& value) : m_content(new holder<remove_cv_t<std::decay_t<const _Ty>>>(value)) {}

    any(const any& right) : m_content(right.m_content ? right.m_content->clone() : 0) {}

    any(any&& right) noexcept : m_content(right.m_content)
    {
        right.m_content = 0;
    }

    template <class _Ty>
    any(_Ty&& value, enable_if_t<!is_same_v<any&, _Ty>>* = 0, enable_if_t<!std::is_const_v<_Ty>>* = 0)
        : m_content(new holder<std::decay_t<_Ty>>(forward<_Ty>(value))) {}

    ~any()
    {
        delete m_content;
    }

    any& swap(any& right) noexcept
    {
        if (addressof(right) != this)
        {
            placeholder* temp = m_content;
            m_content = right.m_content;
            right.m_content = temp;
        }
        return *this;
    }

    any& operator=(const any& right)
    {
        any(right).swap(*this);
        return *this;
    }

    any& operator=(any&& right) noexcept
    {
        right.swap(*this);
        any().swap(right);
        return *this;
    }

    template <class _Ty>
    any& operator=(_Ty&& right)
    {
        any(forward<_Ty>(right)).swap(*this);
        return *this;
    }

    bool empty() const noexcept
    {
        return !m_content;
    }

    void clear() noexcept
    {
        any().swap(*this);
    }

    const std::type_info& type() const noexcept
    {
        return m_content ? m_content->type() : typeid(void);
    }

private:
    class placeholder : public detail_any::placeholder
    {
    public:
        virtual placeholder* clone() const = 0;
    };

    template <class _Ty>
    class holder final : public placeholder
    {
    public:
        holder(const _Ty& value) :held(value) {}

        holder(_Ty&& value) :held(static_cast<_Ty&&>(value)) {}

        const std::type_info& type() const noexcept override
        {
            return typeid(_Ty);
        }

        placeholder* clone() const override
        {
            return new holder(held);
        }

        _Ty held;
    private:
        holder& operator=(const holder&);
    };

private:
    template <class _Ty>
    friend _Ty* unsafe_any_cast(any*) noexcept;
    placeholder* m_content;
};

inline void swap(any& left, any& right) noexcept
{
    left.swap(right);
}

template <class _Ty>
inline _Ty* unsafe_any_cast(any* operand) noexcept
{
    return addressof(static_cast<any::holder<_Ty>*>(operand->m_content)->held);
}

template <class _Ty>
inline const _Ty* unsafe_any_cast(const any* operand) noexcept
{
    return unsafe_any_cast<_Ty>(const_cast<any*>(operand));
}

template <class _Ty>
_Ty* any_cast(any* operand) noexcept
{
    return operand && operand->type() == typeid(_Ty) ? unsafe_any_cast<remove_cv_t<_Ty>>(operand) : 0;
}

template <class _Ty>
inline const _Ty* any_cast(const any* operand) noexcept
{
    return any_cast<_Ty>(const_cast<any*>(operand));
}

template <class _Ty>
_Ty any_cast(any& operand)
{
    using nonref = remove_reference_t<_Ty>;

    nonref* result = any_cast<nonref>(addressof(operand));
    if (!result)
        throw(bad_any_cast());
    return static_cast<conditional_t<is_reference_v<_Ty>, _Ty, add_lvalue_reference_t<_Ty>>>(*result);
}

template <class _Ty>
inline _Ty any_cast(const any& operand)
{
    return any_cast<const remove_reference_t<_Ty>&>(const_cast<any&>(operand));
}

template <class _Ty>
inline _Ty any_cast(any&& operand)
{
    static_assert(is_rvalue_reference_v<_Ty&&> || std::is_const_v<remove_reference_t<_Ty>>, "any_cast shall not be used for getting nonconst references to temporary objects");
    return any_cast<_Ty>(operand);
}

// any operations
namespace aops
{
    // TODO: finish aops
    bool is_null(const any& data);
    int as_int(const any& data);
    float as_float(const any& data);
    double as_double(const any& data);
    char as_char(const any& data);
    unsigned int as_uint(const any& data);
    unsigned char as_uchar(const any& data);
    long as_long(const any& data);
    long long as_long_long(const any& data);
    unsigned long as_ulong(const any& data);
    unsigned long long as_ulong_long(const any& data);
    short as_short(const any& data);
    long double as_long_double(const any& data);
    unsigned short as_ushort(const any& data);

} // namespace aops

_WHIP_END

#pragma warning(pop)

#endif // !_WHIP_ANY_