#pragma once

#include "Allocator.h"
#include "Defines.h"

namespace whip::memory
{
    /**
     * @brief General-purpose allocator backed by the platform C runtime heap.
     *
     * SystemAllocator is the default fallback allocator for WhipMemory. It uses
     * std::malloc/std::free internally while preserving WhipMemory's alignment and
     * statistics contract. It is useful as a root backing allocator for arenas,
     * pools, free lists, and debug/tracking wrappers.
     *
     * This allocator is flexible but not intended to be the fastest option for
     * high-frequency engine allocations. Prefer specialized allocators such as
     * LinearAllocator, PoolAllocator, or ArenaAllocator for predictable allocation
     * patterns.
     */
    class SystemAllocator final : public Allocator
    {
    public:
        explicit SystemAllocator(const char* Name = "SystemAllocator");
        ~SystemAllocator() override = default;

        SystemAllocator(const SystemAllocator&) = delete;
        SystemAllocator& operator=(const SystemAllocator&) = delete;


        [[nodiscard]] MemoryStats GetStats() const override;
        [[nodiscard]] const char* GetName() const override;

        static SystemAllocator& Get();

    protected:
        [[nodiscard]] void* AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location) override;
        void DeallocateImpl(void* Pointer) override;

    private:
        struct AllocationHeader
        {
            void* RawPointer = nullptr;
            Size SizeInBytes = 0;
            Size Alignment = 0;
        };

    private:
        const char* m_Name = nullptr;
        MemoryStats m_Stats{};
    };
}
