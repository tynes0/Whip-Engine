#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/TemplatesAndContainers/Tag.h>
#include <Whip/Core/TemplatesAndContainers/TypeTraits.h>
#include <Whip/Core/TemplatesAndContainers/Utility.h>
#include <Whip/Core/TemplatesAndContainers/ReferenceWrapper.h>
#include <Whip/Core/TemplatesAndContainers/Concepts.h>
#include <Whip/Core/TemplatesAndContainers/Pair.h>

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
    struct get_tuple_base<std::index_sequence<I...>, T...> 
    {
        using type = type_map<tuple_elem<I, T>...>;
    };
} // namespace detail_tuple

template <class... T>
using tuple_base_t = typename detail_tuple::get_tuple_base<tag_range<sizeof...(T)>, T...>::type;

template <class... T>
struct tuple;

namespace detail_tuple 
{
    template <class Tup, class F, class... B>
    WHP_TUPLE_INLINE constexpr void _for_each(Tup&& tup, F&& func, type_list<B...>) 
    {
        (void(func(WHP_FWD_M(Tup, B, tup, value))), ...);
    }

    template <class Tup, class F, class... B>
    WHP_TUPLE_INLINE constexpr bool _any(Tup&& tup, F&& func, type_list<B...>) 
    {
#ifdef _MSC_VER
        return [&](auto&&... v1) -> bool 
        {
            return (bool(func(static_cast<decltype(v1)&&>(v1))) || ...);
        }(WHP_FWD_M(Tup, B, tup, value)...);
#else
        return (bool(func(WHP_FWD_M(Tup, B, tup, value))) || ...);
#endif
    }

    template <class Tup, class F, class... B>
    WHP_TUPLE_INLINE constexpr bool _all(Tup&& tup, F&& func, type_list<B...>) {
#ifdef _MSC_VER
        return [&](auto&&... v1) -> bool 
        {
            return (bool(func(static_cast<decltype(v1)&&>(v1))) && ...);
        }(WHP_FWD_M(Tup, B, tup, value)...);
#else
        return (bool(func(WHP_FWD_M(Tup, B, tup, value))) && ...);
#endif
    }

    template <class Tup, class F, class... B>
    WHP_TUPLE_INLINE constexpr auto _map(Tup&& tup, F&& func, type_list<B...>)-> tuple<decltype(func(WHP_FWD_M(Tup, B, tup, value)))...> 
    {
        return { func(WHP_FWD_M(Tup, B, tup, value))... };
    }

    template <class Tup, class F, class... B>
    WHP_TUPLE_INLINE constexpr decltype(auto) _apply(Tup&& t, F&& f, type_list<B...>) 
    {
        return static_cast<F&&>(f)(WHP_FWD_M(Tup, B, t, value)...);
    }

    template <class U, class Tup, class... B>
    WHP_TUPLE_INLINE constexpr U _convert(Tup&& t, type_list<B...>) 
    {
        return U{ WHP_FWD_M(Tup, B, t, value)... };
    }
} // namespace detail_tuple

template <class... T>
struct tuple : tuple_base_t<T...> 
{
    constexpr static size_t N = sizeof...(T);
    constexpr static bool
#if _MSC_VER
        nothrow_swappable = ::whip::detail_tuple::extra_detail::_all_true<std::is_nothrow_swappable_v<T>...>();
#else
        nothrow_swappable = (std::is_nothrow_swappable_v<T> && ...);
#endif
    using super = tuple_base_t<T...>;
    using super::operator[];
    using base_list = typename super::base_list;
    using element_list = type_list<T...>;
    using super::decl_elem;

    template <WHP_OTHER_THAN(tuple, U)>
    WHP_TUPLE_INLINE constexpr auto& operator=(U&& tup) 
    {
        using tuple2 = std::decay_t<U>;
        if constexpr (base_list_tuple_v<tuple2>) 
        {
            _assign_tup(static_cast<U&&>(tup), base_list{}, typename tuple2::base_list{});
        }
        else 
        {
            _assign_index_tup(static_cast<U&&>(tup), tag_range<N>());
        }
        return *this;
    }

