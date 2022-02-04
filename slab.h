#pragma once

namespace KERF_NAMESPACE {

struct SLAB
{
  //We can't make SLABs virtual until someday when C++ has a way to enforce object layout (vtable position). In the meantime, see above for how to "disable" polymorphism.

  // TODO. bitfields and endianness, portability
  // 2020.07.11 Waiting on clang to add
  // __attribute__((scalar_storage_order("big-endian")))
  // so we can do network order by default
  //or 
  // #pragma scalar_storage_order big-endian
  // #pragma scalar_storage_order default
  //we can also do
  // #ifdef LITTLE_ENDIAN
  // #else
  //   // do bitfields in reverse order.
  // #endif
  //alternatively, use chars and attribute masks.
  // Question. Does it affect speed if we go against the grain on system byte ordering? Maybe we shouldn't
  // Note. Fixing network order was not that hard, see: useNetworkOrder in kerf1.0

  union{
  int8_t  slab_word0[16];
  int16_t slab_word1[8];
  int32_t slab_word2[4];
  int64_t slab_word3[2];

  struct{
         // POTENTIAL_OPTIMIZATION_POINT: We can always change bit width of these later (done carefully) without breaking anything (except invasive/improper library code)

         // POTENTIAL_OPTIMIZATION_POINT: Do atoms need reference counts? No, but this doesn't matter until you do 8-byte slabs, and really, not even then since we already shrunk the header.

         // POTENTIAL_OPTIMIZATION_POINT: We could turn these "attributes" into a index into a table [multi-d array] with only the valid combinations set. You can use a few bits at the beginning to indicate which table, etc. Just more efficient packing. 

         // Remark. note that for small values of the memory lane (0,1,2, anything less than struct min) we can recast what the layout means, and infer a lot about what must happen. This is one way to do, instead of 16-byte atoms, 8-byte, 4-byte, and even 2-byte atoms, for mixed lists, most of which are probably unnecessary. [They include headers which is what separates them from vectors.] You could even do 1-byte atoms with packing, but, why. (Many, but not all, of these cases are covered well enough by an ENUM.)

         // CURRENT_RESTRICTION : For simplification reasons, we are disallowing implementing multi-dimensional (>1) vector types inside this basic SLAB struct object, at least in this latest Kerf 2.0 Alpha. We can implement them easily enough later as a "strips" type with a charvec of data and some kind of header that tells how to interpet the accesses.
         // CURRENT_RESTRICTION: Similarly, atoms/boxes/singular structs of size less than 16 bytes are disallowed for now.
         // CURRENT_RESTRICTION: For now, currently do not intersperse reserved bits in between other attributes in an attempt to anticipate reallocations
         // CURRENT_RESTRICTION: Skip TAPE_HEAD_VECTORS for now, just to simplify things. Only do single-items. It's a pain cause they won't fit inside 16=sizeof(SLAB). Actually, they will, just do them as a SLAB_VECTOR type and the extra pointers, counters, and so on will be implied. (Alternatively, increment t_slab_object_layout_type (or whatever it's renamed to) for an additional one past PACKED_VECTOR, VECTOR, SLAB_VECTOR if you prefer.) Then add a char* and unsigned bitoffset:3;
         // CURRENT_RESTRICTION: Don't bother with 32-bit compilation at first, but don't do anything crazy to disallow it either.
         // CURRENT_RESTRICTION: We don't need uint64_t, make them use BIGINT instead
         // CURRENT_RESTRICTION: (Actually, seems like libevent will wrap all these for us and compile cross-platform. Still, we should leave this alone for now.) Leave as select() for now and don't bother with evport/kqueue/epoll until there's a need https://stackoverflow.com/questions/26420947
         // CURRENT_RESTRICTION: Don't actually implement any BigInt stuff yet
         // CURRENT_RESTRICTION: For BigInts we could expand the utility of PACKED VECTORS (1==sizeof(count) not 8) by always forcing them to be pow2 size using the pow2 attribute flag. Because of leading zeroes this doesn't change anything. Then a packed vector can identify a bigint of arbitrarily large size (<=2^255 bits, 255<<) without the need for count8. We won't realize any savings on this for a long, long time. You could play some kind of similar games with STRINGS, especially null-terminating ones, but what that gains I don't know. Maybe this implies BIGINTs and STRINGS should use container_width_current for packed vecs instead of the packed count, and a packed count of -1.
         // CURRENT_RESTRICTION: Wall off everything possible related to bigint (struct packing excepted). We don't need `bc` inside of kerf yet.
         // CURRENT_RESTRICTION. 2020.12.01 sigh. We can also pretty easily at this point get total-width-8 atoms (compared to 16 now), with 4 byte headers and 4 byte payloads. I'm not entirely sure what we'd actually use this for (you can't have any sub-objects with pointers, so no strings, sub lists, etc. only small ints, small floats, small chars, small dates. so then what good is it? if you want to mix small floats and small ints use a float2 vector.) You can cheat it by going out of your way to make sure pointers won't be in a small address space and then making sure plain atom headers fall within that address space prefix, this way you do get pointers. Seems silly. I don't see the utility in this at this time. I'd wait until there is a clear use case. (By then, possibly too late to go back and easily refactor.)
         // CURRENT_RESTRICTION: "implicit type groupings" (previously "SLAB VECTORS" with implicit/known-fixed-size slab count by type, width specified by PRESENTED()) are not necessary because they are too close to SMALLCOUNT vectors, and more awkward. Et cetera.

