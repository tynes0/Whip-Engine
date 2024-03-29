#pragma once
#ifndef _WHIP_TYPE_TRAITS_
#define _WHIP_TYPE_TRAITS_

#include "Whip/Core/Core.h"

#pragma warning(push)
#pragma warning(disable : _WHP_DISABLED_WARNINGS)

_WHIP_START

template <class _Ty, _Ty _Val>
struct integral_constant
{
	static constexpr _Ty value = _Val;

	using type = typename _Ty;
	using value_type = integral_constant;

	constexpr operator type() { return value; }
	WHP_NODISCARD constexpr type operator()() { return value; }
};

template <bool _Test>
using bool_constant = integral_constant<bool, _Test>;

using true_type = bool_constant<true>;
using false_type = bool_constant<false>;

template <class... _Types>
using void_t = void;

template <class _Ty>
struct identity 
{
	using type = _Ty;
};
template <class _Ty>
using identity_t _WHP_MSVC_KNOWN_SEMANTICS = typename identity<_Ty>::type;

template <bool _Test, class _Ty = void>
struct enable_if {};

template <class _Ty>
struct enable_if<true, _Ty>
{
	using type = _Ty;
};

template <bool _Test, class _Ty = void>
using enable_if_t = typename enable_if<_Test, _Ty>::type;

namespace detail_type_traits
{
	template <class _Ty, class = void>
	struct add_reference
	{ // add reference (non-referenceable type)
		using _Lvalue = _Ty;
		using _Rvalue = _Ty;
	};

	template <class _Ty>
	struct add_reference<_Ty, void_t<_Ty&>>
	{ // (referenceable type)
		using _lvalue = _Ty&;
		using _rvalue = _Ty&&;
	};
}

template <class _Ty>
struct add_lvalue_reference 
{
	using type = typename detail_type_traits::add_reference<_Ty>::_lvalue;
};

template <class _Ty>
using add_lvalue_reference_t = typename detail_type_traits::add_reference<_Ty>::_lvalue;

template <class _Ty>
struct add_rvalue_reference 
{
	using type = typename detail_type_traits::add_reference<_Ty>::_rvalue;
};

template <class _Ty>
using add_rvalue_reference_t = typename detail_type_traits::add_reference<_Ty>::_rvalue;

template <class _Ty>
add_rvalue_reference_t<_Ty> declval() noexcept {}

template <class _Ty>
struct remove_extent
{
	using type = _Ty;
};

template <class _Ty, size_t _Ix>
struct remove_extent<_Ty[_Ix]>
{
	using type = _Ty;
};

template <class _Ty>
struct remove_extent<_Ty[]>
{
	using type = _Ty;
};

template <class _Ty>
using remove_extent_t = typename remove_extent<_Ty>::type;

template <class _Ty>
struct remove_reference
{
	using type = _Ty;
	using const_ref_type = const _Ty;
};

template <class _Ty>
struct remove_reference<_Ty&>
{
	using type = _Ty;
	using const_ref_type = const _Ty&;
};

template <class _Ty>
struct remove_reference<_Ty&&>
{
	using type = _Ty;
	using const_ref_type = const _Ty&&;
};

template <class _Ty>
using remove_reference_t = typename remove_reference<_Ty>::type;

template <class _Ty>
using const_ref = typename remove_reference<_Ty>::const_ref_type;

template <class _Ty>
struct remove_pointer
{
	using type = _Ty;
	using const_ptr_type = const _Ty;
};

template <class _Ty>
struct remove_pointer<_Ty*>
{
	using type = _Ty;
	using const_ptr_type = const _Ty*;
};

template <class _Ty>
using remove_pointer_t = typename remove_pointer<_Ty>::type;

template <class _Ty>
struct remove_all_extents 
{
	using type = _Ty;
};

template <class _Ty, size_t _Ix>
struct remove_all_extents<_Ty[_Ix]> 
{
	using type = typename remove_all_extents<_Ty>::type;
};

template <class _Ty>
struct remove_all_extents<_Ty[]>
{
	using type = typename remove_all_extents<_Ty>::type;
};

template <class _Ty>
using remove_all_extents_t = typename remove_all_extents<_Ty>::type;

template <class _Ty>
struct remove_cv 
{
	using type = _Ty;

	template <template <class> class _Fn>
	using _Apply = _Fn<_Ty>;
};

template <class _Ty>
struct remove_cv<const _Ty> 
{
	using type = _Ty;

	template <template <class> class _Fn>
	using _Apply = const _Fn<_Ty>;
};

