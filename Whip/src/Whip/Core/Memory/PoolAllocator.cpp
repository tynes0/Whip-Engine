#include "WhipPch.h"
#include "Whip/Core/Memory/PoolAllocator.h"
#include "Whip/Core/Memory/SystemAllocator.h"

#include <algorithm>

namespace whip::memory
{
    PoolAllocator::PoolAllocator(Size BlockSize, Size BlockAlignment, Size BlockCount, Allocator* BackingAllocator, const char* Name)
    {
        Init(BlockSize, BlockAlignment, BlockCount, BackingAllocator, Name);
    }

    PoolAllocator::PoolAllocator(void* Memory, Size MemorySize, Size BlockSize, Size BlockAlignment, const char* Name)
    {
        Init(Memory, MemorySize, BlockSize, BlockAlignment, Name);
    }

    PoolAllocator::~PoolAllocator()
    {
        Shutdown();
    }

    PoolAllocator::PoolAllocator(PoolAllocator&& Other) noexcept
    {
        MoveFrom(std::move(Other));
    }

    PoolAllocator& PoolAllocator::operator=(PoolAllocator&& Other) noexcept
    {
        if (this != &Other)
        {
            Shutdown();
            MoveFrom(std::move(Other));
        }

        return *this;
    }

    void PoolAllocator::Init(Size BlockSize, Size BlockAlignment, Size BlockCount, Allocator* BackingAllocator, const char* Name)
    {
        WHIP_MEMORY_ASSERT(IsPowerOfTwo(BlockAlignment));
        Shutdown();

        m_BlockAlignment = BlockAlignment;
        m_BlockSize = AlignUp(std::max(BlockSize, sizeof(FreeNode)), BlockAlignment);
        m_BlockCount = BlockCount;
        m_MemorySize = m_BlockSize * m_BlockCount + BlockAlignment;
        m_BackingAllocator = BackingAllocator ? BackingAllocator : &SystemAllocator::Get();
        m_RawStart = m_BackingAllocator->Allocate(m_MemorySize, BlockAlignment, MemoryTag::Core, WHIP_MEMORY_LOCATION);
        m_Start = AlignForwardPointer(m_RawStart, BlockAlignment);
        m_OwnsMemory = true;
        m_Name = Name;
        m_Stats = {};
        m_Stats.TotalMemory = m_BlockSize * m_BlockCount;

        BuildFreeList();
    }

    void PoolAllocator::Init(void* Memory, Size MemorySize, Size BlockSize, Size BlockAlignment, const char* Name)
    {
        WHIP_MEMORY_ASSERT(IsPowerOfTwo(BlockAlignment));
        Shutdown();

        m_RawStart = Memory;
        m_BlockAlignment = BlockAlignment;
        m_BlockSize = AlignUp(std::max(BlockSize, sizeof(FreeNode)), BlockAlignment);
        m_Start = AlignForwardPointer(Memory, BlockAlignment);
        const Size Adjustment = static_cast<Size>(reinterpret_cast<std::uintptr_t>(m_Start) - reinterpret_cast<std::uintptr_t>(Memory));
        m_MemorySize = MemorySize > Adjustment ? MemorySize - Adjustment : 0;
        m_BlockCount = m_MemorySize / m_BlockSize;
        m_BackingAllocator = nullptr;
        m_OwnsMemory = false;
        m_Name = Name;
        m_Stats = {};
        m_Stats.TotalMemory = m_BlockSize * m_BlockCount;

        BuildFreeList();
    }

    void PoolAllocator::Shutdown()
    {
        if (m_OwnsMemory && m_BackingAllocator && m_RawStart)
            m_BackingAllocator->Deallocate(m_RawStart);

        m_RawStart = nullptr;
        m_Start = nullptr;
        m_MemorySize = 0;
        m_BlockSize = 0;
        m_BlockAlignment = 0;
        m_BlockCount = 0;
        m_FreeBlockCount = 0;
        m_FreeList = nullptr;
        m_BackingAllocator = nullptr;
        m_OwnsMemory = false;
        m_Stats = {};
    }

