#pragma once

#include <memory>


#define _WHIP_START namespace Whip {		// Whip namespace start
#define _WHIP_END }							// Whip namespace end

#define _WHIP ::Whip::						// Whip namespace

#define WHP_BIT(x) (1 << x)					// Whip bit converter

#define DREF(x) *(x)						// Dereferance pointer

#define WHP_BIND_EVENT_FN(FUN) std::bind(&FUN, this, std::placeholders::_1)					// Whip event functions binder

#define WHP_NODISCARD [[nodiscard]]							// nodiscard attribute define
#define WHP_NODISCARD_MSG(msg) [[nodiscard(msg)]]			// nodiscard attribute define

#ifdef WHP_PLATFORM_WINDOWS
	#ifdef WHP_DYNAMIC_LINK
		#ifdef WHP_BUILD_DLL
			#define WHIP_API __declspec(dllexport)			// Whip api
		#else // WHP_BUILD_DLL
			#define WHIP_API __declspec(dllimport)			// Whip api
		#endif // WH_BUILD_DLL
	#else
		#define WHIP_API
	#endif
#else // WHP_PLATFORM_DLL
	#error Whip engine only support Windows! (for now)
#endif // WHP_PLATFORM_DLL

#ifdef WHP_DEBUG
	#define WHP_ENABLE_ASSERTS				// Whip asserts enabled
#endif // WHP_DEBUG

#ifdef WHP_ENABLE_ASSERTS
	#define WHP_ASSERT(x, ...) { if(!(x)) { WHP_CLIENT_CRITICAL("Whip Assertion Failed: File -> ({0}) Line -> ({1}) Error Message -> {2}", __FILE__, __LINE__ ,__VA_ARGS__); __debugbreak(); } }
	#define WHP_CORE_ASSERT(x, ...) { if(!(x)) { WHP_CORE_CRITICAL("Whip Assertion Failed: File -> ({0}) Line -> ({1}) Error Message -> {2}", __FILE__, __LINE__ ,__VA_ARGS__); __debugbreak(); } }
#else //WHP_ENABLE_ASSERTS
	#define WHP_ASSERT(x, ...)				// Whip assert not enabled
	#define WHP_CORE_ASSERT(x, ...)			// Whip core assert not enabled
#endif //WHP_ENABLE_ASSERTS


_WHIP_START

template <class _Ty>
constexpr void swap(_Ty _Left, _Ty _Right)
{
	auto temp = _Left;
	_Left = _Right;
	_Right = temp;
}

template <class _Ty>
constexpr void callable_swap(_Ty _Left, _Ty _Right)
{
	swap<_Ty>(_Left, _Right);
}

typedef unsigned int renderer_id_t;

template <class _Ty>
using scope = std::unique_ptr<_Ty>;

template <class _Ty>
using ref = std::shared_ptr<_Ty>;

template <class _Ty, class... _Types>
ref<_Ty> make_scope(_Types&&... _Args) {
	return std::make_unique<_Ty, _Types...>(_Args...);
}

template <class _Ty, class... _Types>
ref<_Ty> make_ref(_Types&&... _Args) {
	return std::make_shared<_Ty, _Types...>(_Args...);
}

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

template<class _Ty>
WHP_NODISCARD constexpr _Ty* addressof(_Ty& _Val) noexcept {
	return __builtin_addressof(_Val);
}

#pragma warning(push)
#pragma warning(disable : 5053)

template <class _Ty1, class _Ty2>
struct pair
{
	using first_type = _Ty1;
	using second_type = _Ty2;
	first_type first;
	second_type second;

	template <class _Uty1 = _Ty1, class _Uty2 = _Ty2>
	constexpr explicit(!std::conjunction_v<std::_Is_implicitly_default_constructible<_Uty1>, std::_Is_implicitly_default_constructible<_Uty2>>)
		pair() noexcept : first(), second() {}

	template <class _Uty1 = _Ty1, class _Uty2 = _Ty2>
	constexpr explicit(!std::conjunction_v<std::is_convertible<const _Uty1&, _Uty1>, std::is_convertible<const _Uty2&, _Uty2>>)
		pair(const _Ty1& first_in, const _Ty2& second_in) noexcept : first(first_in), second(second_in) {}

	template <class _Other1, class _Other2>
	constexpr explicit(!std::conjunction_v<std::is_convertible<const _Other1&, _Ty1>, std::is_convertible<const _Other2&, _Ty2>>) 
		pair(const pair<_Other1, _Other2>& other) noexcept : first(other.first), second(other.second) {}

	pair(const pair&) = default;
	pair(pair&&) = default;

	pair& operator=(const volatile pair&) = delete;
	 
	constexpr pair& operator=(const pair& _Right)
	{
		first = _Right.first;
		second = _Right.second;
		return *this;
	}

	constexpr void swap(pair<_Ty1, _Ty2>& _Other)
	{
		if (this != addressof(_Other))
		{
			callable_swap(first, _Other.first);
			callable_swap(second, _Other.second);
		}
	}
};

#pragma warning(pop)

_WHIP_END