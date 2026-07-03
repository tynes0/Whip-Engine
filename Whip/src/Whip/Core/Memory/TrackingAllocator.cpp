#include "WhipPch.h"
#include "Whip/Core/Memory/TrackingAllocator.h"
#include "Whip/Core/Memory/Defines.h"
#include "Whip/Core/Memory/MemoryTag.h"

#include <algorithm>

namespace whip::memory
{
    TrackingAllocator::TrackingAllocator(Allocator& BackingAllocator, const char* Name, TrackingMode Mode)
        : m_BackingAllocator(&BackingAllocator), m_Name(Name), m_Mode(Mode)
    {
    }

    TrackingAllocator::TrackingAllocator(Allocator& BackingAllocator, TrackingMode Mode, const char* Name)
        : TrackingAllocator(BackingAllocator, Name, Mode)
    {
    }

    TrackingAllocator::~TrackingAllocator() = default;

    void* TrackingAllocator::AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location)
    {
        void* Pointer = m_BackingAllocator->Allocate(SizeInBytes, Alignment, Tag, Location);
        if (!Pointer)
        {
            std::lock_guard<std::mutex> Lock(m_Mutex);
            ++m_Stats.FailedAllocations;
            RecordFailedAllocation(Tag);
            return nullptr;
        }

        std::lock_guard<std::mutex> Lock(m_Mutex);

        if (m_Mode == TrackingMode::Full)
        {
            m_Records.emplace(Pointer, AllocationRecord{ SizeInBytes, Alignment, Tag, Location, m_NextAllocationIndex++ });
            m_Stats.UsedMemory += SizeInBytes;
            m_Stats.PeakMemory = std::max(m_Stats.PeakMemory, m_Stats.UsedMemory);
            ++m_Stats.AllocationCount;
        }

        ++m_Stats.TotalAllocations;
        RecordAllocation(Tag, SizeInBytes);
        return Pointer;
    }

    void TrackingAllocator::DeallocateImpl(void* Pointer)
    {
        if (!Pointer)
            return;

        if (m_Mode == TrackingMode::StatsOnly)
        {
            {
                std::scoped_lock Lock(m_Mutex);
                ++m_Stats.TotalFrees;
                RecordDeallocation(MemoryTag::Unknown, 0);
            }

            m_BackingAllocator->Deallocate(Pointer);
            return;
        }

        MemoryTag Tag;
        {
            std::scoped_lock Lock(m_Mutex);
            auto Iterator = m_Records.find(Pointer);
            WHIP_MEMORY_ASSERT(Iterator != m_Records.end() && "Trying to free pointer that is not tracked by TrackingAllocator");

            if (Iterator != m_Records.end())
            {
                Size SizeInBytes = Iterator->second.SizeInBytes;
                Tag = Iterator->second.Tag;
                m_Records.erase(Iterator);

                WHIP_MEMORY_ASSERT(m_Stats.UsedMemory >= SizeInBytes);
                m_Stats.UsedMemory -= SizeInBytes;
                --m_Stats.AllocationCount;
                ++m_Stats.TotalFrees;
                RecordDeallocation(Tag, SizeInBytes);
            }
        }

        m_BackingAllocator->Deallocate(Pointer);
    }

    void TrackingAllocator::Reset()
    {
        std::scoped_lock Lock(m_Mutex);
        m_Records.clear();
        m_TagStats = {};
        m_BackingAllocator->Reset();
        m_Stats.UsedMemory = 0;
        m_Stats.AllocationCount = 0;
    }

    MemoryStats TrackingAllocator::GetStats() const
    {
        std::lock_guard<std::mutex> Lock(m_Mutex);
        MemoryStats Stats = m_Stats;
        Stats.TotalMemory = m_BackingAllocator->GetStats().TotalMemory;
        return Stats;
    }

    const char* TrackingAllocator::GetName() const
    {
        return m_Name;
    }

    TrackingMode TrackingAllocator::GetMode() const
    {
        return m_Mode;
    }

    Size TrackingAllocator::GetLiveAllocationCount() const
    {
        std::lock_guard<std::mutex> Lock(m_Mutex);
        if (m_Mode == TrackingMode::StatsOnly)
            return 0;

        return m_Records.size();
    }

    Size TrackingAllocator::GetTotalTrackedBytes() const
    {
        std::lock_guard<std::mutex> Lock(m_Mutex);
        Size Total = 0;
        for (const TrackingTagStats& Stats : m_TagStats)
            Total += Stats.AllocatedBytes;
        return Total;
    }

    TrackingTagStats TrackingAllocator::GetTagStats(MemoryTag Tag) const
    {
        std::lock_guard<std::mutex> Lock(m_Mutex);
        return m_TagStats[TagIndex(Tag)];
    }

    bool TrackingAllocator::HasLeaks() const
    {
        if (m_Mode == TrackingMode::StatsOnly)
            return false;

        return GetLiveAllocationCount() > 0;
    }

    void TrackingAllocator::DumpLeaks(std::ostream& Output) const
    {
        std::lock_guard<std::mutex> Lock(m_Mutex);

        if (m_Mode == TrackingMode::StatsOnly)
        {
            Output << "[WhipMemory] Leak dump is unavailable for " << m_Name << " because it is running in StatsOnly mode\n";
            return;
        }

        if (m_Records.empty())
        {
            Output << "[WhipMemory] No leaks in " << m_Name << "\n";
            return;
        }

        Output << "[WhipMemory] Leaks in " << m_Name << ": " << m_Records.size() << " allocation(s)\n";
        for (const auto& [Pointer, Record] : m_Records)
        {
            Output << "  #" << Record.AllocationIndex
                   << " ptr=" << Pointer
                   << " size=" << Record.SizeInBytes
                   << " alignment=" << Record.Alignment
                   << " tag=" << ToString(Record.Tag)
                   << " location=" << Record.Location.File << ":" << Record.Location.Line
                   << " function=" << Record.Location.Function
                   << "\n";
        }
    }

    bool TrackingAllocator::Contains(void* Pointer) const
    {
        std::lock_guard<std::mutex> Lock(m_Mutex);
        if (m_Mode == TrackingMode::StatsOnly)
            return false;

        return m_Records.find(Pointer) != m_Records.end();
    }

    Size TrackingAllocator::TagIndex(MemoryTag Tag)
    {
        const Size Index = static_cast<Size>(Tag);
        return Index < TagSlotCount ? Index : 0;
    }

    void TrackingAllocator::RecordAllocation(MemoryTag Tag, Size SizeInBytes)
    {
        TrackingTagStats& Stats = m_TagStats[TagIndex(Tag)];
        ++Stats.AllocationCount;
        Stats.AllocatedBytes += SizeInBytes;
    }

    void TrackingAllocator::RecordFailedAllocation(MemoryTag Tag)
    {
        ++m_TagStats[TagIndex(Tag)].FailedAllocationCount;
    }

    void TrackingAllocator::RecordDeallocation(MemoryTag Tag, Size SizeInBytes)
    {
        TrackingTagStats& Stats = m_TagStats[TagIndex(Tag)];
        ++Stats.DeallocationCount;
        Stats.FreedBytes += SizeInBytes;
    }
}
