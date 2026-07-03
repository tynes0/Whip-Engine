#pragma once

#include "MemoryStats.h"
#include "MemoryTag.h"
#include "SourceLocation.h"

namespace whip::memory
{
    /**
     * @brief Abstract base interface for WhipMemory allocator implementations.
     *
     * Allocator defines the common runtime-polymorphic allocation contract used by
     * all concrete allocators in the library. It exposes a stable public API with
     * aligned allocations, memory tags, optional source locations, resettable
     * allocators, and lightweight statistics reporting.
     *
     * The class uses the non-virtual interface pattern: public Allocate() and
     * Deallocate() own default arguments and forward to protected virtual
     * AllocateImpl() and DeallocateImpl(). This avoids redefining default
     * arguments in derived allocators and prevents static-type-dependent default
     * argument behavior on virtual calls.
     *
     * Use this interface when an allocation strategy must be selected at runtime or
     * passed through engine systems without exposing the concrete allocator type.
     */
    class Allocator
    {
    public:
        virtual ~Allocator() = default;

        [[nodiscard]] void* Allocate(
            Size SizeInBytes,
            Size Alignment = alignof(std::max_align_t),
            MemoryTag Tag = MemoryTag::Unknown,
            const SourceLocation& Location = {})
        {
            return AllocateImpl(SizeInBytes, Alignment, Tag, Location);
        }

        void Deallocate(void* Pointer)
        {
            DeallocateImpl(Pointer);
        }

        virtual void Reset() {}

        [[nodiscard]] virtual MemoryStats GetStats() const = 0;
        [[nodiscard]] virtual const char* GetName() const = 0;

        [[nodiscard]] Size GetUsedMemory() const { return GetStats().UsedMemory; }
        [[nodiscard]] Size GetTotalMemory() const { return GetStats().TotalMemory; }
        [[nodiscard]] Size GetPeakMemory() const { return GetStats().PeakMemory; }
        [[nodiscard]] Size GetAllocationCount() const { return GetStats().AllocationCount; }

    protected:
        [[nodiscard]] virtual void* AllocateImpl(
            Size SizeInBytes,
            Size Alignment,
            MemoryTag Tag,
            SourceLocation Location) = 0;

        virtual void DeallocateImpl(void* Pointer) = 0;
    };
}
