#include "whippch.h"
#include "Random.h"

_WHIP_START

WHP_NODISCARD inline int random::random_in_range_int(int min_val, int max_val)
{
	return int_type_randomizer<int>(min_val, max_val);
}

WHP_NODISCARD inline short random::random_in_range_short(short min_val, short max_val)
{
	return int_type_randomizer<short>(min_val, max_val);;
}

WHP_NODISCARD long long random::random_in_range_ll(long long min_val, long long max_val)
{
	return int_type_randomizer<long long>(min_val, max_val);;
}

WHP_NODISCARD float random::random_in_range_float(float min_val, float max_val)
{
	return real_type_randomizer<float>(min_val, max_val);
}

WHP_NODISCARD double random::random_in_range_double(double min_val, double max_val)
{
	return real_type_randomizer<double>(min_val, max_val);
}

WHP_NODISCARD int random::random_int()
{
	return int_type_randomizer<int>(INT_MIN, INT_MAX);
}

WHP_NODISCARD short random::random_short()
{
	return int_type_randomizer<short>(SHRT_MIN, SHRT_MAX);
}

WHP_NODISCARD long long random::random_ll()
{
	return int_type_randomizer<long long>(LLONG_MIN, LLONG_MAX);
}

WHP_NODISCARD float random::random_float()
{
	return real_type_randomizer<float>(FLT_MIN, FLT_MAX);
}

WHP_NODISCARD double random::random_double()
{
	return real_type_randomizer<double>(DBL_MIN, DBL_MAX);
}

WHP_NODISCARD int random::random_pos_int()
{
	return int_type_randomizer<int>(0, INT_MAX);
}

WHP_NODISCARD short random::random_pos_short()
{
	return int_type_randomizer<short>(0, SHRT_MAX);
}

WHP_NODISCARD long long random::random_pos_ll()
{
	return int_type_randomizer<long long>(0, LLONG_MAX);
}

WHP_NODISCARD float random::random_pos_float()
{
	return real_type_randomizer<float>(0.0f, FLT_MAX);
}

WHP_NODISCARD double random::random_pos_double()
{
	return real_type_randomizer<double>(0.0, DBL_MAX);
}

WHP_NODISCARD int random::random_neg_int()
{
	return int_type_randomizer<int>(INT_MIN, 0);
}

WHP_NODISCARD short random::random_neg_short()
{
	return int_type_randomizer<short>(SHRT_MIN, 0);
}

WHP_NODISCARD long long random::random_neg_ll()
{
	return int_type_randomizer<long long>(LLONG_MIN, 0);
}

WHP_NODISCARD float random::random_neg_float()
{
	return real_type_randomizer<float>(FLT_MIN, 0);
}

WHP_NODISCARD double random::random_neg_double()
{
	return real_type_randomizer<double>(DBL_MIN, 0);
}

_WHIP_END