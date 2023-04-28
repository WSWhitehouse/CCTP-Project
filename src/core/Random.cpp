#include "core/Random.hpp"

#include <random>

#include "core/Logging.hpp"
#include "core/Assert.hpp"

static thread_local std::random_device device = {};
static thread_local std::mt19937 generator    = {};
static thread_local b8 initialised            = false;

#define INIT_RANDOM()                      \
  if (!initialised)                        \
  {                                        \
     initialised = true;                   \
     generator   = std::mt19937{device()}; \
  }

f32 Random::Range(f32 min, f32 max)
{
  INIT_RANDOM()

  const std::uniform_real_distribution<f32> dist(min, max);
  return dist(generator);
}

i32 Random::Range(i32 min, i32 max)
{
  INIT_RANDOM()

  const std::uniform_int_distribution<i32> dist(min, max);
  return dist(generator);
}

u32 Random::Range(u32 min, u32 max)
{
  INIT_RANDOM()

  const std::uniform_int_distribution<u32> dist(min, max);
  return dist(generator);
}

b8 Random::Bool(f32 truePercent)
{
  ASSERT_MSG(truePercent > -F32_EPSILON,       "The true percentage must be between 0.0f and 1.0f. The value is below 0!");
  ASSERT_MSG(truePercent < 1.0f + F32_EPSILON, "The true percentage must be between 0.0f and 1.0f. The value is above 1!");

  const std::bernoulli_distribution dist(truePercent);
  return dist(generator);
}