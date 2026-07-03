#pragma once

namespace whip::memory
{
    /**
     * @brief High-level category attached to allocations for diagnostics and reports.
     *
     * MemoryTag is intentionally small and engine-facing. It lets tracking/debug
     * allocators group allocations by subsystem without forcing every allocator to
     * know about the caller's concrete system type.
     */
    enum class MemoryTag
    {
        Unknown = 0,
        Core,
        Renderer,
        Physics,
        Audio,
        Scripting,
        Asset,
        Editor,
        Scene,
        Temporary,
        Test
    };

    /**
     * @brief Converts a memory tag to a stable, human-readable string.
     *
     * The function is constexpr and header-only so MemoryTag has no translation-unit
     * dependency. This keeps the tag utility cheap, link-safe, and usable in contexts
     * where a compile-time string mapping is helpful.
     */
    [[nodiscard]] constexpr const char* ToString(MemoryTag Tag) noexcept
    {
        switch (Tag)
        {
            case MemoryTag::Unknown:   return "Unknown";
            case MemoryTag::Core:      return "Core";
            case MemoryTag::Renderer:  return "Renderer";
            case MemoryTag::Physics:   return "Physics";
            case MemoryTag::Audio:     return "Audio";
            case MemoryTag::Scripting: return "Scripting";
            case MemoryTag::Asset:     return "Asset";
            case MemoryTag::Editor:    return "Editor";
            case MemoryTag::Scene:     return "Scene";
            case MemoryTag::Temporary: return "Temporary";
            case MemoryTag::Test:      return "Test";
        }

        return "Unknown";
    }
}
