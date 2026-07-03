#pragma once

#include "Types.h"

namespace whip::memory
{
    struct MemoryStats
    {
        Size TotalMemory = 0;
        Size UsedMemory = 0;
        Size PeakMemory = 0;
        Size AllocationCount = 0;
        Size TotalAllocations = 0;
        Size TotalFrees = 0;
        Size FailedAllocations = 0;
    };
}
