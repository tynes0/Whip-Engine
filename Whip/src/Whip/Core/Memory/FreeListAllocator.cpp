#include "WhipPch.h"
#include "Whip/Core/Memory/FreeListAllocator.h"
#include "Whip/Core/Memory/SystemAllocator.h"

#include <algorithm>

namespace whip::memory
{
    FreeListAllocator::FreeListAllocator(Size Capacity, Allocator* BackingAllocator, FreeListPlacementPolicy Policy, const char* Name)
    {
        Init(Capacity, BackingAllocator, Policy, Name);
    }

    FreeListAllocator::FreeListAllocator(void* Memory, Size Capacity, FreeListPlacementPolicy Policy, const char* Name)
    {
        Init(Memory, Capacity, Policy, Name);
    }

    FreeListAllocator::~FreeListAllocator()
    {
        Shutdown();
    }

    FreeListAllocator::FreeListAllocator(FreeListAllocator&& Other) noexcept
    {
        MoveFrom(std::move(Other));
    }

    FreeListAllocator& FreeListAllocator::operator=(FreeListAllocator&& Other) noexcept
    {
        if (this != &Other)
        {
            Shutdown();
            MoveFrom(std::move(Other));
        }

        return *this;
    }

    void FreeListAllocator::Init(Size Capacity, Allocator* BackingAllocator, FreeListPlacementPolicy Policy, const char* Name)
    {
        Shutdown();

        m_BackingAllocator = BackingAllocator ? BackingAllocator : &SystemAllocator::Get();
        m_Start = m_BackingAllocator->Allocate(Capacity, alignof(std::max_align_t), MemoryTag::Core, WHIP_MEMORY_LOCATION);
        m_Capacity = Capacity;
        m_OwnsMemory = true;
        m_Policy = Policy;
        m_Name = Name;
        m_Stats = {};
        m_Stats.TotalMemory = Capacity;
        Reset();
    }

    void FreeListAllocator::Init(void* Memory, Size Capacity, FreeListPlacementPolicy Policy, const char* Name)
    {
        Shutdown();

        m_Start = Memory;
        m_Capacity = Capacity;
        m_BackingAllocator = nullptr;
        m_OwnsMemory = false;
        m_Policy = Policy;
        m_Name = Name;
        m_Stats = {};
        m_Stats.TotalMemory = Capacity;
        Reset();
    }

    void FreeListAllocator::Shutdown()
    {
        if (m_OwnsMemory && m_BackingAllocator && m_Start)
            m_BackingAllocator->Deallocate(m_Start);

        m_Start = nullptr;
        m_Capacity = 0;
        m_FreeBlocks = nullptr;
        m_BackingAllocator = nullptr;
        m_OwnsMemory = false;
        m_Stats = {};
    }

