#pragma once

#include <Whip/Core/Core.h>
#include "TypeTraits.h"

#include <type_traits>
_WHIP_START

#if !defined(__cpp_concepts)
constexpr const char* whip_concept_library_error_message = "c++ concepts is not avaible";
#else // !(__cpp_concepts)

template <class T, class U>
concept same_as = is_same_v<T, U>&& is_same_v<U, T>;

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
concept partial_comparable = equality_comparable<T> && requires(T const& t)
{
    { t < t } -> same_as<bool>;
};

template <class T, class U>
concept partial_comparable_with = equality_comparable_with<T, U>&& requires(T const& t, U const& u)
{
    { t < u } -> same_as<bool>;
    { t > u } -> same_as<bool>;
};
#endif // __cpp_concepts

_WHIP_END