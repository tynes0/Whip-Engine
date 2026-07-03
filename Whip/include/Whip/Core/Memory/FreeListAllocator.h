#pragma once

#include "Allocator.h"
#include "MemoryUtils.h"

namespace whip::memory
{
    /**
     * @brief Allocation search strategy used by FreeListAllocator.
     *
     * FirstFit chooses the first free block large enough for the request. BestFit
     * scans for the smallest suitable block to reduce leftover fragments at the cost
     * of a potentially longer search.
     */
    enum class FreeListPlacementPolicy
    {
        FirstFit,
        BestFit
    };

    /**
     * @brief Snapshot of free-list health and fragmentation.
     *
     * ExternalFragmentation is reported in the range [0, 1]. A value of 0 means all
     * free memory is contained in one block. Higher values mean free memory is split
     * into smaller pieces, making large allocations harder to satisfy even when total
     * free memory is high.
     */
    struct FreeListDiagnostics
    {
        Size FreeMemory = 0;
        Size LargestFreeBlock = 0;
        Size FreeBlockCount = 0;
        double ExternalFragmentation = 0.0;
    };

    /**
     * @brief Variable-size allocator backed by a coalescing free-list.
     *
     * FreeListAllocator manages a contiguous memory region and services arbitrary
     * sized aligned allocations by splitting and merging free blocks. Unlike linear,
     * stack, and pool allocators, it supports general allocation/deallocation order
     * and can be used for longer-lived dynamic memory.
     *
     * This allocator is useful for persistent engine memory, asset runtime data,
     * editor data structures, and systems where allocation sizes vary but memory
     * should still be contained inside a known arena.
     *
     * @note Free-list allocators are more flexible than specialized allocators but
     * also more complex and can fragment over time. Use LinearAllocator or
     * PoolAllocator when the lifetime pattern allows it.
     */
    class FreeListAllocator final : public Allocator
    {
    public:
        FreeListAllocator() = default;
        FreeListAllocator(Size Capacity, Allocator* BackingAllocator = nullptr, FreeListPlacementPolicy Policy = FreeListPlacementPolicy::FirstFit, const char* Name = "FreeListAllocator");
        FreeListAllocator(void* Memory, Size Capacity, FreeListPlacementPolicy Policy = FreeListPlacementPolicy::FirstFit, const char* Name = "FreeListAllocator");
        ~FreeListAllocator() override;

        FreeListAllocator(const FreeListAllocator&) = delete;
        FreeListAllocator& operator=(const FreeListAllocator&) = delete;

        FreeListAllocator(FreeListAllocator&& Other) noexcept;
        FreeListAllocator& operator=(FreeListAllocator&& Other) noexcept;

        void Init(Size Capacity, Allocator* BackingAllocator = nullptr, FreeListPlacementPolicy Policy = FreeListPlacementPolicy::FirstFit, const char* Name = "FreeListAllocator");
        void Init(void* Memory, Size Capacity, FreeListPlacementPolicy Policy = FreeListPlacementPolicy::FirstFit, const char* Name = "FreeListAllocator");
        void Shutdown();

        void Reset() override;

        [[nodiscard]] MemoryStats GetStats() const override;
        [[nodiscard]] const char* GetName() const override;
        [[nodiscard]] Size GetFreeMemory() const;
        [[nodiscard]] Size GetLargestFreeBlock() const;
        [[nodiscard]] Size GetFreeBlockCount() const;
        [[nodiscard]] double GetExternalFragmentation() const;
        [[nodiscard]] FreeListDiagnostics GetDiagnostics() const;

    protected:
        [[nodiscard]] void* AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location) override;
        void DeallocateImpl(void* Pointer) override;

    private:
        struct FreeBlock
        {
            Size SizeInBytes = 0;
            FreeBlock* Next = nullptr;
        };

        struct AllocationHeader
        {
            Size SizeInBytes = 0;
            Size Adjustment = 0;
        };

        FreeBlock* FindBlock(Size SizeInBytes, Size Alignment, FreeBlock*& PreviousBlock, Size& Adjustment, Size& TotalSize);
        void InsertFreeBlock(FreeBlock* Block);
        void Coalesce(FreeBlock* PreviousBlock, FreeBlock* Block);
        void MoveFrom(FreeListAllocator&& Other) noexcept;

    private:
        void* m_Start = nullptr;
        Size m_Capacity = 0;
        FreeBlock* m_FreeBlocks = nullptr;
        Allocator* m_BackingAllocator = nullptr;
        bool m_OwnsMemory = false;
        FreeListPlacementPolicy m_Policy = FreeListPlacementPolicy::FirstFit;
        const char* m_Name = "FreeListAllocator";
        MemoryStats m_Stats{};
    };
}
