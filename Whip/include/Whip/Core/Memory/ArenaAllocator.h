#pragma once

#include "Allocator.h"
#include "MemoryUtils.h"
#include "StlAllocator.h"

#include <vector>

namespace whip::memory
{
    /**
     * @brief Growing chunk-based arena allocator for grouped lifetime management.
     *
     * ArenaAllocator allocates memory linearly inside one or more chunks. When the
     * current chunk cannot satisfy a request, a new chunk is requested from the
     * backing allocator. Individual Deallocate() calls are ignored; memory is released
     * collectively through Reset() or Release().
     *
     * This allocator is a good fit for asset import jobs, scene loading, temporary
     * compiler/transpiler data, editor operations, and other workloads where many
     * allocations share the same lifetime but the final size is not known upfront.
     */
    class ArenaAllocator final : public Allocator
    {
    public:
        explicit ArenaAllocator(Size DefaultChunkSize = Megabytes(4), Allocator* BackingAllocator = nullptr, const char* Name = "ArenaAllocator");
        ~ArenaAllocator() override;

        ArenaAllocator(const ArenaAllocator&) = delete;
        ArenaAllocator& operator=(const ArenaAllocator&) = delete;

        ArenaAllocator(ArenaAllocator&& Other) noexcept;
        ArenaAllocator& operator=(ArenaAllocator&& Other) noexcept;

        void Reset() override;
        void Release();

        [[nodiscard]] MemoryStats GetStats() const override;
        [[nodiscard]] const char* GetName() const override;
        [[nodiscard]] Size GetChunkCount() const;

    protected:
        [[nodiscard]] void* AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location) override;
        void DeallocateImpl(void* Pointer) override;

    private:
        struct Chunk
        {
            void* Memory = nullptr;
            Size Capacity = 0;
            Size Offset = 0;
        };

        Chunk& AddChunk(Size MinimumSize);
        void MoveFrom(ArenaAllocator&& Other) noexcept;

    private:
        Size m_DefaultChunkSize = 0;
        Allocator* m_BackingAllocator = nullptr;
        const char* m_Name = "ArenaAllocator";
        std::vector<Chunk, StlAllocator<Chunk>> m_Chunks;
        MemoryStats m_Stats{};
    };
}
