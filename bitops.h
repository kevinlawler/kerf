#pragma once 

namespace KERF_NAMESPACE {

#define POW2(x) (1LL<<(x))

// could use std::bit_ceil in c++20, std::bit_floor
constexpr inline char ceiling_log_2(unsigned long long v)
{
#if __has_builtin(__builtin_clzll)
  return ((sizeof(unsigned long long) * 8 - 1) - __builtin_clzll(v)) + (!!(v & (v - 1)));
#else
  return floor_log_2(v) + (!!(v & (v - 1)));
#endif
}

constexpr inline char floor_log_2(unsigned long long v)
{
#if __has_builtin(__builtin_clzll)
  return ((sizeof(unsigned long long) * 8 - 1) - __builtin_clzll(v));
#else

  //POTENTIAL_OPTIMIZATION_POINT
  //try this method? needs easy conversion to DOUBLE not FLOAT
  //int RoundedUpLogTwo(uint64_t input)
  //{
  //    assert(input > 0);
  //    union Float_t num;
  //    num.f = (float)input;
  //    // Increment the representation enough that for any non power-
  //    // of-two (FLT_MIN or greater) we will move to the next exponent.
  //    num.i += 0x7FFFFF;
  //    // Return the exponent after subtracting the bias.
  //    return num.parts.exponent - 127;
  //}
  //
  //
  //
  ///* See
  //https://randomascii.wordpress.com/2012/01/11/tricks-with-the-floating-point-format/
  //for the potential portability problems with the union and bit-fields below.
  //*/
  //union Float_t
  //{
  //    Float_t(float num = 0.0f) : f(num) {}
  //    // Portable extraction of components.
  //    bool Negative() const { return (i >> 31) != 0; }
  //    int32_t RawMantissa() const { return i & ((1 << 23) - 1); }
  //    int32_t RawExponent() const { return (i >> 23) & 0xFF; }
  // 
  //    int32_t i;
  //    float f;
  //#ifdef _DEBUG
  //    struct
  //    {   // Bitfields for exploration. Do not use in production code.
  //        uint32_t mantissa : 23;
  //        uint32_t exponent : 8;
  //        uint32_t sign : 1;
  //    } parts;
  //#endif
  //};


  //POTENTIAL_OPTIMIZATION_POINT
  //do any of these beat dgobbi? http://www.hackersdelight.org/hdcodetxt/nlz.c.txt
  
  //fall back to dgobbi method
  static const unsigned long long t[6] = 
  {
    0xFFFFFFFF00000000ull,
    0x00000000FFFF0000ull,
    0x000000000000FF00ull,
    0x00000000000000F0ull,
    0x000000000000000Cull,
    0x0000000000000002ull
  };

  char y = 0;
  char j = 32;
  char i;

  for (i = 0; i < 6; i++)
  {
    char k = (((v & t[i]) == 0) ? 0 : j);
    y += k;
    v >>= k;
    j >>= 1;
  }

  return y;
#endif
}

constexpr inline char is_power_of_2(I v){return !(v & (v - 1));}
constexpr inline I __attribute__ ((hot))  round_up_nearest_power_of_2(I v){return POW2(ceiling_log_2(v));}
constexpr inline I __attribute__ ((pure)) round_up_nearest_multiple_of_8(I v){I m = 8-1; return (v&~m) + 8*(!!(v&m));} // formerly return (v&m)?8+(v&~m):v;
constexpr inline I round_up_nearest_multiple(I v, I x){I m = x-1; return (v&~m) + x*(!!(v&m));}
constexpr inline I round_up_nearest_multiple_slab_align(I v){return round_up_nearest_multiple(v, SLAB_ALIGN);};

#define BIT_WORD(p,n) (((UC*)(p))[(n)/CHAR_BIT])
#define BIT_MASK(n)   (((uintmax_t)1)<<(BYTE_MSBIT0_TO_LSBIT7_IF_TRUE ?((CHAR_BIT - 1)-((n)%CHAR_BIT)):((n)%CHAR_BIT)))

#define BIT_ASSIGN(p, n, v) (BIT_WORD(p,n)  = (BIT_WORD(p,n) & ~BIT_MASK(n)) | (-((uintmax_t)!!(v)) & BIT_MASK(n)))
#define BIT_SET(   p, n)    (BIT_WORD(p,n) |=  BIT_MASK(n))
#define BIT_CLEAR( p, n)    (BIT_WORD(p,n) &= ~BIT_MASK(n))
#define BIT_FLIP(  p, n)    (BIT_WORD(p,n) ^=  BIT_MASK(n))
#define BIT_GET(   p, n) (!!(BIT_WORD(p,n) &   BIT_MASK(n)))

inline void bitwise_memmove(UC * dest, size_t destBitOffset, UC * src, size_t srcBitOffset, size_t nBits)
{
  // POTENTIAL_OPTIMIZATION_POINT: 100 ways to improve this
  // see for instance, follow pointers from here https://stackoverflow.com/questions/32043911
  // as a general strategy: look for wider-than-bit accesses with shifts. can shore up the ends with utility functions

  bool perfect_overlap = (dest == src && destBitOffset == srcBitOffset);
  if(perfect_overlap) return;

  if(0==destBitOffset && 0==srcBitOffset && 0==(nBits%CHAR_BIT)) {

    memmove(dest, src, nBits/CHAR_BIT);
  }

  bool left_to_right_layout_inclusive = dest < src || (dest == src && destBitOffset <= srcBitOffset);

  // POTENTIAL_OPTIMIZATION_POINT: if there's no overlap you can pick whichever write direction is fastest. can't imagine this is worthwhile given the other available optimizations

  bool write_forward = !left_to_right_layout_inclusive;

  size_t j;
  size_t i;
  int direction;
  UC srcBit;

  if(write_forward) {
    i = 0;
    direction = 1;
  } else {
    i = nBits-1;
    direction = -1;
  }
  
  for (j=0; j<nBits; j++) 
  {
    srcBit = BIT_GET(src, srcBitOffset + i);
    BIT_ASSIGN(dest, destBitOffset + i, srcBit);
    i+=direction;
  }

}

inline void bitwise_memcpy(UC* dest, size_t destBitOffset, const UC * src, size_t srcBitOffset, size_t nBits)
{
  // POTENTIAL_OPTIMIZATION_POINT: 100 ways to improve this

  if(0==destBitOffset && 0==srcBitOffset && 0==(nBits%CHAR_BIT)) {
    memcpy(dest, src, nBits/CHAR_BIT);
  }

  bitwise_memmove(dest, destBitOffset, (UC*)src, srcBitOffset, nBits);
}


}
