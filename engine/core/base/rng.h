// Deterministic PCG32 random generator. All gameplay randomness must come
// from the Battle's Rng so battles replay identically on every platform
// (no std::rand, no Lua math.random).
#pragma once
#include <cstdint>

namespace ote {

struct Rng {
  uint64_t state = 0x853c49e6748fea9bULL;
  uint64_t inc = 0xda3e39cb94b95bdbULL;

  void seed(uint64_t s) {
    state = 0;
    inc = (s << 1u) | 1u;
    next();
    state += 0x9e3779b97f4a7c15ULL ^ s;
    next();
  }
  uint32_t next() {
    uint64_t old = state;
    state = old * 6364136223846793005ULL + inc;
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
  }
  // Uniform integer in [0, n). n must be > 0.
  int range(int n) { return (int)(next() % (uint32_t)n); }
  // True with the given percent chance (<=0 never, >=100 always).
  bool chance(int percent) { return range(100) < percent; }
};

}  // namespace ote
