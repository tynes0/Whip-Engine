#include "whippch.h"
#include "Random.h"

_WHIP_START

static unsigned int* detail_random::wrand_seed()
{
    static unsigned int m_seed = 1;
    return &m_seed;
}

static unsigned int* detail_random::xrand_seed()
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

constexpr float float_from_bits(const uint32_t i) noexcept
{
    return (i >> 8) * 0x1.0p-24f;
}

constexpr double double_from_bits(const uint64_t i) noexcept
{
    return (i >> 11) * 0x1.0p-53;
}

#define MT_M32(x) (0x80000000 & x)
#define MT_L32(x) (0x7FFFFFFF & x)
#define UNROLL(st, x, i, i1, i2, i3, i4)    x = MT_M32(st.mt[i1]) | MT_L32(st.mt[i2]);                                              \
                                            st.mt[i3] = state.mt[i4] ^ (x >> 1) ^ (((int32_t(x) << 31) >> 31) & detail_random::mt::MTconstants::magic);\
                                            ++i;

unsigned int random_device::operator()()
{
    return xrand(static_cast<unsigned int>(time(NULL)));
}

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
    state.index = detail_random::mt::MTconstants::size;
    for (uint_fast32_t i = 1; i < detail_random::mt::MTconstants::size; ++i)
        state.mt[i] = 0x6c078965 * (state.mt[i - 1] ^ state.mt[i - 1] >> 30) + i;
}

uint32_t mersenne_twister::rand_u32()
{
    if (state.index == detail_random::mt::MTconstants::size)
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
    
    while (i < detail_random::mt::MTconstants::diff)
    {
        UNROLL(state, x, i, i, i + 1, i, i + detail_random::mt::MTconstants::period);
#ifdef _WHIP_MT_UNROLL_MORE
        UNROLL(state, x, i, i, i + 1, i, i + detail_random::mt::MTconstants::period);
#endif // _WHIP_MT_UNROLL_MORE
    }
    while (i < detail_random::mt::MTconstants::size - 1)
    {
        UNROLL(state, x, i, i, i + 1, i, i - detail_random::mt::MTconstants::diff);
#ifdef _WHIP_MT_UNROLL_MORE
        UNROLL(state, x, i, i, i + 1, i, i - detail_random::mt::MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - detail_random::mt::MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - detail_random::mt::MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - detail_random::mt::MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - detail_random::mt::MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - detail_random::mt::MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - detail_random::mt::MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - detail_random::mt::MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - detail_random::mt::MTconstants::diff);
        UNROLL(state, x, i, i, i + 1, i, i - detail_random::mt::MTconstants::diff);
#endif // _WHIP_MT_UNROLL_MORE
    }
    UNROLL(state, x, i, detail_random::mt::MTconstants::size - 1, 0, detail_random::mt::MTconstants::size - 1, detail_random::mt::MTconstants::period);
    
    for (size_t j = 0; j < detail_random::mt::MTconstants::size; ++j)
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

#define ROTL64(x, s) ((x << s) | (x >> (64 - s)))
#define ROTL32(x, s) ((x << s) | (x >> (32 - s)))

constexpr split_mix64::split_mix64(state_type state) noexcept : m_state(state) {}

constexpr split_mix64::result_type split_mix64::operator()() noexcept
{
    std::uint64_t z = (m_state += 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

constexpr split_mix64::state_type split_mix64::get_state() const noexcept
{
    return m_state;
}

constexpr void split_mix64::set_state(state_type state) noexcept
{
    m_state = state;
}

bool split_mix64::operator==(const split_mix64& right) const noexcept
{
    return m_state == right.m_state;
}

bool split_mix64::operator!=(const split_mix64& right) const noexcept
{
    return m_state != right.m_state;
}


#if _WHP_HAS_CPP_VERSION(20)

constexpr xoshiro256plus::xoshiro256plus(uint64_t seed) noexcept : m_state(split_mix64{seed}.generate_seed_sequence<4>()) {}

constexpr xoshiro256plus::xoshiro256plus(state_type state) noexcept :m_state(state) {}

constexpr xoshiro256plus::result_type xoshiro256plus::operator()() noexcept
{
    const uint64_t result = m_state[0] + m_state[3];
    const uint64_t t = m_state[1] << 17;
    m_state[2] ^= m_state[0];
    m_state[3] ^= m_state[1];
    m_state[1] ^= m_state[2];
    m_state[0] ^= m_state[3];
    m_state[2] ^= t;
    m_state[3] = ROTL64(m_state[3], 45);
    return result;
}

constexpr void xoshiro256plus::jump() noexcept
{
    constexpr whip::array<uint64_t, 4> jump_arr = { 0x180ec6d33cfd0aba, 0xd5a61266f0c9392c, 0xa9582618e03fc9aa, 0x39abdc4529b1661c };
    whip::array<uint64_t, 4> s = { 0, 0, 0, 0 };

    for (uint64_t item : jump_arr)
    {
        for (int b = 0; b < 64; ++b)
        {
            if (item & UINT64_C(1) << b)
            {
                s[0] ^= m_state[0];
                s[1] ^= m_state[1];
                s[2] ^= m_state[2];
                s[3] ^= m_state[3];
            }
            operator()();
        }
    }
    m_state[0] = s[0];
    m_state[1] = s[1];
    m_state[2] = s[2];
    m_state[3] = s[3];
}

constexpr void xoshiro256plus::long_jump() noexcept
{
}

#endif // _WHP_HAS_CPP_VERSION(20)

#undef ROTL64
#undef ROTL32

_WHIP_END

