#pragma once

#include "Allocator.h"

namespace whip::memory
{
    /**
     * @brief Diagnostic allocator wrapper that detects memory corruption patterns.
     *
     * DebugAllocator wraps another allocator and adds guard bytes before and after
     * each user allocation. It fills allocated and freed memory with recognizable
     * byte patterns to make buffer overruns, underruns, and use-after-free bugs easier
     * to spot during debugging.
     *
     * Use this allocator in debug builds around another allocator such as
     * SystemAllocator, FreeListAllocator, or PoolAllocator. It is intentionally more
     * expensive than the wrapped allocator and should not be used as a release-mode
     * hot-path allocator unless the diagnostics are explicitly desired.
     */
    class DebugAllocator final : public Allocator
    {
    public:
        explicit DebugAllocator(Allocator& BackingAllocator, const char* Name = "DebugAllocator");
        ~DebugAllocator() override = default;

        DebugAllocator(const DebugAllocator&) = delete;
        DebugAllocator& operator=(const DebugAllocator&) = delete;

        void Reset() override;

        [[nodiscard]] MemoryStats GetStats() const override;
        [[nodiscard]] const char* GetName() const override;

        [[nodiscard]] bool ValidateAllocation(void* Pointer) const;

    protected:
        [[nodiscard]] void* AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location) override;
        void DeallocateImpl(void* Pointer) override;

    private:
        static constexpr Size GuardSize = 16;
        static constexpr Byte GuardValue = 0xFD;
        static constexpr Byte AllocatedValue = 0xCD;
        static constexpr Byte FreedValue = 0xDD;

        struct DebugHeader
        {
            void* RawPointer = nullptr;
            Size RequestedSize = 0;
            Size TotalSize = 0;
            Size Alignment = 0;
            MemoryTag Tag = MemoryTag::Unknown;
            SourceLocation Location{};
        };

    private:
        Allocator* m_BackingAllocator = nullptr;
        const char* m_Name = "DebugAllocator";
        MemoryStats m_Stats{};
    };
}
