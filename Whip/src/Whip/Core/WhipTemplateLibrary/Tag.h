#pragma once
#ifndef _WHIP_TAG_
#define _WHIP_TAG_

#include <Whip/Core/Core.h>
#include "TypeTraits.h"
#include "Concepts.h"

#include <type_traits>

#pragma warning(push)
#pragma warning(disable : _WHP_DISABLED_WARNINGS)

_WHIP_START

template <size_t I>
using tag = integral_constant<size_t, I>;

template <size_t I>
constexpr tag<I> tag_v{};

template <size_t N>
using tag_range = std::make_index_sequence<N>;

#if _WHP_TEST_CPP_FT(concepts)

template <class T>
concept indexable = stateless<T> || requires(T t)
{
    t[tag<0>()];
};

#endif // _WHP_TEST_CPP_FT(concepts)

inline namespace literals
{
    inline namespace tag_literals
    {
        namespace detail_tag_literals
        {
            template <char... D>
            constexpr size_t size_t_from_digits()
            {
                static_assert((('0' <= D && D <= '9') && ...), "Must be integral literal");
                size_t num = 0;
                return ((num = num * 10 + (D - '0')), ..., num);
            }
        }

        template <char... D>
        constexpr auto operator""_tag() noexcept -> tag<detail_tag_literals::size_t_from_digits<D...>()>
        {
            return {};
        }
    }
}

_WHIP_END

#pragma warning(pop)

#endif // !_WHIP_TAG_