    void* PoolAllocator::AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location)
    {
        WHIP_MEMORY_UNUSED(Tag);
        WHIP_MEMORY_UNUSED(Location);
        WHIP_MEMORY_ASSERT(IsPowerOfTwo(Alignment));

        if (SizeInBytes == 0)
            return nullptr;

        if (SizeInBytes > m_BlockSize || Alignment > m_BlockAlignment || !m_FreeList)
        {
            ++m_Stats.FailedAllocations;
            return nullptr;
        }

        FreeNode* Node = m_FreeList;
        m_FreeList = Node->Next;
        --m_FreeBlockCount;

        m_Stats.UsedMemory += m_BlockSize;
        m_Stats.PeakMemory = std::max(m_Stats.PeakMemory, m_Stats.UsedMemory);
        ++m_Stats.AllocationCount;
        ++m_Stats.TotalAllocations;

        return Node;
    }

    void PoolAllocator::DeallocateImpl(void* Pointer)
    {
        if (!Pointer)
            return;

        auto* Node = static_cast<FreeNode*>(Pointer);
        Node->Next = m_FreeList;
        m_FreeList = Node;
        ++m_FreeBlockCount;

        m_Stats.UsedMemory -= m_BlockSize;
        --m_Stats.AllocationCount;
        ++m_Stats.TotalFrees;
    }

    void PoolAllocator::Reset()
    {
        BuildFreeList();
        m_Stats.UsedMemory = 0;
        m_Stats.AllocationCount = 0;
    }

    MemoryStats PoolAllocator::GetStats() const
    {
        return m_Stats;
    }

    const char* PoolAllocator::GetName() const
    {
        return m_Name;
    }

    Size PoolAllocator::GetBlockSize() const
    {
        return m_BlockSize;
    }

    Size PoolAllocator::GetBlockCount() const
    {
        return m_BlockCount;
    }

    Size PoolAllocator::GetFreeBlockCount() const
    {
        return m_FreeBlockCount;
    }

    bool PoolAllocator::Contains(const void* Pointer) const
    {
        if (!Pointer || !m_Start || m_BlockSize == 0 || m_BlockCount == 0)
            return false;

        const auto Address = reinterpret_cast<std::uintptr_t>(Pointer);
        const auto Start = reinterpret_cast<std::uintptr_t>(m_Start);
        const auto End = Start + m_BlockSize * m_BlockCount;
        if (Address < Start || Address >= End)
            return false;

        return ((Address - Start) % m_BlockSize) == 0;
    }

    void PoolAllocator::BuildFreeList()
    {
        m_FreeList = nullptr;
        m_FreeBlockCount = m_BlockCount;

        if (!m_Start || m_BlockCount == 0)
            return;

        Byte* Current = static_cast<Byte*>(m_Start);
        for (Size Index = 0; Index < m_BlockCount; ++Index)
        {
            auto* Node = reinterpret_cast<FreeNode*>(Current + Index * m_BlockSize);
            Node->Next = m_FreeList;
            m_FreeList = Node;
        }
    }

    void PoolAllocator::MoveFrom(PoolAllocator&& Other) noexcept
    {
        m_RawStart = Other.m_RawStart;
        m_Start = Other.m_Start;
        m_MemorySize = Other.m_MemorySize;
        m_BlockSize = Other.m_BlockSize;
        m_BlockAlignment = Other.m_BlockAlignment;
        m_BlockCount = Other.m_BlockCount;
        m_FreeBlockCount = Other.m_FreeBlockCount;
        m_FreeList = Other.m_FreeList;
        m_BackingAllocator = Other.m_BackingAllocator;
        m_OwnsMemory = Other.m_OwnsMemory;
        m_Name = Other.m_Name;
        m_Stats = Other.m_Stats;

        Other.m_RawStart = nullptr;
        Other.m_Start = nullptr;
        Other.m_MemorySize = 0;
        Other.m_BlockSize = 0;
        Other.m_BlockAlignment = 0;
        Other.m_BlockCount = 0;
        Other.m_FreeBlockCount = 0;
        Other.m_FreeList = nullptr;
        Other.m_BackingAllocator = nullptr;
        Other.m_OwnsMemory = false;
        Other.m_Stats = {};
    }
}
