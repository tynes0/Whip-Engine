#pragma once

#include "Core.h"
#include <memory>

_WHIP_START

typedef unsigned int renderer_id_t;

template <class _Ty>
using scope = std::unique_ptr<_Ty>;

template <class _Ty>
using ref = std::shared_ptr<_Ty>;

template <class _Ty, class... _Types, std::enable_if_t<!std::is_array_v<_Ty>, int> = 0>
WHP_NODISCARD_MSG("scope returned as unnecessary") inline scope<_Ty> make_scope(_Types&&... _Args)
{
	return scope<_Ty>(new _Ty(forward<_Types>(_Args)...));
}

template <class _Ty, std::enable_if_t<std::is_array_v<_Ty>&& std::extent_v<_Ty> == 0, int> = 0>
WHP_NODISCARD_MSG("scope returned as unnecessary") inline scope<_Ty> make_scope(const size_t _Size)
{
	using _Elem = std::remove_extent_t<_Ty>;
	return scope<_Ty>(new _Elem[_Size]());
}

template <class _Ty, class... _Types, std::enable_if_t<std::extent_v<_Ty> != 0, int> = 0>
void make_scope(_Types&&...) = delete;

template <class T, class... _Types>
WHP_NODISCARD std::enable_if_t<!std::is_array_v<T>, ref<T>> make_ref(_Types&&... args)
{
	return std::make_shared<T, _Types...>(args...);
}

_WHIP_END
