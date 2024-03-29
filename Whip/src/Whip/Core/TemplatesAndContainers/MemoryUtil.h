#pragma once
#ifndef _WHIP_MEMORY_UTIL_
#define _WHIP_MEMORY_UTIL_

#include "Whip/Core/Core.h"
#include "Allocator.h"

#pragma warning(push)
#pragma warning(disable : _WHP_DISABLED_WARNINGS)

_WHIP_START

namespace memory
{
	template <class _Ty>
	inline void construct_range(_Ty* begin, _Ty* end)
	{
		allocator<_Ty> al;
		while (begin != end)
			al.construct(begin++);
	}

	template <class _Ty>
	inline void copy_range(const _Ty* begin, const _Ty* end, _Ty* dest)
	{
		allocator<_Ty> al;
		while (begin != end)
			al.construct(dest++, *(begin++));
	}

	template <class _Ty>
	inline void destruct_range(_Ty* begin, _Ty* end)
	{
		allocator<_Ty> al;
		while (begin != end)
			al.destroy(begin++);
	}
}

_WHIP_END

#pragma warning(pop)

#endif // !_WHIP_MEMORY_UTIL_