         // A mixed-list is just a vector with slab/K0 container type
         // Question: What about compacted mixed lists? do we ever store them like that in memory?

         // Question: what about read/write/compile-ref mutexes/counters. Some of these maybe exist only as MAP/dictionary tertiary+ attributes? which of these live only on the KTREE?

         //TODO. we're overloading names here because bigints and strings will be stored as charvecs but will be considered "atoms", so we can call that slab-storage-type-fw-item or BOX or singular or something. Vector is still vector. Strips is badly named, maybe RACK or RAKE or PACKAGE.
         //TODO. Need to be able to resolve "FOLIO" type from vector storage. Others as well. I mean, technically we could force them into the strips ghetto. Depends what needs to go on it, really. We could even force it to limit itself to strips-width, but that doesn't sound smart.
         //TODO. we need dates/times/months/etc. So make sure these make sense here...mostly we'll be aliasing int64_t
         //      maybe we need C_RESOLVED_TYPE, left aligned to atom, and KERF_PRESENTATION_TYPE
         //      TODO. list all C_RESOLVED_TYPES (to count them), list all KERF_PRESENTATION_TYPEs/APPARENT_TYPE (to get lower bound)
         //            so maybe APPARENT_TYPE (eg array or Signed-INT) > IMPLEMENTATION_TYPE/STORAGE_TYPE (eg FOLIO exposing array or 27-bit UINT) > C_RESOLVED_TYPE (uint64_t)
         //TODO. we run into trouble because uint32_t^2 needs uint64_t... we can just cap it if we want.
         //TODO. If we run out of space, we can move vector_attributes up to top-level, or reduce it to 2 bits
         //TODO. Atoms should have a flag/attribute-char which means resolve its POINTER as the type, for iteration. (eg, an int pointer that we dereference to get a real pointer). [This sounds important enough that we may need it as a flag on all types, VECTOR and possibly STRIPS included (the size req for these new tape heads is the same as for a vanilla strips thing, aka >16). We can do it as just an INDIRECT/POINTER flag, with all the same headers, but this makes VECTORS need an additional pointer AFTER the counter, which it needs to keep. maybe call it TAPE_HEAD and TAPE_HEAD_VECTOR.  Don't forget also just a generic LINK atom which is a passthrough to an entire struct, so maybe LINKALL, LINKPAYLOAD or something. These need to be mindful of whether or not they reference count, and the ones that do need to be mindful of the object they reference's memory flags] Then we get a WIDE ITERATOR suitable for traversing tables and such, all you have to do is increment the pointer (/bit-offset). You could even do this in O(1) time, for a table of c atom pointers, instead of doing c increments just have the modifiable offset stored at the beginning somewhere, or allow it to be passed as arg. We could also allow this thing to AMEND the table in place (Consider disallowing amending lists to the inside of a column, or account for it at least). Call it a COMB: it may need to reference increment its table, maybe not. | A COMB is how you get sql indexes on a TABLE, using a landis-INDEX, you may need a DESCENDING flag on them to indicate that instead of ascending.| I don't think COMBS will work on ZIPPED columns
         // Question. On the subject of TAPE_HEAD and stuff, we should figure out what's allowed to live IN THE SLAB and what must live inside SLAB_POINTER_WRAPPER. Our choices may be influenced by 1. lifecycle of memory and 2. whether or not it's stored in slab: for instance, would we persist a TAPE_HEAD anywhere? Maybe not, but it is convenient to have it here.
         //TODO. Do we need an attribute that makes the header inside a vector fatter? for that kind of iterator thing above. (Not if we pass args.) It makes the minimal vector larger than 16 bytes, but you don't need anything special (even a flag) as long as you look at the memory-lane-size and it's big enough.
         //TODO. we can do things like make a little vector inside of an atom, it can store a bitfield of length 64 which is really a vector
         //TODO. have A_BASE-style objects that register with a "workstack" for ctrl-c longjmp protection, have this whole scheme be enabled/disabled by a compiler flag. so we can test performance.

