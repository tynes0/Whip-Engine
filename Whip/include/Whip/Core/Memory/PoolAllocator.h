#pragma once

#include "Allocator.h"
#include "MemoryUtils.h"

namespace whip::memory
{
    /**
     * @brief Fixed-size block allocator with O(1) allocation and deallocation.
     *
     * PoolAllocator divides a memory region into equally sized blocks and links free
     * blocks through an intrusive free list. It is ideal for objects with uniform or
     * bounded size that are created and destroyed frequently.
     *
     * Typical use cases include particles, events, log entries, profiler samples,
     * physics contacts, ECS-like runtime records, and other high-churn engine data.
     * Requests larger than the configured block size fail rather than falling back to
     * another allocator.
     */
    class PoolAllocator final : public Allocator
    {
    public:
        PoolAllocator() = default;
        PoolAllocator(Size BlockSize, Size BlockAlignment, Size BlockCount, Allocator* BackingAllocator = nullptr, const char* Name = "PoolAllocator");
        PoolAllocator(void* Memory, Size MemorySize, Size BlockSize, Size BlockAlignment, const char* Name = "PoolAllocator");
        ~PoolAllocator() override;

        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        PoolAllocator(PoolAllocator&& Other) noexcept;
        PoolAllocator& operator=(PoolAllocator&& Other) noexcept;

        void Init(Size BlockSize, Size BlockAlignment, Size BlockCount, Allocator* BackingAllocator = nullptr, const char* Name = "PoolAllocator");
        void Init(void* Memory, Size MemorySize, Size BlockSize, Size BlockAlignment, const char* Name = "PoolAllocator");
        void Shutdown();

        void Reset() override;

        [[nodiscard]] MemoryStats GetStats() const override;
        [[nodiscard]] const char* GetName() const override;
        [[nodiscard]] Size GetBlockSize() const;
        [[nodiscard]] Size GetBlockCount() const;
        [[nodiscard]] Size GetFreeBlockCount() const;

    protected:
        [[nodiscard]] void* AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location) override;
        void DeallocateImpl(void* Pointer) override;

    private:
        struct FreeNode
        {
            FreeNode* Next = nullptr;
        };

        void BuildFreeList();
        void MoveFrom(PoolAllocator&& Other) noexcept;

    private:
        void* m_RawStart = nullptr;
        void* m_Start = nullptr;
        Size m_MemorySize = 0;
        Size m_BlockSize = 0;
        Size m_BlockAlignment = 0;
        Size m_BlockCount = 0;
        Size m_FreeBlockCount = 0;
        FreeNode* m_FreeList = nullptr;
        Allocator* m_BackingAllocator = nullptr;
        bool m_OwnsMemory = false;
        const char* m_Name = "PoolAllocator";
        MemoryStats m_Stats{};
    };
}
