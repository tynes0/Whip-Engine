#pragma once

#include "Allocator.h"

#include <mutex>

namespace whip::memory
{
    /**
     * @brief Thread-safe wrapper that serializes access to another allocator.
     *
     * SynchronizedAllocator protects Allocate(), Deallocate(), Reset(), and GetStats()
     * calls with a mutex before forwarding them to the backing allocator. It is useful
     * when an allocator that is otherwise not thread-safe must be shared across
     * multiple threads.
     *
     * Prefer per-thread allocators or more specialized synchronization strategies for
     * very hot paths, because every operation through this wrapper acquires a lock.
     */
    class SynchronizedAllocator final : public Allocator
    {
    public:
        explicit SynchronizedAllocator(Allocator& BackingAllocator, const char* Name = "SynchronizedAllocator");

        void Reset() override;

        [[nodiscard]] MemoryStats GetStats() const override;
        [[nodiscard]] const char* GetName() const override;

    protected:
        [[nodiscard]] void* AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location) override;
        void DeallocateImpl(void* Pointer) override;

    private:
        Allocator* m_BackingAllocator = nullptr;
        const char* m_Name = "SynchronizedAllocator";
        mutable std::mutex m_Mutex;
    };
}
