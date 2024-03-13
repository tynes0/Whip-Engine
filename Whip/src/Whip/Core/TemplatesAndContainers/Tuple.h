#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/TemplatesAndContainers/TypeTraits.h>
#include <Whip/Core/TemplatesAndContainers/ReferenceWrapper.h>

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
#define TUPLET_OTHER_THAN(Self, Other) _WHIP other_than<Self> Other
#define TUPLET_WEAK_CONCEPT(...) __VA_ARGS__
#define TUPLET_WEAK_REQUIRES(...) requires __VA_ARGS__
#define _TUPLET_TYPES_EQ_WITH(T, U) bool requires(_WHIP equality_comparable_with<T, U> && ...)
#define _TUPLET_TYPES_CMP_WITH(T, U) bool requires(_WHIP equality_comparable_with<T, U> && ...)
#else
#define TUPLET_OTHER_THAN(Self, Other) class Other, class = _WHIP detail_tuple::other_than<Self, Other>
#define TUPLET_WEAK_CONCEPT(...) class
#define TUPLET_WEAK_REQUIRES(...)
#if _MSC_VER < 1930
#define _TUPLET_TYPES_EQ_WITH(T, U) ::std::enable_if_t<_WHIP detail_tuple::extra_detail::_all_true<_WHIP detail_tuple::extra_detail::_has_eq<T, U>...>(), bool>
#define _TUPLET_TYPES_CMP_WITH(T, U) ::std::enable_if_t<_WHIP detail_tuple::extra_detail::_all_true<_WHIP detail_tuple::extra_detail::_has_cmp<T, U>...>(), bool>
#else
#define _TUPLET_TYPES_EQ_WITH(T, U) ::std::enable_if_t<((_WHIP detail_tuple::extra_detail::_has_eq<T, U>)&&...), bool>
#define _TUPLET_TYPES_CMP_WITH(T, U) ::std::enable_if_t<((_WHIP detail_tuple::extra_detail::_has_cmp<T, U>)&&...), bool>
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
    namespace extra_detail {
        template <class T, class U, class = decltype(declval<T>() == declval<U>())>
        constexpr bool _test_eq(int)
        {
            return true;
        }

        template <class T, class U>
        constexpr bool _test_eq(long long)
        {
            return false;
        }

        template <class T, class U, class = decltype(declval<T>() < declval<U>())>
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

    template <class T>
    struct unwrap_ref_decay : unwrap_reference<std::decay_t<T>> {};

    template <class T>
    using unwrap_ref_decay_t = typename unwrap_ref_decay<T>::type;

    template <class T>
    using identity_t = T;

} // namespace detail_tuple
     
_WHIP_END