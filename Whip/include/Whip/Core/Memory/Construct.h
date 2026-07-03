#pragma once

#include "Allocator.h"
#include "Defines.h"
#include "MemoryUtils.h"

#include <limits>
#include <new>
#include <type_traits>
#include <utility>

#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
    #include <concepts>
#endif

namespace whip::memory
{
    template<typename T>
    struct ArrayHeader
    {
        void* RawPointer = nullptr;
        Size Count = 0;
    };

    namespace Detail
    {
        template<typename T>
        inline constexpr bool IsAllocatableObject =
            std::is_object_v<T> &&
            !std::is_void_v<T> &&
            !std::is_reference_v<T> &&
            !std::is_function_v<T> &&
            !std::is_array_v<T>;

        template<typename T, typename... Args>
        inline constexpr bool IsNewConstructible =
            IsAllocatableObject<T> &&
            std::is_constructible_v<T, Args...> &&
            std::is_destructible_v<T>;

        template<typename T>
        inline constexpr bool IsDeletableObject =
            IsAllocatableObject<T> &&
            std::is_destructible_v<T>;

        template<typename T>
        inline constexpr bool IsNewArrayConstructible =
            IsAllocatableObject<T> &&
            std::is_default_constructible_v<T> &&
            std::is_destructible_v<T>;

        template<typename T>
        inline constexpr bool IsDeleteArrayCompatible =
            IsAllocatableObject<T> &&
            std::is_destructible_v<T>;

        template<typename T, typename... Args>
        constexpr void ValidateNewType()
        {
            static_assert(IsAllocatableObject<T>,
                "whip::memory::New<T>: T must be a non-array object type. "
                "void, references, functions and raw array types are not supported.");

            static_assert(std::is_constructible_v<T, Args...>,
                "whip::memory::New<T>: T is not constructible with the supplied constructor arguments.");

            static_assert(std::is_destructible_v<T>,
                "whip::memory::New<T>: T must be destructible so whip::memory::Delete can destroy it safely.");
        }

        template<typename T>
        constexpr void ValidateDeleteType()
        {
            static_assert(IsAllocatableObject<T>,
                "whip::memory::Delete<T>: T must be a non-array object type. "
                "Use DeleteArray for arrays created by NewArray.");

            static_assert(std::is_destructible_v<T>,
                "whip::memory::Delete<T>: T must be destructible.");
        }

        template<typename T>
        constexpr void ValidateNewArrayType()
        {
            static_assert(IsAllocatableObject<T>,
                "whip::memory::NewArray<T>: T must be the element type, not an array type. "
                "Use NewArray<Foo>(Allocator, Count), not NewArray<Foo[]>.");

            static_assert(std::is_default_constructible_v<T>,
                "whip::memory::NewArray<T>: T must be default constructible. "
                "Use repeated New<T> or a typed container when elements need constructor arguments.");

            static_assert(std::is_destructible_v<T>,
                "whip::memory::NewArray<T>: T must be destructible so DeleteArray can destroy it safely.");
        }

        template<typename T>
        constexpr void ValidateDeleteArrayType()
        {
            static_assert(IsAllocatableObject<T>,
                "whip::memory::DeleteArray<T>: T must be the element type, not an array type.");

            static_assert(std::is_destructible_v<T>,
                "whip::memory::DeleteArray<T>: T must be destructible.");
        }

        template<typename T>
        constexpr Size GetArrayAllocationSize(Size Count)
        {
            constexpr Size HeaderSize = sizeof(ArrayHeader<T>);
            constexpr Size AlignmentPadding = alignof(T);
            constexpr Size MaxSize = std::numeric_limits<Size>::max();

            if (Count > (MaxSize - HeaderSize - AlignmentPadding) / sizeof(T))
                throw std::bad_array_new_length{};

            return HeaderSize + AlignmentPadding + sizeof(T) * Count;
        }

#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
        template<typename T, typename... Args>
        concept NewConstructible = IsNewConstructible<T, Args...>;

        template<typename T>
        concept DeletableObject = IsDeletableObject<T>;

        template<typename T>
        concept NewArrayConstructible = IsNewArrayConstructible<T>;

