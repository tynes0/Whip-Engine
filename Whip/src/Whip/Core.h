#pragma once

#define _WHIP_START namespace Whip {		// Whip namespace start
#define _WHIP_END }							// Whip namespace end

#define _WHIP ::Whip::						// Whip namespace
#define WHP_LOG ::Whip::Log::				// Whip Log class

#define WHP_BIT(x) (1 << x)					// Whip bit converter

#define DREF(x) *(x)						// Dereferance pointer

#define WHP_BIND_EVENT_FN(FUN) std::bind(&FUN, this, std::placeholders::_1)					// Whip event functions binder

#define WHP_SWAP(type, param1, param2) type t = param1; param1 = param2; param2 = t			// Whip swap 2 element

#define WHP_NODISCARD [[nodiscard]]			// nodiscard attribute define
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
	#define WHP_ASSERT(x, ...) { if(!(x)) { WHP_CLIENT_CRITICAL("Whip Assertion Failed: File -> ({0}) Line -> ({1}) Error Message -> {2}",__FILE__, __LINE__ ,__VA_ARGS__); __debugbreak(); } }
	#define WHP_CORE_ASSERT(x, ...) { if(!(x)) { WHP_CORE_CRITICAL("Whip Assertion Failed: File -> ({0}) Line -> ({1}) Error Message -> {2}",__FILE__, __LINE__ ,__VA_ARGS__); __debugbreak(); } }
#else //WHP_ENABLE_ASSERTS
	#define WHP_ASSERT(x, ...)				// Whip assert not enabled
	#define WHP_CORE_ASSERT(x, ...)			// Whip core assert not enabled
#endif //WHP_ENABLE_ASSERTS

_WHIP_START

template <class _Ty, _Ty _Val>
struct integral_constant
{
	using type = _Ty;
	static constexpr type value = _Val;

	constexpr operator type() { return value; }
	constexpr type operator()() { return value; }
};

template <bool _Val>
using bool_constant = integral_constant<bool, _Val>;

template <class, class>
inline constexpr bool is_same_v = false;

template <class _Ty>
inline constexpr bool is_same_v<_Ty, _Ty> = true;

template <class _Ty1, class _Ty2>
struct is_same : bool_constant < is_same_v<_Ty1, _Ty2>>{};

typedef unsigned int renderer_id_t;

_WHIP_END