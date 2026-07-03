#include "WhipPch.h"

#include "Whip/Core/Memory/Allocator.h"
#include "Whip/Core/Memory/DebugAllocator.h"
#include "Whip/Core/Memory/MemoryUtils.h"

#include <algorithm>


namespace whip::memory
{
    DebugAllocator::DebugAllocator(Allocator& BackingAllocator, const char* Name)
        : m_BackingAllocator(&BackingAllocator), m_Name(Name)
    {
    }

    void* DebugAllocator::AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location)
    {
        WHIP_MEMORY_ASSERT(IsPowerOfTwo(Alignment));

        if (SizeInBytes == 0)
            return nullptr;

        const Size TotalSize = SizeInBytes + Alignment + sizeof(DebugHeader) + GuardSize * 2;
        void* RawMemory = m_BackingAllocator->Allocate(TotalSize, alignof(std::max_align_t), Tag, Location);
        if (!RawMemory)
        {
            ++m_Stats.FailedAllocations;
            return nullptr;
        }

        auto RawAddress = reinterpret_cast<std::uintptr_t>(RawMemory);
        auto UserAddress = AlignForwardAddress(RawAddress + GuardSize + sizeof(DebugHeader), Alignment);
        auto* Header = reinterpret_cast<DebugHeader*>(UserAddress - sizeof(DebugHeader));
        Byte* FrontGuard = reinterpret_cast<Byte*>(Header) - GuardSize;
        Byte* UserMemory = reinterpret_cast<Byte*>(UserAddress);
        Byte* BackGuard = UserMemory + SizeInBytes;

        Header->RawPointer = RawMemory;
        Header->RequestedSize = SizeInBytes;
        Header->TotalSize = TotalSize;
        Header->Alignment = Alignment;
        Header->Tag = Tag;
        Header->Location = Location;

        FillBytes(FrontGuard, GuardSize, GuardValue);
        FillBytes(UserMemory, SizeInBytes, AllocatedValue);
        FillBytes(BackGuard, GuardSize, GuardValue);

        m_Stats.UsedMemory += SizeInBytes;
        m_Stats.PeakMemory = std::max(m_Stats.PeakMemory, m_Stats.UsedMemory);
        ++m_Stats.AllocationCount;
        ++m_Stats.TotalAllocations;

        return UserMemory;
    }

    void DebugAllocator::DeallocateImpl(void* Pointer)
    {
        if (!Pointer)
            return;

        WHIP_MEMORY_ASSERT(ValidateAllocation(Pointer) && "DebugAllocator guard bytes are corrupted");

        auto* Header = PointerSub<DebugHeader>(Pointer, sizeof(DebugHeader));
        FillBytes(Pointer, Header->RequestedSize, FreedValue);

        m_Stats.UsedMemory -= Header->RequestedSize;
        --m_Stats.AllocationCount;
        ++m_Stats.TotalFrees;

        m_BackingAllocator->Deallocate(Header->RawPointer);
    }

    void DebugAllocator::Reset()
    {
        m_BackingAllocator->Reset();
        m_Stats.UsedMemory = 0;
        m_Stats.AllocationCount = 0;
    }

    MemoryStats DebugAllocator::GetStats() const
    {
        MemoryStats Stats = m_Stats;
        Stats.TotalMemory = m_BackingAllocator->GetStats().TotalMemory;
        return Stats;
    }

    const char* DebugAllocator::GetName() const
    {
        return m_Name;
    }

    bool DebugAllocator::ValidateAllocation(void* Pointer) const
    {
        if (!Pointer)
            return false;

        auto* Header = PointerSub<DebugHeader>(Pointer, sizeof(DebugHeader));
        Byte* FrontGuard = reinterpret_cast<Byte*>(Header) - GuardSize;
        Byte* UserMemory = static_cast<Byte*>(Pointer);
        Byte* BackGuard = UserMemory + Header->RequestedSize;

        return CheckPattern(FrontGuard, GuardSize, GuardValue) &&
               CheckPattern(BackGuard, GuardSize, GuardValue);
    }
}
