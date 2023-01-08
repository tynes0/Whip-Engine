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

#define BIT(x) (1 << x)