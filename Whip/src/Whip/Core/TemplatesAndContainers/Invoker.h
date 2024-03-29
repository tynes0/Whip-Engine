#pragma once
#ifndef _WHIP_INVOKER_
#define _WHIP_INVOKER_

#include <Whip/Core/Core.h>
#include <Whip/Core/TemplatesAndContainers/TypeTraits.h>

#include <type_traits>

#pragma warning(push)
#pragma warning(disable : _WHP_DISABLED_WARNINGS)

_WHIP_START

template <class _Ty>
class reference_wrapper;

namespace detail_invoker
{
    enum class invoker_strategy 
    {
        functor,
        pmf_object,
        pmf_refwrap,
        pmf_pointer,
        pmd_object,
        pmd_refwrap,
        pmd_pointer
    };

    struct invoker_functor
    {
        static constexpr invoker_strategy strategy = invoker_strategy::functor;

        template <class _Callable, class... _Types>
        static constexpr auto call(_Callable&& _Obj, _Types&&... _Args) noexcept(noexcept(static_cast<_Callable&&>(_Obj)(static_cast<_Types&&>(_Args)...)))-> decltype(static_cast<_Callable&&>(_Obj)(static_cast<_Types&&>(_Args)...)) 
        {
            return static_cast<_Callable&&>(_Obj)(static_cast<_Types&&>(_Args)...);
        }
    };

    struct invoker_pmf_object 
    {
        static constexpr invoker_strategy strategy = invoker_strategy::pmf_object;

        template <class _Decayed, class _Ty1, class... _Types2>
        static constexpr auto call(_Decayed _Pmf, _Ty1&& _Arg1, _Types2&&... _Args2) noexcept(noexcept((static_cast<_Ty1&&>(_Arg1).*_Pmf)(static_cast<_Types2&&>(_Args2)...)))-> decltype((static_cast<_Ty1&&>(_Arg1).*_Pmf)(static_cast<_Types2&&>(_Args2)...)) 
        {
            return (static_cast<_Ty1&&>(_Arg1).*_Pmf)(static_cast<_Types2&&>(_Args2)...);
        }
    };

    struct invoker_pmf_refwrap 
    {
        static constexpr invoker_strategy strategy = invoker_strategy::pmf_refwrap;

        template <class _Decayed, class _Refwrap, class... _Types2>
        static constexpr auto call(_Decayed _Pmf, _Refwrap _Rw, _Types2&&... _Args2) noexcept(noexcept((_Rw.get().*_Pmf)(static_cast<_Types2&&>(_Args2)...)))-> decltype((_Rw.get().*_Pmf)(static_cast<_Types2&&>(_Args2)...)) 
        {
            return (_Rw.get().*_Pmf)(static_cast<_Types2&&>(_Args2)...);
        }
    };

    struct invoker_pmf_pointer 
    {
        static constexpr invoker_strategy strategy = invoker_strategy::pmf_pointer;

        template <class _Decayed, class _Ty1, class... _Types2>
        static constexpr auto call(_Decayed _Pmf, _Ty1&& _Arg1, _Types2&&... _Args2) noexcept(noexcept(((*static_cast<_Ty1&&>(_Arg1)).*_Pmf)(static_cast<_Types2&&>(_Args2)...)))-> decltype(((*static_cast<_Ty1&&>(_Arg1)).*_Pmf)(static_cast<_Types2&&>(_Args2)...)) 
        {
            return ((*static_cast<_Ty1&&>(_Arg1)).*_Pmf)(static_cast<_Types2&&>(_Args2)...);
        }
    };

    struct invoker_pmd_object 
    {
        static constexpr invoker_strategy strategy = invoker_strategy::pmd_object;

        template <class _Decayed, class _Ty1>
        static constexpr auto call(_Decayed _Pmd, _Ty1&& _Arg1) noexcept -> decltype(static_cast<_Ty1&&>(_Arg1).*_Pmd) 
        {
            return static_cast<_Ty1&&>(_Arg1).*_Pmd;
        }
    };

    struct invoker_pmd_refwrap 
    {
        static constexpr invoker_strategy strategy = invoker_strategy::pmd_refwrap;

        template <class _Decayed, class _Refwrap>
        static constexpr auto call(_Decayed _Pmd, _Refwrap _Rw) noexcept -> decltype(_Rw.get().*_Pmd) 
        {
            return _Rw.get().*_Pmd;
        }
    };

    struct invoker_pmd_pointer
    {
        static constexpr invoker_strategy strategy = invoker_strategy::pmd_pointer;

        template <class _Decayed, class _Ty1>
        static constexpr auto call(_Decayed _Pmd, _Ty1&& _Arg1) noexcept(noexcept((*static_cast<_Ty1&&>(_Arg1)).*_Pmd))-> decltype((*static_cast<_Ty1&&>(_Arg1)).*_Pmd) 
        {
            return (*static_cast<_Ty1&&>(_Arg1)).*_Pmd;
        }
    };

