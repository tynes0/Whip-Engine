#pragma once

#include "Allocator.h"

#include <array>
#include <iosfwd>
#include <mutex>
#include <string>
#include <unordered_map>

namespace whip::memory
{
    /**
     * @brief Controls how much information TrackingAllocator records.
     *
     * Full mode records every live pointer in a hash map, enabling leak reports,
     * Contains(), exact live allocation count, and exact UsedMemory accounting.
     * StatsOnly mode avoids the live-pointer map and only records cumulative
     * allocation/deallocation counters. It is cheaper, but it cannot detect leaks
     * or know exact live memory because Deallocate() does not receive size/tag
     * information.
     */
    enum class TrackingMode
    {
        StatsOnly,
        Full
    };

    /**
     * @brief Cumulative tracking counters grouped by MemoryTag.
     *
     * In Full mode both allocation and deallocation counters are exact. In
     * StatsOnly mode allocation counters are exact, while deallocation counters are
     * accumulated under MemoryTag::Unknown because pointer metadata is not stored.
     */
    struct TrackingTagStats
    {
        Size AllocationCount = 0;
        Size DeallocationCount = 0;
        Size FailedAllocationCount = 0;
        Size AllocatedBytes = 0;
        Size FreedBytes = 0;
    };

    /**
     * @brief Metadata recorded for a live tracked allocation.
     *
     * AllocationRecord stores the size, alignment, memory tag, source location, and
     * monotonically increasing allocation index used by TrackingAllocator for leak
     * reports and allocation inspection.
     */
    struct AllocationRecord
    {
        Size SizeInBytes = 0;
        Size Alignment = 0;
        MemoryTag Tag = MemoryTag::Unknown;
        SourceLocation Location{};
        U64 AllocationIndex = 0;
    };

    /**
     * @brief Thread-safe allocator wrapper for allocation diagnostics.
     *
     * TrackingAllocator forwards allocation requests to a backing allocator and can
     * run in either Full or StatsOnly mode. Full mode records all live pointers with
     * source locations, enabling leak reports and pointer validation. StatsOnly mode
     * skips the live-pointer map and records lightweight cumulative counters.
     *
     * Use Full mode for debug/editor leak hunting. Use StatsOnly mode when you only
     * need low-overhead allocation totals by tag and do not need live-pointer
     * inspection.
     */
    class TrackingAllocator final : public Allocator
    {
    public:
        explicit TrackingAllocator(Allocator& BackingAllocator, const char* Name = "TrackingAllocator", TrackingMode Mode = TrackingMode::Full);
        TrackingAllocator(Allocator& BackingAllocator, TrackingMode Mode, const char* Name = "TrackingAllocator");
        ~TrackingAllocator() override;

        TrackingAllocator(const TrackingAllocator&) = delete;
        TrackingAllocator& operator=(const TrackingAllocator&) = delete;

        void Reset() override;

        [[nodiscard]] MemoryStats GetStats() const override;
        [[nodiscard]] const char* GetName() const override;
        [[nodiscard]] TrackingMode GetMode() const;
        [[nodiscard]] Size GetLiveAllocationCount() const;
        [[nodiscard]] Size GetTotalTrackedBytes() const;
        [[nodiscard]] TrackingTagStats GetTagStats(MemoryTag Tag) const;
        [[nodiscard]] bool HasLeaks() const;

        void DumpLeaks(std::ostream& Output) const;
        bool Contains(void* Pointer) const;

    protected:
        [[nodiscard]] void* AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location) override;
        void DeallocateImpl(void* Pointer) override;

    private:
        static constexpr Size TagSlotCount = 11;

        [[nodiscard]] static Size TagIndex(MemoryTag Tag);
        void RecordAllocation(MemoryTag Tag, Size SizeInBytes);
        void RecordFailedAllocation(MemoryTag Tag);
        void RecordDeallocation(MemoryTag Tag, Size SizeInBytes);

    private:
        Allocator* m_BackingAllocator = nullptr;
        const char* m_Name = "TrackingAllocator";
        TrackingMode m_Mode = TrackingMode::Full;
        mutable std::mutex m_Mutex;
        std::unordered_map<void*, AllocationRecord> m_Records;
        std::array<TrackingTagStats, TagSlotCount> m_TagStats{};
        MemoryStats m_Stats{};
        U64 m_NextAllocationIndex = 1;
    };
}
