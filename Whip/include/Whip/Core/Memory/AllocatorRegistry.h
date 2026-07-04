#pragma once

#include "Allocator.h"
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
    void ResetFrameAllocator();
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

    [[nodiscard]] inline String MakeString(MemoryTag Tag = MemoryTag::Unknown)
    {
        return String(StlAllocator<char>(GetAllocator(Tag), Tag));
    }
}