         // Remark. Maybe `grouping` for elements (eg, a map out of several lists) is best indicated with a bit or somesuch, since it would allow us to combine atoms (strings, bigints) and "grouped" elements (maps) using a standard collection of vector-like things (chararrays, simpler lists of lists). Note that 'GROUPED_ELEMENT_SLAB' is a lot like a `1==grouping` mixed_atom_vector with a slightly different header.
         // Idea. If we stored atoms and vectors using the same ->n structure and maybe a flag, it's very easy to add onto atoms. Alternatively, spend two bits counting how wide the counter is in bytes: [0, 2^0, 2^3]

         // This looks ugly but is actually very sensible and necessary for what we're doing

         // Primary Attributes, memory stats (Header, Struct Bytes 0-3)

         // NB. We can either have a lookup table that we index with t_slab_object_layout type for starting position of payload, or virtual functions on some class, or we can have a separate attribute bitfield (maybe of size :1 or :2 or :3) which is a simple count of how large the header is, maybe in terms of sizeof(int64_t) units.
         // How about slab_object_layout_type: atom (PAYLOAD), vector (COUNT+flexarray), strips ie 3rd-4th-level-attribute-user/bundled-link-thing as in ZIPS, HASHES, &such (ATTRIBUTES + flexarray). Specifying does it even HAVE attributes or reference counting, etc, which can free up more struct space. The final value should always be reserved, an escape that says look further in for more options (a la unicode escapes). | We've updated these now and this description should be changed

         // Remark. There's a couple ways to expand the r_slab_reference_count bits. Take them from unused memory attributes, take them from the reserved byte a̶f̶t̶e̶r̶ ̶p̶r̶e̶s̶e̶n̶t̶e̶d̶_̶t̶y̶p̶e̶ with extra presented/layout attributes, take them from the other bits in vector that are also not used by anything else. None of this is going to give us 32 or 64 bits, I don't think. | There is another major way which is to create a new t_slab_object_layout_type which munges say m_memory_expansion_size (if unused) into its neighbor r_slab_reference_count, as in the case of drive files. You could also just do this on existing layout types without adding a new one, possibly, if you can verify/hack in its correctness.
         // Idea. Expanding reference counts to large widths. There may be some creative ways around such things [bit limitations], however, such as creating a special memory arena that persists objects for longer durations, permanently, or otherwise somehow offloads the burden of maintaining a large reference count from the typical individual highly-packed vectors. You could, for instance, have a REFERENCE_MANAGEMENT_ARENA_HASHED_BIGCOUNT, then SOMETHING like the following (fix the broken parts, like maybe the add'l reference needed as a key), globally [or per-thread, or thread-safely] store the o̶b̶j̶e̶c̶t̶s̶ slab-pointers as `keys`, the `values` as the 64-bit int reference counts (a hashbag or std-cpp-map), when the count hits zero the object can be freed. Maybe we do this anyway when a refcount overflows at reference_increment time. 

         // Remark. the all-zeroes 16byte should be something sensible or as close to it as possible, eg, a null atom or an untyped empty list
         
