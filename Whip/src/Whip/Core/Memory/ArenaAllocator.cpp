#include "WhipPch.h"
#include "Whip/Core/Memory/ArenaAllocator.h"
#include "Whip/Core/Memory/SystemAllocator.h"

#include <algorithm>
#include <new>

namespace whip::memory
{
    ArenaAllocator::ArenaAllocator(Size DefaultChunkSize, Allocator* BackingAllocator, const char* Name)
        : m_DefaultChunkSize(DefaultChunkSize),
          m_BackingAllocator(BackingAllocator ? BackingAllocator : &SystemAllocator::Get()),
          m_Name(Name)
    {
    }

    ArenaAllocator::~ArenaAllocator()
    {
        Release();
    }

    ArenaAllocator::ArenaAllocator(ArenaAllocator&& Other) noexcept
    {
        MoveFrom(std::move(Other));
    }

    ArenaAllocator& ArenaAllocator::operator=(ArenaAllocator&& Other) noexcept
    {
        if (this != &Other)
        {
            Release();
            MoveFrom(std::move(Other));
        }

        return *this;
    }

    void* ArenaAllocator::AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location)
    {
        WHIP_MEMORY_UNUSED(Location);
        WHIP_MEMORY_ASSERT(IsPowerOfTwo(Alignment));

        if (SizeInBytes == 0)
            return nullptr;

        if (m_Chunks.empty())
            AddChunk(SizeInBytes + Alignment);

        Chunk* CurrentChunk = &m_Chunks.back();
        auto CurrentAddress = reinterpret_cast<std::uintptr_t>(CurrentChunk->Memory) + CurrentChunk->Offset;
        Size Adjustment = AlignForwardAdjustment(reinterpret_cast<void*>(CurrentAddress), Alignment);

        if (CurrentChunk->Offset + Adjustment + SizeInBytes > CurrentChunk->Capacity)
        {
            CurrentChunk = &AddChunk(SizeInBytes + Alignment);
            CurrentAddress = reinterpret_cast<std::uintptr_t>(CurrentChunk->Memory) + CurrentChunk->Offset;
            Adjustment = AlignForwardAdjustment(reinterpret_cast<void*>(CurrentAddress), Alignment);
        }

        const auto AlignedAddress = CurrentAddress + Adjustment;
        CurrentChunk->Offset += Adjustment + SizeInBytes;

        m_Stats.UsedMemory += Adjustment + SizeInBytes;
        m_Stats.PeakMemory = std::max(m_Stats.PeakMemory, m_Stats.UsedMemory);
        ++m_Stats.AllocationCount;
        ++m_Stats.TotalAllocations;

        WHIP_MEMORY_UNUSED(Tag);
        return reinterpret_cast<void*>(AlignedAddress);
    }

    void ArenaAllocator::DeallocateImpl(void* Pointer)
    {
        WHIP_MEMORY_UNUSED(Pointer);
        // Arena allocator intentionally frees memory only through Reset or Release.
    }

    void ArenaAllocator::Reset()
    {
        for (Chunk& ChunkRef : m_Chunks)
            ChunkRef.Offset = 0;

        m_Stats.UsedMemory = 0;
        m_Stats.AllocationCount = 0;
    }

    void ArenaAllocator::Release()
    {
        if (m_BackingAllocator)
        {
            for (Chunk& ChunkRef : m_Chunks)
                m_BackingAllocator->Deallocate(ChunkRef.Memory);
        }

        m_Chunks.clear();
        m_Stats = {};
    }

    MemoryStats ArenaAllocator::GetStats() const
    {
        MemoryStats Stats = m_Stats;
        Stats.TotalMemory = 0;
        for (const Chunk& ChunkRef : m_Chunks)
            Stats.TotalMemory += ChunkRef.Capacity;
        return Stats;
    }

    const char* ArenaAllocator::GetName() const
    {
        return m_Name;
    }

    Size ArenaAllocator::GetChunkCount() const
    {
        return m_Chunks.size();
    }

    ArenaAllocator::Chunk& ArenaAllocator::AddChunk(Size MinimumSize)
    {
        const Size ChunkSize = std::max(m_DefaultChunkSize, AlignUp(MinimumSize, alignof(std::max_align_t)));
        void* Memory = m_BackingAllocator->Allocate(ChunkSize, alignof(std::max_align_t), MemoryTag::Core, WHIP_MEMORY_LOCATION);
        if (!Memory)
        {
            ++m_Stats.FailedAllocations;
            throw std::bad_alloc{};
        }

        m_Chunks.push_back(Chunk{ Memory, ChunkSize, 0 });
        m_Stats.TotalMemory += ChunkSize;
        return m_Chunks.back();
    }

    void ArenaAllocator::MoveFrom(ArenaAllocator&& Other) noexcept
    {
        m_DefaultChunkSize = Other.m_DefaultChunkSize;
        m_BackingAllocator = Other.m_BackingAllocator;
        m_Name = Other.m_Name;
        m_Chunks = std::move(Other.m_Chunks);
        m_Stats = Other.m_Stats;

        Other.m_DefaultChunkSize = 0;
        Other.m_BackingAllocator = nullptr;
        Other.m_Stats = {};
    }
}
