#pragma once

#include <Whip/Core/Core.h>

_WHIP_START

template <class _Ty = void>
struct plus 
{
    WHP_NODISCARD constexpr _Ty operator()(const _Ty& _Left, const _Ty& _Right) const
    {
        return _Left + _Right;
    }
};

template <class _Ty = void>
struct minus 
{
    WHP_NODISCARD constexpr _Ty operator()(const _Ty& _Left, const _Ty& _Right) const
    {
        return _Left - _Right;
    }
};

template <class _Ty = void>
struct multiplies 
{
    WHP_NODISCARD constexpr _Ty operator()(const _Ty& _Left, const _Ty& _Right) const
    {
        return _Left * _Right;
    }
};

template <class _Ty = void>
struct equal_to 
{
    WHP_NODISCARD constexpr bool operator()(const _Ty& _Left, const _Ty& _Right) const
    {
        return _Left == _Right;
    }
};

template <class _Ty = void>
struct not_equal_to 
{
    WHP_NODISCARD constexpr bool operator()(const _Ty& _Left, const _Ty& _Right) const
    {
        return _Left != _Right;
    }
};

template <class _Ty = void>
struct greater 
{
    WHP_NODISCARD constexpr bool operator()(const _Ty& _Left, const _Ty& _Right) const
    {
        return _Left > _Right;
    }
};

template <class _Ty = void>
struct less 
{
    WHP_NODISCARD constexpr bool operator()(const _Ty& _Left, const _Ty& _Right) const
    {
        return _Left < _Right;
    }
};

template <class _Ty = void>
struct greater_equal 
{
    WHP_NODISCARD constexpr bool operator()(const _Ty& _Left, const _Ty& _Right) const
    {
        return _Left >= _Right;
    }
};

template <class _Ty = void>
struct less_equal 
{
    WHP_NODISCARD constexpr bool operator()(const _Ty& _Left, const _Ty& _Right) const
    {
        return _Left <= _Right;
    }
};

template <>
struct plus<void> 
{
    template <class _Ty1, class _Ty2>
    WHP_NODISCARD constexpr auto operator()(_Ty1&& _Left, _Ty2&& _Right) const
        noexcept(noexcept(static_cast<_Ty1&&>(_Left) + static_cast<_Ty2&&>(_Right)))
        -> decltype(static_cast<_Ty1&&>(_Left) + static_cast<_Ty2&&>(_Right)) 
    {
        return static_cast<_Ty1&&>(_Left) + static_cast<_Ty2&&>(_Right);
    }

    using is_transparent = int;
};

template <>
struct minus<void> 
{
    template <class _Ty1, class _Ty2>
    WHP_NODISCARD constexpr auto operator()(_Ty1&& _Left, _Ty2&& _Right) const
        noexcept(noexcept(static_cast<_Ty1&&>(_Left) - static_cast<_Ty2&&>(_Right)))
        -> decltype(static_cast<_Ty1&&>(_Left) - static_cast<_Ty2&&>(_Right)) 
    {
        return static_cast<_Ty1&&>(_Left) - static_cast<_Ty2&&>(_Right);
    }

    using is_transparent = int;
};

template <>
struct multiplies<void> 
{
    template <class _Ty1, class _Ty2>
    WHP_NODISCARD constexpr auto operator()(_Ty1&& _Left, _Ty2&& _Right) const
        noexcept(noexcept(static_cast<_Ty1&&>(_Left)* static_cast<_Ty2&&>(_Right)))
        -> decltype(static_cast<_Ty1&&>(_Left)* static_cast<_Ty2&&>(_Right)) 
    {
        return static_cast<_Ty1&&>(_Left) * static_cast<_Ty2&&>(_Right);
    }

    using is_transparent = int;
};

template <>
struct equal_to<void>
{
    template <class _Ty1, class _Ty2>
    WHP_NODISCARD constexpr auto operator()(_Ty1&& _Left, _Ty2&& _Right) const
        noexcept(noexcept(static_cast<_Ty1&&>(_Left) == static_cast<_Ty2&&>(_Right)))
        -> decltype(static_cast<_Ty1&&>(_Left) == static_cast<_Ty2&&>(_Right)) 
    {
        return static_cast<_Ty1&&>(_Left) == static_cast<_Ty2&&>(_Right);
    }

    using is_transparent = int;
};

template <>
struct not_equal_to<void> 
{
    template <class _Ty1, class _Ty2>
    WHP_NODISCARD constexpr auto operator()(_Ty1&& _Left, _Ty2&& _Right) const
        noexcept(noexcept(static_cast<_Ty1&&>(_Left) != static_cast<_Ty2&&>(_Right)))
        -> decltype(static_cast<_Ty1&&>(_Left) != static_cast<_Ty2&&>(_Right))
    {
        return static_cast<_Ty1&&>(_Left) != static_cast<_Ty2&&>(_Right);
    }

    using is_transparent = int;
};

template <>
struct greater<void>
{
    template <class _Ty1, class _Ty2>
    WHP_NODISCARD constexpr auto operator()(_Ty1&& _Left, _Ty2&& _Right) const
        noexcept(noexcept(static_cast<_Ty1&&>(_Left) > static_cast<_Ty2&&>(_Right)))
        -> decltype(static_cast<_Ty1&&>(_Left) > static_cast<_Ty2&&>(_Right)) 
    {
        return static_cast<_Ty1&&>(_Left) > static_cast<_Ty2&&>(_Right);
    }

    using is_transparent = int;
};

template <>
struct less<void> 
{
    template <class _Ty1, class _Ty2>
    WHP_NODISCARD constexpr auto operator()(_Ty1&& _Left, _Ty2&& _Right) const
        noexcept(noexcept(static_cast<_Ty1&&>(_Left) < static_cast<_Ty2&&>(_Right)))
        -> decltype(static_cast<_Ty1&&>(_Left) < static_cast<_Ty2&&>(_Right)) 
    {
        return static_cast<_Ty1&&>(_Left) < static_cast<_Ty2&&>(_Right);
    }

    using is_transparent = int;
};

template <>
struct greater_equal<void> 
{
    template <class _Ty1, class _Ty2>
    WHP_NODISCARD constexpr auto operator()(_Ty1&& _Left, _Ty2&& _Right) const
        noexcept(noexcept(static_cast<_Ty1&&>(_Left) >= static_cast<_Ty2&&>(_Right)))
        -> decltype(static_cast<_Ty1&&>(_Left) >= static_cast<_Ty2&&>(_Right))
    {
        return static_cast<_Ty1&&>(_Left) >= static_cast<_Ty2&&>(_Right);
    }

    using is_transparent = int;
};

template <>
struct less_equal<void> 
{
    template <class _Ty1, class _Ty2>
    WHP_NODISCARD constexpr auto operator()(_Ty1&& _Left, _Ty2&& _Right) const
        noexcept(noexcept(static_cast<_Ty1&&>(_Left) <= static_cast<_Ty2&&>(_Right)))
        -> decltype(static_cast<_Ty1&&>(_Left) <= static_cast<_Ty2&&>(_Right)) 
    {
        return static_cast<_Ty1&&>(_Left) <= static_cast<_Ty2&&>(_Right);
    }

    using is_transparent = int;
};

_WHIP_END