         // NB. to pack:  __attribute__ ((packed)) Alternatively, #pragma pack(push, 1) #pragma pack(pop) around the bytes of the struct/union
          
         union {
           struct {
              UC                 t_slab_object_layout_type:4; 
REFERENCE_MANAGEMENT_ARENA_TYPE reference_management_arena:2;
              UC                   m_memory_expansion_size:6;
              UC                    r_slab_reference_count:4;   
              UC               a_memory_attribute_reserved:3;
           } __attribute__ ((packed));
           // You can add say UI filler:13 or corresponding UC here but you need to pack this part of the struct
           // Separately, if you remove packed here, you'll cause a test to fail, as it will run over the beginning of SUTEX
           // POP If we get rid of a_memory_attribute_reserved:3 (reducing it to two bytes total), we can likely remove the packed attribute safely

           struct {
            UI same_bit_width_as_layout_type:4;
            UI memory_header_wo_layout:15;
            UI mh_thing_filler:13;
           }  __attribute__ ((packed));

           struct {
             // Putting more bits in sutex here likely means stealing from ref count.
             // At 4 refcount is getting tight. 3 is probably the limit, 2 is probably too small, 1 is not really useful.
             UC sutex_spacing_bytes[2];
             SUTEX sutex;
           };

           struct {
             // Also, you'd get 5 more bits from sutex, giving 1 to refcount and 4 to memory attributes.
             UC fake_spacing[3];
             UC fake_presented_type;
           };

           C first_four_char[4];
           int32_t first_four;
         };

         // putative memory attributes:
         // ---------------------------
         // TODO. For m_memory_expansion, {0,1,2} are not used (4 is the minimum currently, maybe 3 later). Maybe 0 can be TENANT which possibly is also COMPACT (minimum implied by wire_size()). Should allow us to reclaim refcount space for different purposes. It may have another wrinkle: what about disk-mapped files? they need to be SELF-MANAGED but may be COMPACT...?
         // Remark. A TENANT atom who doesn't live on the DISK (aka ANONYMOUS), we can probably assume lives in a list that is a mixed-slab-vector, and so we can rewrite him into a link [to the new value] if he gets turned into say an intvec of size 3 that wouldn't fit in place.
         // 1. weak (not needed if obviated by reference_management_arena): 0==TENANT / 1==NONTENANT: 0==TENANT{automatic-allocated,cannot-be-linked-to-normally-must-be-copied,can reclaim "refcount" space since unused, noresize/noexpand/nocontract, efficient-compact-not-full-lane-use-wire-size}/1==NONTENANT{self-managed-address, needs-repool-or-munmap, non-automatic-is-heap-or-disk-allocated, refcounted, "resizable", can-be-linked-to} ["tenant" actually refers to at least two distinct types of 'transient' memory: automatic-function-local-memory, and a child section inside of heap-allocated memory which does not live at the main-allocated-heap-memory-pointer-address and which does not "own" itself and hence could be reclaimed or rewritten at at the parent's discretion. so perhaps TRANSIENT breaks down into AUTOMATIC and TENANT, and we can call this TRANSIENT with the potential of splitting it later] compare: reference_management_arena, which may get a TRANSIENT/TENANT/AUTO/UNMANAGED property. This can also be a synonym of "not a root address from the memory pool or valid there or memory-pool-invalid or memory-pool-address"
         // 2.   MMAP-MAP-PRIVATE / MMAP-MAP-SHARED  (covers shmem shared as well) see: https://stackoverflow.com/a/48887374. We moved this to A_MEMORY_MAPPED, the reason being, we can't very well update a long mmapped array of subelements with headers in O(1) time based on the map shared/private attribute.
         // 3. MMAP-MAP-ANONYMOUS / MMAP-MAP-DISK-FILE  stored-in-ram/stored-on-disk. This we've kept so far. Question. Is storage on DRIVE actually better represented as an ARENA? Remark. Seems like we should eliminate, see MEMORY_MAPPED discussion, we may be able to obviate
         // 4. can-mmap-file-and-leave-it-mapped/read-directly-into-memory-and-close-any-mmaps (reclaimable to something else after read) | this again may be something better stored on the MESSAGE type
         // 5. full-pow2-width-specified-by-m/efficient-compact-not-full-lane (is this same as tenant? note: size should be derivable from headers. should everything be like this anyway? is this the same as read-directly-into...?) minimum-implied-by-WIRE-SIZE (rounds up nearest byte if bits). | Sounds like this may be attribute better suited for MESSAGE type when writing to disk?
         // 6. weak: exists-on-the-kerftree-OR-multithread-aware (possibly a maps-on-the-kerftree attribute) (may be obviated by reference_management_owner which includes a kerf-tree value, additionally, may exist farther down as a SMALLCOUNT-freed special add'l attribute on a map/dictionary. iow, I doubt we need. )
         // 7. weak: writeable/read-only (do we have any use cases for this besides zip cache (read only tables?)? if not, it could be a zip attributes. can't set this if it's read-only anyway, well maybe in some cases.) Remark. could also be a flag inside the mutex, eg, a writer id > max thread count. Warning. You don't want to do this because it hoses mmappings, you could MAYBE fix it, but consider, I have s/t written to disk with the writeable flag on, but then I map it MAP_PRIVATE & ~PROT_WRITE, there's no convenient way to fix that bit now. Seemingly, we're handling such things at the MEMORY_MAPPED presented object level. 
         // 8. note: shm_open posix shmem shared memory is different (process-shared) from mmap MAP_SHARED. Not clear that we'd need to track this anyway, if we're always freeing our handles. Possibly something for MEMORY_MAPPED object to track.
         // ---------------------------

