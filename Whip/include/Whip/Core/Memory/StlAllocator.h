#pragma once

#include "Allocator.h"

#include <limits>
#include <new>
#include <type_traits>

namespace whip::memory
{
    [[nodiscard]] Allocator& GetDefaultAllocator();

    /**
     * @brief STL-compatible allocator adapter backed by a WhipMemory Allocator.
     *
     * StlAllocator allows standard containers such as std::vector, std::string,
     * std::deque, and std::unordered_map to allocate through WhipMemory. It forwards
     * all memory requests to a user-provided Allocator instance and attaches a memory
     * tag for diagnostics.
     *
     * The adapter follows the standard allocator shape expected by STL containers,
     * while the actual allocation strategy remains controlled by the backing
     * WhipMemory allocator. The backing allocator must outlive every container that
     * uses this adapter.
     */
    template<typename T>
    class StlAllocator
    {
    public:
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::false_type;

        StlAllocator() noexcept
            : m_Allocator(&GetDefaultAllocator())
        {
        }

        explicit StlAllocator(Allocator& AllocatorRef, MemoryTag Tag = MemoryTag::Unknown) noexcept
            : m_Allocator(&AllocatorRef), m_Tag(Tag)
        {
        }

        template<typename U>
        StlAllocator(const StlAllocator<U>& Other) noexcept
            : m_Allocator(Other.GetAllocator()), m_Tag(Other.GetTag())
        {
        }

        [[nodiscard]] T* allocate(std::size_t Count)
        {
            WHIP_MEMORY_ASSERT(m_Allocator && "StlAllocator has no backing allocator");
            if (Count > std::numeric_limits<std::size_t>::max() / sizeof(T))
                throw std::bad_alloc{};

            void* Memory = m_Allocator->Allocate(sizeof(T) * Count, alignof(T), m_Tag, WHIP_MEMORY_LOCATION);
            if (!Memory)
                throw std::bad_alloc{};

            return static_cast<T*>(Memory);
        }

        void deallocate(T* Pointer, std::size_t Count) noexcept
        {
            WHIP_MEMORY_UNUSED(Count);
            if (m_Allocator)
                m_Allocator->Deallocate(Pointer);
        }

        [[nodiscard]] Allocator* GetAllocator() const noexcept { return m_Allocator; }
        [[nodiscard]] MemoryTag GetTag() const noexcept { return m_Tag; }

        template<typename U>
        friend class StlAllocator;

    private:
        Allocator* m_Allocator = nullptr;
        MemoryTag m_Tag = MemoryTag::Unknown;
    };

    template<typename T, typename U>
    bool operator==(const StlAllocator<T>& Left, const StlAllocator<U>& Right) noexcept
    {
        return Left.GetAllocator() == Right.GetAllocator();
    }

    template<typename T, typename U>
    bool operator!=(const StlAllocator<T>& Left, const StlAllocator<U>& Right) noexcept
    {
        return !(Left == Right);
    }
}
