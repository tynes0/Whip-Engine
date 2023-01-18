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

#ifdef WHP_PLATFORM_WINDOWS
	#ifdef WHP_BUILD_DLL
		#define WHIP_API __declspec(dllexport)			// Whip api
	#else // WHP_BUILD_DLL
		#define WHIP_API __declspec(dllimport)			// Whip api
	#endif // WH_BUILD_DLL
#else // WHP_PLATFORM_DLL
#error Whip engine only support Windows! (for now)
#endif // WHP_PLATFORM_DLL

#ifdef WHP_DEBUG
	#define WHP_ENABLE_ASSERTS				// Whip asserts enabled
#endif // WHP_DEBUG


#ifdef WHP_ENABLE_ASSERTS
	#define WHP_ASSERT(x, ...) { if(!(x)) { WHP_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define WHP_CORE_ASSERT(x, ...) { if(!(x)) { WHP_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else //WHP_ENABLE_ASSERTS
	#define WHP_ASSERT(x, ...)				// Whip assert not enabled
	#define WHP_CORE_ASSERT(x, ...)			// Whip core assert not enabled
#endif //WHP_ENABLE_ASSERTS