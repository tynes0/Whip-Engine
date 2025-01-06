#pragma once

#include <version>

#define _WHIP_START namespace whip {		// Whip namespace start
#define _WHIP_END }							// Whip namespace end

#define _WHIP ::whip::						// Whip namespace

#define WHP_BIT(x) (1 << x)					// Whip bit converter

#define DREF(x) *(x)						// Dereferance pointer

#define WHP_UNUSED(x) ((void)x)

#define _WHP_STRINGIZE_(x) #x
#define WHP_STRINGIZE(x) _WHP_STRINGIZE_(x)

#define WHP_EXPAND(x) x

#define _WHP_CONCATENATE_(a, b) a ## b
#define WHP_CONCATENATE(a, b)  _WHP_CONCATENATE_(a, b)

#define _WHP_HAS_CPP_VERSION(x) WHP_CONCATENATE(_HAS_CXX, x)

#define _WHP_TEST_CPP_FT(x) WHP_CONCATENATE(__cpp_, x)
	
#if defined(_MSC_VER)
	#if defined(__CUDACC__) || defined(__INTEL_COMPILER)
		#define _WHP_PRAGMA(PRAGMA) __pragma(PRAGMA)
	#else // defined(__CUDACC__) || defined(__INTEL_COMPILER)
		#define _WHP_PRAGMA(PRAGMA) _Pragma(#PRAGMA)
	#endif
#else
	#define _WHP_PRAGMA(PRAGMA)
#endif

#define _WHP_PRAGMA_MESSAGE(MESSAGE) _WHP_PRAGMA(message(MESSAGE))
#define _WHP_PRAGMA_WARNING(MESSAGE) _WHP_PRAGMA(warning(MESSAGE))
#define _WHP_PRAGMA_WARNING_DISABLE(warning_code) _WHP_PRAGMA(warning(disable : warning_code))

#ifndef __has_cpp_attribute
	#define _WHP_HAS_CPP_ATTRIBUTE(x) 0
#elif defined(__CUDACC__)
	#define _WHP_HAS_CPP_ATTRIBUTE(x) 0
#else
	#define _WHP_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#endif

#if _WHP_HAS_CPP_ATTRIBUTE(nodiscard)
	#define WHP_NODISCARD [[nodiscard]]								// nodiscard attribute define
#else // WHP_HAS_CPP_ATTRIBUTE(nodiscard)
	#define WHP_NODISCARD 											// nodiscard attribute define
#endif // WHP_HAS_CPP_ATTRIBUTE(nodiscard)

#if _WHP_HAS_CPP_ATTRIBUTE(nodiscard) >= 201907L
	#define WHP_NODISCARD20 [[nodiscard]]
	#define WHP_NODISCARD_MSG(msg) [[nodiscard(msg)]]
#else // _WHP_HAS_CPP_ATTRIBUTE(nodiscard) >= 201907L
	#define WHP_NODISCARD20 
	#if _WHP_HAS_CPP_ATTRIBUTE(nodiscard)
		#define WHP_NODISCARD_MSG(msg) [[nodiscard]]				
	#else // _WHP_HAS_CPP_ATTRIBUTE(nodiscard)
		#define WHP_NODISCARD_MSG(msg) 
	#endif // _WHP_HAS_CPP_ATTRIBUTE(nodiscard)
#endif //_WHP_HAS_CPP_ATTRIBUTE(nodiscard) >= 201907L

#if _WHP_HAS_CPP_ATTRIBUTE(noreturn)
	#define WHP_NORETURN [[noreturn]]
#else // !_WHP_HAS_CPP_ATTRIBUTE(noreturn)
	#define WHP_NORETURN
#endif //_WHP_HAS_CPP_ATTRIBUTE(noreturn)

#if _WHP_HAS_CPP_VERSION(17)
	#define WHP_INLINE inline			// inline keyword for constants
#else // _HAS_CXX17
	#define WHP_INLINE					// inline keyword for constants
#endif // _HAS_CXX17

#if _MSC_VER
	#define WHP_FORCE_INLINE __forceinline
#elif __GNUC__ || __clang__
	#define WHP_FORCE_INLINE [[gnu::always_inline]]
#else
	#define WHP_FORCE_INLINE
#endif

#if _WHP_HAS_CPP_VERSION(17)
	#define WHP_CONSTEXPR17 constexpr
#else
	#define WHP_CONSTEXPR17
#endif

#if _WHP_HAS_CPP_VERSION(20)
	#define WHP_CONSTEXPR constexpr		// constexpr keyword for constants
#else
	#define WHP_CONSTEXPR		// constexpr keyword for constants
#endif // _WHP_HAS_CPP_VERSION(20)

#if _WHP_HAS_CPP_VERSION(23)
	#define WHP_CONSTEXPR23 constexpr
#else
	#define WHP_CONSTEXPR23
#endif

#pragma push_macro("msvc")
#pragma push_macro("known_semantics")
#pragma push_macro("noop_dtor")
#pragma push_macro("intrinsic")
#undef msvc
#undef known_semantics
#undef noop_dtor
#undef intrinsic

#define _WHP_HAS_MSVC_ATTRIBUTE(x) _WHP_HAS_CPP_ATTRIBUTE(msvc::x)

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

#if defined(WHP_PLATFORM_WINDOWS)
	#define WHP_DEBUGBREAK() __debugbreak()
#elif defined(WHP_PLATFORM_LINUX)
	#include <signal.h>
	#define WHP_DEBUGBREAK() raise(SIGTRAP)
#else
	#error "Platform doesn't support debugbreak yet!"
#endif

#ifdef WHP_DEBUG
	#define WHP_ENABLE_ASSERTS  // Whip asserts enabled
#endif

#ifndef WHP_DIST
	#define WHP_ENABLE_VERIFY 
#endif // WHP_DIST
