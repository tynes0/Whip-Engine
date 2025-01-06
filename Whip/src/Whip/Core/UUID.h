#pragma once

#include "Core.h"
#include <cstdint>

_WHIP_START

class UUID
{
public:
	UUID();
	UUID(uint64_t uuid);
	UUID(const UUID&) = default;

	operator uint64_t() const { return m_UUID; }
private:
	uint64_t m_UUID;
};

class UUID32
{
public:
	UUID32();
	UUID32(uint32_t uuid);
	UUID32(const UUID32&) = default;

	operator uint32_t() const { return m_UUID; }
private:
	uint32_t m_UUID;
};

_WHIP_END

namespace std
{
	template <typename T> struct hash;

	template<>
	struct hash<whip::UUID>
	{
		uint64_t operator()(const whip::UUID& uuid) const
		{
			return (uint64_t)uuid;
		}
	};

	template<>
	struct hash<whip::UUID32>
	{
		uint32_t operator()(const whip::UUID32& uuid) const
		{
			return (uint32_t)uuid;
		}
	};
}