template <class _Ty>
struct remove_cv<volatile _Ty> 
{
	using type = _Ty;

	template <template <class> class _Fn>
	using _Apply = volatile _Fn<_Ty>;
};

template <class _Ty>
struct remove_cv<const volatile _Ty>
{
	using type = _Ty;

	template <template <class> class _Fn>
	using _Apply = const volatile _Fn<_Ty>;
};

template <class _Ty>
using remove_cv_t = typename remove_cv<_Ty>::type;

template <class _Ty>
using const_ptr = typename remove_pointer<_Ty>::const_ptr_type;

template <class _Ty>
struct remove_const
{
	using type = _Ty;
};

template <class _Ty>
struct remove_const<const _Ty>
{
	using type = _Ty;
};

template <class _Ty>
using remove_const_t = typename remove_const<_Ty>::type;

template <class, class>
WHP_INLINE constexpr bool is_same_v = false;

template <class _Ty>
WHP_INLINE constexpr bool is_same_v<_Ty, _Ty> = true;

template <class _Ty1, class _Ty2>
struct is_same : bool_constant<is_same_v<_Ty1, _Ty2>> {};

template <class>
WHP_INLINE constexpr bool is_array_v = false;

template <class _Ty, size_t n>
WHP_INLINE constexpr bool is_array_v<_Ty[n]> = true;

template <class _Ty>
WHP_INLINE constexpr bool is_array_v<_Ty[]> = true;

template <class _Ty>
struct is_array : bool_constant<is_array_v<_Ty>> {};

template <class>
WHP_INLINE constexpr bool is_bounded_array_v = false;

template <class _Ty, size_t _Nx>
WHP_INLINE constexpr bool is_bounded_array_v<_Ty[_Nx]> = true;

template <class _Ty>
struct is_bounded_array : bool_constant<is_bounded_array_v<_Ty>> {};

template <class>
WHP_INLINE constexpr bool is_unbounded_array_v = false;

template <class _Ty>
WHP_INLINE constexpr bool is_unbounded_array_v<_Ty[]> = true;

template <class _Ty>
struct is_unbounded_array : bool_constant<is_unbounded_array_v<_Ty>> {};

template <class>
WHP_INLINE constexpr bool is_lvalue_reference_v = false;

template <class _Ty>
WHP_INLINE constexpr bool is_lvalue_reference_v<_Ty&> = true;

template <class _Ty>
struct is_lvalue_reference : bool_constant<is_lvalue_reference_v<_Ty>> {};

template <class>
WHP_INLINE constexpr bool is_rvalue_reference_v = false;

template <class _Ty>
WHP_INLINE constexpr bool is_rvalue_reference_v<_Ty&&> = true;

template <class _Ty>
struct is_rvalue_reference : bool_constant<is_rvalue_reference_v<_Ty>> {};

template <class>
WHP_INLINE constexpr bool is_reference_v = false;

template <class _Ty>
WHP_INLINE constexpr bool is_reference_v<_Ty&> = true;

template <class _Ty>
WHP_INLINE constexpr bool is_reference_v<_Ty&&> = true;

template <class _Ty>
struct is_reference : bool_constant<is_reference_v<_Ty>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_pointer_v = false;

template <class _Ty>
WHP_INLINE constexpr bool is_pointer_v<_Ty*> = true;

template <class _Ty>
struct is_pointer : bool_constant<is_pointer_v<_Ty>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_empty_v = (sizeof(_Ty) == 0) || (sizeof(_Ty) == 1);

template <class _Ty>
struct is_empty : bool_constant<is_empty_v<_Ty>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_final_v = __isfinal(_Ty);

template <class _Ty>
struct is_final : bool_constant<is_final_v<_Ty>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_void_v = is_same_v<remove_cv_t<_Ty>, void>;

template <class _Ty>
struct is_void : bool_constant<is_void_v<_Ty>> {};

namespace detail_type_traits
{
	template <class, class = void_t<>>
	struct has_value_type_helper : false_type {};

	template <class _Ty>
	struct has_value_type_helper<_Ty, void_t<typename _Ty::value_type>> : true_type {};

	template <class, class = void_t<>>
	struct has_difference_type_helper : false_type {};

	template <class _Ty>
	struct has_difference_type_helper<_Ty, void_t<typename _Ty::difference_type>> : true_type {};

	template <class, class = void_t<>>
	struct has_pointer_type_helper : false_type {};

	template <class _Ty>
	struct has_pointer_type_helper<_Ty, void_t<typename _Ty::pointer>> : true_type {};

