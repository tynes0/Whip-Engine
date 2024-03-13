#pragma once

#include <Whip/Core/Core.h>

#include "TypeTraits.h"
#include "Invoker.h"

#include <type_traits>

_WHIP_START

namespace detail_refwraper
{

	template <class... _Types>
	struct arg_types {}; // provide argument_type, etc. when sizeof...(_Types) is 1 or 2

	template <class _Ty1>
	struct arg_types<_Ty1> 
	{
		using _ARGUMENT_TYPE_NAME _CXX17_DEPRECATE_ADAPTOR_TYPEDEFS = _Ty1;
	};

	template <class _Ty1, class _Ty2>
	struct arg_types<_Ty1, _Ty2> 
	{
		using _FIRST_ARGUMENT_TYPE_NAME _CXX17_DEPRECATE_ADAPTOR_TYPEDEFS = _Ty1;
		using _SECOND_ARGUMENT_TYPE_NAME _CXX17_DEPRECATE_ADAPTOR_TYPEDEFS = _Ty2;
	};

	template <class _Ty>
	struct function_args {}; // determine whether _Ty is a function

#define _FUNCTION_ARGS(CALL_OPT, CV_OPT, REF_OPT, NOEXCEPT_OPT)                                           \
    template <class _Ret, class... _Types>                                                                \
    struct function_args<_Ret CALL_OPT(_Types...) CV_OPT REF_OPT NOEXCEPT_OPT> : arg_types<_Types...> {  \
    };

	_NON_MEMBER_CALL_CV_REF_NOEXCEPT(_FUNCTION_ARGS)
#undef _FUNCTION_ARGS

#define _FUNCTION_ARGS_ELLIPSIS(CV_REF_NOEXCEPT_OPT)                                                            \
    template <class _Ret, class... _Types>                                                                      \
    struct function_args<_Ret(_Types..., ...) CV_REF_NOEXCEPT_OPT> { /* no calling conventions for ellipsis */ \
    };

		_CLASS_DEFINE_CV_REF_NOEXCEPT(_FUNCTION_ARGS_ELLIPSIS)
#undef _FUNCTION_ARGS_ELLIPSIS

		template <class _Ty, class = void>
	struct weak_result_type {}; // default definition

	_STL_DISABLE_DEPRECATED_WARNING
		template <class _Ty>
	struct weak_result_type<_Ty, void_t<typename _Ty::result_type>> 
	{
		using _RESULT_TYPE_NAME _CXX17_DEPRECATE_ADAPTOR_TYPEDEFS = typename _Ty::result_type;
	};
	_STL_RESTORE_DEPRECATED_WARNING

		template <class _Ty, class = void>
	struct weak_argument_type : weak_result_type<_Ty> {}; // default definition

	_STL_DISABLE_DEPRECATED_WARNING
		template <class _Ty>
	struct weak_argument_type<_Ty, void_t<typename _Ty::argument_type>> : weak_result_type<_Ty> {
		// defined if _Ty::argument_type exists
		using _ARGUMENT_TYPE_NAME _CXX17_DEPRECATE_ADAPTOR_TYPEDEFS = typename _Ty::argument_type;
	};
	_STL_RESTORE_DEPRECATED_WARNING

		template <class _Ty, class = void>
	struct weak_binary_args : weak_argument_type<_Ty> {}; // default definition

	_STL_DISABLE_DEPRECATED_WARNING
		template <class _Ty>
	struct weak_binary_args<_Ty, void_t<typename _Ty::first_argument_type,
		typename _Ty::second_argument_type>>
		: weak_argument_type<_Ty>
	{
		using _FIRST_ARGUMENT_TYPE_NAME _CXX17_DEPRECATE_ADAPTOR_TYPEDEFS = typename _Ty::first_argument_type;
		using _SECOND_ARGUMENT_TYPE_NAME _CXX17_DEPRECATE_ADAPTOR_TYPEDEFS = typename _Ty::second_argument_type;
	};
	_STL_RESTORE_DEPRECATED_WARNING

