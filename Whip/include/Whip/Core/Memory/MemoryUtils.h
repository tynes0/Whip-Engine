#pragma once

#include "Defines.h"
#include "Types.h"

#include <cstring>

namespace whip::memory
{
    constexpr Size Kilobytes(Size Value) { return Value * 1024ULL; }
    constexpr Size Megabytes(Size Value) { return Kilobytes(Value) * 1024ULL; }
    constexpr Size Gigabytes(Size Value) { return Megabytes(Value) * 1024ULL; }

    constexpr bool IsPowerOfTwo(Size Value)
    {
        return Value != 0 && (Value & (Value - 1)) == 0;
    }

    constexpr Size AlignUp(Size Value, Size Alignment)
    {
        return (Value + Alignment - 1) & ~(Alignment - 1);
    }

    constexpr std::uintptr_t AlignForwardAddress(std::uintptr_t Address, Size Alignment)
    {
        WHIP_MEMORY_ASSERT(IsPowerOfTwo(Alignment));
        const std::uintptr_t Mask = static_cast<std::uintptr_t>(Alignment - 1);
        return (Address + Mask) & ~Mask;
    }

    WHIP_MEMORY_FORCE_INLINE void* AlignForwardPointer(void* Pointer, Size Alignment)
    {
        return reinterpret_cast<void*>(AlignForwardAddress(reinterpret_cast<std::uintptr_t>(Pointer), Alignment));
    }

    WHIP_MEMORY_FORCE_INLINE const void* AlignForwardPointer(const void* Pointer, Size Alignment)
    {
        return reinterpret_cast<const void*>(AlignForwardAddress(reinterpret_cast<std::uintptr_t>(Pointer), Alignment));
    }

    WHIP_MEMORY_FORCE_INLINE Size AlignForwardAdjustment(const void* Pointer, Size Alignment)
    {
        const std::uintptr_t Address = reinterpret_cast<std::uintptr_t>(Pointer);
        const std::uintptr_t Aligned = AlignForwardAddress(Address, Alignment);
        return static_cast<Size>(Aligned - Address);
    }

    template<typename Header>
    WHIP_MEMORY_FORCE_INLINE Size AlignForwardAdjustmentWithHeader(const void* Pointer, Size Alignment)
    {
        Size Adjustment = AlignForwardAdjustment(Pointer, Alignment);
        if (Adjustment < sizeof(Header))
        {
            const Size NeededSpace = sizeof(Header) - Adjustment;
            Adjustment += Alignment * ((NeededSpace + Alignment - 1) / Alignment);
        }

        return Adjustment;
    }

    template<typename T = void>
    WHIP_MEMORY_FORCE_INLINE T* PointerAdd(void* Pointer, Size Bytes)
    {
        return reinterpret_cast<T*>(static_cast<Byte*>(Pointer) + Bytes);
    }

    template<typename T = const void>
    WHIP_MEMORY_FORCE_INLINE const T* PointerAdd(const void* Pointer, Size Bytes)
    {
        return reinterpret_cast<const T*>(static_cast<const Byte*>(Pointer) + Bytes);
    }

    template<typename T = void>
    WHIP_MEMORY_FORCE_INLINE T* PointerSub(void* Pointer, Size Bytes)
    {
        return reinterpret_cast<T*>(static_cast<Byte*>(Pointer) - Bytes);
    }

    template<typename T = const void>
    WHIP_MEMORY_FORCE_INLINE const T* PointerSub(const void* Pointer, Size Bytes)
    {
        return reinterpret_cast<const T*>(static_cast<const Byte*>(Pointer) - Bytes);
    }

    inline void FillBytes(void* Pointer, Size SizeInBytes, Byte Value)
    {
        if (Pointer && SizeInBytes > 0)
            std::memset(Pointer, Value, SizeInBytes);
    }

    inline bool CheckPattern(const void* Pointer, Size SizeInBytes, Byte Value)
    {
        if (!Pointer)
            return SizeInBytes == 0;

        const Byte* Bytes = static_cast<const Byte*>(Pointer);
        for (Size Index = 0; Index < SizeInBytes; ++Index)
        {
            if (Bytes[Index] != Value)
                return false;
        }

        return true;
    }
}
