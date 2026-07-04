#include "WhipPch.h"

#include "Whip/Core/Memory/AllocatorRegistry.h"
#include "Whip/Core/Memory/SystemAllocator.h"
#include "Whip/Core/Memory/TrackingAllocator.h"

namespace whip::memory
{
    Allocator& GetDefaultAllocator()
    {
#if WHIP_MEMORY_DEBUG
        static TrackingAllocator Allocator(SystemAllocator::Get(), "WhipDefaultAllocator", TrackingMode::Full);
        return Allocator;
#else
        return SystemAllocator::Get();
#endif
    }

    Allocator& GetAllocator(MemoryTag Tag)
    {
        WHIP_MEMORY_UNUSED(Tag);
        return GetDefaultAllocator();
    }

    LinearAllocator& GetFrameAllocator()
    {
        static LinearAllocator Allocator(Megabytes(16), &GetDefaultAllocator(), "WhipFrameAllocator");
        return Allocator;
    }

    ArenaAllocator& GetScratchArenaAllocator()
    {
        static ArenaAllocator Allocator(Megabytes(8), &GetDefaultAllocator(), "WhipScratchArenaAllocator");
        return Allocator;
    }

    void ResetFrameAllocator()
    {
        GetFrameAllocator().Reset();
    }

    void ResetScratchArenaAllocator()
    {
        GetScratchArenaAllocator().Reset();
    }

    MemoryStats GetDefaultAllocatorStats()
    {
        return GetDefaultAllocator().GetStats();
    }
}
