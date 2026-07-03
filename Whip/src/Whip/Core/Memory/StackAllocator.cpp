#include "WhipPch.h"
#include "Whip/Core/Memory/StackAllocator.h"
#include "Whip/Core/Memory/SystemAllocator.h"

#include <algorithm>

namespace whip::memory
{
    StackAllocator::StackAllocator(Size Capacity, Allocator* BackingAllocator, const char* Name)
    {
        Init(Capacity, BackingAllocator, Name);
    }

    StackAllocator::StackAllocator(void* Memory, Size Capacity, const char* Name)
    {
        Init(Memory, Capacity, Name);
    }

    StackAllocator::~StackAllocator()
    {
        Shutdown();
    }

    StackAllocator::StackAllocator(StackAllocator&& Other) noexcept
    {
        MoveFrom(std::move(Other));
    }

    StackAllocator& StackAllocator::operator=(StackAllocator&& Other) noexcept
    {
        if (this != &Other)
        {
            Shutdown();
            MoveFrom(std::move(Other));
        }

        return *this;
    }

    void StackAllocator::Init(Size Capacity, Allocator* BackingAllocator, const char* Name)
    {
        Shutdown();

        m_BackingAllocator = BackingAllocator ? BackingAllocator : &SystemAllocator::Get();
        m_Start = m_BackingAllocator->Allocate(Capacity, alignof(std::max_align_t), MemoryTag::Core, WHIP_MEMORY_LOCATION);
        m_Capacity = Capacity;
        m_Offset = 0;
        m_LastAllocationOffset = InvalidOffset;
        m_OwnsMemory = true;
        m_Name = Name;
        m_Stats = {};
        m_Stats.TotalMemory = Capacity;
    }

    void StackAllocator::Init(void* Memory, Size Capacity, const char* Name)
    {
        Shutdown();

        m_Start = Memory;
        m_Capacity = Capacity;
        m_Offset = 0;
        m_LastAllocationOffset = InvalidOffset;
        m_BackingAllocator = nullptr;
        m_OwnsMemory = false;
        m_Name = Name;
        m_Stats = {};
        m_Stats.TotalMemory = Capacity;
    }

    void StackAllocator::Shutdown()
    {
        if (m_OwnsMemory && m_BackingAllocator && m_Start)
            m_BackingAllocator->Deallocate(m_Start);

        m_Start = nullptr;
        m_Capacity = 0;
        m_Offset = 0;
        m_LastAllocationOffset = InvalidOffset;
        m_BackingAllocator = nullptr;
        m_OwnsMemory = false;
        m_Stats = {};
    }

    void* StackAllocator::AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location)
    {
        WHIP_MEMORY_UNUSED(Tag);
        WHIP_MEMORY_UNUSED(Location);
        WHIP_MEMORY_ASSERT(m_Start != nullptr);
        WHIP_MEMORY_ASSERT(IsPowerOfTwo(Alignment));

        if (SizeInBytes == 0)
            return nullptr;

        auto CurrentAddress = reinterpret_cast<std::uintptr_t>(m_Start) + m_Offset;
        Size Adjustment = AlignForwardAdjustmentWithHeader<AllocationHeader>(reinterpret_cast<void*>(CurrentAddress), Alignment);

        if (m_Offset + Adjustment + SizeInBytes > m_Capacity)
        {
            ++m_Stats.FailedAllocations;
            return nullptr;
        }

        auto AlignedAddress = CurrentAddress + Adjustment;
        auto* Header = reinterpret_cast<AllocationHeader*>(AlignedAddress - sizeof(AllocationHeader));
        Header->PreviousOffset = m_Offset;
        Header->PreviousAllocationOffset = m_LastAllocationOffset;

        m_Offset += Adjustment + SizeInBytes;
        m_LastAllocationOffset = static_cast<Size>(AlignedAddress - reinterpret_cast<std::uintptr_t>(m_Start));

        m_Stats.UsedMemory = m_Offset;
        m_Stats.PeakMemory = std::max(m_Stats.PeakMemory, m_Stats.UsedMemory);
        ++m_Stats.AllocationCount;
        ++m_Stats.TotalAllocations;

        return reinterpret_cast<void*>(AlignedAddress);
    }

    void StackAllocator::DeallocateImpl(void* Pointer)
    {
        if (!Pointer)
            return;

        const auto PointerOffset = static_cast<Size>(reinterpret_cast<std::uintptr_t>(Pointer) - reinterpret_cast<std::uintptr_t>(m_Start));
        WHIP_MEMORY_ASSERT(PointerOffset == m_LastAllocationOffset && "StackAllocator requires LIFO deallocation");
        WHIP_MEMORY_UNUSED(PointerOffset);

        auto* Header = PointerSub<AllocationHeader>(Pointer, sizeof(AllocationHeader));
        m_Offset = Header->PreviousOffset;
        m_LastAllocationOffset = Header->PreviousAllocationOffset;

        m_Stats.UsedMemory = m_Offset;
        --m_Stats.AllocationCount;
        ++m_Stats.TotalFrees;
    }

    void StackAllocator::Reset()
    {
        m_Offset = 0;
        m_LastAllocationOffset = InvalidOffset;
        m_Stats.UsedMemory = 0;
        m_Stats.AllocationCount = 0;
    }

    Size StackAllocator::GetMarker() const
    {
        return m_Offset;
    }

    void StackAllocator::RollbackTo(Size Marker)
    {
        WHIP_MEMORY_ASSERT(Marker <= m_Offset);
        m_Offset = Marker;
        m_LastAllocationOffset = InvalidOffset;
        m_Stats.UsedMemory = Marker;
    }

    MemoryStats StackAllocator::GetStats() const
    {
        return m_Stats;
    }

    const char* StackAllocator::GetName() const
    {
        return m_Name;
    }

    void StackAllocator::MoveFrom(StackAllocator&& Other) noexcept
    {
        m_Start = Other.m_Start;
        m_Capacity = Other.m_Capacity;
        m_Offset = Other.m_Offset;
        m_LastAllocationOffset = Other.m_LastAllocationOffset;
        m_BackingAllocator = Other.m_BackingAllocator;
        m_OwnsMemory = Other.m_OwnsMemory;
        m_Name = Other.m_Name;
        m_Stats = Other.m_Stats;

        Other.m_Start = nullptr;
        Other.m_Capacity = 0;
        Other.m_Offset = 0;
        Other.m_LastAllocationOffset = InvalidOffset;
        Other.m_BackingAllocator = nullptr;
        Other.m_OwnsMemory = false;
        Other.m_Stats = {};
    }
}