    template <class... U>
    WHP_WEAK_REQUIRES((assignable_to<U, T> && ...)) 
    constexpr auto& assign(U&&... values) 
    {
        _assign(base_list{}, static_cast<U&&>(values)...);
        return *this;
    }

#if WHP_DEFAULTED_COMPARISON
    WHP_TUPLE_INLINE auto operator<=>(tuple const&) const = default;
    WHP_TUPLE_INLINE bool operator==(tuple const&) const = default;
    WHP_TUPLE_INLINE bool operator!=(tuple const&) const = default;
    WHP_TUPLE_INLINE bool operator<(tuple const&) const = default;
    WHP_TUPLE_INLINE bool operator>(tuple const&) const = default;
    WHP_TUPLE_INLINE bool operator<=(tuple const&) const = default;
    WHP_TUPLE_INLINE bool operator>=(tuple const&) const = default;
#else
    WHP_TUPLE_INLINE constexpr auto operator==(tuple const& other) const 
    {
        return detail_tuple::_equals(*this, other, base_list{});
    }
    WHP_TUPLE_INLINE constexpr auto operator!=(tuple const& other) const 
    {
        return !(*this == other);
    }
    WHP_TUPLE_INLINE constexpr auto operator<(tuple const& other) const 
    {
        return detail_tuple::::_less(*this, other, base_list{});
    }
    WHP_TUPLE_INLINE constexpr auto operator<=(tuple const& other) const 
    {
        return detail_tuple::::_less_eq(*this, other, base_list{});
    }
    WHP_TUPLE_INLINE constexpr auto operator>(tuple const& other) const 
    {
        return detail_tuple::::_less(other, *this, base_list{});
    }
    WHP_TUPLE_INLINE constexpr auto operator>=(tuple const& other) const 
    {
        return detail_tuple::::_less_eq(other, *this, base_list{});
    }
#endif
    template <class... U>
    WHP_TUPLE_INLINE constexpr auto operator==(tuple<U...> const& other) const -> WHP_TYPES_EQ_WITH(T, U) 
    {
        using other_base_list = typename tuple<U...>::base_list;
        return detail_tuple::_equals(*this, other, base_list{}, other_base_list{});
    }
    template <class... U>
    WHP_TUPLE_INLINE constexpr auto operator!=(tuple<U...> const& other) const -> WHP_TYPES_EQ_WITH(T, U) 
    {
        return !(*this == other);
    }
    template <class... U>
    WHP_TUPLE_INLINE constexpr auto operator<(tuple<U...> const& other) const -> WHP_TYPES_CMP_WITH(T, U) 
    {
        using other_base_list = typename tuple<U...>::base_list;
        return detail_tuple::_less(*this, other, base_list{}, other_base_list{});
    }
    template <class... U>
    WHP_TUPLE_INLINE constexpr auto operator<=(tuple<U...> const& other) const ->WHP_TYPES_CMP_WITH(T, U) 
    {
        using other_base_list = typename tuple<U...>::base_list;
        return detail_tuple::_less_eq(*this, other, base_list{}, other_base_list{});
    }
    template <class... U>
    WHP_TUPLE_INLINE constexpr auto operator>(tuple<U...> const& other) const ->WHP_TYPES_CMP_WITH(T, U) 
    {
        using other_base_list = typename tuple<U...>::base_list;
        return detail_tuple::_less(other, *this, other_base_list{}, base_list{});
    }
    template <class... U>
    WHP_TUPLE_INLINE constexpr auto operator>=(tuple<U...> const& other) const ->WHP_TYPES_CMP_WITH(T, U) 
    {
        using other_base_list = typename tuple<U...>::base_list;
        return detail_tuple::_less_eq(other, *this, other_base_list{}, base_list{});
    }

    WHP_TUPLE_INLINE constexpr void swap(tuple& other) noexcept(nothrow_swappable) 
    {
        _swap(other, base_list{});
    }

    template <class F>
    WHP_TUPLE_INLINE constexpr void for_each(F&& func) & 
    {
        detail_tuple::_for_each(*this, static_cast<F&&>(func), base_list{});
    }

    template <class F>
    WHP_TUPLE_INLINE constexpr void for_each(F&& func) const& 
    {
        detail_tuple::_for_each(*this, static_cast<F&&>(func), base_list{});
    }

    template <class F>
    WHP_TUPLE_INLINE constexpr void for_each(F&& func) && 
    {
        detail_tuple::_for_each(static_cast<tuple&&>(*this), static_cast<F&&>(func), base_list{});
    }

    template <class F>
    WHP_TUPLE_INLINE constexpr bool any(F&& func) & 
    {
        return detail_tuple::_any(*this, static_cast<F&&>(func), base_list{});
    }

    template <class F>
    WHP_TUPLE_INLINE constexpr bool any(F&& func) const& 
    {
        return detail_tuple::_any(*this, static_cast<F&&>(func), base_list{});
    }

    template <class F>
    WHP_TUPLE_INLINE constexpr bool any(F&& func) && 
    {
        return detail_tuple::_any(static_cast<tuple&&>(*this), static_cast<F&&>(func), base_list{});
    }

