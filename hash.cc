#pragma once

namespace KERF_NAMESPACE
{

#pragma mark - Hash Functions

// Fast-hash Copyright 2012 Zilong Tan (eric.zltan@gmail.com) - MIT License
// Compression function for Merkle-Damgard construction.
// This function is generated using the framework provided.
#define ztmix(h) ({          \
      (h) ^= (h) >> 23;    \
      (h) *= 0x2127599bf4325c37ULL;  \
      (h) ^= (h) >> 47; })
uint64_t fasthash64(const void *buf, size_t len, uint64_t seed)
{
  const uint64_t    m = 0x880355f21e6d1965ULL;
  const uint64_t *pos = (const uint64_t *)buf;
  const uint64_t *end = pos + (len / 8);
  const unsigned char *pos2;
  uint64_t h = seed ^ (len * m);
  uint64_t v;

  while (pos != end) {
    v  = *pos++;
    h ^= ztmix(v);
    h *= m;
  }

  pos2 = (const unsigned char*)pos;
  v = 0;

  switch (len & 7) {
  case 7: v ^= (uint64_t)pos2[6] << 48;
  case 6: v ^= (uint64_t)pos2[5] << 40;
  case 5: v ^= (uint64_t)pos2[4] << 32;
  case 4: v ^= (uint64_t)pos2[3] << 24;
  case 3: v ^= (uint64_t)pos2[2] << 16;
  case 2: v ^= (uint64_t)pos2[1] << 8;
  case 1: v ^= (uint64_t)pos2[0];
    h ^= ztmix(v);
    h *= m;
  }

  return ztmix(h);
} 
#undef ztmix

#pragma mark -

void hash_init()
{
  if(HASH_USES_RANDOMIZED_SEED_ON_INIT)
  {
    RNG::dev_urandom_bits_fill(&The_Hash_Key, sizeof(The_Hash_Key));
    return;

    // I *k = (I*)(void *)&The_Hash_Key;
    // DO(key_bytes/sizeof(*k), k[i] = RNG::dev_urandom_bits_64())
  }
  else
  {
    UC *k = (UC*)(void *)&The_Hash_Key;
    DO(sizeof(The_Hash_Key), k[i] = (UC)((i<<4)|i))  //0x0011223344556677...
  }

  //O("Hash key value: ");UC *k=(V)&The_Hash_Key; DO(key_bytes, printf("%02x",k[i])) O("\n");
}

HASH_CPP_TYPE hash_byte_stream(HASH_METHOD_MEMBER method, const void *in, uint64_t length, HASH_CPP_TYPE seed)
{
  switch(method)
  {
    default:
    case HASH_METHOD_UNSPECIFIED:
      uint64_t out = fasthash64(in, length, seed);
      return out;
  }
}

#pragma mark -

HASH_CPP_TYPE PRESENTED_BASE::hash(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed)
{
  auto h = seed;

  if(HASH_AVOID_TYPE_COLLISIONS) //slower, avoid O(1) collisions on type by hashing rep type first
  {
    auto rep = get_representational_type();
    h = hash_byte_stream(method, &rep, sizeof(rep), h);
  }

  return hash_code(method, h);
}

HASH_CPP_TYPE A_UNIT::hash_code(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed)
{
  return hash_byte_stream(method, layout()->payload_pointer_begin(), layout()->payload_bytes_current_used(), seed);
}

HASH_CPP_TYPE A_UNIT_USING_ARRAY_LAYOUT::hash_code(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed)
{
  return hash_byte_stream(method, layout()->payload_pointer_begin(), layout()->payload_bytes_current_used(), seed);
}

HASH_CPP_TYPE A_BIGINT_UNIT::hash_code(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed)
{
  // Rule: When a BIGINT is representable as an int64_t/I3/I, BIGINT will hash as and compare-as (<=>) the corresponding int64_t. When it is not, it will not compare equal to any int64_t (greater than all or less than all) and the hash value is irrelevant, though it can hash on type INT==BIGINT and the payload, including the 0 or 1 for the sign bit if any is in use (you can give it to the hash function as a byte if you need to).
  C c = layout()->header_get_bigint_sign(); 
  seed = hash_byte_stream(method, &c, sizeof(c), seed);
  return hash_byte_stream(method, layout()->payload_pointer_begin(), layout()->payload_bytes_current_used(), seed);
}

HASH_CPP_TYPE A_ARRAY::hash_code(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed)
{
  auto h = seed;
  // POP you can hash int3 vector, float3 vector on just the bytefields of the layout payload (but not other sizes eg int0_vector)
  auto g = [&](const SLOP& x) {h = x.hash(method, h);};
  parent()->iterator_uniplex_presented_subslop(g);
  return h;
}

HASH_CPP_TYPE A_MAP_ABSTRACT::hash_code(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed)
{
  auto h = seed;
  // TODO skip deleted keys, probably needs triplex iterator
  if(HASH_MODS_OUT_DICTIONARY_ORDER)
  {
    auto g = [&](const SLOP& u, const SLOP& v) {h += v.hash(method, u.hash(method, seed));};
    parent()->keys().iterator_duplex_presented_subslop(g, parent()->values());
    return h;
  }

  return parent()->values().hash(method, parent()->keys().hash(method, h));
}

HASH_CPP_TYPE PRESENTED_BASE::hash_code(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed)
{
  die(hash code unimplemented);
}



}

