#pragma once

#include "Core.h"
#include <cstdint>
#include <cstring>
#include <type_traits>

_WHIP_START
	// Non-owning raw buffer class
struct raw_buffer
{
	uint8_t* data = nullptr;
	uint64_t size = 0;

	raw_buffer() = default;

	raw_buffer(uint64_t size_in)
	{
		allocate(size_in);
	}

	raw_buffer(const void* data_in, uint64_t size_in) : data((uint8_t*)data_in), size(size_in) {}

	raw_buffer(const raw_buffer&) = default;
	raw_buffer& operator=(const raw_buffer&) = default;

	raw_buffer(nullptr_t) {};
	raw_buffer& operator=(nullptr_t) { release(); };

	static raw_buffer copy(raw_buffer other)
	{
		raw_buffer result(other.size);
		std::memcpy(result.data, other.data, other.size);
		return result;
	}

	void allocate(uint64_t size_in)
	{
		release();

		data = (uint8_t*)std::malloc(size_in);
		size = size_in;
	}

	void release()
	{
		std::free(data);
		data = nullptr;
		size = 0;
	}

	template<typename T>
	T* as()
	{
		return (T*)data;
	}

	operator bool() const
	{
		return (bool)data;
	}

	size_t operator-(const raw_buffer& buffer)
	{
		return data - buffer.data;
	}
};

struct scoped_buffer
{
	scoped_buffer(raw_buffer buffer) : m_buffer(buffer) {}
	scoped_buffer(uint64_t size) : m_buffer(size) {}

	~scoped_buffer() { m_buffer.release(); }

	uint8_t* data() { return m_buffer.data; }
	uint64_t size() const { return m_buffer.size; }

	template<typename T>
	T* as() { return m_buffer.as<T>(); }

	operator bool() const { return (bool)m_buffer; }
private:
	raw_buffer m_buffer;
};

template <size_t _Size>
struct stack_buffer
{
	uint8_t data[_Size];
	static constexpr size_t size = _Size;

	void zero()
	{
		std::memset(data, 0, size);
	}

	void set(const void* src, size_t copy_size = 0)
	{
		if (copy_size == 0 || copy_size > size)
			copy_size = size;
		std::memcpy(data, src, copy_size);
	}

	template <class T>
	void set_value(const T& value)
	{
		static_assert(sizeof(T) <= size, "Type too large!");
		std::memcpy(data, &value, size);
	}

	template<class T>
	T* as()
	{
		return (T*)data;
	}

	template<class T>
	const T* as() const
	{
		return (T*)data;
	}

	template <class T>
	T& value()
	{
		static_assert(sizeof(T) <= size, "Type too large!");
		return *as<T>();
	}

	template <class T>
	const T& value() const
	{
		static_assert(sizeof(T) <= size, "Type too large!");
		return *as<T>();
	}

	bool is_filled_with_zero() const
	{
		uint8_t temp[size]{0};
		return std::memcmp(data, temp, size);
	}

	bool const is_null() const
	{
		return false;
	}
};

template<>
struct stack_buffer<0>
{
	uint8_t* data = nullptr;
	static constexpr size_t size = 0;

	void zero() {}
	void set(const void*, size_t = 0) {}
	template <class T>
	void set_value(const T&) { static_assert(false, "null stack buffer!"); }
	template<class T>
	T* as() { return nullptr; }
	template<class T>
	const T* as() const { return nullptr; }
	template <class T>
	T& value() { static_assert(false, "null stack buffer!"); return *as<T>(); }
	template <class T>
	const T& value() const { static_assert(false, "null stack buffer!"); return *as<T>(); }
	bool is_filled_with_zero() const { return true; }
	bool is_null() const { return true; }
};

_WHIP_END
