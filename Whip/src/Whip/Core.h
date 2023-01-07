#pragma once

#define _WHIP_START namespace Whip {
#define _WHIP_END }

#define _WHIP ::Whip::

#ifdef WH_PLATFORM_WINDOWS
	#ifdef WH_BUILD_DLL
		#define WHIP_API __declspec(dllexport)
	#else // WH_BUILD_DLL
		#define WHIP_API __declspec(dllimport)
	#endif // WH_BUILD_DLL
#else // WH_PLATFORM_DLL
#error Whip engine only support Windows!
#endif // WH_PLATFORM_DLL