#pragma once

#include "Whip/Core/Core.h"
#include "Utility.h"
#include "TypeTraits.h"
#include "Allocator.h"
#include "Pair.h"

_WHIP_START

template <class _Iter>
WHP_NODISCARD constexpr void* voidify_iter(_Iter it) noexcept
{
	if constexpr (is_pointer_v<_Iter>)
	{
		return const_cast<void*>(static_cast<const volatile void*>(it));
	}
	else
	{
		return const_cast<void*>(static_cast<const volatile void*>(addressof(it)));
	}
}

#ifdef _HAS_CXX20
template <class _Ty, class... _Types, class = void_t<decltype(::new(declval<void*>()) _Ty(declval<_Types>()...))>>
#endif // _HAS_CXX20
constexpr _Ty* construct_at(_Ty* const location, _Types&&... args) noexcept(noexcept(::new(voidify_iter(location)) _Ty(forward<_Types>(args)...)))
{
	return ::new (voidify_iter(location)) _Ty(forward<_Types>(args)...);
}

template <class _Ty, class... _Types>
constexpr void construct_in_place(_Ty& obj, _Types&&... args) noexcept(is_nothrow_constructible_v<_Ty, _Types...>)
{
#if _HAS_CXX20
	if (std::is_constant_evaluated())
	{
		construct_at(addressof(obj), forward<_Types>(args)...);
	}
	else
#endif // _HAS_CXX20
	{
		::new (voidify_iter(addressof(obj))) _Ty(forward<_Types>(args)...);
	}
}

template <class _Ty>
void default_construct_in_place(_Ty& obj) noexcept(is_nothrow_default_constructible_v<_Ty>)
{
	::new (voidify_iter(addressof(obj))) _Ty;
}

template <class _NoThrowFwdIt, class _NoThrowSentinel>
constexpr void destroy_range(_NoThrowFwdIt first, _NoThrowSentinel last) noexcept;

template <class _Ty>
constexpr void destroy_in_place(_Ty& obj) noexcept
{
	if constexpr (is_array_v<_Ty>)
	{
		destroy_range(obj, obj + extent_v<_Ty>);
	}
	else
	{
		obj.~_Ty();
	}
}

template <class _Ty>
constexpr void destroy_at(_Ty* const location) noexcept
{
	if constexpr (is_array_v<_Ty>)
	{
		destroy_range(begin(*location), end(*location));
	}
	else
	{
		location->~_Ty();
	}
}

template <class _NoThrowFwdIt, class _NoThrowSentinel>
constexpr void destroy_range(_NoThrowFwdIt first, _NoThrowSentinel last) noexcept
{
	if constexpr (!is_trivially_destructible_v<std::_Iter_value_t<_NoThrowFwdIt>>)
	{
		for (; first != last; ++first)
		{
			destroy_in_place(*first);
		}
	}
}

/*
class bad_weak_ptr : public exception
{
public:
	bad_weak_ptr() noexcept {}

	WHP_NODISCARD const char* what() const noexcept override
	{
		return "bad_weak_ptr";
	}
};

[[noreturn]] inline void throw_bad_weak_ptr()
{
	throw(bad_weak_ptr{});
}
*/

class ref_count_base
{
private:
	virtual void destroy() noexcept = 0;
	virtual void delete_this() noexcept = 0;

	ref_counter_t uses = 1;
	ref_counter_t weaks = 1;

protected:
	constexpr ref_count_base() noexcept = default;

public:
	ref_count_base(const ref_count_base&) = delete;
	ref_count_base operator=(const ref_count_base&) = delete;

	virtual ~ref_count_base() noexcept {}

	bool incref_nz() noexcept
	{
		if (uses == 0)
		{
			return false;
		}
		++uses;
		return true;
	}

	void incref() noexcept
	{
		++uses;
	}

	void incwref() noexcept
	{
		++weaks;
	}

	void decref() noexcept
	{
		if (--uses == 0)
		{
			destroy();
			decwref();
		}
	}

	void decwref() noexcept
	{
		if (--weaks == 0)
		{
			delete_this();
		}
	}

	long use_count() const noexcept
	{
		return static_cast<long>(uses);
	}

	virtual void* get_deleter(const std::type_info&) const noexcept
	{
		return nullptr;
	}
};

template <class _Ty>
class ref_count : public ref_count_base
{
public:
	explicit ref_count(_Ty* ptr) : ref_count_base(), m_ptr(ptr) {}
private:
	virtual void destroy() noexcept override
	{
		delete m_ptr;
	}

	virtual void delete_this() noexcept override
	{
		delete this;
	}

	_Ty* m_ptr;
};

template <class _Resource, class _Dx>
class ref_count_resource : public ref_count_base
{
public:
	ref_count_resource(_Resource ptr, _Dx dt) : ref_count_base(), m_pair(one_then_variadic_args_t{}, move(dt), ptr) {}

	~ref_count_resource() noexcept override {}

	virtual void* get_deleter(const std::type_info& type_id) const noexcept override
	{
		if (type_id == typeid(_Dx))
		{
			return const_cast<_Dx*>(addressof(m_pair.get_first()));
		}

		return nullptr;
	}

private:
	virtual void destroy() noexcept override
	{
		m_pair.get_first()(m_pair.second);
	}

	virtual void delete_this() noexcept override
	{
		delete this;
	}

	compressed_pair<_Dx, _Resource> m_pair;
};

template <class _Resource, class _Dx, class _Alloc = allocator<_Resource>>
class ref_count_resource_alloc : public ref_count_base
{
public:
	ref_count_resource_alloc(_Resource ptr, _Dx dt, const _Alloc& alx)
		: ref_count_base(), m_pair(one_then_variadic_args_t{}, move(dt), one_then_variadic_args_t{}, alx, ptr) {}

	~ref_count_resource_alloc() noexcept override {}

	virtual void* get_deleter(const std::type_info& type_id) const noexcept override
	{
		if (type_id == typeid(_Dx))
		{
			return const_cast<_Dx*>(addressof(m_pair.get_first()));
		}
		return nullptr;
	}

private:
	using m_al_t = std::_Rebind_alloc_t<_Alloc, ref_count_resource_alloc>;

	virtual void destroy() noexcept override
	{
		m_pair.get_first()(m_pair.second.second);
	}

	virtual void delete_this() noexcept override
	{
		m_al_t al = m_pair.second.get_first();
		this->~ref_count_base();
		al.deallocate(); // TODO: buraya bi bak, burasý hatalý muhtemelen
	}

	compressed_pair<_Dx, compressed_pair<m_al_t, _Resource>> m_pair;
};


struct for_overwrite_tag
{
	explicit for_overwrite_tag() = default;
};