    template <class F>
    WHP_TUPLE_INLINE constexpr bool all(F&& func) &
    {
        return detail_tuple::_all(*this, static_cast<F&&>(func), base_list{});
    }

    template <class F>
    WHP_TUPLE_INLINE constexpr bool all(F&& func) const& 
    {
        return detail_tuple::_all(*this, static_cast<F&&>(func), base_list{});
    }

    template <class F>
    WHP_TUPLE_INLINE constexpr bool all(F&& func) && 
    {
        return detail_tuple::_all(static_cast<tuple&&>(*this), static_cast<F&&>(func), base_list{});
    }

    template <class F>
    WHP_TUPLE_INLINE constexpr auto map(F&& func)&
    {
        return detail_tuple::_map(*this, static_cast<F&&>(func), base_list{});
    }
    template <class F>
    WHP_TUPLE_INLINE constexpr auto map(F&& func) const& 
    {
        return detail_tuple::_map(*this, static_cast<F&&>(func), base_list{});
    }
    template <class F>
    WHP_TUPLE_INLINE constexpr auto map(F&& func) && 
    {
        return detail_tuple::_map(static_cast<tuple&&>(*this), static_cast<F&&>(func), base_list{});
    }

    template <class F>
    WHP_TUPLE_INLINE constexpr decltype(auto) apply(F&& func) & 
    {
        return detail_tuple::_apply(*this, static_cast<F&&>(func), base_list{});
    }
    template <class F>
    WHP_TUPLE_INLINE constexpr decltype(auto) apply(F&& func) const& 
    {
        return detail_tuple::_apply(*this, static_cast<F&&>(func), base_list{});
    }
    template <class F>
    WHP_TUPLE_INLINE constexpr decltype(auto) apply(F&& func) && 
    {
        return detail_tuple::_apply(static_cast<tuple&&>(*this), static_cast<F&&>(func), base_list{});
    }


    template <class... U>
    constexpr explicit operator whip::tuple<U...>() & 
    {
        static_assert(sizeof...(U) == N, "Can only convert to tuples with the same number of items");
        return detail_tuple::_convert<whip::tuple<U...>>(*this, base_list{});
    }
    template <class... U>
    constexpr explicit operator whip::tuple<U...>() const&
    {
        static_assert(sizeof...(U) == N, "Can only convert to tuples with the same number of items");
        return detail_tuple::_convert<whip::tuple<U...>>(*this, base_list{});
    }
    template <class... U>
    constexpr explicit operator whip::tuple<U...>()&& 
    {
        static_assert(sizeof...(U) == N, "Can only convert to tuples with the same number of items");
        return detail_tuple::_convert<whip::tuple<U...>>(static_cast<tuple&&>(*this), base_list{});
    }

    template <class U>
    WHP_TUPLE_INLINE constexpr U as()& 
    {
        return detail_tuple::_convert<U>(*this, base_list{});
    }

    template <class U>
    WHP_TUPLE_INLINE constexpr U as() const& 
    {
        return detail_tuple::_convert<U>(*this, base_list{});
    }

    template <class U>
    WHP_TUPLE_INLINE constexpr U as() && 
    {
        return detail_tuple::_convert<U>(static_cast<tuple&&>(*this), base_list{});
    }

private:
    template <class... B>
    WHP_TUPLE_INLINE constexpr void _swap(tuple& other, type_list<B...>) noexcept(nothrow_swappable) 
    {
        (whip::swap(B::value, WHP_GET_M(B, other, value)), ...);
    }

    template <class U, class... B1, class... B2>
    WHP_TUPLE_INLINE constexpr void _assign_tup(U&& u, type_list<B1...>, type_list<B2...>) 
    {

        (void(B1::value = WHP_FWD_M(U, B2, u, value)), ...);
    }

    template <class U, size_t... I>
    WHP_TUPLE_INLINE constexpr void _assign_index_tup(U&& u, std::index_sequence<I...>) 
    {
        // todo: make this whip
        using std::get;
        (void(tuple_elem<I, T>::value = get<I>(static_cast<U&&>(u))), ...);
    }
    template <class... U, class... B>
    WHP_TUPLE_INLINE constexpr void _assign(type_list<B...>, U&&... u) 
    {
        (void(B::value = static_cast<U&&>(u)), ...);
    }
};

template <class Tuple>
struct convert 
{
    using base_list = typename std::decay_t<Tuple>::base_list;
    Tuple tuple;
    template <class U>
    constexpr operator U() && 
    {
        return detail_tuple::_convert<U>(static_cast<Tuple&&>(tuple), base_list{});
    }
};

