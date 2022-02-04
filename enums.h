namespace KERF_NAMESPACE {

typedef enum LAYOUT_TYPE_MEMBER : UC {
  LAYOUT_TYPE_UNCOUNTED_ATOM            = 0, // Remark. Better names might be SEGMENT+RAY or perhaps BUFFER, BYTES, BUF16/BUF4
  LAYOUT_TYPE_COUNTED_VECTOR            = 1,
  LAYOUT_TYPE_COUNTED_VECTOR_FILLED     = 2,
  LAYOUT_TYPE_COUNTED_VECTOR_PACKED     = 3,     // SMALLCOUNT that starts payload where ->n was.
  // Idea. you could also do like a VECTOR_SMALLCOUNT_UNPACKED and make the header even bigger if you need (doubtful we would) 
  // Idea. you could also make a VECTOR_VARIABLY_SPECIFIED (header size, counter width, payload start) and use LAYOUT's virtual methods to act based on slab attributes. but, yuck. also, diminishing returns on expanding header, versus say using vector of slab4. - this is liable to break some useful hacks though (in promote-expand?)
  // POTENTIAL_OPTIMIZATION_POINT  LAYOUT_TYPE_COUNTED_VECTOR_SUPER_PACKED_STRING: start the payload after ->presented_type + a 1byte_counter (alternatively null terminate, but then some strings ('\0'-containing) are verboten). This yields a small improvement from 8byte PACKED strings to 11byte/12byte. If you really cheat and do this with layout_type only, you can free up 15.5 bytes of string per slab. (Overload things like returning [what was] ->m as constant and setting ->m as no-op.) Idea. What we could do for a refactor/could've done is start by building layout with a minimalist LAYOUT_TYPE and fully specified class methods which don't actually have access to bits past the layout_type on the slab (including no-op writes). Then the code would already be built around this concept. TODO: go through and turn references to SLAB elements like `r_slab_reference_count` into getter/setter wherever possible. reference counting should probably be in here [layout.h/cc] anyway?
  LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM  = 4, // NB. It's OK to wall off access to TAPE_HEADs (or have them mutate/return into plain old atoms/layouts/slops), they're supposed to be iterators primarily.
  LAYOUT_TYPE_TAPE_HEAD_COUNTED_VECTOR  = 5, // // Remark: String TAPE_HEADs need these, for eg STRING_FIBER
  LAYOUT_TYPE_COUNTED_LIST              = 6,
  LAYOUT_TYPE_COUNTED_LIST_PACKED       = 7,
  // LAYOUT_TYPE_COUNTED_LIST_SMALLCOUNT | LAYOUT_TYPE_COUNTED_LIST_PACKED, // P_O_P use two-bytes of counter, freeing 8bytes at ->n (y tho?) | Warning: Will this play nicely with GROUPED? Only if you promote on threatened overflow of byte counter (which can happen), and if you weren't using the previously additional space to pack in more attributes.
  LAYOUT_TYPE_COUNTED_JUMP_LIST         = 8, // NB. Do not pack: only gives advantage if payload is longer than packed anyway. Remark. If you write many separate small JUMP_LIST files to the drive, there's a trivial advantage in packing, not worth it (and why would you do that?).
  LAYOUT_TYPE_MESSAGE_HEADER            = 9,
  LAYOUT_TYPE_ESCAPE_VALUE              = 10,
  /////////////////
  // LAYOUT_TYPE_COUNTED_VECTOR_SMALLCOUNT = 3, // frees ->n in header, for attributes and such, but the tradeoff is only counts to 255 P_O_P: if we never literally use this in objects, we can get rid of the LAYOUT_TYPE and simply keep the CLASS to be available to the other LAYOUT classes that depend on the logic. Well, we're planning to use SMALLCOUNT for MAPS in the KERF TREE so they have compiled reference count, but it seems possible so far we could still do this with PACKED instead, possibly. 2021.10.10 Doesn't seem like we relied on this anywhere (possibly we did on the LAYOUT classes), so I've tentatively disabled it to free up address space
  // Idea. LAYOUT_TYPE_COUNTED_VECTOR_FILLED_VAN_EMDE_BOAS. the indexing methods handle the math conversions
  LAYOUT_TYPE_SIZE
} LAYOUT_TYPE;
inline std::ostream &operator<<(std::ostream &os, LAYOUT_TYPE c) { return os << static_cast<int>(c); }
#undef FOO
#define FOO(x) [x]=#x,
const char *LAYOUT_TYPE_MEMBER_STRINGS[LAYOUT_TYPE_SIZE] = 
{
  FOO(LAYOUT_TYPE_UNCOUNTED_ATOM)
  FOO(LAYOUT_TYPE_COUNTED_VECTOR)
  FOO(LAYOUT_TYPE_COUNTED_VECTOR_FILLED)
  FOO(LAYOUT_TYPE_COUNTED_VECTOR_PACKED)
  FOO(LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM)
  FOO(LAYOUT_TYPE_TAPE_HEAD_COUNTED_VECTOR)
  FOO(LAYOUT_TYPE_COUNTED_LIST)
  FOO(LAYOUT_TYPE_COUNTED_LIST_PACKED)
  FOO(LAYOUT_TYPE_COUNTED_JUMP_LIST)
  FOO(LAYOUT_TYPE_MESSAGE_HEADER)
  FOO(LAYOUT_TYPE_ESCAPE_VALUE)
  // FOO(LAYOUT_TYPE_COUNTED_VECTOR_SMALLCOUNT)
};
#undef FOO

typedef enum REFERENCE_MANAGEMENT_ARENA_MEMBER : UC {
  REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT = 0, // AUTOMATIC-CPP-FUNCTION-STACK-MEMORY and its special instantiation SLOP-AUTO-SLAB-LOCAL-STORAGE; TENANT-AKA-HEAP-POOL-OR-DISK-NON-ROOT-ADDRESS; looking more like we want to split these.  Observation. Mostly, these things don't grow [in place]. Call it "NOEXPAND", perhaps its a separate arena or a memory attribute. Observation: the semantics that govern RC_SLABP3_ARRAY's pointers will(/should) implicitly be the same that govern [the pointers in] SLAB4_ARRAY's inline RC_SLABP3_UNITs, so consider whether the implied area and such match up. Observation. There's a memory concept WEDGED or backended or abutted or blocked: when there's another object right behind it, so it can't expand. True in general for tenants, except tenants on the end (recursively) Observation. Really, we need like a `->m` equal zero flag, it's a tenant-style thing, where the PARENT manages your space (you can't expand without permission - permissioned), but this is different from say a SLOP auto_slab which is AUTOMATIC, BUT, it can control its own expansion (unpermissioned), and its `->m` is active and meaningful. Observation. It's also a question of OWNER/UNPERMISSIONED and also LEADING_POINTER/TENANT Idea. there's probably another arena that indicates global_static_cpp memory: can't ever be removed/released and isn't transient and begins zeroed.
  REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK       = 1,
  REFERENCE_MANAGEMENT_ARENA_KERFVM_WORKSTACK    = 2, // Maybe a kerfvm would like things stored as RLINK atoms so you can edit objects in place [eg, expand without losing the reference when it changes].
  REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED    = 3, // should disallow auto/tenant as PARENTs. Kerf Tree or Function Tree, general PARENTED
  /////////////////////////
  REFERENCE_MANAGEMENT_ARENA_SIZE
  
  //2021.11.18 I think ARENAs break down into a few qualities (we may be able to eliminate one or more ARENAs or split them out somehow).
  //One is VISIBILITY: visibility to current thread only (cpp_workstack) or to multiple threads from kerf tree ("hidden" behind dictionaries+lock guards) or to [just one? from] kerfvm stack (this VISIBILITY is a quality which I don't think needs ARENA for, it's managed implicitly maybe, so for instance, maybe tree vs kerfvm is redundant. Really, one of the currently present issues is that VISIBILITY manages things implicitly, and ARENA explicitly, but you can't really look at an ARENA value after the fact, eg when an object has been freed, and so on, this implies VISIBILITY already needs to be solved implicitly).
  // One is REFERENCE COUNTING: unmanaged transient will not increment.
  // One is indication of PARENTAGE: it's linked to by another object. [SLAB object typically?]
  // Similarly SLOP-REFERENCE-COUNT: in the case of cpp workstack, they aren't parent object slabs.
  // Another is EXPANDABILITY/NOEXPAND: you mostly can't expand unmanaged_transient or append to it.
  // Another is INLINE_IN_PARENT/TENANCY: unmanaged_transient again.
  // Another is type of PROGRAM_MEMORY: automatic/stack, heap, [unused?: static_global].
  // You could make a boolean table of these and see what's actually necessary. Another problem is, we're already down to 2 bits [of ARENA attribute], so, it's unlikely we can get to 1. Maybe 1.5. If I had to conjecture, you could get it down to 1 bit UNMANAGED vs. PARENTED, merging kerfvm==tree, and eliminating CPP_workstack by tracking that as part of SLOP memory instead of on the SLAB (consider, auto_slab() RLINK3 does parented management). This sounds like a big lift though at this point in the code, especially for 1 bit, but may make more sense as part of a "logical" refactor. Also remember that, aside from this [ARENA], you still have REFERENCE_COUNT attribute, and that can help you with management. We also have 3 unused memory attribute bits at this point, so we're not hurting on the space front. Caveat: the utility of kerfvm ARENA may be that if we know that we're going to pop it off the stack into oblivion soon, we can write on it, but again, this may be something we can track inside of the SLOP, with something similar to all the other trackers, but instead of say `RLINK3 PARENTED` we would track it as `KERF VM SLOP TRACKED` and we'd know we could overwrite it/modify it. Question. What good is reference_count on a TRANSIENT? Answer. ???
} REFERENCE_MANAGEMENT_ARENA_TYPE;
inline std::ostream &operator<<(std::ostream &os, REFERENCE_MANAGEMENT_ARENA_TYPE c) { return os << static_cast<int>(c); }
#undef FOO
#define FOO(x) [x]=#x,
const char *REFERENCE_MANAGEMENT_ARENA_MEMBER_STRINGS[REFERENCE_MANAGEMENT_ARENA_SIZE] = 
{
  FOO(REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT)
  FOO(REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK)
  FOO(REFERENCE_MANAGEMENT_ARENA_KERFVM_WORKSTACK)
  FOO(REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED)
};
#undef FOO

typedef enum CHUNK_TYPE_MEMBER {
  // Remark. Only varint, varfloat, and unchunked (bytes) will be below the 8byte threshold. (This assumes 64-bit arch. Otherwise, RLINK3 needs help as well.) Slabs are above it obviously. Only SLABs will be 16 bytes. Everything else unspecified is 8 bytes.
     CHUNK_TYPE_UNCHUNKED_RAW_BYTE_DATA = 0,
     CHUNK_TYPE_VARINT                  = 1,
     CHUNK_TYPE_VARFLOAT                = 2,
     CHUNK_TYPE_RLINK3                  = 3,
     // CHUNK_TYPE_SLAB4,                    // could do VARSLAB (set of fixed sizes like VARINT), but aren't: I'm guessing not performant
     CHUNK_TYPE_VARSLAB                 = 4, // not limited in size or width or regularity, so unlike VARINT
     /////////////////////////
     CHUNK_TYPE_SIZE
} CHUNK_TYPE;
inline std::ostream &operator<<(std::ostream &os, CHUNK_TYPE c) { return os << static_cast<int>(c); }
#undef FOO
#define FOO(x) [x]=#x,
const char *CHUNK_TYPE_MEMBER_STRINGS[CHUNK_TYPE_SIZE] = 
{
  FOO(CHUNK_TYPE_UNCHUNKED_RAW_BYTE_DATA)
  FOO(CHUNK_TYPE_VARINT)
  FOO(CHUNK_TYPE_VARFLOAT)
  FOO(CHUNK_TYPE_RLINK3)
  FOO(CHUNK_TYPE_VARSLAB)
};
#undef FOO

typedef enum REPRESENTATIONAL_TYPE_MEMBER : UC {

  // REP BASE OBJECTS //////////////////////////////
  REP_NIL = 0,
  REP_CHAR,   // different hash from/not equal to string of length 1 (Rule? Representational type equivalence implies equivalent expected *logical* behavior (not performance behavior), outside of an exceptional subset that peeks under the covers, eg a func returning detailed type information Question. If that's the case, what different about a LIST and a MAP? A LIST is a subset of a MAP with numeric keys 0..n-1. Answer. We can enforce a difference (eg, lists won't allow non-numeric keys) or we can work to make sure one doesn't exist: promoting LISTS to MAPs and so on (eg when a non-numeric or out-of-order key is used.) Lua does this with its tables (ie a LIST is a MAP), in my experience this functionality is not appropriate here, so, assuming our rule is true, we should enforce differences. Remark. Based on this, so then empty integer vectors and empty charvecs should compare equal because they are boths empty lists? I ask because I don't think they did in Kerf1.0 (but perhaps should have. Actually I'm not sure because it just returns the first empty list for atomization/iteration purposes. Looking at the c-source, though, they didn't compare equal. ). )
  REP_INT,
  REP_FLOAT,
  REP_STRING, // different hash from/not equal to charvec/list of chars
  REP_LAMBDA,

  // REP CONTAINERS ////////////////////////////////
  REP_ARRAY,
  REP_MAP,  // ? Question. is a table a REP_MAP or its own thing?

  // REFERENCE: Not Used ///////////////////////////
  REP_SET    = REP_ARRAY,  // a SET is also REP_ARRAY
  REP_VECTOR = REP_ARRAY,  // all "vectors" and arrays will hash as type LIST.
  REP_LIST   = REP_ARRAY, 
  REP_BIGINT = REP_INT,   // INT(123) same as BIGINT(123), same hash (promote for comparison as need be).
  //////////////////////////////////////////////////

  // Remark. For simplicity, we're going to try to map all these to INT first, though I don't think it'll work
  // REP_STAMP_DATETIME,
  // REP_STAMP_YEAR,
  // REP_STAMP_MONTH,
  // REP_STAMP_DAY,
  // REP_STAMP_HOUR,
  // REP_STAMP_MINUTE,
  // REP_STAMP_SECONDS,
  // REP_STAMP_MILLISECONDS,
  // REP_STAMP_MICROSECONDS,
  // REP_STAMP_NANOSECONDS,
  // 
  // REP_SPAN_DATETIME,
  //
  // REP_SPAN_YEAR,
  // REP_SPAN_MONTH,
  // REP_SPAN_DAY,
  // REP_SPAN_HOUR,
  // REP_SPAN_MINUTE,
  // REP_SPAN_SECONDS,
  // REP_SPAN_MILLISECONDS,
  // REP_SPAN_MICROSECONDS,
  // REP_SPAN_NANOSECONDS,

} REPRESENTATIONAL_TYPE;
inline std::ostream &operator<<(std::ostream &os, REPRESENTATIONAL_TYPE c) { return os << static_cast<int>(c); }

// Feature. use "X Macros" to get this to also simply generate the string values
typedef enum PRESENTED_TYPE_MEMBER : UC {
     
     /////////////////////////
     // UNITS
     /////////////////////////

     NIL_UNIT     = 0,
     CHAR3_UNIT   = 1,
     INT3_UNIT    = 2,
     FLOAT3_UNIT  = 3,
     RLINK3_UNIT  = 4,            // reference-counted (managed) link to *SLABM*. found most often inline in SLAB4_ARRAY. RLINK3(==64bit) even on 32-bit systems.
     // ULINK_UNIT,               // un-r-c. these are obviated by tape_head? also kinda by INT3_UNIT
     SLABM_UNIT   = 5,            // not normally found in the wild? for TAPE_HEAD_UNCOUNTED_ATOM, at least. cf. URC_SLABLINK_UNIT. possibly for layout_appends???  

     /////////////////////////
     // UNITS CHRONO
     /////////////////////////

     // STAMP (ABS) / SPAN (REL): Time & Date & Temporal & Chronological
     STAMP_DATETIME_UNIT = 6,     // (3) 2020.01.02T22:04:05.0123456789
     STAMP_YEAR_UNIT,             // (3) 2020Y
     STAMP_MONTH_UNIT,            // (3) 2020.01M
     STAMP_DAY_UNIT,              // (3) 2020.01.02
     STAMP_HOUR_UNIT,             // (3) 23H
     STAMP_MINUTE_UNIT,           // (3) 12:34
     STAMP_SECONDS_UNIT,          // (3) 22:34:56
     STAMP_MILLISECONDS_UNIT,     // (3) 12:34:56.001
     STAMP_MICROSECONDS_UNIT,     // (3) 12:34:56.001002
     STAMP_NANOSECONDS_UNIT,      // (3) 12:34:56.001002003
     /////////////////////////
     SPAN_YEAR_UNIT,              // (3) 3y  --  all units (3), see array (.) below
     SPAN_MONTH_UNIT,             // (3) 2m
     SPAN_DAY_UNIT,               // (3) 4d   
     SPAN_HOUR_UNIT,              // (3) 2h   
     SPAN_MINUTE_UNIT,            // (3) 5i or 5min
     SPAN_SECONDS_UNIT,           // (3) 20s   question: 20.001002003s -> nanos?
     SPAN_MILLISECONDS_UNIT,      // (3) 330t or 330milli[s]
     SPAN_MICROSECONDS_UNIT,      // (3) 1u or 1micro[s]
     SPAN_NANOSECONDS_UNIT = 24,  // (3) 10n or 10nano[s]

     /////////////////////////
     // ARRAYS 
     /////////////////////////

     UNTYPED_ARRAY = 25,               // can vectorize on append (selects minimal width)
     CHAR0_ARRAY,
     INT0_ARRAY,
     INT1_ARRAY,
     INT2_ARRAY,
     INT3_ARRAY,
     FLOAT1_ARRAY,
     FLOAT2_ARRAY,
     FLOAT3_ARRAY,
     UNTYPED_RLINK3_ARRAY = 34,
     // ULINK3_ARRAY,
     /////////////////////////
     STAMP_DATETIME_ARRAY = 35,   // (3) - width log2 bytes, for array cells. all units (3)
     STAMP_YEAR_ARRAY,            // (1)
     STAMP_MONTH_ARRAY,           // (2)
     STAMP_DAY_ARRAY,             // (2)
     STAMP_HOUR_ARRAY,            // (0)
     STAMP_MINUTE_ARRAY,          // (1)
     STAMP_SECONDS_ARRAY,         // (2)
     STAMP_MILLISECONDS_ARRAY,    // (2)
     STAMP_MICROSECONDS_ARRAY,    // (3)
     STAMP_NANOSECONDS_ARRAY = 44,// (3)
     /////////////////////////    // Remark. it's not clear t/ we need any of the 9 SPAN (REL) arrays
     // SPAN_DATETIME_ARRAY,      // (*) *can't/won't exist as a vector (it isn't rel_nanos_array)
     // SPAN_YEAR_ARRAY,          // (1)
     // SPAN_MONTH_ARRAY,         // (2)
     // SPAN_DAY_ARRAY,           // (2)
     // SPAN_HOUR_ARRAY,          // (0)
     // SPAN_MINUTE_ARRAY,        // (1)
     // SPAN_SECONDS_ARRAY,       // (2)
     // SPAN_MILLISECONDS_ARRAY,  // (2)
     // SPAN_MICROSECONDS_ARRAY,  // (3)
     // SPAN_NANOSECONDS_ARRAY,   // (3)

     /////////////////////////
     // WIDE UNITS aka ARRAY UNITS  (implemented as lesser arrays)
     /////////////////////////

     STRING_UNIT = 45,
     BIGINT_UNIT,                 // signed bigint atom decimal MSD->LSD,
     SPAN_DATETIME_UNIT = 47,     // (*) 3y5m2d6h2i1s8n *mixed somehow? dt-map? packed length 7 int64_t array?

     /////////////////////////
     // LIST
     /////////////////////////

     UNTYPED_LIST = 48,    // O(n) index and amend, unless detects regular-stride/RLINK, then O(1)-index/amend/append
     UNTYPED_SLAB4_ARRAY,  // simple presented wrapper over UNTYPED_LIST
     UNTYPED_JUMP_LIST,    // O(1) index and amend, O(n) append
     
     /////////////////////////
     // GROUPED
     /////////////////////////

     STRING_FIBER = 51,           // optional
     BIGINT_FIBER,                // optional
     SET_UNHASHED,
     SET_HASHTABLE,               // HASHSET - UNIQUE elements 
     MAP_NO_NO,
     MAP_UPG_UPG, MAP=MAP_UPG_UPG,
     MAP_YES_UPG,
     MAP_YES_YES,
     AFFINE,
     STRIDE_ARRAY = 60,
     ENUM_INTERN,
     ZIP_ARRAY,
     MEMORY_MAPPED = 63,
     FUNCTION_LAMBDA,
     FOLIO_ARRAY,
     FLIPPED_TRANSPOSED,
     TABLE,
     ATLAS,
     DISTRIBUTED_ARRAY_FOLIO,
     REPLICATED_SOCKET_THINGS,
     SORT_INDEX_withOptUNIQUE_withOptNONNULLS,

     /////////////////////////
     // NOT WRITTEN [TO BUS]
     /////////////////////////


     /////////////////////////
     // CURRENTLY UNKNOWN (maybe grouped? move if so)
     /////////////////////////
    
     COMB,
     ERROR_UNIT,
     MESSAGE,                     // use network order
     DEQUE,                       // we can probably use std::deque & postpone our own

     OUT_OF_ORDER_SLAB_FLAG,
     MEMORY_MAPPED_READONLY_SHARED_FLAG,

     /////////////////////////
     PRESENTED_TYPE_SIZE
} PRESENTED_TYPE;
inline std::ostream &operator<<(std::ostream &os, PRESENTED_TYPE c) { return os << static_cast<int>(c); }

enum CAPPED_WIDTH_TYPE : UC {
     CAPPED_WIDTH_0 = 0,
     CAPPED_WIDTH_1 = 1,
     CAPPED_WIDTH_2 = 2,
     CAPPED_WIDTH_3 = 3,
     CAPPED_WIDTH_4 = 4, CAPPED_WIDTH_BOUNDLESS = CAPPED_WIDTH_4,
     CAPPED_WIDTH_RESERVED_0, // Idea. CAPPED_WIDTH_UNSPECIFIED/UNMANAGED
     CAPPED_WIDTH_RESERVED_1,
     CAPPED_WIDTH_RESERVED_2,
};
inline std::ostream &operator<<(std::ostream &os, CAPPED_WIDTH_TYPE c) { return os << static_cast<int>(c); }

typedef enum ERROR_TYPE_MEMBERS { ERROR_DISK=1, //TODO: fix items in place as errors can be saved/looked at
  ERROR_FILE, ERROR_VMEM, ERROR_DEPTH, ERROR_CTRL_C, ERROR_SIZE, ERROR_SIGN,
  ERROR_LENGTH, ERROR_REFERENCE, ERROR_VARIABLE, ERROR_RANK, ERROR_INDEX, ERROR_ARITY,
  ERROR_VALENCE, ERROR_REPEAT, ERROR_ARGS, ERROR_CONFORMABLE, ERROR_OBJECTTYPE, ERROR_STRING,
  ERROR_ARRAY, ERROR_MAP, ERROR_TABLE, ERROR_KEYS, ERROR_COLUMN, ERROR_ROW, ERROR_RAGGED,
  ERROR_LEX_UNKNOWN, ERROR_LEX_INCOMPLETE, ERROR_PARSE_UNKNOWN,
  ERROR_PARSE_UNMATCH, ERROR_PARSE_INCOMPLETE, ERROR_PARSE_DEPTH,
  ERROR_PARSE_OVERMATCH, ERROR_PARSE_MISMATCH, ERROR_PARSE_NESTED_SQL,
  ERROR_PARSE_DERIVED, ERROR_PARSE_LAMBDA_ARGS, ERROR_PARSE_SQL_VALUES,
  ERROR_PARSE_NUM_VAR, 
  ERROR_SUBERROR, ERROR_NET, ERROR_HOST, ERROR_DYLIB, ERROR_PARALLEL, 
  ERROR_ATLAS_MAP, ERROR_TIME, ERROR_JSON_KEY, ERROR_RADIX, ERROR_FORMAT_STRING,
  ERROR_ZIP, ERROR_REMOTE_HUP, ERROR_FORKED_VERB, ERROR_MISSING,
  ERROR_MEMORY_MAPPED, ERROR_CAPPED_APPEND,
} ERROR_TYPE;

typedef enum DRIVE_ACCESS_KIND_MEMBER : UC {
  DRIVE_ACCESS_KIND_MMAP_SHARED_WRITEABLE             = 0, // ¬MAP_PRIVATE ¬RLOCK_FILE  PROT_WRITE  MMAP_RLINKS  EARLY_QUEUE
  DRIVE_ACCESS_KIND_MMAP_PRIVATE_READONLY             = 1, //  MAP_PRIVATE  RLOCK_FILE ¬PROT_WRITE ¬MMAP_RLINKS  EARLY_QUEUE
  DRIVE_ACCESS_KIND_MMAP_PRIVATE_WRITEABLE            = 2, //  MAP_PRIVATE  RLOCK_FILE  PROT_WRITE  MMAP_RLINKS ¬EARLY_QUEUE
  DRIVE_ACCESS_KIND_POSIX_READ_FLUSH                  = 3, // read() a whole file/dir/multifile
  DRIVE_ACCESS_KIND_READ_WORKSPACE                    = 4, // Question. are we using this?
  DRIVE_ACCESS_KIND_POSIX_WRITE_FLUSH_SINGLEFILE      = 5, // write() a whole file
  DRIVE_ACCESS_KIND_WRITE_DIRECTORY                   = 6, // write() a whole file, directory-expand RLINKs in layout, recursively
  DRIVE_ACCESS_KIND_WRITE_MULTIFILE                   = 7, // write() a whole file, expand RLINKs in layout at depth d into d-numbered sibling files in same directory
  DRIVE_ACCESS_KIND_WRITE_WORKSPACE                   = 8, // literal MEMORY_MAPPEDs, plus w/e we want in terms of expansion (dir vs. multifile)
} DRIVE_ACCESS_KIND; 
inline std::ostream &operator<<(std::ostream &os, DRIVE_ACCESS_KIND c) { return os << static_cast<int>(c); }

typedef enum HASH_METHOD_MEMBER : UC {
  HASH_METHOD_UNSPECIFIED = 0,
} HASH_METHOD; 
inline std::ostream &operator<<(std::ostream &os, HASH_METHOD_MEMBER c) { return os << static_cast<int>(c); }




}
