#pragma once
#ifndef _WHIP_CONCEPTS_
#define _WHIP_CONCEPTS_

#include <Whip/Core/Core.h>
#include "TypeTraits.h"

#include <type_traits>

#if !_WHP_TEST_CPP_FT(concepts)
_EMIT_WHP_WARNING(WHP0005, "The contents of whip concept library are available only with C++20 concept support.");
#else // !_WHP_TEST_CPP_FT(concepts)

#pragma warning(push)
#pragma warning(disable : _WHP_DISABLED_WARNINGS)

_WHIP_START

template <class T>
concept arithmetic = std::is_integral_v<T> || std::is_floating_point_v<T>;

template <class T, class U>
concept same_as = is_same_v<T, U> && is_same_v<U, T>;

template <class T, class U>
concept other_than = !is_same_v<std::decay_t<T>, std::decay_t<U>>;

template <class T>
concept stateless = is_empty_v<std::decay_t<T>>;

template <class U, class T>
concept assignable_to = requires(U u, T t)
{
    t = u;
};

template <class T>
concept ordered = requires(T const& t)
{
    { t <=> t };
};

template <class T, class U>
concept ordered_with = requires(T const& t, U const& u)
{
    { t <=> u };
};

template <class T>
concept equality_comparable = requires(T const& t)
{
    { t == t } -> same_as<bool>;
};

template <class T, class U>
concept equality_comparable_with = requires(T const& t, U const& u)
{
    { t == u } -> same_as<bool>;
};

template <class T>
concept not_equality_comparable = requires(T const& t)
{
    { t != t } -> same_as<bool>;
};

template <class T>
concept equality_and_not_equality_compareable = equality_comparable<T> && not_equality_comparable<T>;

template <class T, class U>
concept not_equality_comparable_with = requires(T const& t, U const& u)
{
    { t != u } -> same_as<bool>;
};

template <class T>
concept less_then_compareable = equality_comparable<T> && requires(T const& t)
{
    { t < t } -> same_as<bool>;
};

template <class T, class U>
concept partial_comparable_with = equality_comparable_with<T, U> && requires(T const& t, U const& u)
{
    { t < u } -> same_as<bool>;
    { t > u } -> same_as<bool>;
};

_WHIP_END

#pragma warning(pop)

#endif // _WHP_HAS_CPP_VERSION(20)

#endif // !_WHIP_CONCEPTS_