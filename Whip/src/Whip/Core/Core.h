#pragma once

#include <version>

#define _WHIP_START namespace whip {		// Whip namespace start
#define _WHIP_END }							// Whip namespace end

#define _WHIP ::whip::						// Whip namespace

#define WHP_BIT(x) (1 << x)					// Whip bit converter

#define DREF(x) *(x)						// Dereferance pointer

#define WHP_BIND_EVENT_FN(FUN) std::bind(&FUN, this, std::placeholders::_1)					// Whip event functions binder

#ifdef _HAS_NODISCARD
	#define WHP_NODISCARD [[nodiscard]]							// nodiscard attribute define
	#define WHP_NODISCARD_MSG(msg) [[nodiscard(msg)]]			// nodiscard attribute define
#else // _HAS_NODISCARD
	#define WHP_NODISCARD 											// nodiscard attribute define
	#define WHP_NODISCARD_MSG(msg)									// nodiscard attribute define
#endif // _HAS_NODISCARD

#if _HAS_CXX17
#define WHP_INLINE inline			// inline keyword for constants
#else // _HAS_CXX17
#define WHP_INLINE					// inline keyword for constants
#endif // _HAS_CXX17

#if _HAS_CXX17
#define WHP_CONSTEXPR17 constexpr
#else
#define WHP_CONSTEXPR17 inline
#endif

#if _HAS_CXX20
#define WHP_CONSTEXPR constexpr		// constexpr keyword for constants
#else
#define WHP_CONSTEXPR inline		// constexpr keyword for constants
#endif // _HAS_CXX20

#if _HAS_CXX23
#define WHP_CONSTEXPR23 constexpr
#else
#define WHP_CONSTEXPR23 inline
#endif

#pragma push_macro("msvc")
#pragma push_macro("known_semantics")
#pragma push_macro("noop_dtor")
#pragma push_macro("intrinsic")
#undef msvc
#undef known_semantics
#undef noop_dtor
#undef intrinsic

#ifndef __has_cpp_attribute
#define _HAS_MSVC_ATTRIBUTE(x) 0
#elif defined(__CUDACC__)
#define _HAS_MSVC_ATTRIBUTE(x) 0
#else
#define _WHP_HAS_MSVC_ATTRIBUTE(x) __has_cpp_attribute(msvc::x)
#endif

#if _WHP_HAS_MSVC_ATTRIBUTE(known_semantics)
#define _WHP_MSVC_KNOWN_SEMANTICS [[msvc::known_semantics]]
#else
#define _WHP_MSVC_KNOWN_SEMANTICS
#endif

#if _WHP_HAS_MSVC_ATTRIBUTE(noop_dtor)
#define _WHP_MSVC_NOOP_DTOR [[msvc::noop_dtor]]
#else
#define _WHP_MSVC_NOOP_DTOR
#endif

#if _WHP_HAS_MSVC_ATTRIBUTE(intrinsic)
#define _WHP_MSVC_INTRINSIC [[msvc::intrinsic]]
#else
#define _WHP_MSVC_INTRINSIC
#endif

#undef _WHP_HAS_MSVC_ATTRIBUTE
#pragma pop_macro("intrinsic")
#pragma pop_macro("noop_dtor")
#pragma pop_macro("known_semantics")
#pragma pop_macro("msvc")

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
		#error "MacOs is not supported!"
	#else
		#error "Unknown Apple platform!"
	#endif 
#elif defined(__ANDROID__)
	#define WHP_PLATFORM_ANDROID 1
	#error "Android is not supported!"
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

typedef unsigned int renderer_id_t;

using ref_counter_t = unsigned long;

_WHIP_END