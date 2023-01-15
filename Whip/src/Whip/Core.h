#pragma once

#define _WHIP_START namespace Whip {
#define _WHIP_END }

#define _WHIP ::Whip::
#define WHP_LOG ::Whip::Log::

#define WHP_BIT(x) (1 << x)

#define WHP_BIND_EVENT_FN(FUN) std::bind(&FUN, this, std::placeholders::_1)

#define WHP_SWAP(type, param1, param2) type t = param1; param1 = param2; param2 = t

#ifdef WHP_PLATFORM_WINDOWS
	#ifdef WHP_BUILD_DLL
		#define WHIP_API __declspec(dllexport)
	#else // WHP_BUILD_DLL
		#define WHIP_API __declspec(dllimport)
	#endif // WH_BUILD_DLL
#else // WHP_PLATFORM_DLL
#error Whip engine only support Windows!
#endif // WHP_PLATFORM_DLL

#ifdef WHP_DEBUG
	#define WHP_ENABLE_ASSERTS
#endif // WHP_DEBUG


#ifdef WHP_ENABLE_ASSERTS
	#define WHP_ASSERT(x, ...) { if(!(x)) { WHP_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define WHP_CORE_ASSERT(x, ...) { if(!(x)) { WHP_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else //WHP_ENABLE_ASSERTS
	#define WHP_ASSERT(x, ...) 
	#define WHP_CORE_ASSERT(x, ...)
#endif //WHP_ENABLE_ASSERTS

