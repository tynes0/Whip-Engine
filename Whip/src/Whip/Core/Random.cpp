#include "whippch.h"
#include "Random.h"

_WHIP_START

bool random::is_initialized = false;

random::generator random::m_gen = random::init();

int random::rint(int const min, int const max) noexcept
{
	return distribution(min, max);
}

short random::rshort(short const min, short const max) noexcept
{
	return distribution(min, max);
}

long random::rlong(long const min, long const max) noexcept
{
	return distribution(min, max);
}

long long random::rlong_long(long long const min, long long const max) noexcept
{
	return distribution(min, max);
}

unsigned int random::runsigned_int(unsigned int const min, unsigned int const max) noexcept
{
	return distribution(min, max);
}

unsigned short random::runsigned_short(unsigned short const min, unsigned short const max) noexcept
{
	return distribution(min, max);
}

unsigned long random::runsigned_long(unsigned long const min, unsigned long const max) noexcept
{
	return distribution(min, max);
}

unsigned long long random::runsigned_long_long(unsigned long long const min, unsigned long long const max) noexcept
{
	return distribution(min, max);
}

float random::rfloat(float const min, float const max) noexcept
{
	return distribution(min, max);
}

double random::rdouble(double const min, double const max) noexcept
{
	return distribution(min, max);
}

long double random::rlong_double(long double const min, long double const max) noexcept
{
	return distribution(min, max);
}

_WHIP_END