#include "whippch.h"
#include "Random.h"

#include <ctime>

_WHIP_START

static unsigned int* wrand_seed()
{
    static unsigned int m_seed = 1;
    return &m_seed;
}

static unsigned int* xrand_seed()
{
    static unsigned int m_seed = 1;
    return &m_seed;
}

void swrand(unsigned int seed)
{
    wrseed = seed;
}

void sxrand(unsigned int seed)
{
    xrseed = seed;
}

unsigned int wrand(unsigned int seed)
{
    wrseed = wrseed * seed;
    wrseed = (1103515245 * wrseed + 12345) & 0x7FFFFFFF;
    return wrseed;
}

unsigned int xrand(unsigned int seed) 
{
    xrseed ^= (xrseed << 13);
    xrseed ^= (xrseed >> 17);
    xrseed ^= (xrseed << 5);
    xrseed += seed;
    return xrseed;
}

int random::random_int()
{
    return static_cast<int>(wrand(static_cast<unsigned int>(time(NULL))));
}

std::mt19937& random::get_generator()
{
    static std::mt19937 generator{ wrand(static_cast<unsigned int>(time(NULL))) };
    return generator;
}

_WHIP_END