    template <class _Callable, class _Ty1, class _Removed_cvref = std::_Remove_cvref_t<_Callable>,bool is_pmf = std::is_member_function_pointer_v<_Removed_cvref>, bool is_pmd = std::is_member_object_pointer_v<_Removed_cvref>>
        struct invoker1;

    template <class _Callable, class _Ty1, class _Removed_cvref>
    struct invoker1<_Callable, _Ty1, _Removed_cvref, true, false>
        : conditional_t<is_base_of_v<typename std::_Is_memfunptr<_Removed_cvref>::_Class_type, remove_reference_t<_Ty1>>, invoker_pmf_object, conditional_t<std::_Is_specialization_v<std::_Remove_cvref_t<_Ty1>, reference_wrapper>, invoker_pmf_refwrap, invoker_pmf_pointer>> {};

    template <class _Callable, class _Ty1, class _Removed_cvref>
    struct invoker1<_Callable, _Ty1, _Removed_cvref, false, true>
        : conditional_t<is_base_of_v<typename std::_Is_member_object_pointer<_Removed_cvref>::_Class_type, remove_reference_t<_Ty1>>, invoker_pmd_object, conditional_t<std::_Is_specialization_v<std::_Remove_cvref_t<_Ty1>, reference_wrapper>, invoker_pmd_refwrap, invoker_pmd_pointer>> {};

    template <class _Callable, class _Ty1, class _Removed_cvref>
    struct invoker1<_Callable, _Ty1, _Removed_cvref, false, false> : invoker_functor {};
} // detail_invoker



_EXPORT_STD template <class _Callable>
WHP_CONSTEXPR17 auto invoke(_Callable&& _Obj) noexcept(noexcept(static_cast<_Callable&&>(_Obj)()))
-> decltype(static_cast<_Callable&&>(_Obj)()) 
{
    return static_cast<_Callable&&>(_Obj)();
}

_EXPORT_STD template <class _Callable, class _Ty1, class... _Types2>
WHP_CONSTEXPR17 auto invoke(_Callable&& _Obj, _Ty1&& _Arg1, _Types2&&... _Args2) noexcept(noexcept(detail_invoker::invoker1<_Callable, _Ty1>::call(static_cast<_Callable&&>(_Obj), static_cast<_Ty1&&>(_Arg1), static_cast<_Types2&&>(_Args2)...)))
    -> decltype(detail_invoker::invoker1<_Callable, _Ty1>::call(static_cast<_Callable&&>(_Obj), static_cast<_Ty1&&>(_Arg1), static_cast<_Types2&&>(_Args2)...)) 
{
    if constexpr (detail_invoker::invoker1<_Callable, _Ty1>::strategy == detail_invoker::invoker_strategy::functor) 
    {
        return static_cast<_Callable&&>(_Obj)(static_cast<_Ty1&&>(_Arg1), static_cast<_Types2&&>(_Args2)...);
    }
    else if constexpr (detail_invoker::invoker1<_Callable, _Ty1>::strategy == detail_invoker::invoker_strategy::pmf_object) 
    {
        return (static_cast<_Ty1&&>(_Arg1).*_Obj)(static_cast<_Types2&&>(_Args2)...);
    }
    else if constexpr (detail_invoker::invoker1<_Callable, _Ty1>::strategy == detail_invoker::invoker_strategy::pmf_refwrap) 
    {
        return (_Arg1.get().*_Obj)(static_cast<_Types2&&>(_Args2)...);
    }
    else if constexpr (detail_invoker::invoker1<_Callable, _Ty1>::strategy == detail_invoker::invoker_strategy::pmf_pointer) 
    {
        return ((*static_cast<_Ty1&&>(_Arg1)).*_Obj)(static_cast<_Types2&&>(_Args2)...);
    }
    else if constexpr (detail_invoker::invoker1<_Callable, _Ty1>::strategy == detail_invoker::invoker_strategy::pmd_object) 
    {
        return static_cast<_Ty1&&>(_Arg1).*_Obj;
    }
    else if constexpr (detail_invoker::invoker1<_Callable, _Ty1>::strategy == detail_invoker::invoker_strategy::pmd_refwrap) 
    {
        return _Arg1.get().*_Obj;
    }
    else {
        static_assert(detail_invoker::invoker1<_Callable, _Ty1>::strategy == detail_invoker::invoker_strategy::pmd_pointer, "bug in invoke");
        return (*static_cast<_Ty1&&>(_Arg1)).*_Obj;
    }
}

namespace detail_invoker
{
    template <class _From, class _To, class = void>
    struct invoke_convertible : false_type {};

    template <class _From, class _To>
    struct invoke_convertible<_From, _To, void_t<decltype(_STD _Fake_copy_init<_To>(_STD _Returns_exactly<_From>()))>> : true_type {};

    template <class _From, class _To>
    struct invoke_nothrow_convertible : bool_constant<noexcept(_STD _Fake_copy_init<_To>(_STD _Returns_exactly<_From>()))> {};

