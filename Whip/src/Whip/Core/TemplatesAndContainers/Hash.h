#pragma once

#include "Whip/Core/Core.h"
#include "TypeTraits.h"

#include <type_traits>
#include <string>


_WHIP_START

class sha256
{
public:
	sha256() {}

#pragma warning(push)
#pragma warning(disable : 4996) // for sprintf
	static std::string do_hash(const std::string& input)
	{
		uint8_t digest[DIGEST_SIZE];
		memset(digest, 0, DIGEST_SIZE);

		sha256 base;
		base.init();
		base.update((const uint8_t*)input.c_str(), input.length());
		base.final(digest);

		char buffer[2 * DIGEST_SIZE + 1];
		buffer[2 * DIGEST_SIZE] = 0;
		for (size_t i = 0; i < DIGEST_SIZE; ++i)
		{
			sprintf(buffer + i * 2, "%02x", digest[i]);
		}
		return { buffer };
	}
#pragma warning(pop)
private:
	
	void init()
	{
		m_hash_values[0] = 0x6a09e667;
		m_hash_values[1] = 0xbb67ae85;
		m_hash_values[2] = 0x3c6ef372;
		m_hash_values[3] = 0xa54ff53a;
		m_hash_values[4] = 0x510e527f;
		m_hash_values[5] = 0x9b05688c;
		m_hash_values[6] = 0x1f83d9ab;
		m_hash_values[7] = 0x5be0cd19;
		m_length = 0;
		m_total_length = 0;
	}
	void update(const uint8_t* text, size_t length)
	{
		size_t temp_length = SHA224_256_BLOCK_SIZE - m_length;
		size_t remaining_length = length < temp_length ? length : temp_length;
		memcpy(&m_block[m_length], text, remaining_length);
		if (m_length + length < SHA224_256_BLOCK_SIZE)
		{
			m_length += length;
			return;
		}
		size_t new_length = length - remaining_length;
		size_t block_n_byte = new_length / SHA224_256_BLOCK_SIZE;
		const uint8_t* shifted_text = text + remaining_length;
		transform(m_block, 1);
		transform(shifted_text, block_n_byte);
		remaining_length = new_length % SHA224_256_BLOCK_SIZE;
		memcpy(m_block, &shifted_text[block_n_byte << 6], remaining_length);
		m_length = remaining_length;
		m_total_length += (block_n_byte + 1) << 6;
	}

	void final(uint8_t* digest)
	{
		size_t block_n_byte = (1 + ((SHA224_256_BLOCK_SIZE - 9) < (m_length % SHA224_256_BLOCK_SIZE)));
		size_t len_byte = (m_total_length + m_length) << 3;
		size_t pm_length = block_n_byte << 6;
		memset(m_block + m_length, 0, pm_length - m_length);
		m_block[m_length] = 0x80;
		unpack32(len_byte, m_block + pm_length - 4);
		transform(m_block, block_n_byte);
		for (size_t i = 0; i < 8; ++i)
		{
			unpack32(m_hash_values[i], &digest[i << 2]);
		}
	}
protected:
	void transform(const uint8_t* text, size_t block_n_byte)
	{
		uint32_t data[64], state[8], first_total, second_total;
		const uint8_t* sub_block;
		for (size_t i = 0; i < block_n_byte; ++i)
		{
			sub_block = text + (i << 6);
			for (size_t j = 0; j < 16; ++j)
			{
				pack32(&sub_block[j << 2], &data[j]);
			}
			for (size_t j = 16; j < 64; ++j)
			{
				data[j] = f4(data[j - 2]) + data[j - 7] + f3(data[j - 15]) + data[j - 16];
			}
			for (size_t j = 0; j < 8; ++j)
			{
				state[j] = m_hash_values[j];
			}
			for (size_t j = 0; j < 64; ++j)
			{
				first_total = state[7] + f2(state[4]) + choose(state[4], state[5], state[6]) + sha256_k[j] + data[j];
				second_total = f1(state[0]) + majority(state[1], state[2], state[3]);
				state[7] = state[6];
				state[6] = state[5];
				state[5] = state[4];
				state[4] = state[3] + first_total;
				state[3] = state[2];
				state[2] = state[1];
				state[1] = state[0];
				state[0] = first_total + second_total;
			}
			for (size_t j = 0; j < 8; ++j)
			{
				m_hash_values[j] += state[j];
			}
		}
	}
private:
	uint32_t shift_right(uint32_t x, uint32_t n)
	{
		return (x >> n);
	}