	template <class, class = void_t<>>
	struct has_reference_type_helper : false_type {};

	template <class _Ty>
	struct has_reference_type_helper<_Ty, void_t<typename _Ty::reference>> : true_type {};

	template <class, class = void_t<>>
	struct has_size_type_helper : false_type {};

	template <class _Ty>
	struct has_size_type_helper<_Ty, void_t<typename _Ty::size_type>> : true_type {};
}

template <class _Ty>
struct has_value_type : public detail_type_traits::has_value_type_helper<_Ty> {};

template <class _Ty>
WHP_INLINE constexpr bool has_value_type_v = has_value_type<_Ty>::value;

template <class _Ty>
struct has_difference_type : public detail_type_traits::has_difference_type_helper<_Ty> {};

template <class _Ty>
WHP_INLINE constexpr bool has_difference_type_v = has_difference_type<_Ty>::value;

template <class _Ty>
struct has_size_type : public detail_type_traits::has_size_type_helper<_Ty> {};

template <class _Ty>
WHP_INLINE constexpr bool has_size_type_v = has_size_type<_Ty>::value;

template <class _Ty>
struct has_pointer_type : public detail_type_traits::has_pointer_type_helper<_Ty> {};

template <class _Ty>
WHP_INLINE constexpr bool has_pointer_type_v = has_pointer_type<_Ty>::value;

template <class _Ty>
struct has_reference_type : public detail_type_traits::has_reference_type_helper<_Ty> {};

template <class _Ty>
WHP_INLINE constexpr bool has_reference_type_v = has_reference_type<_Ty>::value;

namespace detail_type_traits
{
	template <bool _First_value, class _First, class... _Rest>
	struct disjunction_helper
	{
		using type = _First;
	};

	template <class _False, class _Next, class... _Rest>
	struct disjunction_helper<false, _False, _Next, _Rest...>
	{
		using type = typename disjunction_helper<_Next::value, _Next, _Rest...>::type;
	};
}

template <class... _Traits>
struct disjunction : false_type {}; // If _Traits is empty, false_type

template <class _First, class... _Rest>
struct disjunction<_First, _Rest...> : detail_type_traits::disjunction_helper<_First::value, _First, _Rest...>::type {};

template <class... _Traits>
WHP_INLINE constexpr bool disjunction_v = disjunction<_Traits...>::value;

template <class _Ty, class... _Types>
WHP_INLINE constexpr bool is_any_of_v = disjunction_v<is_same<_Ty, _Types>...>;

template <class _From, class _To>
WHP_INLINE constexpr bool is_convertible_v = __is_convertible_to(_From, _To);

template <class _From, class _To>
struct is_convertible : bool_constant<is_convertible_v<_From, _To>> {};

template <typename Base, typename Derived>
struct is_base_of
{
private:
	static Derived* createDerived() {};
	static char test(Base*) {};
	static int test(...) {};
public:
	static constexpr bool value = sizeof(test(createDerived())) == sizeof(char);
};

template <typename Base, typename Derived>
WHP_INLINE constexpr bool is_base_of_v = is_base_of<Base, Derived>::value;

template <class _Ty>
WHP_INLINE constexpr bool is_trivial_v = __is_trivially_constructible(_Ty) && __is_trivially_copyable(_Ty);

template <class _Ty>
struct is_trivial : bool_constant<is_trivial_v<_Ty>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_destructible_v = __is_destructible(_Ty);

template <class _Ty>
struct is_destructible : bool_constant<is_destructible_v<_Ty>> {};

template <class _Ty, class... _Args>
WHP_INLINE constexpr bool is_trivially_constructible_v = __is_trivially_constructible(_Ty, _Args...);

template <class _Ty, class... _Args>
struct is_trivially_constructible : bool_constant<is_trivially_constructible_v<_Ty, _Args...>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_trivially_copy_constructible_v = __is_trivially_constructible(_Ty, add_lvalue_reference_t<const _Ty>);

template <class _Ty>
struct is_trivially_copy_constructible : bool_constant<is_trivially_constructible_v<_Ty, add_lvalue_reference_t<const _Ty>>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_trivially_default_constructible_v = __is_trivially_constructible(_Ty);

template <class _Ty>
struct is_trivially_default_constructible : bool_constant<is_trivially_default_constructible_v<_Ty>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_trivially_move_constructible_v = __is_trivially_constructible(_Ty, _Ty);

template <class _Ty>
struct is_trivially_move_constructible : bool_constant<is_trivially_move_constructible_v<_Ty>> {};

