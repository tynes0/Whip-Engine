#pragma once

#include "Whip/Core/Core.h"
#include "Allocator.h"

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