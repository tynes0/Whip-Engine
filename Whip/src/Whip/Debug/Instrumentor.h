#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>

#ifdef _DEBUG
// #define WHP_PROFILE // closed manually
#endif

#ifndef WHP_PROFILE
#define WHP_PROFILE_BEGIN_SESSION(name, filepath)	
#define WHP_PROFILE_END_SESSION()					
#define WHP_PROFILE_SCOPE(name)						
#define WHP_PROFILE_FUNCTION()						

#else // WHP_PROFILE

#ifndef COCO_ASSERT
#define COCO_ASSERT(cond, message)					WHP_ASSERT(cond, message)
#endif // !COCO_ASSERT

#include "../vendor/coco/coco.h"

#define WHP_PROFILE_BEGIN_SESSION(name, filepath)	COCO_PROFILE_BEGIN_SESSION(name, filepath)
#define WHP_PROFILE_END_SESSION()					COCO_PROFILE_END_SESSION()
#define WHP_PROFILE_SCOPE(name)						COCO_PROFILE_SCOPE(name)
#define WHP_PROFILE_FUNCTION()						COCO_PROFILE_FUNCTION()

#endif // WHP_PROFILE
