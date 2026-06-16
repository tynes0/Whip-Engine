#include "WhipPch.h"
#include <Whip/Core/UUID.h>

#include <random>

#include <unordered_map>

_WHIP_START

static std::random_device s_RandomDevice;
static std::mt19937_64 s_Engine(s_RandomDevice());
static std::mt19937 s_Engine32(s_RandomDevice());
static std::uniform_int_distribution<uint64_t> s_UniformDistribution;
static std::uniform_int_distribution<uint32_t> s_UniformDistribution32;

UUID::UUID() : m_UUID(s_UniformDistribution(s_Engine)) {}

UUID::UUID(uint64_t uuid) : m_UUID(uuid) {}

UUID32::UUID32() : m_UUID(s_UniformDistribution32(s_Engine32)) {}

UUID32::UUID32(uint32_t uuid) : m_UUID(uuid) {}

_WHIP_END
