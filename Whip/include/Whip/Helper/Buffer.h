#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/Memory/AllocatorRegistry.h>
#include <Whip/Core/Memory/Construct.h>

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

_WHIP_START

// Non-owning raw buffer class.
struct RawBuffer
{
	uint8_t* m_Data = nullptr;
	uint64_t m_Size = 0;
	bool m_OwnsData = false;
	memory::Allocator* m_Allocator = &memory::GetAllocator(memory::MemoryTag::Core);

	RawBuffer() = default;
	RawBuffer(uint64_t sizeIn) { Allocate(sizeIn); }
	RawBuffer(const void* dataIn, uint64_t sizeIn) : m_Data(const_cast<uint8_t*>(static_cast<const uint8_t*>(dataIn))), m_Size(sizeIn) {}
	RawBuffer(const RawBuffer& other)
		: m_Data(other.m_Data), m_Size(other.m_Size), m_OwnsData(false), m_Allocator(other.m_Allocator)
	{
	}

	RawBuffer(RawBuffer&& other) noexcept
		: m_Data(other.m_Data), m_Size(other.m_Size), m_OwnsData(other.m_OwnsData), m_Allocator(other.m_Allocator)
	{
		other.m_Data = nullptr;
		other.m_Size = 0;
		other.m_OwnsData = false;
	}

	RawBuffer& operator=(const RawBuffer& other)
	{
		if (this == &other)
			return *this;

		Release();
		m_Data = other.m_Data;
		m_Size = other.m_Size;
		m_OwnsData = false;
		m_Allocator = other.m_Allocator;
		return *this;
	}

	RawBuffer& operator=(RawBuffer&& other) noexcept
	{
		if (this == &other)
			return *this;

		Release();
		m_Data = other.m_Data;
		m_Size = other.m_Size;
		m_OwnsData = other.m_OwnsData;
		m_Allocator = other.m_Allocator;

		other.m_Data = nullptr;
		other.m_Size = 0;
		other.m_OwnsData = false;
		return *this;
	}

	RawBuffer(nullptr_t) {}
	RawBuffer& operator=(nullptr_t) { Release(); return *this; }

	static RawBuffer Copy(RawBuffer other)
	{
		RawBuffer result(other.m_Size);
		std::memcpy(result.m_Data, other.m_Data, other.m_Size);
		return result;
	}

	void Allocate(uint64_t sizeIn)
	{
		if (m_OwnsData && sizeIn == m_Size)
			return;

		Release();

		m_Allocator = &memory::GetAllocator(memory::MemoryTag::Core);
		m_Data = memory::NewArray<uint8_t>(*m_Allocator, static_cast<memory::Size>(sizeIn), memory::MemoryTag::Core, WHIP_MEMORY_LOCATION);
		WHP_CORE_ASSERT(m_Data, "Memory allocation failed!");
		m_Size = sizeIn;
		m_OwnsData = true;
	}

	void Release()
	{
		if (m_Data)
		{
			if (m_OwnsData)
				memory::DeleteArray(*m_Allocator, m_Data);
			m_Data = nullptr;
			m_Size = 0;
			m_OwnsData = false;
		}
	}

	uint8_t* Unbound()
	{
		uint8_t* buffer = m_Data;
		m_Data = nullptr;
		m_Size = 0;
		m_OwnsData = false;
		return buffer;
	}

	template <class T>
	void Store(const T& dataIn)
	{
		Allocate(sizeof(T));
		std::memcpy(m_Data, &dataIn, m_Size);
	}

	template <class T>
	T& Load()
	{
		WHP_CORE_ASSERT(sizeof(T) <= m_Size, "Buffer overflow!");
		return *As<T>();
	}

	template <class T>
	bool CanCastTo() const { return sizeof(T) <= m_Size; }
	template<typename T>
	T* As() { return (T*)m_Data; }
	template<typename T>
	const T* As() const { return (T*)m_Data; }
	const uint8_t* begin() const { return m_Data; }
	const uint8_t* end() const { return m_Data + m_Size; }
	operator bool() const { return (bool)m_Data; }
	size_t operator-(const RawBuffer& buffer) { return m_Data - buffer.m_Data; }
};

struct ScopedBuffer
{
	ScopedBuffer(RawBuffer buffer) : m_Buffer(std::move(buffer)) {}
	ScopedBuffer(uint64_t size) : m_Buffer(size) {}
	~ScopedBuffer() { m_Buffer.Release(); }

	uint8_t* Data() { return m_Buffer.m_Data; }
	const uint8_t* Data() const { return m_Buffer.m_Data; }
	uint8_t* Unbound() { return m_Buffer.Unbound(); }
	uint64_t Size() const { return m_Buffer.m_Size; }

	template<typename T>
	T* As() { return m_Buffer.As<T>(); }

	operator bool() const { return (bool)m_Buffer; }
private:
	RawBuffer m_Buffer;
};

namespace detail
{
	template <uint64_t Size, uint64_t Align>
	concept ValidAlign = ((Align & (Align - 1)) == 0) && Size >= Align;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 6385 6386)
#endif

template <size_t _MinimumSize, uint64_t _Align = 1>
	requires detail::ValidAlign<_MinimumSize, _Align>
struct StackBuffer
{
	uint8_t alignas(_Align) m_Data[(_MinimumSize + _Align - 1) & ~(_Align - 1)];
	static constexpr size_t m_Size = sizeof(m_Data);
	static constexpr uint64_t m_Align = _Align;

	constexpr void Zero()
	{
		std::memset(m_Data, 0, m_Size);
	}

	constexpr void Set(const void* src, size_t copySize = 0)
	{
		if (copySize == 0 || copySize > m_Size)
			copySize = m_Size;
		std::memcpy(m_Data, src, copySize);
	}

	template <class T>
	constexpr void Store(const T& value)
	{
		static_assert(sizeof(T) <= m_Size, "Type too large!");
		std::memcpy(m_Data, &value, sizeof(value));
	}

	template<class T>
	constexpr T* As()
	{
		return (T*)m_Data;
	}

	template<class T>
	constexpr const T* As() const
	{
		return (T*)m_Data;
	}

	template <class T>
	constexpr T& Load()
	{
		static_assert(sizeof(T) <= m_Size, "Type too large!");
		return *As<T>();
	}

	template <class T>
	constexpr const T& Load() const
	{
		static_assert(sizeof(T) <= m_Size, "Type too large!");
		return *As<T>();
	}

	constexpr bool FilledWithZeros() const
	{
		uint8_t temp[m_Size]{};
		return (std::memcmp(m_Data, temp, m_Size) == 0);
	}

	constexpr bool const IsNull() const { return false; }

	template <class T>
	static constexpr bool Fits = sizeof(T) <= m_Size;
};

template<>
struct StackBuffer<0, 0>
{
	uint8_t* m_Data = nullptr;
	static constexpr size_t m_Size = 0;

	void Zero() {}
	void Set(const void*, size_t = 0) {}
	template <class T>
	void Store(const T&) { static_assert(false, "null stack buffer!"); }
	template<class T>
	T* As() { return nullptr; }
	template<class T>
	const T* As() const { return nullptr; }
	template <class T>
	T& Load() { static_assert(false, "null stack buffer!"); return *As<T>(); }
	template <class T>
	const T& Load() const { static_assert(false, "null stack buffer!"); return *As<T>(); }
	bool FilledWithZeros() const { return true; }
	bool IsNull() const { return true; }
	template <class>
	static constexpr bool Fits = false;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

_WHIP_END