         // ...
         // some of these we can move deeper down I'm sure, or remove/omit
         // we can also claim another 8 by getting rid of packed vector (its count byte)

         // TODO. Could we turn fixed_width_container_type into a static table for general_type?
         // C accessor_type_fixed_width_container_item_type_or_general_type:8;    //(for atoms and vectors; for strips will always be slab/K0)


         // TODO. Tape head needs pointer for ref count (if 0, don't refcount). I don't think tapeheads should refcount, they point to the payload, but we should get rid of the tapehead, rather than storing+managing a reference to the containing struct
         // TODO. should we convert slab vector (b/c of restriction on flexarray) to be a 8==sizeof(SLAB*) vector to use refcounted pointer, instead of an 16==sizeof(struct SLAB) array? what happens if you mmap and munmap this thing? Well, you get O(#strips) pointer initializations at least. This is an integral jump table effectively. You could store pointer-offsets (getting kludgey with overloading)? You can still do both/either of them, use a vector of ints (+jump flag) and a vector of raw pointers. Think on this (all cases) before committing.

         // Secondary Attributes (Header, Struct Bytes 4-7)
         // NB. (I think this discussion forgets signed vs. unsigned values, which we were indicated via positive/negative. So maybe this thing doesn't work at all, at least without accounting for that) "vector_interpret_widths_pow2_bits" vs. overloaded storage type: We can use a separate flag for interpreting bitwidths as pow2 (1bit + 7bit == 8bits), or we can overload a single signed (7bit) operation
         // so that [max(==2^70 bits==2^67 bytes)),...,128bit(==2^7bits==2^4bytes),64bit(==2^6bits==2^3bytes),0bit (==flag),1bit,2,...,63] = [f(-64), ..., f(-2), f(-1), f(0), f(1), f(2), ... f(63)]. Also, aside from 0bits, we could also have as flags 2^67, 2^66, and 2^65. More even if you want. If you use the full 8bits for representation, you get twice as much stuff. 
         // I don't even know what this would imply for INTS, where we only need at most indicators for 0-64 bits and an unbounded_to_bigint indicator. Maybe we can have vectors of bigints, but this seems unlikely. Who knows.
         union{
               int32_t second_four;
               
               struct{ 
                 int16_t third_two;
                 int16_t fourth_two;
               };
         
               struct{

                 union{

