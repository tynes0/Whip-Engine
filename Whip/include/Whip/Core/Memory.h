#pragma once

#include "Core.h"
#include <Whip/Core/Memory/AllocatorRegistry.h>
#include <Whip/Core/Memory/Construct.h>

#include <memory>
#include <type_traits>
#include <utility>

_WHIP_START

using RendererId = unsigned int;

namespace detail
{
	template <class T>
	struct AllocatorDelete
	{
		memory::Allocator* m_Allocator = &memory::GetDefaultAllocator();

		AllocatorDelete() = default;
		explicit AllocatorDelete(memory::Allocator& allocator)
			: m_Allocator(&allocator)
		{
		}

		void operator()(T* pointer) const
		{
			memory::Delete(*m_Allocator, pointer);
		}
	};

	template <class T>
	struct AllocatorDelete<T[]>
	{
		memory::Allocator* m_Allocator = &memory::GetDefaultAllocator();

		AllocatorDelete() = default;
		explicit AllocatorDelete(memory::Allocator& allocator)
			: m_Allocator(&allocator)
		{
		}

		void operator()(T* pointer) const
		{
			memory::DeleteArray(*m_Allocator, pointer);
		}
	};
}

template <class _Ty>
using Scope = std::unique_ptr<_Ty, detail::AllocatorDelete<_Ty>>;

template <class _Ty>
using Ref = std::shared_ptr<_Ty>;

template <class _Ty>
WHP_NODISCARD_MSG("Scope returned as unnecessary") inline Scope<_Ty[]> MakeScopeArrayTagged(memory::MemoryTag tag, const size_t size)
{
	memory::Allocator& allocator = memory::GetAllocator(tag);
	return Scope<_Ty[]>(
		memory::NewArray<_Ty>(allocator, size, tag, WHIP_MEMORY_LOCATION),
		detail::AllocatorDelete<_Ty[]>(allocator));
}

template <class _Ty, class... _Types, std::enable_if_t<!std::is_array_v<_Ty>, int> = 0>
WHP_NODISCARD_MSG("Scope returned as unnecessary") inline Scope<_Ty> MakeScopeTagged(memory::MemoryTag tag, _Types&&... _Args)
{
	memory::Allocator& allocator = memory::GetAllocator(tag);
	return Scope<_Ty>(
		memory::New<_Ty>(allocator, tag, WHIP_MEMORY_LOCATION, std::forward<_Types>(_Args)...),
		detail::AllocatorDelete<_Ty>(allocator));
}

template <class _Ty, std::enable_if_t<std::is_array_v<_Ty> && std::extent_v<_Ty> == 0, int> = 0>
WHP_NODISCARD_MSG("Scope returned as unnecessary") inline Scope<_Ty> MakeScopeTagged(memory::MemoryTag tag, size_t size)
{
	using _Elem = std::remove_extent_t<_Ty>;
	return MakeScopeArrayTagged<_Elem>(tag, size);
}

template <class T, class... _Types>
WHP_NODISCARD std::enable_if_t<!std::is_array_v<T>, Ref<T>> MakeRefTagged(memory::MemoryTag tag, _Types&&... args);

template <class T, class... _Types>
WHP_NODISCARD std::enable_if_t<!std::is_array_v<T>, T*> MakeRawTagged(memory::MemoryTag tag, _Types&&... args);

template <class _Ty, class... _Types, std::enable_if_t<!std::is_array_v<_Ty>, int> = 0>
WHP_NODISCARD_MSG("Scope returned as unnecessary") inline Scope<_Ty> MakeScope(_Types&&... _Args)
{
	return MakeScopeTagged<_Ty>(memory::MemoryTag::Unknown, std::forward<_Types>(_Args)...);
}

template <class _Ty, std::enable_if_t<std::is_array_v<_Ty> && std::extent_v<_Ty> == 0, int> = 0>
WHP_NODISCARD_MSG("Scope returned as unnecessary") inline Scope<_Ty> MakeScope(const size_t _Size)
{
	using _Elem = std::remove_extent_t<_Ty>;
	return MakeScopeArrayTagged<_Elem>(memory::MemoryTag::Unknown, _Size);
}

template <class _Ty, class... _Types, std::enable_if_t<std::extent_v<_Ty> != 0, int> = 0>
void MakeScope(_Types&&...) = delete;

template <class T, class... _Types>
WHP_NODISCARD std::enable_if_t<!std::is_array_v<T>, Ref<T>> MakeRef(_Types&&... args)
{
	return MakeRefTagged<T>(memory::MemoryTag::Unknown, std::forward<_Types>(args)...);
}

template <class T, class... _Types>
WHP_NODISCARD std::enable_if_t<!std::is_array_v<T>, Ref<T>> MakeRefTagged(memory::MemoryTag tag, _Types&&... args)
{
	return std::allocate_shared<T>(
		memory::StlAllocator<T>(memory::GetAllocator(tag), tag),
		std::forward<_Types>(args)...);
}

template <class T, class... _Types>
WHP_NODISCARD std::enable_if_t<!std::is_array_v<T>, T*> MakeRaw(_Types&&... args)
{
	return MakeRawTagged<T>(memory::MemoryTag::Unknown, std::forward<_Types>(args)...);
}

template <class T, class... _Types>
WHP_NODISCARD std::enable_if_t<!std::is_array_v<T>, T*> MakeRawTagged(memory::MemoryTag tag, _Types&&... args)
{
	return memory::New<T>(memory::GetAllocator(tag), tag, WHIP_MEMORY_LOCATION, std::forward<_Types>(args)...);
}

template <class T>
void DeleteRaw(T* pointer)
{
	memory::Delete(memory::GetDefaultAllocator(), pointer);
}

_WHIP_END
