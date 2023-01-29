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
	#define WHP_ASSERT(x, ...) { if(!(x)) { WHP_CLIENT_CRITICAL("Whip Assertion Failed: File -> ({0}) Line -> ({1}) Error Message -> {2}",__FILE__, __LINE__ ,__VA_ARGS__); __debugbreak(); } }
	#define WHP_CORE_ASSERT(x, ...) { if(!(x)) { WHP_CORE_CRITICAL("Whip Assertion Failed: File -> ({0}) Line -> ({1}) Error Message -> {2}",__FILE__, __LINE__ ,__VA_ARGS__); __debugbreak(); } }
#else //WHP_ENABLE_ASSERTS
	#define WHP_ASSERT(x, ...)				// Whip assert not enabled
	#define WHP_CORE_ASSERT(x, ...)			// Whip core assert not enabled
#endif //WHP_ENABLE_ASSERTS


_WHIP_START

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

_WHIP_END