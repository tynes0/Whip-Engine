#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/TemplatesAndContainers/TypeTraits.h>
#include <Whip/Core/TemplatesAndContainers/ReferenceWrapper.h>
#include <Whip/Core/TemplatesAndContainers/Concepts.h>

#if WHP_TUPLE_NO_INLINE
#define WHP_TUPLE_INLINE
#else

#if _MSC_VER
#define WHP_TUPLE_INLINE __forceinline
#elif __GNUC__ || __clang__
#define WHP_TUPLE_INLINE [[gnu::always_inline]]
#else
#define WHP_TUPLE_INLINE
#endif
#endif

#define WHP_TUPLE_COMPARISON_OPERATOR_1(type, member, op)                      \
    WHP_TUPLE_INLINE constexpr auto operator op(type const& other)             \
        const noexcept(noexcept(member op other.member))                       \
    {                                                                          \
        return member op other.member;                                         \
    }



#if _MSC_VER
#define WHP_FWD_M(TupleType, BaseType, tup, value) static_cast<_WHIP forward_as_t<TupleType&&, BaseType>>(tup).value
#define WHP_GET_M(BaseType, tup, value) tup._WHIP identity_t<BaseType>::value
#elif __clang__
#define WHP_FWD_M(TupleType, BaseType, tup, value) static_cast<TupleType&&>(tup)._WHIP identity_t<BaseType>::value
#define WHP_GET_M(BaseType, tup, value) tup._WHIP identity_t<BaseType>::value
#else   
#define WHP_FWD_M(TupleType, BaseType, tup, value) static_cast<TupleType&&>(tup).BaseType::value
#define WHP_GET_M(BaseType, tup, value) tup.BaseType::value
#endif



#if __cpp_impl_three_way_comparison && __cpp_lib_three_way_comparison && !defined(WHP_DEFAULTED_COMPARISON)
#define WHP_DEFAULTED_COMPARISON 1
#include <compare>
#else
#define WHP_DEFAULTED_COMPARISON 0
#endif

#if __cpp_concepts
#define WHP_OTHER_THAN(Self, Other) _WHIP other_than<Self> Other
#define WHP_WEAK_CONCEPT(...) __VA_ARGS__
#define WHP_WEAK_REQUIRES(...) requires __VA_ARGS__
#define WHP_TYPES_EQ_WITH(T, U) bool requires(_WHIP equality_comparable_with<T, U> && ...)
#define WHP_TYPES_CMP_WITH(T, U) bool requires(_WHIP equality_comparable_with<T, U> && ...)
#else
#define WHP_OTHER_THAN(Self, Other) class Other, class = _WHIP detail_tuple::other_than<Self, Other>
#define WHP_WEAK_CONCEPT(...) class
#define WHP_WEAK_REQUIRES(...)
#if _MSC_VER < 1930
#define WHP_TYPES_EQ_WITH(T, U) ::std::enable_if_t<_WHIP detail_tuple::extra_detail::_all_true<_WHIP detail_tuple::extra_detail::_has_eq<T, U>...>(), bool>
#define WHP_TYPES_CMP_WITH(T, U) ::std::enable_if_t<_WHIP detail_tuple::extra_detail::_all_true<_WHIP detail_tuple::extra_detail::_has_cmp<T, U>...>(), bool>
#else
#define WHP_TYPES_EQ_WITH(T, U) ::std::enable_if_t<((_WHIP detail_tuple::extra_detail::_has_eq<T, U>)&&...), bool>
#define WHP_TYPES_CMP_WITH(T, U) ::std::enable_if_t<((_WHIP detail_tuple::extra_detail::_has_cmp<T, U>)&&...), bool>
#endif
#endif


#if defined(WHP_NO_UNIQUE_ADDRESS) && !WHP_NO_UNIQUE_ADDRESS
#define WHP_NO_UNIQUE_ADDRESS
#else
#if _MSVC_LANG >= 202002L && (!defined __clang__)

#define WHP_HAS_NO_UNIQUE_ADDRESS 1
#define WHP_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]

#elif _MSC_VER
// no_unique_address is not available (C++17)
#define WHP_HAS_NO_UNIQUE_ADDRESS 0
#define WHP_NO_UNIQUE_ADDRESS

#elif __cplusplus > 201703L && (__has_cpp_attribute(no_unique_address))

#define WHP_HAS_NO_UNIQUE_ADDRESS 1
#define WHP_NO_UNIQUE_ADDRESS [[no_unique_address]]

