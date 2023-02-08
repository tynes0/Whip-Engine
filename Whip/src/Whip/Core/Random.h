#pragma once

#include <random>
#include <Whip/Core/Core.h>

_WHIP_START

class WHIP_API random
{
private:
	template <typename T>
	WHP_NODISCARD inline static T int_type_randomizer(T min_val, T max_val);
	template <typename T>
	WHP_NODISCARD inline static T real_type_randomizer(T min_val, T max_val);
public:
	WHP_NODISCARD static int random_in_range_int(int min_val, int max_val);
	WHP_NODISCARD static short random_in_range_short(short min_val, short max_val);
	WHP_NODISCARD static long long random_in_range_ll(long long min_val, long long max_val);
	WHP_NODISCARD static float random_in_range_float(float min_val, float max_val);
	WHP_NODISCARD static double random_in_range_double(double min_val, double max_val);
	WHP_NODISCARD static int random_int();
	WHP_NODISCARD static short random_short();
	WHP_NODISCARD static long long random_ll();
	WHP_NODISCARD static float random_float();
	WHP_NODISCARD static double random_double();
	WHP_NODISCARD static int random_pos_int();
	WHP_NODISCARD static short random_pos_short();
	WHP_NODISCARD static long long random_pos_ll();
	WHP_NODISCARD static float random_pos_float();
	WHP_NODISCARD static double random_pos_double();
	WHP_NODISCARD static int random_neg_int();
	WHP_NODISCARD static short random_neg_short();
	WHP_NODISCARD static long long random_neg_ll();
	WHP_NODISCARD static float random_neg_float();
	WHP_NODISCARD static double random_neg_double();
};

template<typename T>
WHP_NODISCARD inline T random::int_type_randomizer(T min_val, T max_val)
{
	if (min_val > max_val) { return 0; }
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<T> distribute(min_val, max_val);
	return distribute(gen);
}

template<typename T>
WHP_NODISCARD inline T random::real_type_randomizer(T min_val, T max_val)
{
	if (min_val > max_val) { return 0; }
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<T> distribute(min_val, max_val);
	return distribute(gen);
}


_WHIP_END