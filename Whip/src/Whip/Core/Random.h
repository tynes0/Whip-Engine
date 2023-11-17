#pragma once

#include <random>

#include "Utility.h"
#include <Whip/Core/Core.h>
#include <Whip/Core/Utility.h>

_WHIP_START

static unsigned int* wrand_seed();
static unsigned int* xrand_seed();

#define wrseed (*wrand_seed())
#define xrseed (*xrand_seed())

void swrand(unsigned int seed);
void sxrand(unsigned int seed);
unsigned int wrand(unsigned int seed);
unsigned int xrand(unsigned int seed);

class random
{
public:
	static int random_int();

	// [begin, end]
	template <class _Ty, enable_if_t<std::is_arithmetic_v<_Ty>, int> = 0>
	static _Ty random_in_range(const range<_Ty>& _range)
	{
		range<_Ty> result_range = (*_range.begin() <= *_range.end()) ? range<_Ty>{ *_range.begin(), * _range.end() } : range<_Ty>{ *_range.end(), *_range.begin() };
		if constexpr (std::is_floating_point_v<_Ty>)
		{
			real_type_dist<_Ty> dist(*result_range.begin(), *result_range.end());
			return dist(get_generator());
		}
		else if constexpr (std::is_integral_v<_Ty>)
		{
			int_type_dist<_Ty> dist(*result_range.begin(), *result_range.end());
			return dist(get_generator());
		}
		return static_cast<_Ty>(0);
	}

	// [minimum, maximum]
	template <class _Ty, enable_if_t<std::is_arithmetic_v<_Ty>, int> = 0>
	static _Ty random_in_range(_Ty minimum, _Ty maximum)
	{
		if (minimum > maximum)
			_WHIP swap(minimum, maximum);
		if constexpr (std::is_floating_point_v<_Ty>)
		{
			real_type_dist<_Ty> dist(minimum, maximum);
			return dist(get_generator());
		}
		else if constexpr (std::is_integral_v<_Ty>)
		{
			int_type_dist<_Ty> dist(minimum, maximum);
			return dist(get_generator());
		}
		return static_cast<_Ty>(0);
	}

private:
	template <class _Ty>
	using int_type_dist = std::uniform_int_distribution<_Ty>;
	template <class _Ty>
	using real_type_dist = std::uniform_real_distribution<_Ty>;

	static std::mt19937& get_generator();
};

_WHIP_END