#else
// no_unique_address is not available.
#define WHP_HAS_NO_UNIQUE_ADDRESS 0
#define WHP_NO_UNIQUE_ADDRESS
#endif
#endif

_WHIP_START

template <class... T>
struct type_list {};

template <class... Ls, class... Rs>
WHP_TUPLE_INLINE constexpr auto operator+(type_list<Ls...>, type_list<Rs...>)
{
    return type_list<Ls..., Rs...> {};
}

namespace detail_tuple
{
    namespace extra_detail 
    {
        template <class T, class U, class = decltype(whip::declval<T>() == whip::declval<U>())>
        constexpr bool _test_eq(int)
        {
            return true;
        }

        template <class T, class U>
        constexpr bool _test_eq(long long)
        {
            return false;
        }

        template <class T, class U, class = decltype(whip::declval<T>() < whip::declval<U>())>
        constexpr bool _test_less(int)
        {
            return true;
        }

        template <class T, class U>
        constexpr bool _test_less(long long)
        {
            return false;
        }

        template <class T, class U>
        constexpr bool _has_eq = _test_eq<T, U>(0);

        template <class T, class U = T>
        constexpr bool _has_cmp = _test_eq<T, U>(0) && _test_less<T, U>(0);

        template <class Tup, class = typename Tup::base_list>
        constexpr bool _has_base_list(int)
        {
            return true;
        }
        template <class Tup>
        constexpr bool _has_base_list(long long)
        {
            return false;
        }

        template <bool... B>
        constexpr bool _all_true()
        {
            return (B && ...);
        };

        template <class... T, class... U>
        constexpr bool _all_has_eq(type_list<T...>, type_list<U...>)
        {
            bool values[]{ _has_eq<T, U>... };
            for (bool b : values)
            {
                if (!b)
                {
                    return false;
                }
            }
            return true;
        }
        template <class... T, class... U>
        constexpr bool _all_has_cmp(type_list<T...>, type_list<U...>)
        {
            bool values[]{ _has_cmp<T, U>... };
            for (bool b : values)
            {
                if (!b)
                {
                    return false;
                }
            }
            return true;
        }

        constexpr bool _all_has_eq(type_list<>, type_list<>) { return true; }
        constexpr bool _all_has_cmp(type_list<>, type_list<>) { return true; }

        template <class A, class B, class = decltype(declval<A>().compare(declval<B>()))>
        constexpr bool _test_m_compare(int)
        {
            return true;
        }

        template <class, class>
        constexpr bool _test_m_compare(long long)
        {
            return false;
        }
    } // namespace extra_detail

    template <class Tup, class B>
    struct _forward_as 
    {
        using type = B&&;
    };

    template <class Tup, class B>
    struct _forward_as<Tup&, B> 
    {
        using type = B&;
    };

    template <class Tup, class B>
    struct _forward_as<Tup&&, B> 
    {
        using type = B&&;
    };

    template <class Tup, class B>
    struct _forward_as<Tup const&, B> 
    {
        using type = B const&;
    };

    template <class Tup, class B>
    struct _forward_as<Tup const&&, B> 
    {
        using type = B const&&;
    };

} // namespace detail_tuple

template <class Tup, class B>
using forward_as_t = typename ::whip::detail_tuple::_forward_as<Tup, B>::type;

template <class First, class...>
using first_t = First;

template <class T>
using type_t = typename T::type;

template <size_t I>
using tag = integral_constant<size_t, I>;

template <size_t I>
constexpr tag<I> tag_v{};

template <size_t N>
using tag_range = std::make_index_sequence<N>;


template <class Tup>
using base_list_t = typename std::decay_t<Tup>::base_list;
template <class Tup>
using element_list_t = typename std::decay_t<Tup>::element_list;

template <class T>
constexpr bool stateless_v = is_empty_v<std::decay_t<T>>;
#if __cpp_concepts
template <class Tuple>
concept base_list_tuple = requires()
{
    typename std::decay_t<Tuple>::base_list;
};

template <class T>
concept indexable = stateless<T> || requires(T t)
{
    t[tag<0>()];
};
#endif // __cpp_concepts
template <class Tuple>
constexpr auto base_list_tuple_v =
#if __cpp_concepts
base_list_tuple<Tuple>;
#else // !__cpp_concepts
whip::detail_tuple::extra_detail::_has_base_list<Tuple>(0);
#endif // __cpp_concepts

