#include "Random.h"

#include "Random.h"

std::mt19937 random::s_random_engine;
std::uniform_int_distribution<std::mt19937::result_type> random::s_distribution;