    void* FreeListAllocator::AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location)
    {
        WHIP_MEMORY_UNUSED(Tag);
        WHIP_MEMORY_UNUSED(Location);
        WHIP_MEMORY_ASSERT(IsPowerOfTwo(Alignment));

        if (SizeInBytes == 0)
            return nullptr;

        FreeBlock* PreviousBlock = nullptr;
        Size Adjustment = 0;
        Size TotalSize = 0;
        FreeBlock* Block = FindBlock(SizeInBytes, Alignment, PreviousBlock, Adjustment, TotalSize);

        if (!Block)
        {
            ++m_Stats.FailedAllocations;
            return nullptr;
        }

        const Size OriginalBlockSize = Block->SizeInBytes;
        FreeBlock* OriginalNext = Block->Next;

        if (OriginalBlockSize - TotalSize <= sizeof(FreeBlock))
            TotalSize = OriginalBlockSize;

        const std::uintptr_t BlockAddress = reinterpret_cast<std::uintptr_t>(Block);
        const std::uintptr_t AlignedAddress = BlockAddress + Adjustment;

        const Size RemainingSize = OriginalBlockSize - TotalSize;
        if (RemainingSize > 0)
        {
            auto* NextBlock = reinterpret_cast<FreeBlock*>(BlockAddress + TotalSize);
            NextBlock->SizeInBytes = RemainingSize;
            NextBlock->Next = OriginalNext;

            if (PreviousBlock)
                PreviousBlock->Next = NextBlock;
            else
                m_FreeBlocks = NextBlock;
        }
        else
        {
            if (PreviousBlock)
                PreviousBlock->Next = OriginalNext;
            else
                m_FreeBlocks = OriginalNext;
        }

        auto* Header = reinterpret_cast<AllocationHeader*>(AlignedAddress - sizeof(AllocationHeader));
        Header->SizeInBytes = TotalSize;
        Header->Adjustment = Adjustment;

        m_Stats.UsedMemory += TotalSize;
        m_Stats.PeakMemory = std::max(m_Stats.PeakMemory, m_Stats.UsedMemory);
        ++m_Stats.AllocationCount;
        ++m_Stats.TotalAllocations;

        return reinterpret_cast<void*>(AlignedAddress);
    }

    void FreeListAllocator::DeallocateImpl(void* Pointer)
    {
        if (!Pointer)
            return;

        auto* Header = PointerSub<AllocationHeader>(Pointer, sizeof(AllocationHeader));
        const Size AllocatedSize = Header->SizeInBytes;
        auto* Block = reinterpret_cast<FreeBlock*>(reinterpret_cast<std::uintptr_t>(Pointer) - Header->Adjustment);
        Block->SizeInBytes = AllocatedSize;
        Block->Next = nullptr;

        InsertFreeBlock(Block);

        WHIP_MEMORY_ASSERT(m_Stats.UsedMemory >= AllocatedSize);
        m_Stats.UsedMemory -= AllocatedSize;
        --m_Stats.AllocationCount;
        ++m_Stats.TotalFrees;
    }

    void FreeListAllocator::Reset()
    {
        if (!m_Start || m_Capacity == 0)
            return;

        m_FreeBlocks = static_cast<FreeBlock*>(m_Start);
        m_FreeBlocks->SizeInBytes = m_Capacity;
        m_FreeBlocks->Next = nullptr;

        m_Stats.UsedMemory = 0;
        m_Stats.AllocationCount = 0;
    }

    MemoryStats FreeListAllocator::GetStats() const
    {
        return m_Stats;
    }

    const char* FreeListAllocator::GetName() const
    {
        return m_Name;
    }

    Size FreeListAllocator::GetFreeMemory() const
    {
        Size FreeMemory = 0;
        for (FreeBlock* Block = m_FreeBlocks; Block; Block = Block->Next)
            FreeMemory += Block->SizeInBytes;

        return FreeMemory;
    }

    Size FreeListAllocator::GetLargestFreeBlock() const
    {
        Size LargestBlock = 0;
        for (FreeBlock* Block = m_FreeBlocks; Block; Block = Block->Next)
            LargestBlock = std::max(LargestBlock, Block->SizeInBytes);

        return LargestBlock;
    }

    Size FreeListAllocator::GetFreeBlockCount() const
    {
        Size Count = 0;
        for (FreeBlock* Block = m_FreeBlocks; Block; Block = Block->Next)
            ++Count;

        return Count;
    }

    double FreeListAllocator::GetExternalFragmentation() const
    {
        const FreeListDiagnostics Diagnostics = GetDiagnostics();
        return Diagnostics.ExternalFragmentation;
    }

    FreeListDiagnostics FreeListAllocator::GetDiagnostics() const
    {
        FreeListDiagnostics Diagnostics{};

        for (FreeBlock* Block = m_FreeBlocks; Block; Block = Block->Next)
        {
            Diagnostics.FreeMemory += Block->SizeInBytes;
            Diagnostics.LargestFreeBlock = std::max(Diagnostics.LargestFreeBlock, Block->SizeInBytes);
            ++Diagnostics.FreeBlockCount;
        }

        if (Diagnostics.FreeMemory > 0)
        {
            Diagnostics.ExternalFragmentation =
                1.0 - (static_cast<double>(Diagnostics.LargestFreeBlock) / static_cast<double>(Diagnostics.FreeMemory));
        }

        return Diagnostics;
    }

    FreeListAllocator::FreeBlock* FreeListAllocator::FindBlock(Size SizeInBytes, Size Alignment, FreeBlock*& PreviousBlock, Size& Adjustment, Size& TotalSize)
    {
        FreeBlock* BestBlock = nullptr;
        FreeBlock* BestPrevious = nullptr;
        Size BestAdjustment = 0;
        Size BestTotalSize = 0;
        Size BestWaste = static_cast<Size>(-1);

        FreeBlock* Previous = nullptr;
        FreeBlock* Block = m_FreeBlocks;

        while (Block)
        {
            const Size CurrentAdjustment = AlignForwardAdjustmentWithHeader<AllocationHeader>(Block, Alignment);
            const Size CurrentTotalSize = SizeInBytes + CurrentAdjustment;

            if (Block->SizeInBytes >= CurrentTotalSize)
            {
                if (m_Policy == FreeListPlacementPolicy::FirstFit)
                {
                    PreviousBlock = Previous;
                    Adjustment = CurrentAdjustment;
                    TotalSize = CurrentTotalSize;
                    return Block;
                }

                const Size Waste = Block->SizeInBytes - CurrentTotalSize;
                if (Waste < BestWaste)
                {
                    BestWaste = Waste;
                    BestBlock = Block;
                    BestPrevious = Previous;
                    BestAdjustment = CurrentAdjustment;
                    BestTotalSize = CurrentTotalSize;
                }
            }

            Previous = Block;
            Block = Block->Next;
        }

        PreviousBlock = BestPrevious;
        Adjustment = BestAdjustment;
        TotalSize = BestTotalSize;
        return BestBlock;
    }

    void FreeListAllocator::InsertFreeBlock(FreeBlock* Block)
    {
        if (!m_FreeBlocks)
        {
            m_FreeBlocks = Block;
            return;
        }

        FreeBlock* Previous = nullptr;
        FreeBlock* Current = m_FreeBlocks;

        while (Current && Current < Block)
        {
            Previous = Current;
            Current = Current->Next;
        }

        Block->Next = Current;
        if (Previous)
            Previous->Next = Block;
        else
            m_FreeBlocks = Block;

        Coalesce(Previous, Block);
    }

    void FreeListAllocator::Coalesce(FreeBlock* PreviousBlock, FreeBlock* Block)
    {
        if (Block->Next && PointerAdd(Block, Block->SizeInBytes) == Block->Next)
        {
            Block->SizeInBytes += Block->Next->SizeInBytes;
            Block->Next = Block->Next->Next;
        }

        if (PreviousBlock && PointerAdd(PreviousBlock, PreviousBlock->SizeInBytes) == Block)
        {
            PreviousBlock->SizeInBytes += Block->SizeInBytes;
            PreviousBlock->Next = Block->Next;
        }
    }

    void FreeListAllocator::MoveFrom(FreeListAllocator&& Other) noexcept
    {
        m_Start = Other.m_Start;
        m_Capacity = Other.m_Capacity;
        m_FreeBlocks = Other.m_FreeBlocks;
        m_BackingAllocator = Other.m_BackingAllocator;
        m_OwnsMemory = Other.m_OwnsMemory;
        m_Policy = Other.m_Policy;
        m_Name = Other.m_Name;
        m_Stats = Other.m_Stats;

        Other.m_Start = nullptr;
        Other.m_Capacity = 0;
        Other.m_FreeBlocks = nullptr;
        Other.m_BackingAllocator = nullptr;
        Other.m_OwnsMemory = false;
        Other.m_Stats = {};
    }
}
