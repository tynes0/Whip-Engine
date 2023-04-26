#pragma once

#include <random>
#include <Whip/Core/Core.h>
#include <Whip/Core/Utility.h>

_WHIP_START

class random
{
	using generator		= std::mt19937;
	using rand_dev		= std::random_device;
	template <class T>
	using uni_int_dis	= std::uniform_int_distribution<T>;
	template <class T>
	using uni_real_dis	= std::uniform_real_distribution<T>;
public:
	template <class T, enable_if_t<std::is_arithmetic_v<T>, int> = 0>
	static T random_in_range(T min, T max) noexcept
	{
		return distribution<T>(min, max);
	}

	static int					rint(int const min = INT_MIN, int const max = INT_MAX) noexcept;
	static short				rshort(short const min = SHRT_MIN, short const max = SHRT_MAX) noexcept;
	static long					rlong(long const min = LONG_MIN, long const max = LONG_MAX) noexcept;
	static long long			rlong_long(long long const min = LLONG_MIN, long long const max = LLONG_MAX) noexcept;
	static unsigned int			runsigned_int(unsigned int const min = 0, unsigned int const max = UINT_MAX) noexcept;
	static unsigned short		runsigned_short(unsigned short const min = 0, unsigned short const max = USHRT_MAX) noexcept;
	static unsigned long		runsigned_long(unsigned long const min = 0, unsigned long const max = ULONG_MAX) noexcept;
	static unsigned long long	runsigned_long_long(unsigned long long const min = 0, unsigned long long const max = ULLONG_MAX) noexcept;
	static float				rfloat(float const min = -FLT_MAX, float const max = FLT_MAX) noexcept;
	static double				rdouble(double const min = -DBL_MAX, double const max = DBL_MAX) noexcept;
	static long double			rlong_double(long double const min = -LDBL_MAX, long double const max = LDBL_MAX) noexcept;

private:
	static generator init() noexcept
	{
		rand_dev rd;
		is_initialized = true;
		return generator(rd());
	}

	template <class T, enable_if_t<std::is_arithmetic_v<T>, int> = 0>
	static T distribution(T min, T max)
	{
		if (!is_initialized)
		{
			init();
		}

		if (min > max)
		{
			swap(min, max);
		}
		
		if constexpr (std::is_integral_v<T>)
		{
			uni_int_dis dis(min, max);
			return dis(m_gen);
		}

		if constexpr (std::is_floating_point_v<T>)
		{
			uni_real_dis dis(min, max);
			return dis(m_gen);
		}

		return 0;
	}

private:
	static generator m_gen;
	static bool is_initialized;
};

_WHIP_END