	uint32_t rotate_right(uint32_t x, uint32_t n)
	{
		return ((x >> n) | (x << ((sizeof(x) << 3) - n)));
	}

	uint32_t rotate_left(uint32_t x, uint32_t n)
	{
		return ((x << n) | (x >> ((sizeof(x) << 3) - n)));
	}

	uint32_t choose(uint32_t x, uint32_t y, uint32_t z)
	{
		return ((x & y) ^ (~x & z));
	}

	uint32_t majority(uint32_t x, uint32_t y, uint32_t z)
	{
		return ((x & y) ^ (x & z) ^ (y & z));
	}

	void unpack32(size_t x, uint8_t* str)
	{
		*((str)+3) = (uint8_t)((x));
		*((str)+2) = (uint8_t)((x) >> 8);
		*((str)+1) = (uint8_t)((x) >> 16);
		*((str)+0) = (uint8_t)((x) >> 24);
	}

	void pack32(const uint8_t* str, uint32_t* x)
	{
		*(x) = ((uint32_t) * ((str)+3))
			| ((uint32_t) * ((str)+2) << 8)
			| ((uint32_t) * ((str)+1) << 16)
			| ((uint32_t) * ((str)+0) << 24);
	}

	uint32_t f1(uint32_t x)
	{
		return (rotate_right(x, 2) ^ rotate_right(x, 13) ^ rotate_right(x, 32));
	}

	uint32_t f2(uint32_t x)
	{
		return (rotate_right(x, 6) ^ rotate_right(x, 11) ^ rotate_right(x, 25));
	}

	uint32_t f3(uint32_t x)
	{
		return (rotate_right(x, 7) ^ rotate_right(x, 18) ^ shift_right(x, 3));
	}

	uint32_t f4(uint32_t x)
	{
		return (rotate_right(x, 17) ^ rotate_right(x, 19) ^ shift_right(x, 10));
	}

public:
	static constexpr size_t DIGEST_SIZE = (256 / 8);
protected:
	static constexpr uint32_t sha256_k[64]
	{
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
		0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
		0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
		0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
		0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
		0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
		0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
		0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
		0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};
	static constexpr size_t SHA224_256_BLOCK_SIZE = (512 / 8);
	size_t m_total_length;
	size_t m_length;
	uint8_t  m_block[2 * SHA224_256_BLOCK_SIZE];
	uint32_t m_hash_values[8];
};

class fnv1a
{
public:
#if defined(_WIN64)
	WHP_INLINE static constexpr size_t offset_basis = 14695981039346656037ull;
	WHP_INLINE static constexpr size_t prime = 1099511628211ull;
#else // !defined(_WIN64)
	WHP_INLINE static constexpr size_t offset_basis = 2166136261u;
	WHP_INLINE static constexpr size_t prime = 16777619u;
#endif // defined(_WIN64)

#define WHIP_HASH_TRIVIAL_ASSERT(type) WHP_ASSERT(std::is_trivial_v<type>, "only trivial types can be directly hashed");

	WHP_NODISCARD static inline size_t append_bytes(size_t val, const unsigned char* const first, const size_t count) noexcept
	{
		for (size_t i = 0; i < count; ++i)
		{
			val ^= static_cast<size_t>(first[i]);
			val *= prime;
		}
		return val;
	}

	template <class _Ty>
	WHP_NODISCARD static size_t append_range(const size_t val, const _Ty* const first, const _Ty* const last) noexcept
	{
		WHIP_HASH_TRIVIAL_ASSERT(_Ty);
		const auto first_byte = reinterpret_cast<const unsigned char*>(first);
		const auto last_byte = reinterpret_cast<const unsigned char*>(last);
		return append_bytes(val, first_byte, static_cast<size_t>(last_byte - first_byte));
	}

