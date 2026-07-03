#pragma once

#include "Allocator.h"
#include "MemoryUtils.h"

namespace whip::memory
{
    /**
     * @brief LIFO allocator for nested allocation/free patterns.
     *
     * StackAllocator behaves like a linear allocator with individual deallocation
     * support, provided allocations are released in reverse order. Each allocation
     * stores a small header that allows the allocator to restore the previous offset
     * when Deallocate() is called for the most recent allocation.
     *
     * This allocator is well-suited for scoped temporary work, recursive systems,
     * staged asset processing, and short-lived intermediate buffers where allocation
     * lifetime naturally follows a stack discipline.
     *
     * @warning Deallocating pointers out of LIFO order is invalid and will trigger
     * debug assertions when enabled.
     */
    class StackAllocator final : public Allocator
    {
    public:
        StackAllocator() = default;
        StackAllocator(Size Capacity, Allocator* BackingAllocator = nullptr, const char* Name = "StackAllocator");
        StackAllocator(void* Memory, Size Capacity, const char* Name = "StackAllocator");
        ~StackAllocator() override;

        StackAllocator(const StackAllocator&) = delete;
        StackAllocator& operator=(const StackAllocator&) = delete;

        StackAllocator(StackAllocator&& Other) noexcept;
        StackAllocator& operator=(StackAllocator&& Other) noexcept;

        void Init(Size Capacity, Allocator* BackingAllocator = nullptr, const char* Name = "StackAllocator");
        void Init(void* Memory, Size Capacity, const char* Name = "StackAllocator");
        void Shutdown();

        void Reset() override;

        [[nodiscard]] Size GetMarker() const;
        void RollbackTo(Size Marker);

        [[nodiscard]] MemoryStats GetStats() const override;
        [[nodiscard]] const char* GetName() const override;

    protected:
        [[nodiscard]] void* AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location) override;
        void DeallocateImpl(void* Pointer) override;

    private:
        static constexpr Size InvalidOffset = static_cast<Size>(-1);

        struct AllocationHeader
        {
            Size PreviousOffset = 0;
            Size PreviousAllocationOffset = InvalidOffset;
        };

        void MoveFrom(StackAllocator&& Other) noexcept;

    private:
        void* m_Start = nullptr;
        Size m_Capacity = 0;
        Size m_Offset = 0;
        Size m_LastAllocationOffset = InvalidOffset;
        Allocator* m_BackingAllocator = nullptr;
        bool m_OwnsMemory = false;
        const char* m_Name = "StackAllocator";
        MemoryStats m_Stats{};
    };
}
