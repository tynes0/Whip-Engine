#pragma once

#ifndef WHIP_MEMORY_DEBUG
    #if defined(_DEBUG) || !defined(NDEBUG)
        #define WHIP_MEMORY_DEBUG 1
    #else
        #define WHIP_MEMORY_DEBUG 0
    #endif
#endif

#ifndef WHIP_MEMORY_ENABLE_ASSERTS
    #define WHIP_MEMORY_ENABLE_ASSERTS WHIP_MEMORY_DEBUG
#endif

#if WHIP_MEMORY_ENABLE_ASSERTS
    #include <Whip/Core/Log.h>
    #define WHIP_MEMORY_ASSERT(Expression) WHP_CORE_ASSERT(Expression)
#else
    #define WHIP_MEMORY_ASSERT(Expression) ((void)0)
#endif

#if defined(_MSC_VER)
    #define WHIP_MEMORY_FORCE_INLINE __forceinline
#else
    #define WHIP_MEMORY_FORCE_INLINE inline __attribute__((always_inline))
#endif

#define WHIP_MEMORY_UNUSED(Value) ((void)(Value))

#define WHIP_MEMORY_LOCATION ::whip::memory::SourceLocation{ __FILE__, static_cast<unsigned int>(__LINE__), __func__ }