    template <class _Result, bool _Nothrow>
    struct invoke_traits_common 
    {
        using type = _Result;
        using is_invocable = true_type;
        using is_nothrow_invocable = bool_constant<_Nothrow>;
        template <class _Rx>
        using is_invocable_r = bool_constant<disjunction_v<is_void<_Rx>, invoke_convertible<type, _Rx>>>;
        template <class _Rx>
        using is_nothrow_invocable_r = bool_constant<conjunction_v<is_nothrow_invocable, disjunction<is_void<_Rx>, conjunction<invoke_convertible<type, _Rx>, invoke_nothrow_convertible<type, _Rx>>>>>;
    };

    template <class _Void, class _Callable>
    struct invoke_traits_zero 
    {
        using is_invocable = false_type;
        using is_nothrow_invocable = false_type;
        template <class _Rx>
        using is_invocable_r = false_type;
        template <class _Rx>
        using is_nothrow_invocable_r = false_type;
    };

    template <class _Callable>
    using decltype_invoke_zero = decltype(_WHIP declval<_Callable>()());

    template <class _Callable>
    struct invoke_traits_zero<void_t<decltype_invoke_zero<_Callable>>, _Callable> 
        : invoke_traits_common<decltype_invoke_zero<_Callable>, noexcept(_WHIP declval<_Callable>()())> {};

    template <class _Void, class... _Types>
    struct invoke_traits_nonzero 
    {
        using is_invocable = false_type;
        using is_nothrow_invocable = false_type;
        template <class _Rx>
        using is_invocable_r = false_type;
        template <class _Rx>
        using is_nothrow_invocable_r = false_type;
    };

    template <class _Callable, class _Ty1, class... _Types2>
    using decltype_invoke_nonzero = decltype(invoker1<_Callable, _Ty1>::call(_WHIP declval<_Callable>(), _WHIP declval<_Ty1>(), _WHIP declval<_Types2>()...));

    template <class _Callable, class _Ty1, class... _Types2>
    struct invoke_traits_nonzero<void_t<decltype_invoke_nonzero<_Callable, _Ty1, _Types2...>>, _Callable, _Ty1, _Types2...> 
        : invoke_traits_common<decltype_invoke_nonzero<_Callable, _Ty1, _Types2...>, noexcept(invoker1<_Callable, _Ty1>::call(_WHIP declval<_Callable>(), _WHIP declval<_Ty1>(), _WHIP declval<_Types2>()...))> {};

    template <class _Callable, class... _Args>
    using select_invoke_traits = conditional_t<sizeof...(_Args) == 0, invoke_traits_zero<void, _Callable>, invoke_traits_nonzero<void, _Callable, _Args...>>;

    template <class _Callable, class... _Args>
    using invoke_result_t = typename select_invoke_traits<_Callable, _Args...>::type;

    template <class _Rx, class _Callable, class... _Args>
    using is_invocable_r_ = typename select_invoke_traits<_Callable, _Args...>::template is_invocable_r<_Rx>;

    template <class _Rx, class _Callable, class... _Args>
    struct is_invocable_r : is_invocable_r_<_Rx, _Callable, _Args...> {};

}

template <class _Callable, class... _Args>
struct invoke_result : detail_invoker::select_invoke_traits<_Callable, _Args...> {};

template <class _Callable, class... _Args>
using invoke_result_t = typename detail_invoker::select_invoke_traits<_Callable, _Args...>::type;

template <class _Callable, class... _Args>
struct is_invocable : detail_invoker::select_invoke_traits<_Callable, _Args...>::is_invocable {};

template <class _Callable, class... _Args>
WHP_INLINE constexpr bool is_invocable_v = detail_invoker::select_invoke_traits<_Callable, _Args...>::is_invocable::value;

template <class _Callable, class... _Args>
struct is_nothrow_invocable : detail_invoker::select_invoke_traits<_Callable, _Args...>::is_nothrow_invocable {};

template <class _Callable, class... _Args>
WHP_INLINE constexpr bool is_nothrow_invocable_v = detail_invoker::select_invoke_traits<_Callable, _Args...>::is_nothrow_invocable::value;

template <class _Rx, class _Callable, class... _Args>
struct is_invocable_r : detail_invoker::is_invocable_r_<_Rx, _Callable, _Args...> {};

template <class _Rx, class _Callable, class... _Args>
WHP_INLINE constexpr bool is_invocable_r_v = detail_invoker::is_invocable_r_<_Rx, _Callable, _Args...>::value;

template <class _Rx, class _Callable, class... _Args>
struct is_nothrow_invocable_r : detail_invoker::select_invoke_traits<_Callable, _Args...>::template is_nothrow_invocable_r<_Rx> {};

template <class _Rx, class _Callable, class... _Args>
WHP_INLINE constexpr bool is_nothrow_invocable_r_v = detail_invoker::select_invoke_traits<_Callable, _Args...>::template is_nothrow_invocable_r<_Rx>::value;

_WHIP_END

#pragma warning(pop)

#endif // !_WHIP_INVOKER_