#include "WhipPch.h"
#include "Whip/Core/Memory/SystemAllocator.h"
#include "Whip/Core/Memory/MemoryUtils.h"

#include <cstdlib>

namespace whip::memory
{
    SystemAllocator::SystemAllocator(const char* Name)
        : m_Name(Name)
    {
    }

    void* SystemAllocator::AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location)
    {
        WHIP_MEMORY_UNUSED(Tag);
        WHIP_MEMORY_UNUSED(Location);
        WHIP_MEMORY_ASSERT(IsPowerOfTwo(Alignment));

        if (SizeInBytes == 0)
            return nullptr;

        const Size TotalSize = SizeInBytes + Alignment + sizeof(AllocationHeader);
        void* RawMemory = std::malloc(TotalSize);
        if (!RawMemory)
        {
            ++m_Stats.FailedAllocations;
            return nullptr;
        }

        auto RawAddress = reinterpret_cast<std::uintptr_t>(RawMemory);
        auto AlignedAddress = AlignForwardAddress(RawAddress + sizeof(AllocationHeader), Alignment);
        auto* Header = reinterpret_cast<AllocationHeader*>(AlignedAddress - sizeof(AllocationHeader));
        Header->RawPointer = RawMemory;
        Header->SizeInBytes = SizeInBytes;
        Header->Alignment = Alignment;

        m_Stats.UsedMemory += SizeInBytes;
        m_Stats.PeakMemory = std::max(m_Stats.PeakMemory, m_Stats.UsedMemory);
        ++m_Stats.AllocationCount;
        ++m_Stats.TotalAllocations;

        return reinterpret_cast<void*>(AlignedAddress);
    }

    void SystemAllocator::DeallocateImpl(void* Pointer)
    {
        if (!Pointer)
            return;

        auto* Header = PointerSub<AllocationHeader>(Pointer, sizeof(AllocationHeader));
        WHIP_MEMORY_ASSERT(Header->RawPointer != nullptr);

        m_Stats.UsedMemory -= Header->SizeInBytes;
        --m_Stats.AllocationCount;
        ++m_Stats.TotalFrees;

        std::free(Header->RawPointer);
    }

    MemoryStats SystemAllocator::GetStats() const
    {
        return m_Stats;
    }

    const char* SystemAllocator::GetName() const
    {
        return m_Name;
    }

    SystemAllocator& SystemAllocator::Get()
    {
        static SystemAllocator Instance;
        return Instance;
    }
}
