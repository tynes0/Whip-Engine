#pragma once

#include <memory>


#define _WHIP_START namespace whip {		// Whip namespace start
#define _WHIP_END }							// Whip namespace end

#define _WHIP ::whip::						// Whip namespace

#define WHP_BIT(x) (1 << x)					// Whip bit converter

#define DREF(x) *(x)						// Dereferance pointer

#define WHP_BIND_EVENT_FN(FUN) std::bind(&FUN, this, std::placeholders::_1)					// Whip event functions binder

#define WHP_NODISCARD [[nodiscard]]							// nodiscard attribute define
#define WHP_NODISCARD_MSG(msg) [[nodiscard(msg)]]			// nodiscard attribute define

#ifdef _WIN32
	#ifdef _WIN64
		#define WHP_PLATFORM_WINDOWS 1
	#else // _WIN64
		#error "x86 Builds are not supported"
	#endif // _WIN64
#elif defined(__APPLE__) || defined(__MACH__)
	#include <TargetConditionals.h>
	#if TARGET_IPHONE_SIMULATOR == 1
		#error "IOS simulator is not supported"
	#elif TARGET_OS_IPHONE == 1
		#define WHP_PLATFORM_IOS 1
		#error "IOS is not supported!"
	#elif TARGET_OS_MAC == 1
		#define WHP_PLATFORM_MACOS 1
		#error "MacOs is not supported! (for now)"
	#else
		#error "Unknown Apple platform!"
	#endif 
#elif defined(__ANDROID__)
	#define WHP_PLATFORM_ANDROID 1
	#error "Android is not supported! (for now)"
#elif defined(__linux__)
	#define WHP_PLATFORM_LINUX 1
	#error "Linux is not supported! (for now)"
#else
	#error "Unknown platform!"
#endif

#ifdef WHP_PLATFORM_WINDOWS
	#ifdef WHP_DYNAMIC_LINK
		#ifdef WHP_BUILD_DLL
			#define WHIP_API __declspec(dllexport)			// Whip api
		#else // WHP_BUILD_DLL
			#define WHIP_API __declspec(dllimport)			// Whip api
		#endif // WH_BUILD_DLL
	#else // WHP_DYNAMIC_LINK
		#define WHIP_API
	#endif // WHP_DYNAMIC_LINK
#else // WHP_PLATFORM_WINDOWS
	#error Whip engine only support Windows! (for now)
#endif // WHP_PLATFORM_WINDOWS

#ifdef WHP_DEBUG
	#define WHP_ENABLE_ASSERTS 1			// Whip asserts enabled
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

template<class _Ty>
WHP_NODISCARD constexpr _Ty* addressof(_Ty& _Val) noexcept 
{
	return __builtin_addressof(_Val);
}

WHP_NODISCARD constexpr float calculate_aspect_ratio(float x, float y)
{
	return (x / y);
}

typedef unsigned int renderer_id_t;

template <class _Ty>
using scope = std::unique_ptr<_Ty>;

template <class _Ty>
using ref = std::shared_ptr<_Ty>;

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

template <bool _Test, class _Ty = void>
struct enable_if {};

template <class _Ty>
struct enable_if<true, _Ty>
{
	using type = _Ty;
};

template <class>
inline constexpr bool is_array_v = false;

template <class _Ty, size_t n>
inline constexpr bool is_array_v<_Ty[n]> = true;

template <class _Ty>
inline constexpr bool is_array_v<_Ty[]> = true;

template <class _Ty>
struct is_array : bool_constant<is_array_v<_Ty>> {};

template <bool _Test, class _Ty = void>
using enable_if_t = typename enable_if<_Test, _Ty>::type;

template <class>
inline constexpr bool is_lvalue_reference_v = false;

template <class _Ty>
inline constexpr bool is_lvalue_reference_v<_Ty&> = true;

template <class _Ty>
struct is_lvalue_reference : bool_constant<is_lvalue_reference_v<_Ty>> {};

template <class>
inline constexpr bool is_rvalue_reference_v = false;

template <class _Ty>
inline constexpr bool is_rvalue_reference_v<_Ty&&> = true;

template <class _Ty>
struct is_rvalue_reference : bool_constant<is_rvalue_reference_v<_Ty>> {};

template <class>
inline constexpr bool is_reference_v = false;

template <class _Ty>
inline constexpr bool is_reference_v<_Ty&> = true;

template <class _Ty>
inline constexpr bool is_reference_v<_Ty&&> = true;

