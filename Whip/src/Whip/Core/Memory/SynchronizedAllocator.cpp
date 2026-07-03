#include "WhipPch.h"
#include "Whip/Core/Memory/SynchronizedAllocator.h"

namespace whip::memory
{
    SynchronizedAllocator::SynchronizedAllocator(Allocator& BackingAllocator, const char* Name)
        : m_BackingAllocator(&BackingAllocator), m_Name(Name)
    {
    }

    void* SynchronizedAllocator::AllocateImpl(Size SizeInBytes, Size Alignment, MemoryTag Tag, SourceLocation Location)
    {
        std::lock_guard<std::mutex> Lock(m_Mutex);
        return m_BackingAllocator->Allocate(SizeInBytes, Alignment, Tag, Location);
    }

    void SynchronizedAllocator::DeallocateImpl(void* Pointer)
    {
        std::lock_guard<std::mutex> Lock(m_Mutex);
        m_BackingAllocator->Deallocate(Pointer);
    }

    void SynchronizedAllocator::Reset()
    {
        std::lock_guard<std::mutex> Lock(m_Mutex);
        m_BackingAllocator->Reset();
    }

    MemoryStats SynchronizedAllocator::GetStats() const
    {
        std::lock_guard<std::mutex> Lock(m_Mutex);
        return m_BackingAllocator->GetStats();
    }

    const char* SynchronizedAllocator::GetName() const
    {
        return m_Name;
    }
}
