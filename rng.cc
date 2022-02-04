#pragma once

namespace KERF_NAMESPACE
{

#pragma mark -


void RNG::dev_urandom_bits_fill(void* buf, size_t buflen)
{
  // POP when we don't need to read from device, we can read from our seeded pseudorandom generator instead
  I f=open("/dev/urandom",0);
  I r=read(f,buf,buflen);
  if(!r)
  {
    kerf_exit(EX_OSERR);
  }
  close(f);
}

I RNG::dev_urandom_bits_64()
{
  I s; 
  dev_urandom_bits_fill(&s,sizeof(s));
  return s;
}

I4 RNG::dev_urandom_bits_128()
{
  I4 s; 
  dev_urandom_bits_fill(&s,sizeof(s));
  return s;
}

void RNG::seed_randomized()
{
  pcg.seed(dev_urandom_bits_128(), dev_urandom_bits_128());
}

void RNG::seed_fixed(I seed)
{
  pcg.seed(std::seed_seq{seed, seed});
}

void RNG::seed_fixed_all_rng(I seed)
{
  DO(KERF_MAX_NORMALIZABLE_THREAD_COUNT, The_Thread_RNGs[i].seed_fixed(seed + i))
}

RNG::RNG()
{
  if(RNG_USES_RANDOMIZED_SEED_ON_INIT && !DEBUG) 
  {
    seed_randomized();
  }
  else
  {
    auto i = global_counter++; // seed thread instances in order, simply but differently
    seed_fixed(i);
  }
}

auto RNG::get_my_rng()
{
  // Remark. Even if you made a global threadsafe singleton that returned
  // pseudo-random values, you wouldn't get reproducible results because the
  // threads would contend for it in unreproducible order. If you could control
  // the thread timings (there may be a way to do this but I don't know it
  // currently 2021.07.21) then you could guarantee reproducible multithreaded
  // randomness. 
  return The_Thread_RNGs[kerf_get_cached_normalized_thread_id()];
}


}

