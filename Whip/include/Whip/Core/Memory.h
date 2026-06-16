#pragma once

#include "Core.h"

#include <memory>
#include <utility>

_WHIP_START

using RendererId = unsigned int;

template <class _Ty>
using Scope = std::unique_ptr<_Ty>;

template <class _Ty>
using Ref = std::shared_ptr<_Ty>;

template <class _Ty, class... _Types, std::enable_if_t<!std::is_array_v<_Ty>, int> = 0>
WHP_NODISCARD_MSG("Scope returned as unnecessary") inline Scope<_Ty> MakeScope(_Types&&... _Args)
{
	return Scope<_Ty>(new _Ty(std::forward<_Types>(_Args)...));
}

template <class _Ty, std::enable_if_t<std::is_array_v<_Ty> && std::extent_v<_Ty> == 0, int> = 0>
WHP_NODISCARD_MSG("Scope returned as unnecessary") inline Scope<_Ty> MakeScope(const size_t _Size)
{
	using _Elem = std::remove_extent_t<_Ty>;
	return Scope<_Ty>(new _Elem[_Size]());
}

template <class _Ty, class... _Types, std::enable_if_t<std::extent_v<_Ty> != 0, int> = 0>
void MakeScope(_Types&&...) = delete;

template <class T, class... _Types>
WHP_NODISCARD std::enable_if_t<!std::is_array_v<T>, Ref<T>> MakeRef(_Types&&... args)
{
	return std::make_shared<T, _Types...>(args...);
}

_WHIP_END