template <class T>
class ref_count_obj2 : public ref_count_base
{
public:
	template <class... Types>
	explicit ref_count_obj2(Types&&... args) : ref_count_base()
	{
		if constexpr (sizeof...(Types) == 1 && (is_same_v<for_overwrite_tag, remove_reference_t<Types>>&& ...))
		{
			default_construct_in_place(m_storage.m_value);
			((void)args, ...);
		}
		else
		{
			construct_in_place(m_storage.m_value, forward<Types>(args)...);
		}
	}

	~ref_count_obj2() noexcept override {}

	union
	{
		wrap<T> m_storage;
	};

private:
	virtual void destroy() noexcept override
	{
		destroy_in_place(m_storage.m_value);
	}

	virtual void delete_this() noexcept override
	{
		delete this;
	}
};

template <size_t type_size>
WHP_NODISCARD constexpr size_t get_size_of_n(const size_t count)
{
	constexpr bool overflow_if_possible = type_size > 1;

	if constexpr (overflow_if_possible)
	{
		constexpr size_t max_possible = static_cast<size_t>(-1) / type_size;
		if (count > max_possible)
		{
			// todo: düzelt _tx_bad_array_new_length();
		}
	}

	return count * type_size;
}

template <size_t m_align>
struct alignas_storage_unit
{
	alignas(m_align) char m_space[m_align];
};

enum class check_overflow : bool
{
	_nope,
	_yes
};

template <class _Refc, check_overflow check>
WHP_NODISCARD size_t calculate_bytes_for_flexible_array(const size_t count) noexcept(check == check_overflow::_nope)
{
	constexpr size_t align = alignof(_Refc);

	size_t bytes = sizeof(_Refc);

	if (count > 1)
	{
		constexpr size_t element_size = sizeof(typename _Refc::element_type);

		size_t extra_bytes;

		if constexpr (check == check_overflow::_yes)
		{
			extra_bytes = get_size_of_n<element_size>(count - 1);

			if (extra_bytes > static_cast<size_t>(-1) - bytes - (align - 1))
			{
				// todo: düzelt _tx_bad_array_new_length();
			}
		}
		else
		{
			extra_bytes = element_size * (count - 1);
		}
		bytes += extra_bytes;

		bytes = (bytes + align - 1) & ~(align - 1);
	}

	return bytes;
}

