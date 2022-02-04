#pragma once

namespace KERF_NAMESPACE
{

struct RNG
{
public:
  static I  global_counter;
  static void dev_urandom_bits_fill(void* buf, size_t buflen);
  static I dev_urandom_bits_64();
  static I4 dev_urandom_bits_128();
  static auto get_my_rng();

  // POP pcg64_oneseq or pcg_unique or perhaps pcg64_fast
  // POP pcg32 (and smaller?) generators with smaller period
  // POP instead of say squishing one 64-bit value into one char, you get 8 chars. and so on.
  pcg64 pcg;

  void seed_randomized();
  void seed_fixed(I seed);
  static void seed_fixed_all_rng(I seed); // for, e.g., a command that sets all rngs to fixed seed

  // Feature. Currently we generate a random 64-bit integer *not* on an arbitrary interval. In Kerf1 we would produce a value inside an interval by multiplying the bound by a random float in [0,1). This is "okay" but you could maybe get an improvement [i'm guessing statistical but not performance] using https://github.com/apple/swift/pull/39143 see extensions to Lemire's work if the link goes dead, regarding unbiased optimal integer sampling on an interval

  UI random_UI(){return pcg();}
  I random_I(){return random_UI();} // well-defined as of C++20
  // generates a random number on [0,1)-real-interval, 53-bit precision
  F random_F_unit_interval(){return (random_UI() >> 11) * (1.0/9007199254740992.0);}

  RNG();
  ~RNG() = default;
  RNG(const RNG &) = default;
};

I RNG::global_counter;

RNG* The_Thread_RNGs = new RNG[KERF_MAX_NORMALIZABLE_THREAD_COUNT];

} // namespace
