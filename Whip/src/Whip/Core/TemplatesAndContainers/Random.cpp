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

unsigned int xrand(unsigned int seed) {
    xrseed ^= (xrseed << 13);
    xrseed ^= (xrseed >> 17);
    xrseed ^= (xrseed << 5);
    xrseed += seed;
    return xrseed;
}

#define MT_M32(x) (0x80000000 & x)
#define MT_L32(x) (0x7FFFFFFF & x)
#define UNROLL(st, x, i, i1, i2, i3, i4)    x = MT_M32(st.mt[i1]) | MT_L32(st.mt[i2]);                                              \
                                            st.mt[i3] = state.mt[i4] ^ (x >> 1) ^ (((int32_t(x) << 31) >> 31) & MTconstants::magic);\
                                            ++i;

mersenne_twister::mersenne_twister(uint32_t value)
{
    seed(value);
}

mersenne_twister::mersenne_twister(random_device rand_device)
{
    seed(rand_device());
}

void mersenne_twister::seed(uint32_t value)
{
    state.mt[0] = value;
    state.index = MTconstants::size;
    for (uint_fast32_t i = 1; i < MTconstants::size; ++i)
        state.mt[i] = 0x6c078965 * (state.mt[i - 1] ^ state.mt[i - 1] >> 30) + i;
}

uint32_t mersenne_twister::rand_u32()
{
    if (state.index == MTconstants::size)
    {
        generate_numbers();
        state.index = 0;
    }
    return state.mt_temperred[state.index++];
}

uint32_t mersenne_twister::operator()()
{
    return rand_u32();
}

void mersenne_twister::generate_numbers() noexcept
{
    size_t i = 0;
    uint32_t x = 0;
    
    while (i < MTconstants::diff)
    {
        UNROLL(state, x, i, i, i + 1, i, i + MTconstants::period);
#ifdef _WHIP_MT_UNROLL_MORE
        UNROLL(state, x, i, i, i + 1, i, i + MTconstants::period);
#endif // _WHIP_MT_UNROLL_MORE
    }
    while (i < MTconstants::size - 1)
    {
        UNROLL(state, x, i, i, i + 1, i, i - MTconstants::diff);
#ifdef _WHIP_MT_UNROLL_MORE
        UNROLL(state, x, i, i, i + 1, i, i - MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - MTconstants::diff);
#endif // _WHIP_MT_UNROLL_MORE
    }
    UNROLL(state, x, i, MTconstants::size - 1, 0, MTconstants::size - 1, MTconstants::period);
    
    for (size_t j = 0; j < MTconstants::size; ++j)
    {
        x = state.mt[j];
        x ^= x >> 11;
        x ^= x << 7 & 0x9d2c5680;
        x ^= x << 15 & 0xefc60000;
        x ^= x >> 18;
        state.mt_temperred[j] = x;
    }
    state.index = 0;
}

#undef MT_M32
#undef MT_L32
#undef UNROLL

_WHIP_END