		template <class _Ty>
	using weak_types = conditional_t<std::is_function_v<remove_pointer_t<_Ty>>, function_args<remove_pointer_t<_Ty>>, conditional_t<std::is_member_function_pointer_v<_Ty>, std::_Is_memfunptr<remove_cv_t<_Ty>>, weak_binary_args<_Ty>>>;

	template <class _Ty>
	void refwrap_ctor_fun(identity_t<_Ty&>) noexcept; // not defined
	template <class _Ty>
	void refwrap_ctor_fun(identity_t<_Ty&&>) = delete;

	template <class _Ty, class _Uty, class = void>
	struct refwrap_has_ctor_from : false_type {};

	template <class _Ty, class _Uty>
	struct refwrap_has_ctor_from<_Ty, _Uty, void_t<decltype(refwrap_ctor_fun<_Ty>(whip::declval<_Uty>()))>>
		: true_type {};
}

template <class _Ty>
class reference_wrapper
{
public:
	static_assert(std::is_object_v<_Ty> || std::is_function_v<_Ty>, "whip::reference_wrapper<T> requires T to be an object type or a function type.");

	using type = _Ty;

	template <class _Uty, enable_if_t<conjunction_v<std::negation<is_same<std::_Remove_cvref_t<_Uty>, reference_wrapper>>, detail_refwraper::refwrap_has_ctor_from<_Ty, _Uty>>, int> = 0>
	WHP_CONSTEXPR reference_wrapper(_Uty&& _Val) noexcept(noexcept(detail_refwraper::refwrap_ctor_fun<_Ty>(whip::declval<_Uty>())))
	{
		_Ty& _Ref = static_cast<_Uty&&>(_Val);
		_Ptr = whip::addressof(_Ref);
	}

	WHP_CONSTEXPR operator _Ty& () const noexcept
	{
		return *_Ptr;
	}

	WHP_NODISCARD WHP_CONSTEXPR _Ty& get() const noexcept
	{
		return *_Ptr;
	}

private:
	_Ty* _Ptr{};

public:
	template <class... _Types>
	WHP_CONSTEXPR auto operator()(_Types&&... _Args) const
		noexcept(noexcept(whip::invoke(*_Ptr, static_cast<_Types&&>(_Args)...)))-> decltype(whip::invoke(*_Ptr, static_cast<_Types&&>(_Args)...))
	{
		return whip::invoke(*_Ptr, static_cast<_Types&&>(_Args)...);
	}
};

#if _HAS_CXX17
template <class _Ty>
reference_wrapper(_Ty&) -> reference_wrapper<_Ty>;
#endif // _HAS_CXX17

template <class _Ty>
WHP_NODISCARD _CONSTEXPR20 reference_wrapper<_Ty> reference(_Ty& val) noexcept
{
	return reference_wrapper<_Ty>(val);
}

template <class _Ty>
void reference(const _Ty&&) = delete;

template <class _Ty>
WHP_NODISCARD _CONSTEXPR20 reference_wrapper<_Ty> reference(reference_wrapper<_Ty> val) noexcept
{
	return val;
}

template <class _Ty>
WHP_NODISCARD _CONSTEXPR20 reference_wrapper<const _Ty> creference(const _Ty& val) noexcept
{
	return reference_wrapper<const _Ty>(val);
}

template <class _Ty>
void creference(const _Ty&&) = delete;

template <class _Ty>
WHP_NODISCARD _CONSTEXPR20 reference_wrapper<const _Ty> creference(reference_wrapper<_Ty> val) noexcept
{
	return val;
}

#if _HAS_CXX20
template <class _Ty>
struct unwrap_reference 
{
	using type = _Ty;
};
template <class _Ty>
struct unwrap_reference<reference_wrapper<_Ty>> 
{
	using type = _Ty&;
};
template <class _Ty>
using unwrap_reference_t = typename unwrap_reference<_Ty>::type;

template <class _Ty>
using unwrap_ref_decay_t = unwrap_reference_t<std::decay_t<_Ty>>;
template <class _Ty>
struct unwrap_ref_decay 
{
	using type = unwrap_ref_decay_t<_Ty>;
};
#endif // _HAS_CXX20

_WHIP_END