                   union{
                      // NB. In case we need to move sutex back it can go h̶e̶r̶e̶
                      // [at the beginning of the second four], probably also
                      // if we need more than 13, especially the full 16 bits,
                      // we'd move it back here and move presented_type up to
                      // the last of the first four. Then sutex would likely
                      // have a zero offset but a non-zero remain (or a zero
                      // remain if using all 16 bits).
                      SUTEX fake_sutex;
                      struct {
                        SUTEX_CONTAINER fake_mutex_claimed_by_prev_structs:SUTEX_ROUND_BIT_WIDTH - SUTEX_BIT_REMAIN;
                      #if SUTEX_BIT_REMAIN > 0
                        SUTEX_CONTAINER fake_remaining_for_eg_vector:SUTEX_BIT_REMAIN;
                      #endif
                      };
                   } ;

                  // Regular Atom
                  struct{
                         // UC atom_reserved_0:8;
                         // UC atom_reserved_1:8;
                         UC atom_reserved_0:8;
                         UC atom_reserved_1:8;
                  };

                  // TAPE_HEAD_ATOM - should be hidden behind slab_object_layout_type struct indicator
                  // Indirection types can be virtualized away, hidden behind C++ vtables
                  // Should regular links (to encapsulated structs) be hidden behind here too? (REDIRECT_ATOM)
                  struct{
                            // UC tape_reserved_0:8;
                            // UC tape_reserved_1:8;
                            UC tape_reserved_0:8;
                            UC tape_reserved_1:8;
                         // UC payload_is_actually_indirection_pointer_for_untyped_struct_encapsulation_solt:1;
                         // UC payload_is_actually_indirection_pointer_to_type_specified_payload:1;
                         // UC payload_pointer_log_derefed_width:8; // chunk width in PRESENTED will indicate this
                         // UC payload_pointer_bit_offset:3;
                         // UC atom_interpret_widths_pow2_bits:1;
                  };

                  // TAPE_HEAD_VECTOR
                  struct{
                         UC thv_reserved_0:8;
                         UC thv_reserved_1:8;
                  };

                  // Regular Vector (8==sizeof(count)) / ?Regular Variable-Width Atom? / Packed Vector (1==sizeof(count)) 
                  // Promoting a Packed Vector to Regular: 
                  //                                       1. move the payload right 8 bytes
                  //                                       2. set ->n equal to the old packed count
                  //                                       3. change t_slab_object_layout_type
                  //                                       4. zero packed_vector_item_count, freeing it for attributes/whatnot
                  // NB. Provided the memory_slab is wide enough, we can actually wait to do this until the packed_vector_item_count would overflow at 255
                  //     This makes packed bigints useful
                  struct{
                         // commented stuff for bits / unsigned versions
                         // UC interpret_widths_pow2_bits:1; //Putative: nicely yields STRINGVECS & BIGNUM vecs, also, enables pow2 lockstop promotion
                         // C        container_width_cap:7; // 7s if no uint. how to signal to bigint though? 0 && !interpret_pow2? maybe we should use 8?
                         // C    container_width_current:7; // widths are all pow2(.) bytes
                         // Idea. this `newly_reserved` could be `presented_attribute_flags` for every layout type (atoms, tape_head, ...)
                         // UC                 newly_reserved:8; // mutex? one thing we can do with this, if we're not using it, is give it all back to reference_count. has the benefit of pushing presented_type farther out of the memory header
                         union{C regular_vector_reserved_3:8;UC smallcount_vector_byte_count;};
                         CAPPED_WIDTH_TYPE vector_container_width_cap_type:3; // cap for varint/varfloat, with unbounded [2^3, 2^2, 2^1, 2^0, unbounded, ?, ?, ?]
                         // UC    container_width_current:4; // widths are all pow2(.) bytes. bigger t/ cap for say bigintvec - freed: chunk in PRESENTED
                         UC                bigint_sign:1; // NB. only necessary for true bitvectors (charvectors) holding BIGINT, never for references to BIGINT (why do we need this? existing 3rd party library calls maybe? yes, if you like. whenever we rehydrate: still have to store sign in dehydrated value...annoying)
                         // UC           attribute_filled:1; // has its own LAYOUT now
                         // UC optional_vector_bit_offset_count:3; // Useful if we ever do BIT ops on top of the standard ints. Char counter will be "1" (length: 1 byte vector), then either 1+(0-7) bits forward representing bits from front or  0+(0-7) bits back (negative) represent the offset of bits from the end of the bytespace. (If 0 bits, then 0 char counter.) This makes it play with our existing infra. | 2021.08.06 we resolved not to do bitops in kerf2, some time before this date.
                         // UC            more_newly_reserved:4;
                         UC             attribute_reserved_1:2; // attr_REJECT_PROMOTE_TO_MIXED_PROMOTE_TO_FLOAT_PROMOTE_TO_BIGINT, or add'l cap type
                         UC       attribute_known_sorted_asc:1; // Idea. both true [asc/desc] => ???, all same/`take`
                         UC      attribute_known_sorted_desc:1; // POP. Neither of these attributes are necessary, truly, for smallcount. So you could keep them where that is stored instead.
                  };