        template<typename T>
        concept DeleteArrayCompatible = IsDeleteArrayCompatible<T>;
#endif
    }
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
    template<typename T, typename... Args>
        requires Detail::NewConstructible<T, Args...>
#else
    template<typename T, typename... Args>
#endif
    T* New(Allocator& AllocatorRef, MemoryTag Tag, SourceLocation Location, Args&&... ConstructorArgs)
    {
        Detail::ValidateNewType<T, Args...>();

        void* Memory = AllocatorRef.Allocate(sizeof(T), alignof(T), Tag, Location);
        if (!Memory)
            throw std::bad_alloc{};

        try
        {
            return new (Memory) T(std::forward<Args>(ConstructorArgs)...);
        }
        catch (...)
        {
            AllocatorRef.Deallocate(Memory);
            throw;
        }
    }

#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
    template<typename T>
        requires Detail::DeletableObject<T>
#else
    template<typename T>
#endif
    void Delete(Allocator& AllocatorRef, T* Object)
    {
        Detail::ValidateDeleteType<T>();

        if (!Object)
            return;

        if constexpr (!std::is_trivially_destructible_v<T>)
            Object->~T();

        AllocatorRef.Deallocate(Object);
    }

#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
    template<typename T>
        requires Detail::NewArrayConstructible<T>
#else
    template<typename T>
#endif
    T* NewArray(Allocator& AllocatorRef, Size Count, MemoryTag Tag, SourceLocation Location)
    {
        Detail::ValidateNewArrayType<T>();

        if (Count == 0)
            return nullptr;

        const Size HeaderSize = sizeof(ArrayHeader<T>);
        const Size TotalSize = Detail::GetArrayAllocationSize<T>(Count);

        void* RawMemory = AllocatorRef.Allocate(TotalSize, std::max(alignof(ArrayHeader<T>), alignof(T)), Tag, Location);
        if (!RawMemory)
            throw std::bad_alloc{};

        Byte* UserMemory = static_cast<Byte*>(AlignForwardPointer(PointerAdd(RawMemory, HeaderSize), alignof(T)));
        auto* Header = reinterpret_cast<ArrayHeader<T>*>(UserMemory - HeaderSize);
        Header->RawPointer = RawMemory;
        Header->Count = Count;

        Size Constructed = 0;
        try
        {
            for (; Constructed < Count; ++Constructed)
                new (UserMemory + sizeof(T) * Constructed) T();
        }
        catch (...)
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (Size Index = Constructed; Index > 0; --Index)
                    reinterpret_cast<T*>(UserMemory + sizeof(T) * (Index - 1))->~T();
            }

            AllocatorRef.Deallocate(RawMemory);
            throw;
        }

        return reinterpret_cast<T*>(UserMemory);
    }

#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
    template<typename T>
        requires Detail::DeleteArrayCompatible<T>
#else
    template<typename T>
#endif
    void DeleteArray(Allocator& AllocatorRef, T* Array)
    {
        Detail::ValidateDeleteArrayType<T>();

        if (!Array)
            return;

        Byte* UserMemory = reinterpret_cast<Byte*>(Array);
        auto* Header = reinterpret_cast<ArrayHeader<T>*>(UserMemory - sizeof(ArrayHeader<T>));

        WHIP_MEMORY_ASSERT(Header->RawPointer != nullptr);

        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            for (Size Index = Header->Count; Index > 0; --Index)
                Array[Index - 1].~T();
        }

        AllocatorRef.Deallocate(Header->RawPointer);
    }
}

#define WHIP_ALLOC(Allocator, Size, Alignment, Tag) \
    (Allocator).Allocate((Size), (Alignment), (Tag), WHIP_MEMORY_LOCATION)

#define WHIP_FREE(Allocator, Pointer) \
    (Allocator).Deallocate((Pointer))

#define WHIP_NEW(Allocator, Type, ...) \
    ::whip::memory::New<Type>((Allocator), ::whip::memory::MemoryTag::Unknown, WHIP_MEMORY_LOCATION, ##__VA_ARGS__)

#define WHIP_NEW_TAGGED(Allocator, Tag, Type, ...) \
    ::whip::memory::New<Type>((Allocator), (Tag), WHIP_MEMORY_LOCATION, ##__VA_ARGS__)

#define WHIP_DELETE(Allocator, Pointer) \
    ::whip::memory::Delete((Allocator), (Pointer))

#define WHIP_NEW_ARRAY(Allocator, Type, Count) \
    ::whip::memory::NewArray<Type>((Allocator), (Count), ::whip::memory::MemoryTag::Unknown, WHIP_MEMORY_LOCATION)

#define WHIP_NEW_ARRAY_TAGGED(Allocator, Tag, Type, Count) \
    ::whip::memory::NewArray<Type>((Allocator), (Count), (Tag), WHIP_MEMORY_LOCATION)

#define WHIP_DELETE_ARRAY(Allocator, Pointer) \
    ::whip::memory::DeleteArray((Allocator), (Pointer))
