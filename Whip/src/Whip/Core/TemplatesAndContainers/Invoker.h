#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/TemplatesAndContainers/TypeTraits.h>

#include <type_traits>

_WHIP_START

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

_WHIP_END