template <class _Refc>
WHP_NODISCARD _Refc* allocate_flexible_array(const size_t count)
{
	const size_t bytes = calculate_bytes_for_flexible_array<_Refc, check_overflow::_yes>(count);
	constexpr size_t align = alignof(_Refc);
	if constexpr (align > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
	{
		return static_cast<_Refc*>(::operator new(bytes, std::align_val_t{ align }));
	}
	else
	{
		return static_cast<_Refc*>(::operator new(bytes));
	}
}

template <class _Refc>
void deallocate_flexible_array(_Refc* const ptr) noexcept
{
	constexpr size_t align = alignof(_Refc);
	if constexpr (align > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
	{
		::operator delete(static_cast<void*>(ptr), std::align_val_t{ align });
	}
	else
	{
		::operator delete(static_cast<void*>(ptr));
	}
}

template <class _NoThrowIt>
struct WHP_NODISCARD uninitialized_rev_destroying_backout
{
	_NoThrowIt first;
	_NoThrowIt last;

	explicit uninitialized_rev_destroying_backout(_NoThrowIt dest) noexcept : first(dest), last(dest) {}

	uninitialized_rev_destroying_backout(const uninitialized_rev_destroying_backout&) = delete;
	uninitialized_rev_destroying_backout operator=(const uninitialized_rev_destroying_backout&) = delete;

	~uninitialized_rev_destroying_backout()
	{
		while (last != first)
		{
			--last;
			destroy_at(addressof(*last));
		}
	}

	template <class... Types>
	void emplace_back(Types&&... vals)
	{
		construct_in_place(*last, forward<Types>(vals)...);
		++last;
	}

	void emplace_back_for_overwrite()
	{
		default_construct_in_place(*last);
		++last;
	}

	_NoThrowIt release() noexcept
	{
		first = last;
		return last;
	}
};

template <class T>
void reverse_destroy_multidimensional_n(T* const arr, size_t size) noexcept
{
	while (size > 0)
	{
		--size;
		if constexpr (is_array_v<T>)
		{
			reverse_destroy_multidimensional_n(arr[size], extent_v<T>);
		}
		else
		{
			destroy_in_place(arr[size]);
		}
	}
}

template <class T>
struct WHP_NODISCARD reverse_destroy_multidimensional_n_guard
{
	T* target;
	size_t idx;

	~reverse_destroy_multidimensional_n_guard()
	{
		if (target)
		{
			reverse_destroy_multidimensional_n(target, idx);
		}
	}
};

template <class _Ty, size_t size>
void uninitialized_copy_multidimensional(const _Ty(&in)[size], _Ty(&out)[size])
{
	if constexpr (is_trivial_v<_Ty>)
	{
		// TODO : kendin yapabilirsin
		std::_Copy_memmove_n(in, size, out);
	}
	else if constexpr (is_array_v<_Ty>)
	{
		reverse_destroy_multidimensional_n_guard<_Ty> guard{ out, 0 };
		for (size_t& idx = guard.idx; idx < size; ++idx)
		{
			uninitialized_copy_multidimensional(in[idx], out[idx]);
		}
		guard.target = nullptr;
	}
	else
	{
		uninitialized_rev_destroying_backout backout{ out };
		for (size_t idx = 0; idx < size; ++idx)
		{
			backout.emplace_back(in[idx]);
		}
	}
}

template <class _Ty>
void uninitialized_value_construct_multidimensional_n(_Ty* const out, const size_t size)
{
	using item = remove_all_extents_t<_Ty>;
	if constexpr (std::_Use_memset_value_construct_v<item*>)
	{
		std::_Zero_range(out, out + size);
	}
	else if constexpr (is_array_v<_Ty>)
	{
		reverse_destroy_multidimensional_n_guard<_Ty> guard{ out, 0 };
		for (size_t idx = guard.idx; idx < size; ++idx)
		{
			uninitialized_value_construct_multidimensional_n(out[idx], extent_v<_Ty>);
		}
		guard.target = nullptr;
	}
	else
	{
		uninitialized_rev_destroying_backout backout{ out };
		for (size_t idx = 0; idx < size; ++idx)
		{
			backout.emplace_back();
		}
		backout.release();
	}
}

template <class _Ty>
void uninitialized_default_construct_multidimensional_n(_Ty* const out, const size_t size)
{
	if constexpr (!is_trivially_default_constructible_v<_Ty>)
	{
		if constexpr (is_array_v<_Ty>)
		{
			reverse_destroy_multidimensional_n_guard<_Ty> guard{ out, 0 };
			for (size_t& idx = guard.idx; idx < size; ++idx)
			{
				uninitialized_default_construct_multidimensional_n(out[idx], extent_v<_Ty>);
			}
			guard.target = nullptr;
		}
		else
		{
			uninitialized_rev_destroying_backout backout{ out };
			for (size_t idx = 0; idx < size; ++idx)
			{
				backout.emplace_back_for_overwrite();
			}
			backout.release();
		}
	}
}

template <class _Ty>
void uninitialized_fill_multidimensional_n(_Ty* const out, const size_t size, const _Ty& val)
{
	if constexpr (is_array_v<_Ty>)
	{
		reverse_destroy_multidimensional_n_guard<_Ty> guard{ out, 0 };
		for (size_t& idx = guard.idx; idx < size; ++idx)
		{
			uninitialized_copy_multidimensional(val, out[idx]);
		}
		guard.target = nullptr;
	}
	else if constexpr (std::_Fill_memset_is_safe<_Ty*, _Ty>)
	{
		// todo
		std::_Fill_memset(out, val, size);
	}
	else
	{
		if constexpr (std::_Fill_zero_memset_is_safe<_Ty*, _Ty>)
		{
			//todo
			if (std::_Is_all_bits_zero(val))
			{
				std::_Fill_zero_memset(out, size);
				return;
			}
		}
		uninitialized_rev_destroying_backout backout{ out };
		for (size_t idx = 0; idx < size; ++idx)
		{
			backout.emplace_back(val);
		}
		backout.release();
	}
}

template <class _Ty, bool = is_trivially_destructible_v<remove_extent_t<_Ty>>>
class ref_count_unbounded_array : public ref_count_base
{
public:
	static_assert(is_unbounded_array_v<_Ty>);
	using element_type = remove_extent_t<_Ty>;

	explicit ref_count_unbounded_array(const size_t count) : ref_count_base()
	{
		uninitialized_value_construct_multidimensional_n(get_ptr(), count);
	}

	template <class _Arg>
	explicit ref_count_unbounded_array(const size_t count, const _Arg val) : ref_count_base()
	{
		if constexpr (is_same_v<for_overwrite_tag, _Arg>)
		{
			uninitialized_default_construct_multidimensional_n(get_ptr(), count);
		}
		else
		{
			uninitialized_fill_multidimensional_n(get_ptr(), count, val);
		}
	}

	WHP_NODISCARD auto get_ptr() noexcept
	{
		return addressof(m_storage.m_value);
	}
private:
	union
	{
		wrap<element_type> m_storage;
	};

	~ref_count_unbounded_array() noexcept override {}

	virtual void destroy() noexcept override {}

	virtual void delete_this() noexcept override
	{
		this->~ref_count_unbounded_array();
		deallocate_flexible_array(this);
	}
};

template <class _Ty>
class ref_count_unbounded_array<_Ty, false> : public ref_count_base
{
public:
	static_assert(is_unbounded_array_v<_Ty>);
	using element_type = remove_extent_t<_Ty>;

	explicit ref_count_unbounded_array(const size_t count) : ref_count_base(), size(count)
	{
		uninitialized_value_construct_multidimensional_n(get_ptr(), size);
	}

	template <class _Arg>
	explicit ref_count_unbounded_array(const size_t count, const _Arg val) : ref_count_base(), size(count)
	{
		if constexpr (is_same_v<for_overwrite_tag, _Arg>)
		{
			uninitialized_default_construct_multidimensional_n(get_ptr(), size);
		}
		else
		{
			uninitialized_fill_multidimensional_n(get_ptr(), size, val);
		}
	}

	WHP_NODISCARD auto get_ptr() noexcept
	{
		return addressof(m_storage.m_value);
	}
private:
	size_t size;

	union
	{
		wrap<element_type> m_storage;
	};

	~ref_count_unbounded_array() noexcept override {}

	virtual void destroy() noexcept override
	{
		reverse_destroy_multidimensional_n(get_ptr(), size);
	}

	virtual void delete_this() noexcept override
	{
		this->~ref_count_unbounded_array();
		deallocate_flexible_array(this);
	}
};

template <class T>
class ref_count_bounded_array : public ref_count_base {
public:
	static_assert(is_bounded_array_v<T>);

	ref_count_bounded_array() : ref_count_base(), m_storage() {}

	template <class Arg>
	explicit ref_count_bounded_array(const Arg& val) : ref_count_base() {
		if constexpr (is_same_v<for_overwrite_tag, Arg>) {
			uninitialized_default_construct_multidimensional_n(m_storage.m_value, extent_v<T>);
		}
		else {
			uninitialized_fill_multidimensional_n(m_storage.m_value, extent_v<T>, val);
		}
	}

	union {
		wrap<T> m_storage;
	};

private:
	~ref_count_bounded_array() noexcept override {}

	void destroy() noexcept override {
		destroy_in_place(m_storage);
	}

	void delete_this() noexcept override {
		delete this;
	}
};

template <class _Refc>
struct WHP_NODISCARD global_delete_guard
{
	_Refc* target;

	~global_delete_guard()
	{
		if (target)
		{
			deallocate_flexible_array(target);
		}
	}
};

template <class T>
struct default_delete;

template <class T, class _Dx = default_delete<T>>
class wunique_ptr;

template <class T>
class wshared_ptr;

template <class T>
class wweak_ptr;

template <class _Yty, class = void>
struct can_enable_shared : true_type {};

template <class _Yty>
struct can_enable_shared <_Yty, void_t<typename _Yty::esft_type>> : is_convertible<remove_cv_t<_Yty>*, typename _Yty::esft_type*>::type {};

struct exception_ptr_access;

template <class _Ty>
class smart_ptr_base
{
public:
	using element_type = remove_extent_t<_Ty>;

	WHP_NODISCARD long use_count() const noexcept
	{
		return m_rep ? m_rep->use_count() : 0;
	}

	template <class _Ty2>
	WHP_NODISCARD bool owner_before(const smart_ptr_base<_Ty2>& right) const noexcept
	{
		return m_rep < right.m_rep;
	}

	smart_ptr_base(const smart_ptr_base&) = delete;
	smart_ptr_base operator=(const smart_ptr_base&) = delete;

protected:
	WHP_NODISCARD element_type* get() const noexcept
	{
		return m_ptr;
	}

	constexpr smart_ptr_base() noexcept = default;

	~smart_ptr_base() = default;

	template <class _Ty2>
	void move_construct_from(smart_ptr_base<_Ty2>&& right) noexcept
	{
		m_ptr = right.m_ptr;
		m_rep = right.m_rep;

		right.m_ptr = nullptr;
		right.m_rep = nullptr;
	}

	template <class _Ty2>
	void copy_construct_from(const smart_ptr_base<_Ty2>& other) noexcept
	{
		other.incref();

		m_ptr = other.m_ptr;
		m_rep = other.m_rep;
	}

	template <class _Ty2>
	void alias_construct_from(const wshared_ptr<_Ty2>& other, element_type* px) noexcept
	{
		other.incref();

		m_ptr = px;
		m_rep = other.m_rep;
	}

	template <class _Ty2>
	void alias_move_construct_from(wshared_ptr<_Ty2>&& other, element_type* px) noexcept
	{
		m_ptr = px;
		m_rep = other.m_rep;

		other.m_ptr = nullptr;
		other.m_rep = nullptr;
	}

	template <class _Ty0>
	friend class xweak_ptr;

	template <class _Ty2>
	bool construct_from_weak(const wweak_ptr<_Ty2>& other) noexcept
	{
		if (other.m_rep && other.m_rep->incref_nz()) {
			m_ptr = other.m_ptr;
			m_rep = other.m_rep;
			return true;
		}

		return false;
	}

	void incref() const noexcept
	{
		if (m_rep)
		{
			m_rep->incref();
		}
	}

	void decref() const noexcept
	{
		if (m_rep)
		{
			m_rep->decref();
		}
	}

	void swapbase(smart_ptr_base& right) noexcept
	{
		swap(m_ptr, right.m_ptr);
		swap(m_rep, right.m_rep);
	}

	template <class _Ty2>
	void weakly_construct_from(const smart_ptr_base<_Ty2>& other) noexcept
	{
		if (other.m_rep)
		{
			m_rep = other.m_rep;
			m_ptr = other.m_ptr;
			m_rep->incwref();
		}
	}

	template <class _Ty2>
	void weakly_convert_lvalue_avoiding_expired_conversation(const smart_ptr_base<_Ty2>& other) noexcept
	{
		if (other.m_rep)
		{
			m_rep = other.m_rep;
			m_rep->incwref();

			if (m_rep->incref_nz())
			{
				m_ptr = other.m_ptr;
				m_rep->decref();
			}
		}
	}

	template <class _Ty2>
	void weakly_convert_rvalue_avoiding_expired_conversation(smart_ptr_base<_Ty2>&& other) noexcept
	{
		m_rep = other.m_rep;
		other.m_rep = nullptr;

		if (m_rep && m_rep->incref_nz())
		{
			m_ptr = other.m_ptr;
			m_rep->decref();
		}

		other.m_ptr = nullptr;
	}

	void incwref() const noexcept
	{
		if (m_rep)
		{
			m_rep->incwref();
		}
	}

	void decwref() const noexcept
	{
		if (m_rep)
		{
			m_rep->decwref();
		}
	}

private:
	element_type* m_ptr{ nullptr };
	ref_count_base* m_rep{ nullptr };

	template <class _Ty0>
	friend class smart_ptr_base;

	friend wshared_ptr<_Ty>;

	friend exception_ptr_access;

	template <class _Dx, class _Ty0>
	friend _Dx* get_deleter(const wshared_ptr<_Ty0>& xsp) noexcept;
};

template <class _Yty, class = void>
struct can_scalar_delete : false_type {};
template <class _Yty>
struct can_scalar_delete<_Yty, void_t<decltype(delete declval<_Yty*>())>> : true_type {};

template <class _Yty, class = void>
struct can_array_delete : false_type {};
template <class _Yty>
struct can_array_delete<_Yty, void_t<decltype(delete[] declval<_Yty*>())>> : true_type {};

template <class Fx, class Arg, class = void>
struct can_call_function_object : false_type {};
template <class Fx, class Arg>
struct can_call_function_object<Fx, Arg, void_t<decltype(declval<Fx>()(declval<Arg>()))>> : true_type {};

template <class _Yty, class Ty>
struct sp_convertible : is_convertible<_Yty*, Ty*>::type {};
template <class _Yty, class _Uty>
struct sp_convertible<_Yty, _Uty[]> : is_convertible<_Yty(*)[], _Uty(*)[]>::type {};
template <class _Yty, class _Uty, size_t ext>
struct sp_convertible<_Yty, _Uty[ext]> : is_convertible<_Yty(*)[ext], _Uty(*)[ext]>::type {};

template <class _Yty, class Ty>
struct sp_pointer_compatible : std::is_convertible<_Yty*, Ty*>::type {};
template <class _Uty, size_t ext>
struct sp_pointer_compatible<_Uty[ext], _Uty[]> : true_type {};
template <class _Uty, size_t ext>
struct sp_pointer_compatible<_Uty[ext], const _Uty[]> : true_type {};
template <class _Uty, size_t ext>
struct sp_pointer_compatible<_Uty[ext], volatile _Uty[]> : true_type {};
template <class _Uty, size_t ext>
struct sp_pointer_compatible<_Uty[ext], const volatile _Uty[]> : true_type {};

template <class _Ux>
struct temporary_owner
{
	_Ux* ptr;

	explicit temporary_owner(_Ux* const _ptr_) noexcept : ptr(_ptr_) {}
	temporary_owner(const temporary_owner&) = delete;
	temporary_owner operator=(const temporary_owner&) = delete;

	~temporary_owner()
	{
		delete ptr;
	}
};

template <class _UxptrOrNullptr, class _Dx>
struct temporary_owner_del
{
	_UxptrOrNullptr ptr;
	_Dx& deleter;
	bool call_deleter = true;

	explicit temporary_owner_del(const _UxptrOrNullptr _ptr_, _Dx& _deleter_) noexcept : ptr(_ptr_), deleter(_deleter_) {}
	temporary_owner_del(const temporary_owner_del&) = delete;
	temporary_owner_del operator=(const temporary_owner_del&) = delete;

	~temporary_owner_del()
	{
		if (call_deleter)
		{
			deleter(ptr);
		}
	}
};

template <class _Ty>
class wshared_ptr : public smart_ptr_base<_Ty>
{
private:
	using m_base = smart_ptr_base<_Ty>;
public:
	using typename m_base::element_type;

	using weak_type = wweak_ptr<_Ty>;

	constexpr wshared_ptr() noexcept = default;

	constexpr wshared_ptr(nullptr_t) noexcept {};

	template <class _Ux, enable_if_t<conjunction_v<conditional_t<is_array_v<_Ty>, can_array_delete<_Ux>, can_scalar_delete<_Ux>>, sp_convertible<_Ux, _Ty>>, int> = 0>
	explicit wshared_ptr(_Ux* ptr)
	{
		if constexpr (is_array_v<_Ty>)
		{
			setpd(ptr, default_delete<_Ux[]>{});
		}
		else
		{
			temporary_owner<_Ux> owner(ptr);
			set_ptr_rep_and_enable_shared(owner.ptr, new ref_count<_Ux>(owner.ptr));
			owner.ptr = nullptr;
		}
	}

	template <class _Ux, class _Dx, enable_if_t<conjunction_v<is_move_constructible<_Dx>, can_call_function_object<_Dx&, _Ux*&>, sp_convertible<_Ux, _Ty>>, int> = 0>
	wshared_ptr(_Ux* ptr, _Dx dt)
	{
		setpd(ptr, move(dt));
	}

	template <class _Ux, class _Dx, class _Alloc, enable_if_t<conjunction_v<is_move_constructible<_Dx>, can_call_function_object<_Dx&, _Ux*&>, sp_convertible<_Ux, _Ty>>, int> = 0>
	wshared_ptr(_Ux ptr, _Dx dt, _Alloc al)
	{
		setpda(ptr, move(dt), al);
	}

	template <class _Dx, enable_if_t<conjunction_v<is_move_constructible<_Dx>, can_call_function_object<_Dx&, nullptr_t*&>>, int> = 0>
	wshared_ptr(nullptr_t, _Dx dt)
	{
		setpd(nullptr, move(dt));
	}

	template <class _Dx, class _Alloc, enable_if_t<conjunction_v<is_move_constructible<_Dx>, can_call_function_object<_Dx&, nullptr_t*&>>, int> = 0>
	wshared_ptr(nullptr_t, _Dx dt, _Alloc al)
	{
		setpda(nullptr, move(dt), al);
	}

	template <class _Ty2>
	wshared_ptr(const wshared_ptr<_Ty2>& right, element_type* ptr) noexcept
	{
		this->alias_construct_from(right, ptr);
	}

	template <class _Ty2>
	wshared_ptr(wshared_ptr<_Ty2>&& right, element_type* ptr) noexcept
	{
		this->alias_move_construct_from(move(right), ptr);
	}

	wshared_ptr(const wshared_ptr& right) noexcept
	{
		this->copy_construct_from(right);
	}

	template <class _Ty2, enable_if_t<sp_pointer_compatible<_Ty2, _Ty>::value, int> = 0>
	wshared_ptr(const wshared_ptr<_Ty2>& right) noexcept
	{
		this->copy_construct_from(right);
	}

	wshared_ptr(wshared_ptr&& right) noexcept
	{
		this->move_construct_from(move(right));
	}

	template <class _Ty2, enable_if_t<sp_pointer_compatible<_Ty2, _Ty>::value, int> = 0>
	wshared_ptr(wshared_ptr<_Ty2>&& right) noexcept
	{
		this->move_construct_from(move(right));
	}

	template <class _Ty2, enable_if_t<sp_pointer_compatible<_Ty2, _Ty>::value, int> = 0>
	explicit wshared_ptr(const wweak_ptr<_Ty2>& other)
	{
		if (!this->construct_from_weak(other))
		{
			// todo // düzelt // throw_bad_xweak_ptr();
		}
	}

	template <class _Ux, class _Dx, enable_if_t<conjunction_v<sp_pointer_compatible<_Ux, _Ty>, is_convertible<typename wunique_ptr<_Ux, _Dx>::pointer, element_type*>>, int> = 0>
	wshared_ptr(wunique_ptr<_Ux, _Dx>&& other)
	{
		using fancy_t = typename wunique_ptr<_Ux, _Dx>::pointer;
		using raw_t = typename wunique_ptr<_Ux, _Dx>::element_type*;
		using deleter_t = conditional_t<is_reference_v<_Dx>, decltype(std::ref(other.get_deleter())), _Dx>;

		const fancy_t fancy = other.get();

		if (fancy)
		{
			const raw_t raw = fancy;
			const auto rx = new ref_count_resource<fancy_t, deleter_t>(fancy, forward<_Dx>(other.get_deleter()));
			set_ptr_rep_and_enable_shared(raw, rx);
			other.release();
		}
	}

	~wshared_ptr() noexcept
	{
		this->decref();
	}

	wshared_ptr& operator=(const wshared_ptr& right) noexcept
	{
		wshared_ptr(right).swap(*this);
		return *this;
	}

	template <class _Ty2, enable_if_t<sp_pointer_compatible<_Ty2, _Ty>::value, int> = 0>
	wshared_ptr& operator=(const wshared_ptr<_Ty2>& right) noexcept
	{
		wshared_ptr(right).swap(*this);
		return *this;
	}

	wshared_ptr& operator=(wshared_ptr&& right) noexcept
	{
		wshared_ptr(move(right)).swap(*this);
		return *this;
	}

	template <class _Ty2, enable_if_t<sp_pointer_compatible<_Ty2, _Ty>::value, int> = 0>
	wshared_ptr& operator=(wshared_ptr<_Ty2>&& right) noexcept
	{
		wshared_ptr(move(right)).swap(*this);
		return *this;
	}

	template <class _Ux, class _Dx, enable_if_t<conjunction_v<sp_pointer_compatible<_Ux, _Ty>, is_convertible<typename wunique_ptr<_Ux, _Dx>::pointer, element_type*>>, int> = 0>
	wshared_ptr& operator=(wunique_ptr<_Ux, _Dx>&& right)
	{
		wshared_ptr(move(right)).swap(*this);
		return *this;
	}

	void swap(wshared_ptr& other) noexcept
	{
		this->swapbase(other);
	}

	void reset() noexcept
	{
		wshared_ptr().swap(*this);
	}

	template <class _Ux, enable_if_t<conjunction_v<conditional_t<is_array_v<_Ty>, can_array_delete<_Ux>, can_scalar_delete<_Ux>>, sp_convertible<_Ux, _Ty>>, int> = 0>
	void reset(_Ux* ptr)
	{
		wshared_ptr(ptr).swap(*this);
	}

	template <class _Ux, class _Dx, enable_if_t<conjunction_v<is_move_constructible<_Dx>, can_call_function_object<_Dx&, _Ux*&>, sp_convertible<_Ux, _Ty>>, int> = 0>
	void reset(_Ux* ptr, _Dx dt)
	{
		wshared_ptr(ptr, dt).swap(*this);
	}

	template <class _Ux, class _Dx, class _Alloc, enable_if_t<conjunction_v<is_move_constructible<_Dx>, can_call_function_object<_Dx&, _Ux*&>, sp_convertible<_Ux, _Ty>>, int> = 0>
	void reset(_Ux* ptr, _Dx dt, _Alloc al)
	{
		wshared_ptr(ptr, dt, al).swap(*this);
	}

	using m_base::get;

	template <class _Ty2 = _Ty, enable_if_t<!disjunction_v<is_array<_Ty2>, is_void<_Ty2>>, int> = 0>
	WHP_NODISCARD _Ty2& operator*() const noexcept
	{
		return *get();
	}

	template <class _Ty2 = _Ty, enable_if_t<!is_array_v<_Ty2>, int> = 0>
	WHP_NODISCARD _Ty2* operator->() const noexcept
	{
		return get();
	}

	template <class _Ty2 = _Ty, class elem = element_type, enable_if_t<is_array_v<_Ty2>, int> = 0>
	WHP_NODISCARD elem& operator[](ptrdiff_t idx) const noexcept
	{
		return get()[idx];
	}

	explicit operator bool() const noexcept
	{
		return get() != nullptr;
	}

private:
	template <class UxptrOrNullptr, class _Dx>
	void setpd(const UxptrOrNullptr ptr, _Dx dt)
	{
		temporary_owner_del<UxptrOrNullptr, _Dx> owner(ptr, dt);
		set_ptr_rep_and_enable_shared(owner.ptr, new ref_count_resource<UxptrOrNullptr, _Dx>(owner.ptr, move(dt)));
		owner.call_deleter = false;
	}

	template <class UxptrOrNullptr, class _Dx, class _Alloc>
	void setpda(const UxptrOrNullptr ptr, _Dx dt, _Alloc alx)
	{
		using alref_alloc = std::_Rebind_alloc_t <_Alloc, ref_count_resource_alloc<UxptrOrNullptr, _Dx, _Alloc>>;

		temporary_owner_del<UxptrOrNullptr, _Dx> owner(ptr, dt);
		alref_alloc alref(alx);
		alloc_construct_ptr<alref_alloc> constructor(ptr);
		constructor.allocate();
		construct_in_place(*constructor.ptr, owner.ptr, move(dt), alx);
		set_ptr_rep_and_enable_shared(owner.ptr, unfancy(constructor.ptr));
		constructor.ptr = nullptr;
		owner.call_deleter = false;
	}

	template <class _Ty0, class... Types>
	friend enable_if_t<!is_array_v<_Ty0>, wshared_ptr<_Ty0>> make_wshared(Types&&... args);

	template <class _Ty0, class _Alloc, class... Types>
	friend enable_if_t<!is_array_v<_Ty0>, wshared_ptr<_Ty0>> allocate_wshared(const _Alloc& al_arg, Types&&... args);

	template <class _Ty0>
	friend enable_if_t<is_bounded_array_v<_Ty0>, wshared_ptr<_Ty0>> make_wshared();

	template <class _Ty0, class _Alloc>
	friend enable_if_t<is_bounded_array_v<_Ty0>, wshared_ptr<_Ty0>> allocate_wshared(const _Alloc& al_arg);

	template <class _Ty0>
	friend enable_if_t<is_bounded_array_v<_Ty0>, wshared_ptr<_Ty0>> make_wshared(const remove_extent_t<_Ty0>& val);

	template <class _Ty0, class _Alloc>
	friend enable_if_t<is_bounded_array_v<_Ty0>, wshared_ptr<_Ty0>> allocate_wshared(const _Alloc& al_arg, const remove_extent_t<_Ty0>& val);

	template <class _Ty0>
	friend enable_if_t<!is_unbounded_array_v<_Ty0>, wshared_ptr<_Ty0>> make_shared_for_overwrite();

	template <class _Ty0, class _Alloc>
	friend enable_if_t<!is_unbounded_array_v<_Ty0>, wshared_ptr<_Ty0>> allocate_wshared_for_overwrite(const _Alloc& al_arg);

	template <class _Ty0, class... ArgTypes>
	friend wshared_ptr<_Ty0> make_wshared_unbounded_array(size_t count, const ArgTypes&... args);

	template <class _Ty0, class _Alloc, class... ArgTypes>
	friend wshared_ptr<_Ty0> allocate_wshared_unbounded_array(const _Alloc& al, size_t count, const ArgTypes&... args);

	template <class _Ux>
	void set_ptr_rep_and_enable_shared(_Ux* const ptr, ref_count_base* const rep) noexcept
	{
		this->m_ptr = ptr;
		this->m_rep = rep;
	}

	void set_ptr_rep_and_enable_shared(nullptr_t, ref_count_base* const rep) noexcept
	{
		this->m_ptr = nullptr;
		this->m_rep = rep;
	}
};

#if _WHP_HAS_CPP_VERSION(17)

template <class _Ty>
wshared_ptr(wweak_ptr<_Ty>) -> wshared_ptr<_Ty>;

template <class _Ty, class _Dx>
wshared_ptr(wunique_ptr<_Ty, _Dx>) -> wshared_ptr<_Ty>;

#endif

template <class _Ty1, class _Ty2>
WHP_NODISCARD bool operator==(const wshared_ptr<_Ty1>& left, const wshared_ptr<_Ty2>& right) noexcept
{
	return left.get() == right.get();
}

template <class T>
WHP_NODISCARD bool operator==(const wshared_ptr<T>& left, nullptr_t) noexcept
{
	return left.get() == nullptr;
}

template <class _Elem, class _Traits, class _Ty>
std::basic_ostream<_Elem, _Traits>& operator<<(std::basic_ostream<_Elem, _Traits>& out, const wshared_ptr<_Ty>& xsp)
{
	return out << xsp.get();
}

template <class _Ty1, class _Ty2>
void swap(wshared_ptr<_Ty1>& left, wshared_ptr<_Ty2>& right) noexcept
{
	left.swap(right());
}

#define WHP_SMART_PTR_NODISCARD WHP_NODISCARD_MSG("smart pointer returned as unneccessary!")

template <class T1, class T2>
WHP_SMART_PTR_NODISCARD wshared_ptr<T1> static_pointer_cast(const wshared_ptr<T2>& that) noexcept
{
	const auto ptr = static_cast<typename wshared_ptr<T1>::element_type*>(that.get());
	return wshared_ptr<T1>(that, ptr);
}

template <class T1, class T2>
WHP_SMART_PTR_NODISCARD wshared_ptr<T1> static_pointer_cast(wshared_ptr<T2>&& that) noexcept
{
	const auto ptr = static_cast<typename wshared_ptr<T1>::element_type*>(that.get());
	return wshared_ptr<T1>(move(that), ptr);
}

template <class T1, class T2>
WHP_SMART_PTR_NODISCARD wshared_ptr<T1> const_pointer_cast(const wshared_ptr<T2>& that) noexcept
{
	const auto ptr = const_cast<typename wshared_ptr<T1>::element_type*>(that.get());
	return wshared_ptr<T1>(that, ptr);
}

template <class T1, class T2>
WHP_SMART_PTR_NODISCARD wshared_ptr<T1> const_pointer_cast(wshared_ptr<T2>&& that) noexcept
{
	const auto ptr = const_cast<typename wshared_ptr<T1>::element_type*>(that.get());
	return wshared_ptr<T1>(move(that), ptr);
}

template <class T1, class T2>
WHP_SMART_PTR_NODISCARD wshared_ptr<T1> reinterpret_pointer_cast(const wshared_ptr<T2>& that) noexcept
{
	const auto ptr = reinterpret_cast<typename wshared_ptr<T1>::element_type*>(that.get());
	return wshared_ptr<T1>(that, ptr);
}

template <class T1, class T2>
WHP_SMART_PTR_NODISCARD wshared_ptr<T1> reinterpret_pointer_cast(wshared_ptr<T2>&& that) noexcept
{
	const auto ptr = reinterpret_cast<typename wshared_ptr<T1>::element_type*>(that.get());
	return wshared_ptr<T1>(move(that), ptr);
}

template <class T1, class T2>
WHP_SMART_PTR_NODISCARD wshared_ptr<T1> dynamic_pointer_cast(const wshared_ptr<T2>& that) noexcept
{
	const auto ptr = dynamic_cast<typename wshared_ptr<T1>::element_type*>(that.get());
	if (ptr)
	{
		return wshared_ptr<T1>(that, ptr);
	}
	return {};
}

template <class T1, class T2>
WHP_SMART_PTR_NODISCARD wshared_ptr<T1> dynamic_pointer_cast(wshared_ptr<T2>&& that) noexcept
{
	const auto ptr = dynamic_cast<typename wshared_ptr<T1>::element_type*>(that.get());
	if (ptr)
	{
		return wshared_ptr<T1>(move(that), ptr);
	}
	return {};
}

template <class T, class... Types>
WHP_SMART_PTR_NODISCARD enable_if_t<!is_array_v<T>, wshared_ptr<T>> make_wshared(Types&&... args)
{
	const auto rptr = new ref_count_obj2<T>(forward<Types>(args)...);
	wshared_ptr<T> result;
	result.set_ptr_rep_and_enable_shared(addressof(rptr->m_storage.m_value), rptr);
	return result;
}

template <class T, class... ArgTypes>
WHP_SMART_PTR_NODISCARD wshared_ptr<T> make_wshared_unbounded_array(const size_t count, const ArgTypes&... args)
{
	static_assert(is_unbounded_array_v<T>);

	using Refc = ref_count_unbounded_array<T>;

	const auto rptr = allocate_flexible_array<Refc>(count);
	global_delete_guard<Refc> guard{ rptr };

	::new (static_cast<void*>(rptr)) Refc(count, args...);
	guard.target = nullptr;
	wshared_ptr<T> result;
	result.set_ptr_rep_and_enable_shared(rptr->get_ptr(), rptr);
	return result;
}

template <class T>
WHP_SMART_PTR_NODISCARD enable_if_t<is_unbounded_array_v<T>, wshared_ptr<T>> make_wshared(const size_t count)
{
	return make_wshared_unbounded_array<T>(count);
}

template <class T>
WHP_SMART_PTR_NODISCARD enable_if_t<is_unbounded_array_v<T>, wshared_ptr<T>> make_wshared(const size_t count, const remove_extent_t<T>& val)
{
	return make_wshared_unbounded_array<T>(count, val);
}

template <class T>
WHP_SMART_PTR_NODISCARD enable_if_t<is_bounded_array_v<T>, wshared_ptr<T>> make_wshared()
{
	const auto rptr = new ref_count_bounded_array<T>();
	wshared_ptr<T> result;
	result.set_ptr_rep_and_enable_shared(rptr->m_storage.m_value, rptr);
	return result;
}

template <class T>
WHP_SMART_PTR_NODISCARD enable_if_t<is_bounded_array_v<T>, wshared_ptr<T>> make_wshared(const remove_extent_t<T>& val)
{
	const auto rptr = new ref_count_bounded_array<T>(val);
	wshared_ptr<T> result;
	result.set_ptr_rep_and_enable_shared(rptr->m_storage.m_value, rptr);
	return result;
}

template <class T>
WHP_SMART_PTR_NODISCARD enable_if_t<!is_unbounded_array_v<T>, wshared_ptr<T>> make_shared_for_overwrite()
{
	wshared_ptr<T> result;
	if constexpr (is_array_v<T>)
	{
		const auto rptr = new ref_count_bounded_array<T>(for_overwrite_tag{});
		result.set_ptr_rep_and_enable_shared(rptr->m_storage.m_value, rptr);
	}
	else
	{
		const auto rptr = new ref_count_obj2<T>(for_overwrite_tag{});
		result.set_ptr_rep_and_enable_shared(addressof(rptr->m_storage.m_value), rptr);
	}
	return result;
}

template <class T>
WHP_SMART_PTR_NODISCARD enable_if_t<is_bounded_array_v<T>, wshared_ptr<T>> make_shared_for_overwrite(const size_t count)
{
	return make_wshared_unbounded_array<T>(count, for_overwrite_tag{});
}

template <class Dx, class T0>
WHP_SMART_PTR_NODISCARD Dx* get_deleter(const wshared_ptr<T0>& xsp) noexcept
{
	if (xsp.m_rep)
	{
		return static_cast<Dx*>(xsp.m_rep->get_deleter(typeid(Dx)));
	}
}

template <class _Ty>
struct default_delete
{
	constexpr default_delete() noexcept = delete;

	template <class _Ty2, enable_if_t<is_convertible_v<_Ty2*, _Ty*>, int> = 0>
	constexpr default_delete(const default_delete<_Ty2>&) noexcept {}

	constexpr void operator()(_Ty* ptr) const noexcept
	{
		static_assert(0 < sizeof(_Ty), "can't delete an incomplate type");
		delete ptr;
	}
};

template <class _Ty>
struct default_delete<_Ty[]>
{
	constexpr default_delete() noexcept = default;

	template <class _Uty, enable_if_t<is_convertible_v<_Uty(*)[], _Ty(*)[]>, int> = 0>
	constexpr default_delete(const default_delete<_Uty[]>&) noexcept {}

	template <class _Uty, enable_if_t<is_convertible_v<_Uty(*)[], _Ty(*)[]>, int> = 0>
	constexpr void operator()(_Uty* ptr) const noexcept
	{
		static_assert(0 < sizeof(_Uty), "can't delete an incomplate type");
		delete[] ptr;
	}
};

template <class _Ty, class Dx_noref, class = void>
struct get_deleter_pointer_type
{
	using type = _Ty*;
};

template <class _Ty, class Dx_noref>
struct get_deleter_pointer_type<_Ty, Dx_noref, std::void_t<typename Dx_noref::pointer>>
{
	using type = typename Dx_noref::pointer;
};

template <class Dx2>
using unique_ptr_enable_default_t = enable_if_t<std::conjunction_v<std::negation<std::is_pointer<Dx2>>, std::is_default_constructible<Dx2>>, int>;

template <class _Ty, class Dx /* = default_delete<_Ty>*/>
class wunique_ptr
{
public:
	using pointer = typename get_deleter_pointer_type<_Ty, remove_reference_t<Dx>>::type;
	using element_type = _Ty;
	using deleter_type = Dx;

	template <class Dx2 = Dx, unique_ptr_enable_default_t<Dx2> = 0>
	constexpr wunique_ptr() noexcept : m_pair(zero_then_variadic_args_t{}) {}

	template <class Dx2 = Dx, unique_ptr_enable_default_t<Dx2> = 0>
	constexpr wunique_ptr(nullptr_t) noexcept : m_pair(zero_then_variadic_args_t{}) {}

	template <class Dx2 = Dx, unique_ptr_enable_default_t<Dx2> = 0>
	constexpr explicit wunique_ptr(pointer ptr) noexcept : m_pair(zero_then_variadic_args_t{}, ptr) {}

	template <class Dx2 = Dx, enable_if_t<std::is_constructible_v<Dx2, const Dx2&>, int> = 0>
	constexpr wunique_ptr(pointer ptr, const Dx& dt) noexcept : m_pair(one_then_variadic_args_t{}, dt, ptr) {}

	template <class Dx2 = Dx, enable_if_t<conjunction_v<std::negation<is_reference<Dx2>>, std::is_constructible<Dx2, Dx2>>, int> = 0>
	constexpr wunique_ptr(pointer ptr, Dx&& dt) noexcept : m_pair(one_then_variadic_args_t{}, move(dt), ptr) {}

	template <class Dx2 = Dx, enable_if_t<conjunction_v<is_reference<Dx2>, std::is_constructible<Dx2, remove_reference_t<Dx2>>>, int> = 0>
	wunique_ptr(pointer, remove_reference_t<Dx>&&) = delete;

	template <class Dx2 = Dx, enable_if_t<std::is_move_constructible_v<Dx2>, int> = 0>
	constexpr wunique_ptr(wunique_ptr&& other) : m_pair(one_then_variadic_args_t{}, _WHIP forward<Dx>(other.get_deleter()), other.release()) {}

	template <class T2, class Dx2, enable_if_t<conjunction_v<std::negation<is_array<_Ty>>,
		is_convertible<typename wunique_ptr<T2, Dx2>::pointer, pointer>,
		conditional_t<is_reference_v<Dx>, is_same<Dx2, Dx>, is_convertible<Dx2, Dx>>>, int> = 0>
	constexpr wunique_ptr(wunique_ptr<T2, Dx2>&& other) noexcept : m_pair(one_then_variadic_args_t{}, _WHIP forward<Dx2>(other.get_deleter()), other.release()) {}

	wunique_ptr(const wunique_ptr&) = delete;
	wunique_ptr& operator=(const wunique_ptr&) = delete;

	constexpr wunique_ptr& operator=(nullptr_t) noexcept
	{
		reset();
		return *this;
	}

	template <class Dx2 = Dx, enable_if_t<std::is_move_assignable_v<Dx2>, int> = 0>
	constexpr wunique_ptr& operator=(wunique_ptr&& right) noexcept
	{
		if (this != addressof(right))
		{
			reset(right.release());
			m_pair.get_first() = _WHIP forward<Dx>(right.m_pair.get_first());
		}
		return *this;
	}

	template <class T2, class Dx2, enable_if_t<conjunction_v<std::negation<is_array<T2>>,
		std::is_assignable<Dx&, Dx2>, is_convertible<typename wunique_ptr<T2, Dx2>::pointer, pointer>>, int> = 0>
	constexpr wunique_ptr& operator=(wunique_ptr<T2, Dx2>&& right) noexcept
	{
		reset(right.release());
		m_pair.get_first() = _WHIP forward<Dx2>(right.m_pair.get_first());
		return *this;
	}

	~wunique_ptr() noexcept
	{
		if (m_pair.second)
		{
			m_pair.get_first()(m_pair.second);
		}
	}

	constexpr void swap(wunique_ptr& right) noexcept
	{
		swap_nt(m_pair.second, right.m_pair.second);
		swap_nt(m_pair.get_first(), right.m_pair.get_first());
	}

	WHP_NODISCARD constexpr Dx& get_deleter() noexcept
	{
		return m_pair.get_first();
	}

	WHP_NODISCARD constexpr const Dx& get_deleter() const noexcept
	{
		return m_pair.get_first();
	}

	WHP_NODISCARD constexpr add_lvalue_reference_t<_Ty> operator*() const noexcept
	{
		return *m_pair.second;
	}

	WHP_NODISCARD constexpr pointer operator->() const noexcept
	{
		return m_pair.second;
	}

	WHP_NODISCARD constexpr pointer get() const noexcept
	{
		return m_pair.second;
	}

	constexpr explicit operator bool() const noexcept
	{
		return static_cast<bool>(m_pair.second);
	}

	constexpr pointer release() noexcept
	{
		return exchange(m_pair.second, nullptr);
	}

	constexpr void reset(pointer ptr = nullptr) noexcept
	{
		pointer old = exchange(m_pair.second, ptr);

		if (old)
		{
			m_pair.get_first()(old);
		}
	}


private:
	template <class, class>
	friend class wunique_ptr;

	compressed_pair<Dx, pointer> m_pair;
};

template <class T, class... Types, enable_if_t<!std::is_array_v<T>, int> = 0>
WHP_SMART_PTR_NODISCARD constexpr wunique_ptr<T> make_wunique(Types&&... args)
{
	return wunique_ptr<T>(new T(_WHIP forward<Types>(args)...));
}

template <class T, enable_if_t<!is_array_v<T>, int> = 0>
WHP_SMART_PTR_NODISCARD constexpr wunique_ptr<T> make_wunique_for_overwrite()
{
	return wunique_ptr<T>(new T);
}

template <class T, class Dx, enable_if_t<is_swappable_v<Dx>, int> = 0>
constexpr void swap(wunique_ptr<T, Dx>& left, wunique_ptr<T, Dx>& right) noexcept
{
	left.swap(right);
}

template <class _Ty>
using scope = std::unique_ptr<_Ty>;

template <class _Ty>
using ref = std::shared_ptr<_Ty>;

template <class _Ty, class... _Types, enable_if_t<!is_array_v<_Ty>, int> = 0>
WHP_NODISCARD_MSG("scope returned as unnecessary") inline scope<_Ty> make_scope(_Types&&... _Args)
{
	return scope<_Ty>(new _Ty(forward<_Types>(_Args)...));
}

template <class _Ty, enable_if_t<is_array_v<_Ty>&& extent_v<_Ty> == 0, int> = 0>
WHP_NODISCARD_MSG("scope returned as unnecessary") inline scope<_Ty> make_scope(const size_t _Size)
{
	using _Elem = std::remove_extent_t<_Ty>;
	return scope<_Ty>(new _Elem[_Size]());
}

template <class _Ty, class... _Types, enable_if_t<extent_v<_Ty> != 0, int> = 0>
void make_scope(_Types&&...) = delete;

template <class T, class... _Types>
WHP_NODISCARD enable_if_t<!is_array_v<T>, ref<T>> make_ref(_Types&&... args)
{
	return std::make_shared<T, _Types...>(args...);
}



_WHIP_END