                  // LIST
                  struct{
                         // UC list_reserved_0:8;
                         // UC list_reserved_2:8;
                         union{C regular_list_reserved_1:8;UC smallcount_list_byte_count;}; // should keep same position as smallcount_vector_byte_count (assert is in test.cc)
                         UC list_attribute_reserved_3:2;
                         UC attribute_zero_if_allows_append_amend_to_break_all_same_stride_width:1;
                         UC attribute_zero_if_known_children_at_every_depth_are_RLINK3_free:1; // aka FLAT LIST. Rule: If zero: Always flatten rhs on amend/append. If one: always add RLINK (equiv to "always add slab4-width item"). Also, provide force-to-one method at least at creation time (if later, potentially wants to rewrite list to fit format). Rule: Defaults to zero
                         UC attribute_zero_if_known_that_top_level_items_all_same_stride_width:1; // for O(1) indexing - hooks for toggle at append and amend. Alt: attr_zero_if_known_even_stride // JUMP_LIST doesn't need this and could overload to be "ragged append on jumplist untracked by intvec"
                         UC attribute_zero_if_jump_list_is_complete_and_no_untracked_terms:1;
                         UC       LIST_HOLDER_attribute_known_sorted_asc:1;
                         UC      LIST_HOLDER_attribute_known_sorted_desc:1;

                         // Observation: Old SLAB4 is just 1. even strides 2. has_RLINK3
                         // Remark: even strides don't need a JUMP_LIST
                         // Remark. The issue with the full-depth RLINK3 attribute is it requires a linear scan past the top level for GROUPED on DRIVE: not if the next level has the flag reporting RLINK-free: the process terminates, as we want it to.

                         // Question. Did we do RLINK correctly? As a SLOP, we may have wanted [and correspondingly, an RLINK atom layout] LAYOUT methods for "literal x,y,z" and "resolved x,y,z", otherwise it's kind of hiding our RLINK???

                  };

                 };

                 struct{ 
                    
                    union{
                      UC layout_and_presented_reserved_byte:8;
                      struct {
                        UC layout_reserved:4;
                        UC presented_reserved:4; // this is a weird thing we may want to return. one of the issues with presented-level attributes is subclasses must respect parent classes and not clobber each other's bytes. layout types are flat not hierarchical
                      };
                    };

                    PRESENTED_TYPE presented_type; // was resolved_type. // 2021.11.22 I see, we'd rather have presented_type in the first_four bytes of slab because then if we ever make an 8-byte (instead of 16-byte) atom/unit then it doesn't break all of the code that depends on that [and then hope we never bother with 4-byte atoms or less]. The natural piece to move to the second_four is the sutex. There is not a good reason to keep the sutex up front except that it's memory-relevant. [Even then, you'd still need guaranteed placement *and* 1-byte width for PRESENTED_TYPE in any smaller layout types, similar to guaranteed placement and width for `t_slab_object_layout_type` at the front]
                 };

               };
         };

         // Payload / Count (Payload, Struct Bytes 8-15+)
         union{
               // Atom
               I i;                   // Payload (Int Atom/Char Atom)
               F f;                   // Payload (Float Atom)
               V v;                   // Payload (Pointer/Link)
               struct SLAB*a;         // Payload (Pointer/Link)
               C* s;
               struct SLAB**spp;

               uint8_t ui8t[8];
               C ca[8];
               UC uca[8];

