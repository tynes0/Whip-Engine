#pragma once

#ifndef _WHIP_PANDOM_
#define _WHIP_PANDOM_

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>

#include "Utility.h"
#include "TypeTraits.h"
#include "Array.h"

#include <cstdint>

#ifdef min
#undef min
#endif // min
#ifdef max
#undef max
#endif // max

#ifdef WHP_PLATFORM_WINDOWS
	#ifdef _WIN64
		#define _WHIP_MT_UNROLL_MORE
	#endif // _WIN64
#else // !WHP_PLATFORM_WINDOWS
// empty here
#endif // WHP_PLATFORM_WINDOWS

#pragma warning(push)
#pragma warning(disable : _WHP_DISABLED_WARNINGS)

_WHIP_START

namespace detail_random
{
	static unsigned int* wrand_seed();
	static unsigned int* xrand_seed();
}

#define wrseed (*_WHIP detail_random::wrand_seed())
#define xrseed (*_WHIP detail_random::xrand_seed())

void swrand(unsigned int seed);
void sxrand(unsigned int seed);

unsigned int wrand(unsigned int seed);
unsigned int xrand(unsigned int seed);

WHP_CONSTEXPR float float_from_bits(const uint32_t i) noexcept;
WHP_CONSTEXPR double double_from_bits(const uint64_t i) noexcept;

namespace detail_random
{
	struct random_base {};

	WHP_INLINE constexpr uint64_t default_seed = 1234567890ull;

	namespace mt
	{
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
			uint32_t mt[MTconstants::size]{};
			uint32_t mt_temperred[MTconstants::size]{};
			size_t index = MTconstants::size;
		};
	}
}

template <class _Rng>
struct is_rng : bool_constant<is_base_of_v<detail_random::random_base, _Rng>> {};

template <class _Rng>
WHP_INLINE constexpr bool is_rng_v = is_rng<_Rng>::value;

class random_device
{
public:
	using result_type = unsigned int;
	random_device() {}	
	WHP_NODISCARD unsigned int operator()();
};

class mersenne_twister : private detail_random::random_base
{
public:
	using result_type = unsigned int;
	mersenne_twister(uint32_t value);
	mersenne_twister(random_device rand_device);
	void seed(uint32_t value);

	WHP_NODISCARD static constexpr result_type(min)() { return 0; }
	WHP_NODISCARD static constexpr result_type(max)() { return detail_random::mt::MTconstants::wmsk; }
 
	WHP_NODISCARD uint32_t rand_u32();
	WHP_NODISCARD uint32_t operator()();
private:
	void generate_numbers() noexcept;

	detail_random::mt::MTstate state;
};

class split_mix64 : private detail_random::random_base
{
public:
	using state_type	= uint64_t;
	using result_type	= uint64_t;

	WHP_NODISCARD20 explicit WHP_CONSTEXPR split_mix64(state_type state = detail_random::default_seed) noexcept;
	WHP_CONSTEXPR result_type operator()() noexcept;
	WHP_NODISCARD WHP_CONSTEXPR state_type get_state() const noexcept;
	WHP_CONSTEXPR void set_state(state_type state) noexcept;
	WHP_NODISCARD bool operator==(const split_mix64& right) const noexcept;
	WHP_NODISCARD bool operator!=(const split_mix64& right) const noexcept;

	WHP_NODISCARD static WHP_CONSTEXPR result_type(min)() { return std::numeric_limits<result_type>::lowest(); }
	WHP_NODISCARD static WHP_CONSTEXPR result_type(max)() { return (std::numeric_limits<result_type>::max)(); }

	template <size_t N>
	WHP_NODISCARD WHP_CONSTEXPR whip::array<uint64_t, N> generate_seed_sequence() noexcept
	{
		whip::array<uint64_t, N> seeds = {};
		for (auto& seed : seeds)
			seed = operator()();
		return seeds;
	}
private:
	state_type m_state;
};

#if !_WHP_HAS_CPP_VERSION(20)
_EMIT_WHP_WARNING(WHP0006, "Xoshiro and xoroshiro classes are available only with C++20 or later.");
#else //_WHP_HAS_CPP_VERSION(20)

class xoshiro256plus : private detail_random::random_base
{
public:
	using state_type = whip::array<uint64_t, 4>;
	using result_type = uint64_t;

	WHP_NODISCARD20 explicit WHP_CONSTEXPR xoshiro256plus(uint64_t seed = detail_random::default_seed) noexcept;
	WHP_NODISCARD20 explicit WHP_CONSTEXPR xoshiro256plus(state_type state) noexcept;

	WHP_CONSTEXPR result_type operator()() noexcept;
	WHP_CONSTEXPR void jump() noexcept;
	WHP_CONSTEXPR void long_jump() noexcept;
	WHP_NODISCARD WHP_CONSTEXPR state_type get_state() const noexcept;
	WHP_CONSTEXPR void set_state(const state_type& state) noexcept;
	WHP_NODISCARD bool operator==(const xoshiro256plus& right) const noexcept;
	WHP_NODISCARD bool operator!=(const xoshiro256plus& right) const noexcept;

	WHP_NODISCARD static WHP_CONSTEXPR result_type(min)() { return std::numeric_limits<result_type>::lowest(); }
	WHP_NODISCARD static WHP_CONSTEXPR result_type(max)() { return (std::numeric_limits<result_type>::max)(); }
private:
	state_type m_state;
};

#endif // _WHP_HAS_CPP_VERSION(20)

namespace detail_random
{
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
		constexpr auto gen_min = static_cast<_Real>((_Gen::min)());
		constexpr auto gen_max = static_cast<_Real>((_Gen::max)());
		constexpr auto range_x = gen_max - gen_min + _Real{ 1 };

		constexpr int gci = _WHIP detail_random::generate_canonical_iterations(minbits, (_Gen::min)(), (_Gen::max)());

		_Real ans{ 0 };
		_Real factor{ 1 };

		for (int i = 0; i < gci; ++i)
		{
			ans += (static_cast<_Real>(generator()) - gen_min) * factor;
			factor *= range_x;
		}

		return ans / factor;
	}

	template <class _Real, class _Gen>
	WHP_NODISCARD _Real nrand(_Gen& generator)
	{
		constexpr auto digits = static_cast<size_t>(std::numeric_limits<_Real>::digits);
		constexpr auto bits = ~size_t{ 0 };
		constexpr auto minbits = digits < bits ? digits : bits;
		return _WHIP detail_random::generate_canonical<_Real, minbits>(generator);
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
			WHP_CORE_ASSERT(min0 <= max0 && (0 <= min0 || max0 <= min0 + (std::numeric_limits<result_type>::max)()), "invalid min and max arguments for whip uniform");

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

}

template <class _Ty = double>
class uniform_real_distribution : public detail_random::uniform_base<_Ty>
{
public:
	static_assert(is_any_of_v<_Ty, double, float, long double>, "whip::uniform_real_distribution type must be double, long double or float.");

	using result_type = _Ty;
	using param_type = detail_random::uniform_param_type<uniform_real_distribution<_Ty>>;

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
		return detail_random::nrand<_Ty>(engine) * (par0.max - par0.min) + par0.min;
	}

	detail_random::uniform_param_type<uniform_real_distribution<_Ty>> param;
};

_WHIP_END

#pragma warning(pop)

#endif // !_WHIP_PANDOM_