namespace detail_tuple
{
    template <class T, class U>
    WHP_TUPLE_INLINE constexpr bool _partial_cmp(T const& a, U const& b, bool& less) 
    {
        if constexpr (::whip::detail_tuple::extra_detail::_test_m_compare<T, U>(0)) 
        {
            int cmp = a.compare(b);

            if (cmp < 0) 
            {
                less = true;
            }
            return cmp == 0;
        }
        else 
        {
#if WHP_DEFAULTED_COMPARISON
            if constexpr (ordered_with<T, U>) 
            {
                auto cmp = a <=> b;

                if (cmp < 0) 
                {
                    less = true;
                }
                return cmp == 0;
            }
            else {
                if (a < b) 
                {
                    less = true;
                    return false;
                }
                else 
                {
                    return a == b;
                }
            }
#else
            if (a < b) 
            {
                less = true;
                return false;
            }
            else 
            {
                return a == b;
            }
#endif
        }
    }

    template <class Tup, class... B1>
    WHP_TUPLE_INLINE constexpr bool _equals(Tup const& t1, Tup const& t2, type_list<B1...>) 
    {
#ifdef _MSC_VER
        return [&](auto&... v1) -> bool 
        {
            return [&](auto&... v2) -> bool 
            {
                return ((v1 == v2) && ...);
            }(WHP_GET_M(B1, t2, value)...);
        }(WHP_GET_M(B1, t1, value)...);
#else
        return ((WHP_GET_M(B1, t1, value) == WHP_GET_M(B1, t2, value)) && ...);
#endif
    }
    
    template <class Tup, class... B1>
    WHP_TUPLE_INLINE constexpr bool _less(Tup const& t1, Tup const& t2, type_list<B1...>) 
    {
        bool is_less = false;
#ifdef _MSC_VER
        [&](auto&... v1) -> bool 
        {
            return [&](auto&... v2) -> bool 
            {
                return (_partial_cmp(v1, v2, is_less) && ...);
            }(WHP_GET_M(B1, t2, value)...);
        }(WHP_GET_M(B1, t1, value)...);
#else
        (_partial_cmp(WHP_GET_M(B1, t1, value), WHP_GET_M(B1, t2, value), is_less) && ...);
#endif
        return is_less;
    }
    
    template <class Tup, class... B1>
    WHP_TUPLE_INLINE constexpr bool _less_eq(Tup const& t1, Tup const& t2, type_list<B1...>)
    {
        bool is_less = false;
#ifdef _MSC_VER
        bool is_eq = [&](auto&... v1) -> bool 
        {
            return [&](auto&... v2) -> bool 
            {
                return (_partial_cmp(v1, v2, is_less) && ...);
            }(WHP_GET_M(B1, t2, value)...);
        }(WHP_GET_M(B1, t1, value)...);
#else
        bool is_eq = (_partial_cmp(WHP_GET_M(B1, t1, value), WHP_GET_M(B1, t2, value), is_less) && ... && true);
#endif
        return is_less || is_eq;
    }
    
    template <class Tup1, class Tup2, class... B1, class... B2>
    WHP_TUPLE_INLINE constexpr bool _equals(Tup1 const& t1, Tup2 const& t2, type_list<B1...>, type_list<B2...>) 
    {
#ifdef _MSC_VER
        return [&](auto&... v1) -> bool 
        {
            return [&](auto&... v2) -> bool 
            {
                return ((v1 == v2) && ...);
            }(WHP_GET_M(B2, t2, value)...);
        }(WHP_GET_M(B1, t1, value)...);
#else
        return ((WHP_GET_M(B1, t1, value) == WHP_GET_M(B2, t2, value)) && ...);
#endif
    }
    
    template <class Tup1, class Tup2, class... B1, class... B2>
    WHP_TUPLE_INLINE constexpr bool _less(Tup1 const& t1, Tup2 const& t2, type_list<B1...>, type_list<B2...>) 
    {
        bool is_less = false;
#ifdef _MSC_VER
        [&](auto&... v1) -> bool 
        {
            return [&](auto&... v2) -> bool 
            {
                return (_partial_cmp(v1, v2, is_less) && ...);
            }(WHP_GET_M(B2, t2, value)...);
        }(WHP_GET_M(B1, t1, value)...);
#else
        (_partial_cmp(WHP_GET_M(B1, t1, value), WHP_GET_M(B2, t2, value),
            is_less)
            && ... && true);
#endif
        return is_less;
    }