               // SMALLCOUNT Freed Attributes (Map/Dictionary Compiled Reference Count, ...)
               struct{
                 // Warning. You don't want to put attributes here because it's a headache, even if you start out as a non-smallcount [the issue is converting between in-memory smallcount styles and so forth, LISTs and JUMPLISTs]. Sure to be complex, not even sure how you'd go about it. I think the main offender is LAYOUT_TYPE_COUNTED_LIST_PACKED. You'd have to scurve that PACKED for all GROUPED or something, anything you expected to use the attr. This is complex.
                 C fake_value_for_warning;
               };

               //CURRENT_RESTRICTION: Don't do anything less than sizeof(8) bytes
               //                        // Answer. should sizeof(.)<8 types (eg char) go at the front or back? I think front
               //                        //          by default the struct will put at front (left, lower address) so that all have same address
               //                        //          We like this because the payload address is always the same. And it's simpler.
               // C c;                   // Payload (Char Atom)
               // UC uc;                 // Payload (Char Atom)
               // float f32;             // Payload (32-bit Float Atom)
               
               // Counted/Filled Vector
               struct{
                      I n;            // Count (this is byte-count now, except possibly for ATTRIBUTE_FILLED, who can union a diff. struct there)
                      union{          // Columnar Accessors via Flexible Array Struct Hack
                            C g[]; 
                            UC ug[]; 
                            UI uia[];
                            V va[];
                            I ia[];
                            I2 i2a[];
                            I1 i1a[];
                            I0 i0a[];
                            F fa[];
                            F2 f2a[];
                            F1 f1a[];
                            uint8_t ui8tflex[];
                            SLAB *slab_ptrs[];
                      };      
               };
         };
  };
  };

  ~SLAB() {};
  void* operator new(size_t j) = delete;
  void operator delete(void *p) = delete;

  static SLAB slab4_transient_with_good_first_four() { return (SLAB){
                                                         .t_slab_object_layout_type = 0,
                                                         .m_memory_expansion_size = LOG_SLAB_WIDTH, // Remark. Had we done this as (m-3), or special-cased `0`, we'd get all 0's here...
                                                         .reference_management_arena = REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT, // "0"
                                                         .r_slab_reference_count = 0, 
                                                         // .sutex = {.counter = 0, .writer_waiting = 0},
                                                         // .presented_type = (PRESENTED_TYPE_MEMBER)0,
                                                         .n = 0,
                                                        };}

  static int32_t good_transient_first_four() { static const SLAB s = slab4_transient_with_good_first_four(); return s.first_four; }

  // Question. I think with presented_type in the second four we can simplify this by removing it in the code? It's used in one place, and sutex can/should be zeroed.
  static int32_t good_transient_memory_header() { static const SLAB s = slab4_transient_with_good_first_four(); return s.memory_header_wo_layout; }

  static SLAB good_rlink_slab(SLAB *s=nullptr)
  { 
    SLAB t = slab4_transient_with_good_first_four();
    t.t_slab_object_layout_type = LAYOUT_TYPE_UNCOUNTED_ATOM, // no-op when 0...
    t.presented_type = RLINK3_UNIT;
    t.a = s;
    return t;
  }

  void zero_sutex_values()
  {
    // Remark. We could put this on layout() if layout type ever determines where the lock values live (or don't live)
    sutex.zero_sutex_claimed_subset_inside_container();
  }


  PRESENTED_TYPE safe_presented_type()
  {
    switch(t_slab_object_layout_type)
    {
      // non-standard cases would go here, eg 4-byte or maybe 8-byte (as opposed to 16-byte) ATOMs, specifically, wherever we aren't guaranteed to have presented_type in the "normal" place, or of the "normal" width (viz. some bits < 1-byte, or even > 1-byte)
      default:
        return presented_type;
    }
  }

  
  template<typename L> auto slab_lock_safe_wrapper(L &&lambda, const bool exclusive = true)
  {
    sutex.sutex_safe_wrapper(lambda, exclusive);
  }
  template<typename L> auto slab_lock_safe_wrapper_exclusive(L &&lambda) { return slab_lock_safe_wrapper(lambda, true); }
  template<typename L> auto slab_lock_safe_wrapper_shared(L &&lambda) {    return slab_lock_safe_wrapper(lambda, false); }

  void callback_from_mapped_queue_to_store_index(I index);
  void callback_from_mapped_queue_to_inform_removed();
};

} // namespace
