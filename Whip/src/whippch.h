#pragma once
#ifndef _WHIPPCH_
#define _WHIPPCH_

#include <Whip/Core/Core.h>

#ifdef WHP_PLATFORM_WINDOWS
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
#endif

#include <memory>
#include <iostream>
#include <utility>
#include <algorithm>
#include <functional>

#include <filesystem>

#include <string>
#include <string_view>
#include <sstream>
#include <array>
#include <vector>
#include <bitset>
#include <unordered_map>
#include <unordered_set>

#include <Whip/Core/Log.h>
#include <Whip/Core/buffer.h>
#include <Whip/Debug/Instrumentor.h>

#ifdef WHP_PLATFORM_WINDOWS
	#include <Windows.h>
#endif // WHP_PLATFORM_WINDOWS


#endif // !_WHIPPCH_