template <class Tuple>
convert(Tuple&) -> convert<Tuple&>;
template <class Tuple>
convert(Tuple const&) -> convert<Tuple const&>;
template <class Tuple>
convert(Tuple&&) -> convert<Tuple>;

template <size_t I, WHP_WEAK_CONCEPT(indexable) Tup>
WHP_TUPLE_INLINE constexpr decltype(auto) get(Tup&& tup) 
{
    return static_cast<Tup&&>(tup)[tag<I>()];
}

template <class... T>
WHP_TUPLE_INLINE constexpr tuple<T&...> tie(T&... t) 
{
    return { t... };
}

template <class F, WHP_WEAK_CONCEPT(base_list_tuple) Tup>
WHP_TUPLE_INLINE constexpr decltype(auto) apply(F&& func, Tup&& tup) 
{
    return detail_tuple::_apply(static_cast<Tup&&>(tup), static_cast<F&&>(func), typename std::decay_t<Tup>::base_list{});
}

template <class... T>
WHP_TUPLE_INLINE void swap(tuple<T...>& a, tuple<T...>& b) noexcept(tuple<T...>::nothrow_swappable) 
{
    a.swap(b);
}

template <typename... Ts>
WHP_TUPLE_INLINE constexpr auto make_tuple(Ts&&... args) 
{
    return tuple<unwrap_ref_decay_t<Ts>...> {static_cast<Ts&&>(args)...};
}

template <typename... T>
WHP_TUPLE_INLINE constexpr auto forward_as_tuple(T&&... a) noexcept 
{
    return tuple<T&&...> {static_cast<T&&>(a)...};
}

namespace detail_tuple
{
    template <class T, class... Q>
    WHP_TUPLE_INLINE constexpr auto _repeat_type(type_list<Q...>) 
    {
        return type_list<first_t<T, Q>...> {};
    }
    template <class... Outer>
    WHP_TUPLE_INLINE constexpr auto _get_outer_bases(type_list<Outer...>)
    {
        return (_repeat_type<Outer>(base_list_t<type_t<Outer>> {}) + ...);
    }
    template <class... Outer>
    WHP_TUPLE_INLINE constexpr auto _get_inner_bases(type_list<Outer...>) 
    {
        return (base_list_t<type_t<Outer>> {} + ...);
    }

    template <class T, class... Outer, class... Inner>
    WHP_TUPLE_INLINE constexpr auto _tuple_cat(T tup, type_list<Outer...>, type_list<Inner...>) -> tuple<type_t<Inner>...> 
    {
        return { WHP_FWD_M(type_t<Outer>, Inner, WHP_GET_M(Outer, tup, value), value)... };
    }
}

template <WHP_WEAK_CONCEPT(base_list_tuple)... T>
constexpr auto tuple_cat(T&&... ts) 
{
    if constexpr (sizeof...(T) == 0) 
    {
        return tuple<>();
    }
    else 
    {
#if !defined(WHP_CAT_BY_FORWARDING_TUPLE)
#if defined(__clang__)
#define WHP_CAT_BY_FORWARDING_TUPLE 0
#else
#define WHP_CAT_BY_FORWARDING_TUPLE 1
#endif
#endif
#if WHP_CAT_BY_FORWARDING_TUPLE
        using big_tuple = tuple<T&&...>;
#else
        using big_tuple = tuple<std::decay_t<T>...>;
#endif
        using outer_bases = base_list_t<big_tuple>;
        constexpr auto outer = detail_tuple::_get_outer_bases(outer_bases{});
        constexpr auto inner = detail_tuple::_get_inner_bases(outer_bases{});
        return detail_tuple::_tuple_cat(big_tuple{ static_cast<T&&>(ts)... }, outer, inner);
    }
}

template <class... T>
struct tuple_size<tuple<T...>> : integral_constant<size_t, sizeof...(T)> {};

template <size_t I, class... T>
struct tuple_element<I, tuple<T...>> 
{
    using type = decltype(whip::tuple<T...>::decl_elem(whip::tag<I>()));
};


#undef WHP_TUPLE_COMPARISON_OPERATOR_1 
#undef WHP_TUPLE_INLINE
#undef WHP_FWD_M
#undef WHP_GET_M
#undef WHP_DEFAULTED_COMPARISON
#undef WHP_OTHER_THAN
#undef WHP_WEAK_CONCEPT
#undef WHP_WEAK_REQUIRES
#undef WHP_TYPES_EQ_WITH
#undef WHP_TYPES_CMP_WITH
#undef WHP_NO_UNIQUE_ADDRESS
#undef WHP_HAS_NO_UNIQUE_ADDRESS
#undef WHP_CAT_BY_FORWARDING_TUPLE

_WHIP_END