template <class _To, class _From>
WHP_INLINE constexpr bool is_trivially_assignable_v = __is_trivially_assignable(_To, _From);

template <class _To, class _From>
struct is_trivially_assignable : bool_constant<is_trivially_assignable_v<_To, _From>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_trivially_copy_assignable_v = __is_trivially_assignable(add_lvalue_reference_t<_Ty>, add_lvalue_reference_t<const _Ty>);

template <class _Ty>
struct is_trivially_copy_assignable : bool_constant<is_trivially_copy_assignable_v<_Ty>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_trivially_move_assignable_v = __is_trivially_assignable(add_lvalue_reference_t<_Ty>, _Ty);

template <class _Ty>
struct is_trivially_move_assignable : bool_constant<is_trivially_move_assignable_v<_Ty>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_trivially_destructible_v = __is_trivially_destructible(_Ty);

template <class _Ty>
struct is_trivially_destructible : bool_constant<is_trivially_destructible_v<_Ty>> {};

template <class _Ty, class... _Args>
WHP_INLINE constexpr bool is_nothrow_constructible_v = __is_nothrow_constructible(_Ty, _Args...);

template <class _Ty, class... _Args>
struct is_nothrow_constructible : bool_constant<is_nothrow_constructible_v<_Ty, _Args>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_nothrow_copy_constructible_v = __is_nothrow_constructible(_Ty, add_lvalue_reference_t<const _Ty>);

template <class _Ty>
struct is_nothrow_copy_constructible : bool_constant<is_nothrow_copy_constructible_v<_Ty>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_nothrow_default_constructible_v = __is_nothrow_constructible(_Ty);

template <class _Ty>
struct is_nothrow_default_constructible : bool_constant<is_nothrow_default_constructible_v<_Ty>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_nothrow_move_constructible_v = __is_nothrow_constructible(_Ty, _Ty);

template <class _Ty>
struct is_nothrow_move_constructible : bool_constant<is_nothrow_move_constructible_v<_Ty>> {};

template <class _To, class _From>
WHP_INLINE constexpr bool is_nothrow_assignable_v = __is_nothrow_assignable(_To, _From);

template <class _To, class _From>
struct is_nothrow_assignable : bool_constant<is_nothrow_assignable_v<_To, _From>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_nothrow_copy_assignable_v = __is_nothrow_assignable(add_lvalue_reference_t<_Ty>, add_lvalue_reference_t<const _Ty>);

template <class _Ty>
struct is_nothrow_copy_assignable : bool_constant<is_nothrow_copy_assignable_v<_Ty>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_nothrow_move_assignable_v = __is_nothrow_assignable(add_lvalue_reference_t<_Ty>, _Ty);

template <class _Ty>
struct is_nothrow_move_assignable : bool_constant<is_nothrow_move_assignable_v<_Ty>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_nothrow_destructible_v = __is_nothrow_destructible(_Ty);

template <class _Ty>
struct is_nothrow_destructible : bool_constant<is_nothrow_destructible_v<_Ty>> {};

template <class _Ty>
WHP_INLINE constexpr bool is_move_constructible_v = __is_constructible(_Ty, _Ty);

template <class _Ty>
struct is_move_constructible : bool_constant<is_move_constructible_v<_Ty>> {};

template <bool _Test, class _Ty1, class _Ty2>
struct conditional 
{
	using type = _Ty1;
};

template <class _Ty1, class _Ty2>
struct conditional<false, _Ty1, _Ty2> 
{
	using type = _Ty2;
};

template <bool _Test, class _Ty1, class _Ty2>
using conditional_t = typename conditional<_Test, _Ty1, _Ty2>::type;

template <class _Ty, unsigned int _Ix = 0>
WHP_INLINE constexpr size_t extent_v = 0;

template <class _Ty, size_t _Nx>
WHP_INLINE constexpr size_t extent_v<_Ty[_Nx], 0> = _Nx;

template <class _Ty, unsigned int _Ix, size_t _Nx>
WHP_INLINE constexpr size_t extent_v<_Ty[_Nx], _Ix> = extent_v<_Ty, _Ix - 1>;

template <class _Ty, unsigned int _Ix>
WHP_INLINE constexpr size_t extent_v<_Ty[], _Ix> = extent_v<_Ty, _Ix - 1>;

template <class _Ty, unsigned int _Ix = 0>
struct extent : integral_constant<size_t, extent_v<_Ty, _Ix>> {};

template <typename... Args>
struct conjunction : true_type {};

template <typename T, typename... Args>
struct conjunction<T, Args...> : conditional_t<T::value, conjunction<Args...>, T> {};

