#include "WhipPch.h"
#include "Whip/Core/Memory/LinearAllocator.h"
#include "Whip/Core/Memory/SystemAllocator.h"

#include <algorithm>

namespace whip::memory
{
    LinearAllocator::LinearAllocator(Size Capacity, Allocator* BackingAllocator, const char* Name)
    {
        Init(Capacity, BackingAllocator, Name);
    }

    LinearAllocator::LinearAllocator(void* Memory, Size Capacity, const char* Name)
    {
        Init(Memory, Capacity, Name);
    }

    LinearAllocator::~LinearAllocator()
    {
        Shutdown();
    }

    LinearAllocator::LinearAllocator(LinearAllocator&& Other) noexcept
    {
        MoveFrom(std::move(Other));
    }

    LinearAllocator& LinearAllocator::operator=(LinearAllocator&& Other) noexcept
    {
        if (this != &Other)
        {
            Shutdown();
            MoveFrom(std::move(Other));
        }

        return *this;
    }

    void LinearAllocator::Init(Size Capacity, Allocator* BackingAllocator, const char* Name)
    {
        Shutdown();

        m_BackingAllocator = BackingAllocator ? BackingAllocator : &SystemAllocator::Get();
        m_Start = m_BackingAllocator->Allocate(Capacity, alignof(std::max_align_t), MemoryTag::Core, WHIP_MEMORY_LOCATION);
        m_Capacity = Capacity;
        m_Offset = 0;
        m_OwnsMemory = true;
        m_Name = Name;
        m_Stats = {};
        m_Stats.TotalMemory = Capacity;
    }

    void LinearAllocator::Init(void* Memory, Size Capacity, const char* Name)
    {
        Shutdown();

        m_Start = Memory;
        m_Capacity = Capacity;
        m_Offset = 0;
        m_BackingAllocator = nullptr;
        m_OwnsMemory = false;
        m_Name = Name;
        m_Stats = {};
        m_Stats.TotalMemory = Capacity;
    }

    void LinearAllocator::Shutdown()
    {
        if (m_OwnsMemory && m_BackingAllocator && m_Start)
            m_BackingAllocator->Deallocate(m_Start);

        m_Start = nullptr;
        m_Capacity = 0;
        m_Offset = 0;
        m_BackingAllocator = nullptr;
        m_OwnsMemory = false;
        m_Stats = {};
    }

    void* LinearAllocator::AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location)
    {
        WHIP_MEMORY_UNUSED(Tag);
        WHIP_MEMORY_UNUSED(Location);
        WHIP_MEMORY_ASSERT(m_Start != nullptr);
        WHIP_MEMORY_ASSERT(IsPowerOfTwo(Alignment));

        if (SizeInBytes == 0)
            return nullptr;

        auto CurrentAddress = reinterpret_cast<std::uintptr_t>(m_Start) + m_Offset;
        Size Adjustment = AlignForwardAdjustment(reinterpret_cast<void*>(CurrentAddress), Alignment);

        if (m_Offset + Adjustment + SizeInBytes > m_Capacity)
        {
            ++m_Stats.FailedAllocations;
            return nullptr;
        }

        auto AlignedAddress = CurrentAddress + Adjustment;
        m_Offset += Adjustment + SizeInBytes;

        m_Stats.UsedMemory = m_Offset;
        m_Stats.PeakMemory = std::max(m_Stats.PeakMemory, m_Stats.UsedMemory);
        ++m_Stats.AllocationCount;
        ++m_Stats.TotalAllocations;

        return reinterpret_cast<void*>(AlignedAddress);
    }

    void LinearAllocator::DeallocateImpl(void* Pointer)
    {
        WHIP_MEMORY_UNUSED(Pointer);
        // Linear allocators intentionally do not free individual allocations.
    }

    void LinearAllocator::Reset()
    {
        m_Offset = 0;
        m_Stats.UsedMemory = 0;
        m_Stats.AllocationCount = 0;
    }

    Size LinearAllocator::GetMarker() const
    {
        return m_Offset;
    }

    void LinearAllocator::RollbackTo(Size Marker)
    {
        WHIP_MEMORY_ASSERT(Marker <= m_Offset);
        m_Offset = Marker;
        m_Stats.UsedMemory = Marker;
    }

    MemoryStats LinearAllocator::GetStats() const
    {
        return m_Stats;
    }

    const char* LinearAllocator::GetName() const
    {
        return m_Name;
    }

    void* LinearAllocator::GetStart() const
    {
        return m_Start;
    }

    Size LinearAllocator::GetCapacity() const
    {
        return m_Capacity;
    }

    void LinearAllocator::MoveFrom(LinearAllocator&& Other) noexcept
    {
        m_Start = Other.m_Start;
        m_Capacity = Other.m_Capacity;
        m_Offset = Other.m_Offset;
        m_BackingAllocator = Other.m_BackingAllocator;
        m_OwnsMemory = Other.m_OwnsMemory;
        m_Name = Other.m_Name;
        m_Stats = Other.m_Stats;

        Other.m_Start = nullptr;
        Other.m_Capacity = 0;
        Other.m_Offset = 0;
        Other.m_BackingAllocator = nullptr;
        Other.m_OwnsMemory = false;
        Other.m_Stats = {};
    }

    LinearAllocatorMarker::LinearAllocatorMarker(LinearAllocator& Allocator)
        : m_Allocator(Allocator), m_Marker(Allocator.GetMarker())
    {
    }

    LinearAllocatorMarker::~LinearAllocatorMarker()
    {
        m_Allocator.RollbackTo(m_Marker);
    }
}
