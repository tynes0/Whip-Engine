#include "whippch.h"
#include "UUID.h"

#include <random>

#include <unordered_map>

_WHIP_START

static std::random_device s_random_device;
static std::mt19937_64 s_engine(s_random_device());
static std::mt19937 s_engine32(s_random_device());
static std::uniform_int_distribution<uint64_t> s_uniform_distribution;
static std::uniform_int_distribution<uint32_t> s_uniform_distribution32;

UUID::UUID() : m_UUID(s_uniform_distribution(s_engine)) {}

UUID::UUID(uint64_t uuid) : m_UUID(uuid) {}

UUID32::UUID32() : m_UUID(s_uniform_distribution32(s_engine32)) {}

UUID32::UUID32(uint32_t uuid) : m_UUID(uuid) {}

_WHIP_END