template <class _Ty>
struct is_reference : bool_constant<is_reference_v<_Ty>> {};

template <class _Ty, unsigned int _Ix = 0>
inline constexpr size_t extent_v = 0;

template <class _Ty, size_t _Nx>
inline constexpr size_t extent_v<_Ty[_Nx], 0> = _Nx;

template <class _Ty, unsigned int _Ix, size_t _Nx>
inline constexpr size_t extent_v<_Ty[_Nx], _Ix> = extent_v<_Ty, _Ix - 1>;

template <class _Ty, unsigned int _Ix>
inline constexpr size_t extent_v<_Ty[], _Ix> = extent_v<_Ty, _Ix - 1>;

template <class _Ty, unsigned int _Ix = 0>
struct extent : integral_constant<size_t, extent_v<_Ty, _Ix>> {};

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
WHP_NODISCARD constexpr _Ty&& forward(remove_reference_t<_Ty>& _Arg) noexcept 
{
	return static_cast<_Ty&&>(_Arg);
}

template <class _Ty>
WHP_NODISCARD constexpr _Ty&& forward(remove_reference_t<_Ty>&& _Arg) noexcept
{
	static_assert(!is_lvalue_reference_v<_Ty>, "bad forward call");
	return static_cast<_Ty&&>(_Arg);
}

template <class _Ty, class... _Types, enable_if_t<!is_array_v<_Ty>, int> = 0>
WHP_NODISCARD_MSG("scope returned as unnecessary") inline scope<_Ty> make_scope(_Types&&... _Args) 
{
	return scope<_Ty>(new _Ty(forward<_Types>(_Args)...));
}

template <class _Ty, enable_if_t<is_array_v<_Ty> && extent_v<_Ty> == 0, int> = 0>
WHP_NODISCARD_MSG("scope returned as unnecessary") inline scope<_Ty> make_scope(const size_t _Size) 
{
    using _Elem = remove_extent_t<_Ty>;
    return scope<_Ty>(new _Elem[_Size]());
}

template <class _Ty, class... _Types, enable_if_t<extent_v<_Ty> != 0, int> = 0>
void make_scope(_Types&&...) = delete;

template <class _Ty, class... _Types>
WHP_NODISCARD_MSG("ref returned as unnecessary") ref<_Ty> make_ref(_Types&&... _Args) 
{
	return std::make_shared<_Ty, _Types...>(_Args...);
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

template <class _Ty1, class _Ty2, class _Ty3>
struct trio
{
	using first_type = _Ty1;
	using second_type = _Ty2;
	using third_type = _Ty3;
	first_type first;
	second_type second;
	third_type third;

	template <class _Uty1 = _Ty1, class _Uty2 = _Ty2, class _Uty3 = _Ty3>
	constexpr explicit(!std::conjunction_v<std::_Is_implicitly_default_constructible<_Uty1>, std::_Is_implicitly_default_constructible<_Uty2>, std::_Is_implicitly_default_constructible<_Uty3>>)
		trio() noexcept : first(), second(), third() {}

	template <class _Uty1 = _Ty1, class _Uty2 = _Ty2, class _Uty3 = _Ty3>
	constexpr explicit(!std::conjunction_v<std::is_convertible<const _Uty1&, _Uty1>, std::is_convertible<const _Uty2&, _Uty2>, std::is_convertible<const _Uty3&, _Uty3>>)
		trio(const _Ty1& first_in, const _Ty2& second_in, const _Ty3& third_in) noexcept : first(first_in), second(second_in), third(third_in) {}

	template <class _Other1, class _Other2, class _Other3>
	constexpr explicit(!std::conjunction_v<std::is_convertible<const _Other1&, _Ty1>, std::is_convertible<const _Other2&, _Ty2>, std::is_convertible<const _Other3&, _Ty3>>)
		trio(const trio<_Other1, _Other2, _Other3>& other) noexcept : first(other.first), second(other.second), third(other.third) {}

	trio(const trio&) = default;
	trio(trio&&) = default;

	trio& operator=(const volatile trio&) = delete;

	constexpr trio& operator=(const trio& _Right)
	{
		first = _Right.first;
		second = _Right.second;
		third = _Right.third;
		return *this;
	}

	constexpr void swap(trio<_Ty1, _Ty2, _Ty3>& _Other)
	{
		if (this != addressof(_Other))
		{
			callable_swap(first, _Other.first);
			callable_swap(second, _Other.second);
			callable_swap(third, _Other.third);
		}
	}
};

#pragma warning(pop)

_WHIP_END