template <typename... Args>
WHP_INLINE constexpr bool conjunction_v = conjunction<Args...>::value;

// TODO: ALL WHIP!

template <class _Ty1, class _Ty2>
struct is_swappable_with : std::_Is_swappable_with<_Ty1, _Ty2>::type {};

template <class _Ty1, class _Ty2>
WHP_INLINE constexpr bool is_swappable_with_v = conjunction_v<std::_Swappable_with_helper<_Ty1, _Ty2>, std::_Swappable_with_helper<_Ty2, _Ty1>>;

template <class _Ty>
struct is_swappable : std::_Is_swappable<_Ty>::type {};

template <class _Ty>
WHP_INLINE constexpr bool is_swappable_v = std::_Is_swappable<_Ty>::value;

template <class _Ty1, class _Ty2>
struct is_nothrow_swappable_with : std::_Is_nothrow_swappable_with<_Ty1, _Ty2>::type {};

template <class _Ty1, class _Ty2>
WHP_INLINE constexpr bool is_nothrow_swappable_with_v = std::_Is_nothrow_swappable_with<_Ty1, _Ty2>::value;

template <class _Ty>
struct is_nothrow_swappable : std::_Is_nothrow_swappable<_Ty>::type {};

template <class _Ty>
WHP_INLINE constexpr bool is_nothrow_swappable_v = std::_Is_nothrow_swappable<_Ty>::value;

namespace detail_type_traits
{
	template <bool>
	struct _Select
	{
		template <class _Ty1, class>
		using _Apply = _Ty1;
	};

	template <>
	struct _Select<false>
	{
		template <class, class _Ty2>
		using _Apply = _Ty2;
	};

	template <size_t>
	struct _Make_signed2;

	template <>
	struct _Make_signed2<1>
	{
		template <class>
		using _Apply = signed char;
	};

	template <>
	struct _Make_signed2<2>
	{
		template <class>
		using _Apply = short;
	};

	template <>
	struct _Make_signed2<4>
	{
		template <class _Ty>
		using _Apply = typename _Select<is_same_v<_Ty, long> || is_same_v<_Ty, unsigned long>>::template _Apply<long, int>;
	};

	template <>
	struct _Make_signed2<8>
	{
		template <class>
		using _Apply = long long;
	};

	template <class _Ty>
	using _Make_signed1 = typename _Make_signed2<sizeof(_Ty)>::template _Apply<_Ty>;


	template <size_t>
	struct _Make_unsigned2;

	template <>
	struct _Make_unsigned2<1>
	{
		template <class>
		using _Apply = unsigned char;
	};

	template <>
	struct _Make_unsigned2<2>
	{
		template <class>
		using _Apply = unsigned short;
	};

	template <>
	struct _Make_unsigned2<4>
	{
		template <class _Ty>
		using _Apply = typename _Select<is_same_v<_Ty, long> || is_same_v<_Ty, unsigned long>>::template _Apply<unsigned long, unsigned int>;
	};

	template <>
	struct _Make_unsigned2<8>
	{
		template <class>
		using _Apply = unsigned long long;
	};

	template <class _Ty>
	using _Make_unsigned1 = typename _Make_unsigned2<sizeof(_Ty)>::template _Apply<_Ty>;

}

template <class _Ty>
struct make_signed 
{ 
	static_assert(std::_Is_nonbool_integral<_Ty> || std::is_enum_v<_Ty>, "make_signed<T> requires that T shall be a (possibly cv-qualified) integral type or enumeration but not a bool type.");
	using type = typename remove_cv<_Ty>::template _Apply<detail_type_traits::_Make_signed1>;
};

template <class _Ty>
using make_signed_t = typename make_signed<_Ty>::type;

template <class _Ty>
struct make_unsigned
{ // unsigned partner to _Ty
	static_assert(std::_Is_nonbool_integral<_Ty> || std::is_enum_v<_Ty>, "make_unsigned<T> requires that T shall be a (possibly cv-qualified) integral type or enumeration but not a bool type.");

	using type = typename remove_cv<_Ty>::template _Apply<detail_type_traits::_Make_unsigned1>;
};

template <class _Ty>
using make_unsigned_t = typename make_unsigned<_Ty>::type;

template <class _Rep>
constexpr make_unsigned_t<_Rep> to_unsigned(_Rep _Val) 
{
	return static_cast<make_unsigned_t<_Rep>>(_Val);
}

_WHIP_END

#pragma warning(pop)

#endif // !_WHIP_TYPE_TRAITS_