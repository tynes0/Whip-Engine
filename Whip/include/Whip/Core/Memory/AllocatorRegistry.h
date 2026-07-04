#pragma once

#include "Allocator.h"
#include "ArenaAllocator.h"
#include "LinearAllocator.h"
#include "MemoryStats.h"
#include "MemoryTag.h"
#include "StlAllocator.h"

#include <deque>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace whip::memory
{
    [[nodiscard]] Allocator& GetDefaultAllocator();
    [[nodiscard]] Allocator& GetAllocator(MemoryTag Tag);
    [[nodiscard]] LinearAllocator& GetFrameAllocator();
    [[nodiscard]] ArenaAllocator& GetScratchArenaAllocator();
    void ResetFrameAllocator();
    void ResetScratchArenaAllocator();
    [[nodiscard]] MemoryStats GetDefaultAllocatorStats();

    template<typename T>
    using Vector = std::vector<T, StlAllocator<T>>;

    template<typename T>
    using Deque = std::deque<T, StlAllocator<T>>;

    template<typename Key, typename Compare = std::less<Key>>
    using Set = std::set<Key, Compare, StlAllocator<Key>>;

    template<typename Key, typename Value, typename Compare = std::less<Key>>
    using Map = std::map<Key, Value, Compare, StlAllocator<std::pair<const Key, Value>>>;

    template<typename Key, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
    using UnorderedSet = std::unordered_set<Key, Hash, Equal, StlAllocator<Key>>;

    template<typename Key, typename Value, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
    using UnorderedMap = std::unordered_map<Key, Value, Hash, Equal, StlAllocator<std::pair<const Key, Value>>>;

    using String = std::basic_string<char, std::char_traits<char>, StlAllocator<char>>;

    template<typename T>
    [[nodiscard]] Vector<T> MakeVector(MemoryTag Tag = MemoryTag::Unknown)
    {
        return Vector<T>(StlAllocator<T>(GetAllocator(Tag), Tag));
    }

    template<typename T>
    [[nodiscard]] Vector<T> MakeVector(Allocator& AllocatorRef, MemoryTag Tag = MemoryTag::Unknown)
    {
        return Vector<T>(StlAllocator<T>(AllocatorRef, Tag));
    }

    template<typename T>
    [[nodiscard]] Vector<T> MakeVectorWithSize(Size Count, MemoryTag Tag = MemoryTag::Unknown)
    {
        return Vector<T>(Count, T{}, StlAllocator<T>(GetAllocator(Tag), Tag));
    }

    template<typename T>
    [[nodiscard]] Vector<T> MakeVectorWithSize(Allocator& AllocatorRef, Size Count, MemoryTag Tag = MemoryTag::Unknown)
    {
        return Vector<T>(Count, T{}, StlAllocator<T>(AllocatorRef, Tag));
    }

    template<typename T>
    [[nodiscard]] Vector<T> MakeFrameVector(Size ReserveCount = 0)
    {
        Vector<T> Result(StlAllocator<T>(GetFrameAllocator(), MemoryTag::Temporary));
        if (ReserveCount > 0)
            Result.reserve(ReserveCount);
        return Result;
    }

    template<typename T>
    [[nodiscard]] Vector<T> MakeFrameVectorWithSize(Size Count)
    {
        return Vector<T>(Count, T{}, StlAllocator<T>(GetFrameAllocator(), MemoryTag::Temporary));
    }

    template<typename T>
    [[nodiscard]] Deque<T> MakeDeque(MemoryTag Tag = MemoryTag::Unknown)
    {
        return Deque<T>(StlAllocator<T>(GetAllocator(Tag), Tag));
    }

    template<typename T>
    [[nodiscard]] Deque<T> MakeDeque(Allocator& AllocatorRef, MemoryTag Tag = MemoryTag::Unknown)
    {
        return Deque<T>(StlAllocator<T>(AllocatorRef, Tag));
    }

    template<typename T>
    [[nodiscard]] Deque<T> MakeFrameDeque()
    {
        return Deque<T>(StlAllocator<T>(GetFrameAllocator(), MemoryTag::Temporary));
    }

    template<typename Key, typename Compare = std::less<Key>>
    [[nodiscard]] Set<Key, Compare> MakeSet(MemoryTag Tag = MemoryTag::Unknown)
    {
        return Set<Key, Compare>(Compare{}, StlAllocator<Key>(GetAllocator(Tag), Tag));
    }

    template<typename Key, typename Compare = std::less<Key>>
    [[nodiscard]] Set<Key, Compare> MakeSet(Allocator& AllocatorRef, MemoryTag Tag = MemoryTag::Unknown)
    {
        return Set<Key, Compare>(Compare{}, StlAllocator<Key>(AllocatorRef, Tag));
    }

    template<typename Key, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
    [[nodiscard]] UnorderedSet<Key, Hash, Equal> MakeUnorderedSet(MemoryTag Tag = MemoryTag::Unknown)
    {
        return UnorderedSet<Key, Hash, Equal>(0, Hash{}, Equal{}, StlAllocator<Key>(GetAllocator(Tag), Tag));
    }

    template<typename Key, typename Value, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
    [[nodiscard]] UnorderedMap<Key, Value, Hash, Equal> MakeUnorderedMap(MemoryTag Tag = MemoryTag::Unknown)
    {
        return UnorderedMap<Key, Value, Hash, Equal>(0, Hash{}, Equal{}, StlAllocator<std::pair<const Key, Value>>(GetAllocator(Tag), Tag));
    }

    [[nodiscard]] inline String MakeString(MemoryTag Tag = MemoryTag::Unknown)
    {
        return String(StlAllocator<char>(GetAllocator(Tag), Tag));
    }

    [[nodiscard]] inline String MakeString(Allocator& AllocatorRef, MemoryTag Tag = MemoryTag::Unknown)
    {
        return String(StlAllocator<char>(AllocatorRef, Tag));
    }

    [[nodiscard]] inline String MakeFrameString()
    {
        return String(StlAllocator<char>(GetFrameAllocator(), MemoryTag::Temporary));
    }

    [[nodiscard]] inline LinearAllocatorMarker MakeFrameMarker()
    {
        return LinearAllocatorMarker(GetFrameAllocator());
    }
}
