#pragma once

#include "AllocatorRegistry.h"
#include "PoolAllocator.h"

#include <new>
#include <type_traits>
#include <utility>

namespace whip::memory
{
    /**
     * @brief Type-safe fixed block pool with a backing allocator fallback.
     *
     * TypedPool is intended for high-churn engine objects with a stable size such
     * as physics handles, contact records, small runtime events, and editor command
     * records. The primary path is O(1) PoolAllocator allocation; if the pool is
     * exhausted, allocation falls back to the configured backing allocator instead
     * of failing gameplay/editor work.
     */
    template<typename T>
    class TypedPool final
    {
    public:
        TypedPool(Size BlockCount, Allocator* BackingAllocator = nullptr, MemoryTag Tag = MemoryTag::Unknown, const char* Name = "TypedPool")
            : m_Pool(sizeof(T), alignof(T), BlockCount, BackingAllocator ? BackingAllocator : &GetAllocator(Tag), Name),
            m_FallbackAllocator(BackingAllocator ? BackingAllocator : &GetAllocator(Tag)),
            m_Tag(Tag)
        {
            static_assert(std::is_object_v<T> && !std::is_array_v<T>, "TypedPool<T> requires a concrete object type.");
            static_assert(std::is_destructible_v<T>, "TypedPool<T> requires a destructible type.");
        }

        TypedPool(const TypedPool&) = delete;
        TypedPool& operator=(const TypedPool&) = delete;

        template<typename... Args>
        [[nodiscard]] T* New(Args&&... ConstructorArgs)
        {
            void* Memory = m_Pool.Allocate(sizeof(T), alignof(T), m_Tag, WHIP_MEMORY_LOCATION);
            Allocator* Owner = &m_Pool;

            if (!Memory)
            {
                Memory = m_FallbackAllocator->Allocate(sizeof(T), alignof(T), m_Tag, WHIP_MEMORY_LOCATION);
                Owner = m_FallbackAllocator;
            }

            if (!Memory)
                throw std::bad_alloc{};

            try
            {
                return new (Memory) T(std::forward<Args>(ConstructorArgs)...);
            }
            catch (...)
            {
                Owner->Deallocate(Memory);
                throw;
            }
        }

        void Delete(T* Object)
        {
            if (!Object)
                return;

            if constexpr (!std::is_trivially_destructible_v<T>)
                Object->~T();

            if (m_Pool.Contains(Object))
                m_Pool.Deallocate(Object);
            else
                m_FallbackAllocator->Deallocate(Object);
        }

        [[nodiscard]] PoolAllocator& GetPoolAllocator() { return m_Pool; }
        [[nodiscard]] const PoolAllocator& GetPoolAllocator() const { return m_Pool; }

    private:
        PoolAllocator m_Pool;
        Allocator* m_FallbackAllocator = nullptr;
        MemoryTag m_Tag = MemoryTag::Unknown;
    };
}
