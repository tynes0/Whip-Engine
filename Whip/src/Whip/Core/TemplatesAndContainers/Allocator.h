#pragma once

#include "Whip/Core/Core.h"

_WHIP_START

template <typename _Ty>
class allocator {
public:
	using value_type = _Ty;
	using pointer = _Ty*;
	using const_pointer = const _Ty*;
	using reference = _Ty&;
	using const_reference = const _Ty&;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	allocator() noexcept = default;

	template <typename U>
	constexpr allocator(const allocator<U>&) noexcept {}

	_Ty* allocate(size_t n) 
	{
		if (pointer ptr = static_cast<_Ty*>(::operator new(n * sizeof(_Ty))))
		{
			return ptr;
		}
		WHP_CORE_ASSERT(false, "Allocation failed.");
		return nullptr;
	}

	void deallocate(_Ty* ptr, size_t n) noexcept
	{
		::operator delete(ptr, n * sizeof(_Ty));
	}

	template <typename U>
	void destroy(U* p) noexcept {
		p->~U();
	}

	template <typename U, typename... Args>
	void construct(U* p, Args&&... args) noexcept
	{
		::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
	}

	template <typename U>
	struct rebind {
		using other = allocator<U>;
	};
};

template <typename _Ty, typename _Uty>
constexpr bool operator==(const allocator<_Ty>&, const allocator<_Uty>&) noexcept {
	return true;
}

template <typename _Ty, typename _Uty>
constexpr bool operator!=(const allocator<_Ty>& a, const allocator<_Uty>& b) noexcept {
	return !(a == b);
}

template <class _Alloc>
using alloc_ptr_t = typename std::allocator_traits<_Alloc>::pointer;

template <class _Alloc>
using alloc_size_t = typename std::allocator_traits<_Alloc>::size_type;

template <class _Alloc>
struct alloc_construct_ptr
{
	using pointer = alloc_ptr_t<_Alloc>;

	_Alloc& al;
	pointer ptr;

	constexpr explicit alloc_construct_ptr(_Alloc _al_) : al(_al_), ptr(nullptr) {}

	WHP_NODISCARD constexpr pointer release() noexcept
	{
		return exchange(ptr, nullptr);
	}

	constexpr void allocate()
	{
		ptr = nullptr;
		ptr = al.allocate(1);
	}

	~alloc_construct_ptr()
	{
		if (ptr)
		{
			al.deallocate(ptr, 1);
		}
	}

	alloc_construct_ptr(const alloc_construct_ptr&) = delete;
	alloc_construct_ptr& operator=(const alloc_construct_ptr&) = delete;
};

struct fake_allocator {};


_WHIP_END