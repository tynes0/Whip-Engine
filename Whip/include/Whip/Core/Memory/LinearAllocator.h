#pragma once

#include "Allocator.h"
#include "MemoryUtils.h"

namespace whip::memory
{
    /**
     * @brief Monotonic allocator for fast sequential allocations from a fixed buffer.
     *
     * LinearAllocator advances a single offset for each allocation and releases all
     * memory at once through Reset(), Shutdown(), or by rolling back to a marker.
     * Individual Deallocate() calls are intentionally ignored because this allocator
     * is designed for frame, scratch, and temporary workloads.
     *
     * Typical use cases include render command buffers, frame-local temporary data,
     * profiler events, serialization scratch memory, and short-lived asset import
     * buffers. Allocation is O(1), cache-friendly, and very cheap.
     *
     * @warning Objects allocated from this allocator must be destroyed manually before
     * Reset() if they have non-trivial destructors. The allocator only manages raw
     * memory lifetime.
     */
    class LinearAllocator final : public Allocator
    {
    public:
        LinearAllocator() = default;
        LinearAllocator(Size Capacity, Allocator* BackingAllocator = nullptr, const char* Name = "LinearAllocator");
        LinearAllocator(void* Memory, Size Capacity, const char* Name = "LinearAllocator");
        ~LinearAllocator() override;

        LinearAllocator(const LinearAllocator&) = delete;
        LinearAllocator& operator=(const LinearAllocator&) = delete;

        LinearAllocator(LinearAllocator&& Other) noexcept;
        LinearAllocator& operator=(LinearAllocator&& Other) noexcept;

        void Init(Size Capacity, Allocator* BackingAllocator = nullptr, const char* Name = "LinearAllocator");
        void Init(void* Memory, Size Capacity, const char* Name = "LinearAllocator");
        void Shutdown();

        void Reset() override;

        [[nodiscard]] Size GetMarker() const;
        void RollbackTo(Size Marker);

        [[nodiscard]] MemoryStats GetStats() const override;
        [[nodiscard]] const char* GetName() const override;
        [[nodiscard]] void* GetStart() const;
        [[nodiscard]] Size GetCapacity() const;

    protected:
        [[nodiscard]] void* AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location) override;
        void DeallocateImpl(void* Pointer) override;

    private:
        void MoveFrom(LinearAllocator&& Other) noexcept;

    private:
        void* m_Start = nullptr;
        Size m_Capacity = 0;
        Size m_Offset = 0;
        Allocator* m_BackingAllocator = nullptr;
        bool m_OwnsMemory = false;
        const char* m_Name = "LinearAllocator";
        MemoryStats m_Stats{};
    };

    /**
     * @brief RAII rollback helper for LinearAllocator marker scopes.
     *
     * LinearAllocatorMarker captures the allocator offset on construction and rolls
     * the allocator back to that marker on destruction. It is useful for nested
     * temporary allocation scopes where all allocations made inside the scope should
     * be discarded together.
     */
    class LinearAllocatorMarker final
    {
    public:
        explicit LinearAllocatorMarker(LinearAllocator& Allocator);
        ~LinearAllocatorMarker();

        LinearAllocatorMarker(const LinearAllocatorMarker&) = delete;
        LinearAllocatorMarker& operator=(const LinearAllocatorMarker&) = delete;

    private:
        LinearAllocator& m_Allocator;
        Size m_Marker = 0;
    };
}
