#pragma once

#ifndef _WHIPPCH_
#define _WHIPPCH_

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <fstream>
#include <iomanip>

#include <string>
#include <sstream>
#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#if _HAS_CXX17
#include <filesystem>
#endif // _HAS_CXX17

#include <Whip/Core/Core.h>
#ifdef WHP_PLATFORM_WINDOWS
	#include <Windows.h>
#endif // WHP_PLATFORM_WINDOWS

#include <Whip/Core/Log.h>
#include <Whip/Debug/Instrumentor.h>

#endif // !_WHIPPCH_