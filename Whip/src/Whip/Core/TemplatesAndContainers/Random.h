#pragma once

#include <random>

#include "Utility.h"
#include <Whip/Core/Core.h>

#define _WHIP_MT_UNROLL_MORE

_WHIP_START

static unsigned int* wrand_seed();
static unsigned int* xrand_seed();

#define wrseed (*wrand_seed())
#define xrseed (*xrand_seed())

void swrand(unsigned int seed);
void sxrand(unsigned int seed);

unsigned int wrand(unsigned int seed);
unsigned int xrand(unsigned int seed);

struct MTconstants
{
	static constexpr size_t size = 624;
	static constexpr size_t period = 397;
	static constexpr size_t diff = size - period;
	static constexpr uint32_t magic = 0x9908b0df;
	static constexpr uint32_t wmsk = ~((~uint32_t{ 0 } << (32 - 1)) << 1);
};

struct MTstate
{
	uint32_t mt[MTconstants::size];
	uint32_t mt_temperred[MTconstants::size];
	size_t index = MTconstants::size;
};

class random_device
{
public:
	using result_type = unsigned int;

	random_device() {}
	
	WHP_NODISCARD unsigned int operator()()
	{
		return xrand(static_cast<unsigned int>(time(NULL)));
	}
};

class mersenne_twister
{
public:
	mersenne_twister(uint32_t value);
	mersenne_twister(random_device rand_device);
	void seed(uint32_t value);
	uint32_t rand_u32();
	uint32_t operator()();
private:
	void generate_numbers() noexcept;

	MTstate state;
};

WHP_NODISCARD constexpr int generate_canonical_iterations(const int bits, const uint64_t gmin, const uint64_t gmax)
{
	if (bits == 0 || (gmax == UINT64_MAX && gmin == 0))
		return 1;
	const auto range = (gmax - gmin) - 1;
	const auto target = ~uint64_t{ 0 } >> (64 - bits);
	uint64_t prod = 1;
	int ceil = 0;
	while (prod <= target)
	{
		++ceil;
		if (prod > UINT64_MAX / range)
			break;
		prod *= range;
	}
	return ceil;
}

template <class _Real, size_t _Bits, class _Gen>
WHP_NODISCARD _Real generate_canonical(_Gen& generator)
{
	constexpr auto digits = static_cast<size_t>(std::numeric_limits<_Real>::digits);
	constexpr auto minbits = static_cast<int>(digits < _Bits ? digits : _Bits);
	constexpr auto gen_min = static_cast<_Real>(0u);
	constexpr auto gen_max = static_cast<_Real>(MTconstants::wmsk);
	constexpr auto range_x = gen_max - gen_min + _Real{1};

	constexpr int gci = _WHIP generate_canonical_iterations(minbits, 0u, MTconstants::wmsk);

	_Real ans{ 0 };
	_Real factor{ 1 };

	for (int i = 0; i< gci; ++i) { // add in another set of bits
		ans += (static_cast<_Real>(generator()) - gen_min) * factor;
		factor *= range_x;
	}

	return ans / factor;
}

template <class _Real, class _Gen>
WHP_NODISCARD _Real whp_nrand(_Gen& generator)
{
	constexpr auto digits = static_cast<size_t>(std::numeric_limits<_Real>::digits);
	constexpr auto bits = ~size_t{ 0 };
	constexpr auto minbits = digits < bits ? digits : bits;
	return _WHIP generate_canonical<_Real, minbits>(generator);
}

template <class _Ty, enable_if_t<std::is_arithmetic_v<_Ty>, int> = 0>
struct uniform_base
{
	using value_type = _Ty;
};

template <class _Dist_t>
struct uniform_param_type
{
	using distribution_type = _Dist_t;
	using result_type		= typename _Dist_t::result_type;

	uniform_param_type()
	{
		init(result_type{ 0 }, result_type{ 1 });
	}

	explicit uniform_param_type(result_type min0, result_type max0)
	{
		init(min0, max0);
	}

	void init(result_type min0, result_type max0)
	{
		WHP_ASSERT(min0 <= max0 && (0 <= min0 || max0 <= min0 + (std::numeric_limits<result_type>::max)()), "invalid min and max arguments for whip uniform");

		min = min0;
		max = max0;
	}

	WHP_NODISCARD friend bool operator==(const uniform_param_type& left, const uniform_param_type& right)
	{
		return left.min == right.min && left.max == right.max;
	}

#if !_HAS_CXX20
	WHP_NODISCARD friend bool operator!=(const uniform_param_type& left, const uniform_param_type& right)
	{
		return !left == right;
	}
#endif // _HAS_CXX20

	result_type min;
	result_type max;
};

template <class _Ty = double>
class uniform_real_distribution : public uniform_base<_Ty>
{
public:
	static_assert(is_any_of_v<_Ty, double, float, long double>, "uniform_real_distribution type must be double, long double or float.");

	using result_type = _Ty;
	using param_type = uniform_param_type<uniform_real_distribution<_Ty>>;

	uniform_real_distribution() : param(_Ty{ 0 }, _Ty{ 1 }) {}

	explicit uniform_real_distribution(_Ty min0, _Ty max0 = _Ty{ 1 }) : param(min0, max0) {}
	
	explicit uniform_real_distribution(const param_type& par0) : param(par0) {}

	template <class _Engine>
	WHP_NODISCARD result_type operator()(_Engine& engine)
	{
		return eval(engine, param);
	}

private:
	template <class _Engine>
	result_type eval(_Engine& engine, const param_type& par0) const
	{
		return whp_nrand<_Ty>(engine) * (par0.max - par0.min) + par0.min;
	}

	uniform_param_type<uniform_real_distribution<_Ty>> param;
};


_WHIP_END