	template <class _Kty>
	WHP_NODISCARD static size_t append_value(const size_t val, const _Kty& key_value) noexcept
	{
		WHIP_HASH_TRIVIAL_ASSERT(_Kty);
		return append_bytes(val, &reinterpret_cast<const unsigned char&>(key_value), sizeof(_Kty));
	}

	template <class _Kty>
	WHP_NODISCARD static size_t hash_representation(const _Kty& key_value) noexcept
	{
		return append_value(offset_basis, key_value);
	}

	template <class _Kty>
	WHP_NODISCARD static size_t hash_array_representation(const _Kty* const first, const size_t count) noexcept
	{
		WHIP_HASH_TRIVIAL_ASSERT(_Kty);
		return append_bytes(offset_basis, reinterpret_cast<const unsigned char*>(first), count * sizeof(_Kty));
	}
#undef WHIP_HASH_TRIVIAL_ASSERT
};

template <class _Kty>
struct hash;

template <class _Kty, bool _Enabled>
struct conditionally_enabled_hash
{
	WHP_NODISCARD size_t operator()(const _Kty key_value) const noexcept (noexcept(hash<_Kty>::do_hash(key_value)))
	{
		return hash<_Kty>::do_hash(key_value);
	}
};

template <class _Kty>
struct conditionally_enabled_hash<_Kty, false>
{
	conditionally_enabled_hash() = delete;
	conditionally_enabled_hash(const conditionally_enabled_hash&) = delete;
	conditionally_enabled_hash(conditionally_enabled_hash&&) = delete;
	conditionally_enabled_hash& operator=(const conditionally_enabled_hash&) = delete;
	conditionally_enabled_hash& operator=(conditionally_enabled_hash&&) = delete;
};

//default hash algorithm is fnv1a
template <class _Kty>
struct hash : conditionally_enabled_hash<_Kty, !std::is_const_v<_Kty> && !std::is_volatile_v<_Kty> && (std::is_enum_v<_Kty> || std::is_integral_v<_Kty> || std::is_pointer_v<_Kty>)>
{
	WHP_NODISCARD static size_t do_hash(const _Kty& key_value) noexcept
	{
		return fnv1a::hash_representation(key_value);
	}
};

template <>
struct hash<float>
{
	WHP_NODISCARD static size_t do_hash(const float key_value) noexcept
	{
		return fnv1a::hash_representation(key_value == 0.0f ? 0.0f : key_value);
	}

	WHP_NODISCARD size_t operator()(const float key_value) const noexcept
	{
		return do_hash(key_value);
	}
};

template <>
struct hash<double>
{
	WHP_NODISCARD static size_t do_hash(const double key_value) noexcept
	{
		return fnv1a::hash_representation(key_value == 0.0 ? 0.0 : key_value);
	}

	WHP_NODISCARD size_t operator()(const double key_value) const noexcept
	{
		return do_hash(key_value);
	}
};

template <>
struct hash<long double>
{
	WHP_NODISCARD static size_t do_hash(const double key_value) noexcept
	{
		return fnv1a::hash_representation(key_value == 0.0l ? 0.0l : key_value);
	}

	WHP_NODISCARD size_t operator()(const long double key_value) const noexcept
	{
		return do_hash(key_value);
	}
};

template <>
struct hash<nullptr_t>
{
	WHP_NODISCARD static size_t do_hash(nullptr_t) noexcept
	{
		void* null{};
		return fnv1a::hash_representation(null);
	}

	WHP_NODISCARD size_t operator()(nullptr_t) const noexcept
	{
		return do_hash(nullptr_t{});
	}
};

template <>
struct hash<std::string>
{
	WHP_NODISCARD static size_t do_hash(const std::string& key_value) noexcept
	{
		return fnv1a::hash_array_representation(key_value.c_str(), key_value.size());
	}
};

_WHIP_END