    template <class Tup1, class Tup2, class... B1, class... B2>
    WHP_TUPLE_INLINE constexpr bool _less_eq(Tup1 const& t1, Tup2 const& t2, type_list<B1...>, type_list<B2...>) 
    {
        bool is_less = false;
#ifdef _MSC_VER
        bool is_eq = [&](auto&... v1) -> bool 
        {
            return [&](auto&... v2) -> bool
            {
                return (_partial_cmp(v1, v2, is_less) && ...);
            }(WHP_GET_M(B2, t2, value)...);
        }(WHP_GET_M(B1, t1, value)...);
#else
        bool is_eq = (_partial_cmp(WHP_GET_M(B1, t1, value), WHP_GET_M(B2, t2, value), is_less) && ... && true);
#endif
        return is_less || is_eq;
    }
} // namespace detail_tuple


template <class... Bases>
struct type_map : Bases... 
{
    using base_list = type_list<Bases...>;
    using Bases::operator[]...;
    using Bases::decl_elem...;

#if WHP_DEFAULTED_COMPARISON
    WHP_TUPLE_INLINE auto operator<=>(type_map const&) const = default;
    WHP_TUPLE_INLINE bool operator==(type_map const&) const = default;
#else
    WHP_TUPLE_INLINE constexpr auto operator==(type_map const& other) const 
    {
        return detail::_equals(*this, other, base_list{});
    }
    WHP_TUPLE_INLINE constexpr auto operator!=(type_map const& other) const 
    {
        return !(*this == other);
    }
    WHP_TUPLE_INLINE constexpr auto operator<(type_map const& other) const 
    {
        return detail::_less(*this, other, base_list{});
    }
    WHP_TUPLE_INLINE constexpr auto operator<=(type_map const& other) const 
    {
        return detail::_less_eq(*this, other, base_list{});
    }
    WHP_TUPLE_INLINE constexpr auto operator>(type_map const& other) const 
    {
        return detail::_less(other, *this, base_list{});
    }
    WHP_TUPLE_INLINE constexpr auto operator>=(type_map const& other) const 
    {
        return detail::_less_eq(other, *this, base_list{});
    }
#endif
};

template <size_t I, class T>
struct tuple_elem 
{
    // Like declval, but with the element
    static T decl_elem(tag<I>);
    using type = T;

    WHP_NO_UNIQUE_ADDRESS T value;

    WHP_TUPLE_INLINE constexpr decltype(auto) operator[](tag<I>)& 
    {
        return (value);
    }
    WHP_TUPLE_INLINE constexpr decltype(auto) operator[](tag<I>) const& 
    {
        return (value);
    }
    WHP_TUPLE_INLINE constexpr decltype(auto) operator[](tag<I>)&& 
    {
        return (static_cast<tuple_elem&&>(*this).value);
    }
#if WHP_DEFAULTED_COMPARISON
    WHP_TUPLE_INLINE auto operator<=>(tuple_elem const&) const = default;
    WHP_TUPLE_INLINE bool operator==(tuple_elem const&) const = default;
    WHP_TUPLE_INLINE constexpr auto operator<=>(tuple_elem const& other) const noexcept(noexcept(value <=> other.value)) requires(std::is_reference_v<T>&& ordered<T>)
    {
        return value <=> other.value;
    }
    WHP_TUPLE_INLINE constexpr bool operator==(tuple_elem const& other) const noexcept(noexcept(value == other.value)) requires(std::is_reference_v<T>&& equality_comparable<T>)
    {
        return value == other.value;
    }
#else
    WHP_TUPLE_COMPARISON_OPERATOR_1(tuple_elem, value, == )
    WHP_TUPLE_COMPARISON_OPERATOR_1(tuple_elem, value, != )
    WHP_TUPLE_COMPARISON_OPERATOR_1(tuple_elem, value, < )
    WHP_TUPLE_COMPARISON_OPERATOR_1(tuple_elem, value, <= )
    WHP_TUPLE_COMPARISON_OPERATOR_1(tuple_elem, value, > )
    WHP_TUPLE_COMPARISON_OPERATOR_1(tuple_elem, value, >= )
#endif
};

namespace detail_tuple
{
    template <class IndexSequence, class... T>
    struct get_tuple_base;

    template <size_t... I, class... T>
    struct get_tuple_base<std::index_sequence<I...>, T...> {
        using type = type_map<tuple_elem<I, T>...>;
    };
} // namespace detail_tuple

template <class... T>
using tuple_base_t = typename detail_tuple::get_tuple_base<tag_range<sizeof...(T)>, T...>::type;

// todo line 699

_WHIP_END