#pragma once

#define _WHIP_START namespace Whip {
#define _WHIP_END }

#define _WHIP ::Whip::

#ifdef WHP_PLATFORM_WINDOWS
	#ifdef WHP_BUILD_DLL
		#define WHIP_API __declspec(dllexport)
	#else // WHP_BUILD_DLL
		#define WHIP_API __declspec(dllimport)
	#endif // WH_BUILD_DLL
#else // WHP_PLATFORM_DLL
#error Whip engine only support Windows!
#endif // WHP_PLATFORM_DLL


#ifdef WHP_ENABLE_ASSERTS
	#define WHP_ASSERT(x, ...) { if(!(x)) { WHP_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define WHP_CORE_ASSERT(x, ...) { if(!(x)) { WHP_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else //WHP_ENABLE_ASSERTS
	#define WHP_ASSERT(x, ...) 
	#define WHP_CORE_ASSERT(x, ...)
#endif //WHP_ENABLE_ASSERTS

#define BIT(x) (1 << x)