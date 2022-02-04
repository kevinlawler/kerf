#pragma once

namespace KERF_NAMESPACE
{


// Note. you can do xxhash's microsoft vpkg thing and just do `build` instead of `install` and cherry pick the `.a` and `.h` files. Currently homebrew will (2021.07.20) produce a directory in cellar with the same stuff, but currently no split into debug and release. in vpkg you can go in and use `make` in the makefile directory after tweaking to -Os, it's a 22kb .a binary vs 46kb. There are endless options to tune at compile time including defines, this is a potential optimization point in the future
// Note. moving xxhash compilation to a subdir/makefile is a task that can be partitioned to the project
// Remark: 
// We want our hash to be consistent across platforms and endianness (some hashes differ these days across x64/arm and little/big endian)
// We don't care (currently) about hash DOS attacks, though it's fine to protection as a toggle later (our model is firewalled system)
// We should have hash algo choosable as an option, with the value/version stored on the hashtable object itself, for persistence
// We don't need anything like a cryptographically secure hash (at the moment, though I guess that's fine to add as an option)
// We need 64-bit and 128-bit as an option
// xxhash satisfies these requirements, fasthash does not b/c author won't provide 128-bit
// Remark. 64 bit hash is fine

// Idea. We can use 64-bit hashes until the hashmap reaches a certain size [in terms of # keys], then use 128-bit. Idea. Also, you can cheat on small keys and use a reduced hash length (64-bit or 32-bit or shorter even) and upsample it to 128-bit, without appreciably increasing collisions (because there aren't many small keys, they take up a negligible part of the space). (This would matter for something like cityhash which isn't tuned for small keys.) So you can cheat on 1. #keys 2. size of keys


// Remark. We may or may not want to make this an object method on presented.h

// NB. [1,2,3] won't hash to [1,2,3] if one is int0 and one is int1 unless you account for that
// • hashmap types should specify non-cryptographic hash algo in the header/attribute etc data
// • having a triggered "promotion", like forcing an indexing of a map with a hashtable finally, can happen at an inbetween value like 1million. this is small enough to happen in realtime wo/ anyone noticing, but large enough to make sense. at 1BB or 1TT ppl are going to notice the overhead of the switch.
// • indexes on dictionaries (hashtables) should be optional: default to linear lookup, except until table is large perhaps? possibly implement this promotion-desire as an attribute bit
// • Idea. Object equivalence (similarity comparison) testing should be some form of representational, obviously. If you want to compare differing types...hm...you can compare their `hash()`es...but consider for instance something like a key on a hashmap where one object is an int0vec and another is an int1vec and they both contain the same (representational) values. Without accounting for representation they will hash differently. [Side remark: under the current regime this would actually be difficult to achieve, since that int1vec list would not be able to exist, or have come into existence, were it possible to store the same in a smaller vector. Question. What did we do in Kerf1 (where we at least had collisions of this type on mixed-lists and integer vectors)? Answer. Kerf1 could and did cheat because all those were the same width, so it hopped from payload to payload (mixed: bubbly, vector: contiguous). We could accomplish the same with an iterator I suppose... [ This is pretty easy to do: just catch VARINT(VARFLOAT) chunk type and use wrapped_int_accessor/I_cast via the iterator ]  and  Remark. Kerf1 also had compiler options HASH_MODS_OUT_DICTIONARY_ORDER.]  So this seems like something we need to consider. Perhaps we need a `hash` method that only considers representation. **Remark. For that matter, `hash` should probably use `ntoh` if we want hashes to apply across the network! The alternative is recalculating indices after xfer.** 
// • for hashed maps, there's an issue where we want 2^n sized arrays of integers, but our headers screw this up, so we waste approximately 1/2 the space [by overallocating pow2(n+1)==pow2(m) sized vectors). Possibly we can solve it with tape_head_vector, using right-sized memory allocations (the existing structs...but we still lose header values...potentially we could reclaim the header there (in the new payload value) and simply have the first one or two hash slots marked as in-use. But then, we could also play this trick on the existing (2^n)-2 sized vectors...maybe we should try that...this is especially powerful if we can also reclaim 0->unused via one-indexing as described later). Another possibility is we have a memory attribute flag or something that adds another few pages onto the thingy: for large values we're mmapping in and out anyway (instead of using pool), also disk is similar. We can repurpose LAYOUT_FILLED for this without adding anything new, possibly. Maybe, map a page in front of it and make it a tenant? If we can make HASH_NULL zero instead of IN, we avoid a lot of these problems. Add 1 to every hash index (one-indexed instead of zero indexed)? We could also add a new layout type that's a memory managed mmap header with a payload that's like a TENANT regular slab.

// Note duped: hashtable DELETE()
//             because the delete value is a reserved impossible index
//             you can add safely add this functionality later
 
 
// Note duped:   hash index is O(2n) twice necessary space: you can fix this
//               by changing the hash function to ignore positions 0 and 1
//               (or 2^n - 1 and 2^n - 2) similar to how DELETE values
//               are ignored. This is a 2x space gain. then the vector
//               is small enough to fit inside the normal m-1 value.
//               If you're going to do this you might as well also fix
//               it to be 1-indexed so you get the benefit of sparse
//               storage for 0==HASH_NULL instead of IN integer null
//               might as well add hash_delete too
//               tweaking load factor doesn't solve the hash index issue

#pragma mark - Hash

HASH_CPP_TYPE The_Hash_Key;

uint64_t fasthash64(const void *buf, size_t len, uint64_t seed);
void hash_init();

HASH_CPP_TYPE hash_byte_stream(HASH_METHOD_MEMBER method, const void *in, uint64_t length, HASH_CPP_TYPE seed);


} // namespace
