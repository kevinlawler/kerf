#pragma once

//////////////////////////////////////////////////////////////
// Testing Settings //////////////////////////////////////////
#ifndef DEBUG
  #define NDEBUG 1 // disables `assert()`. do not define 0
  #define DEBUG 0
#endif
#if DEBUG
  #define TEST_MACROS (true)
#else
  #define TEST_MACROS (false)
#endif

#define TEST_DRIVE_CASES                        (false && TEST_MACROS) // Feature. later, separate this as a command line -D define like -DDEBUG. Separate kerf tests into "unit" and "full"
#define TEST_TRACK_ALLOCATIONS                  (true && TEST_MACROS)
#define TEST_TRY_POOL_ZEROES_DEALLOC            (true && TEST_MACROS)
#define TEST_TRY_POOL_NEVER_REUSES_STRUCTS      (false && TEST_MACROS)
//////////////////////////////////////////////////////////////
#define CONSOLE                                 (true)   // see kerf1 CONSOL defines for expansion
#define KERF_NAMESPACE                          kerf 
#define FILENAME_BINARY_EXTENSION               (".dat") // kerf objects I write to drive end in this filename extension
#define FILENAME_MULTIFILE_SEPARATOR            ("|")    // Try '|' or '-'. Dash is maybe too common (collisions). Dot ('.') would interfere w/ extensions
#define FILENAME_DIRECTORY_BASE                 ("_self")
#define FILENAME_WORKSPACE_MARKER               ("_workspace")
//////////////////////////////////////////////////////////////
// User-editable Toggleable Compiler Constants ///////////////
#define SLOP_DESTRUCT_IS_CRITICAL_SECTION       (true)  // true: slower and ctrl-c leaks fewer objects; false: faster and ctrl-c leaks more objects.
#define PERMIT_SEGV_LIKE_IN_CRITICAL_SECTION    (true)  // when false, you'll hang silently on a SIGSEGV and it will look like a deadlock, but we'd rather crash
#define CPP_WORKSTACK_DESIRED                   (true)
#define PREFERRED_RLINK3_SLAB4_FLAT_JUMP        (0)
#define KERF_MAX_NORMALIZABLE_THREAD_COUNT      (12)    // (2048) // we may elect to tie this to system limits or CPU count or somesuch. I think maybe even, we want > c*(1+c) where c==#cpus, one thread for each vm, and then a way for a vm to use the other processors. but this ignores other utility threads
#define EARLY_QUEUE_DEFAULT_CAPACITY            (8)     // 1234. the higher, the more performance. should probably be based off whatever the system allows us to have maximum. we want it higher than GENERIC_DEPTH_LIMIT (if recursively opening handles during directory write)
#define INT_INFS_AND_NANS_SQUASH                (true)  // should a 64-bit int inf become a 32-bit int inf?
#define INT_INFS_AND_NANS_UNSQUASH              (true)  // should a 32-bit int inf become a 64-bit int inf?
#define INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS     (0)     // {0,1,2,3} minimal log byte width of chunks. When 0, int0 has infs and nans.
#define INT_CAPPED_APPEND_STYLE                 (CAPPED_APPEND_CLIPS)
#define INT_CAPPED_APPEND_CLIPS_TO_NEG_INF      (true)
#define FLOAT_NANS_COMPARE_AS_SMALLEST          (true)
#define NUMERIC_COMPARE_INT_FLOAT               (true)
#define LAYOUT_PACKED_CROSSOVER_LOG_WIDTH       (LAYOUT_SMALLCOUNT_MAX_FILLED_LOG_WIDTH)
#define POOL_LANE_STRUCT_MIN                    (6)     // TODO apparently, there's more to import from kerf1 here? // POP programmatically tie to cache line size?
#define LENGTH_1_LISTS_RECONCILE_WITH_N         (true)
#define BYTE_MSBIT0_TO_LSBIT7_IF_TRUE           (true)
#define CTRL_C_EXIT_CODE                        (130)   // bash ctrl+c script exit code
#define SECURITY_ALLOW_LONGJMP_FROM_SIGNAL      (true)  // technically, CTRL+C is a risk (sendmail bug) | Note: "POSIX.1-2008 Technical Corrigendum 2 adds longjmp() and siglongjmp() to the list of async-signal-safe functions."
#define TRUTHTABLE_VALUE_FOR_NIL                (false)
#define THREAD_POOL_SUPPORTS_FUTURES            (true)  // TODO. I didn't have time to see if the C++ futures implementation does "bad stuff" or is just simple objects without mutex/threads/shared-leaks/etc.
#define DATES_ALLOW_DASHED                      (false)
#define GENERIC_DEPTH_LIMIT                     (100)
#define SCHEDULER_YIELD_OVER_CPU_PAUSE          (false)
//////////////////////////////////////////////////////////////
#define PARSE_RESERVED_CASE_INSENSITIVE         (true)
#define PARSE_MAX_DEPTH                         (GENERIC_DEPTH_LIMIT)
#define RNG_USES_RANDOMIZED_SEED_ON_INIT        (true)
#define HASH_USES_RANDOMIZED_SEED_ON_INIT       (false)
#define HASH_MODS_OUT_DICTIONARY_ORDER          (true)
#define HASH_AVOID_TYPE_COLLISIONS              (true)
#define HASH_CPP_TYPE                           UI4     // alternatively, UI3, I4, or I3
//////////////////////////////////////////////////////////////
// Non-toggle Compiler Constants /////////////////////////////
#define SLAB_ALIGN                              (8)          // Align is determined by the maximal width of the union's primitive types, in this case I or F and so on
#define MINIMAL_ALIGN                           (SLAB_ALIGN) // If we allow smaller layout types/atoms, such as 4 == maximal I2/int32_t/F2/float32, we can use s/t smaller than SLAB_ALIGN
#define LOG_SLAB_WIDTH                          (4)
#define MEMORY_UNPERMISSIONED_TENANT_NOEXPAND   (0)
//////////////////////////////////////////////////////////////
#define SUTEX_BIT_OFFSET                        (3)
#define SUTEX_BIT_LENGTH                        (13) // bits:max_threads {12:1024, 13:2048, 14:4096, 15:8192, 16:16384}
#define SUTEX_FLAG_BITS                         (1)
#define SUTEX_MAX_ALLOWED_READERS               (SUTEX_COUNTER_MAX) // reclaimable combinations
#define SUTEX_COUNTER_BITS                      (SUTEX_BIT_LENGTH - SUTEX_FLAG_BITS)
#define SUTEX_COUNTER_MAX                       (POW2(SUTEX_COUNTER_BITS - 1) - 1)
#define SUTEX_COUNTER_MIN                       (-SUTEX_COUNTER_MAX - 1)
#define SUTEX_ROUND_BYTE_WIDTH                  (sizeof(SUTEX_CONTAINER))
#define SUTEX_ROUND_BIT_WIDTH                   (SUTEX_ROUND_BYTE_WIDTH * CHAR_BIT)
#define SUTEX_BIT_REMAIN                        ((CHAR_BIT - (SUTEX_BIT_LENGTH % CHAR_BIT))%CHAR_BIT - SUTEX_BIT_OFFSET)
//////////////////////////////////////////////////////////////
#define SYSTEM_LONGJMP_UNWINDS                  (false) // TODO. some platforms apparently have a stack unwind baked in. possible to detect at runtime. NB. this isn't quite right, because ctrl-c (!= error throw) still wants unwinds I think
#define CPP_WORKSTACK_ENABLED                   (CPP_WORKSTACK_DESIRED && !SYSTEM_LONGJMP_UNWINDS)
#define CAPPED_APPEND_C_ASSIGN                  (0)     // let C do a narrowing assign (implementation-defined behavior)
#define CAPPED_APPEND_CLIPS                     (1)     // clip to the max (min) value of the narrower width. P_O_P: see https://stackoverflow.com/a/33482123
#define CAPPED_APPEND_REJECTS                   (2)     // throw an error
#define LAYOUT_SMALLCOUNT_MAX_FILLED_LOG_WIDTH  (8)     // we've "precomputed" this instead of dynamically calculating in the cpp style
#define STRING_QUOTE_FRONT                      ("\"")
#define STRING_QUOTE_BACK                       ("\"")
#define STRING_QUOTE_ESCAPE_QUOTE               ("\"")  // use "\\" to no-op
#define CHAR_UNIT_QUOTE_FRONT                   ("'")
#define CHAR_UNIT_QUOTE_BACK                    ("'")
#define CHAR_UNIT_QUOTE_ESCAPE_QUOTE            ("'")   // use "\\" to no-op
#define CHAR_ARRAY_QUOTE_FRONT                  ("`")
#define CHAR_ARRAY_QUOTE_BACK                   ("`")
#define CHAR_ARRAY_QUOTE_ESCAPE_QUOTE           ("`")   // use "\\" to no-op
#if   PREFERRED_RLINK3_SLAB4_FLAT_JUMP == 0
  #define PREFERRED_MIXED_TYPE                  UNTYPED_RLINK3_ARRAY
  #define PREFERRED_MIXED_CLASS                 A_UNTYPED_RLINK3_ARRAY
  // #define PREFERRED_MIXED_LAYOUT               LAYOUT_TYPE_COUNTED_VECTOR_PACKED
#elif PREFERRED_RLINK3_SLAB4_FLAT_JUMP == 1
  #define PREFERRED_MIXED_TYPE                  UNTYPED_SLAB4_ARRAY
  #define PREFERRED_MIXED_CLASS                 A_UNTYPED_SLAB4_ARRAY
  // #define PREFERRED_MIXED_LAYOUT               LAYOUT_TYPE_COUNTED_LIST_PACKED
#elif PREFERRED_RLINK3_SLAB4_FLAT_JUMP == 2
  #define PREFERRED_MIXED_TYPE                  UNTYPED_LIST
  #define PREFERRED_MIXED_CLASS                 A_UNTYPED_LIST
  // #define PREFERRED_MIXED_LAYOUT               LAYOUT_TYPE_COUNTED_LIST_PACKED
#elif PREFERRED_RLINK3_SLAB4_FLAT_JUMP == 3     // Warning. This unusual speculative testing setting depends on [at least] having some form of supported append() (either total rewrite or "untracked")
  #define PREFERRED_MIXED_TYPE                  UNTYPED_JUMP_LIST
  #define PREFERRED_MIXED_CLASS                 A_UNTYPED_JUMP_LIST
  // #define PREFERRED_MIXED_LAYOUT               LAYOUT_TYPE_COUNTED_JUMP_LIST
#endif
//////////////////////////////////////////////////////////////
#define TWO(x,y) (((x)<<8)|(y))
//////////////////////////////////////////////////////////////

#ifndef __has_builtin  // both clang and gcc support this
#define __has_builtin(x) 0
#endif

#define __kerfthread thread_local // this is a port from Kerf1. normally we use normalized thread ids.

#include <atomic>
#include <cfloat>
#include <deque>
#include <compare>
#include <iostream>
#include <fcntl.h>
#include <filesystem>
#include <functional>
#include <future>
#include <fstream>
#include <math.h>
#include <map>
#include <new>  
#include <random>
#include <set>
#include <setjmp.h>
#include <shared_mutex>
#include <signal.h>
#include <sstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <vector>

//// Third-party //////////////////////////
#if DEBUG
  #include <gtest/gtest.h> // macOS: $ brew install googletest
#endif
#include "pcg/pcg_random.hpp"
#include <event2/event.h>
#include <editline/readline.h>
///////////////////////////////////////////

#ifdef __linux
// #include <sys/eventfd.h>
#endif

#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__)
// #include <netinet/in.h>
#endif

#ifdef __MACH__ //Apple OSX
// #include <mach/clock.h>
// #include <mach/mach.h>
// #include <netinet/tcp.h>
// #include <sys/sysctl.h>
// 
// #include <Accelerate/Accelerate.h>
// #undef vA
// #undef vs
// #undef si

#endif

#if defined (__i386__) || defined (__x86_64__)
  #include <immintrin.h>
#endif


//POTENTIAL_OPTIMIZATION_POINT
//almost certainly doesn't matter, but Hacker's Delight, as well as Stanford bithacks, has
//the bytewise ops for ABS, etc.
#ifndef ABS
#define ABS(x)     ((x) < 0 ? -(x) : (x))
#endif
#ifndef SIGN
#define SIGN(x)    ((x) < 0 ? -(1) : (1))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#if __has_builtin(__builtin_isnan)
#undef  isnan                        //see: https://github.com/kevinlawler/kerf-source/issues/34
#define isnan(x) __builtin_isnan(x)  //this gives a 4x speedup for eg `sum()` on Linux. 
#endif

#if __has_builtin(__builtin_isinf)
#undef  isinf                        
#define isinf(x) __builtin_isinf(x)  
#endif

#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))
#define SLAB_ALIGNED(x) (0 == ((x) % SLAB_ALIGN))

#define II3 (INT64_MAX) // I Infinity (Use -II for I Negative Infinity)
#define IN3 (INT64_MIN) // I Null (one less than -II)
#define II2 (INT32_MAX)
#define IN2 (INT32_MIN)
#define II1 (INT16_MAX)
#define IN1 (INT16_MIN)
#define II0 (INT8_MAX)
#define IN0 (INT8_MIN)
#define II  (II3)
#define IN  (IN3)

#define FI3 (((F3)1.   )/((F3)0.   )) // IEEE should work everywhere 
#define FN3 (((F3)0.   )/((F3)0.   )) // Alternatively, nan("")
#define FI2 (((F2)1.f  )/((F2)0.f  ))
#define FN2 (((F2)0.f  )/((F2)0.f  ))
#define FI1 (((F1)1.f16)/((F1)0.f16))
#define FN1 (((F1)0.f16)/((F1)0.f16))
#define FI  (FI3)
#define FN  (FN3)

#ifndef FLT16_DIG
#define FLT16_DIG (3)
#endif

#define RTIME(d,...) {d=clock();{__VA_ARGS__;}d=(clock()-d)/CLOCKS_PER_SEC;} //Note: sleep() does not count against clock()
#define TIME(...) {F _d; RTIME(_d,__VA_ARGS__); fprintf(stderr,"[DEBUG] Elapsed:%.7f\n",_d);}
// #define er(...) {fprintf(stderr, "[DEBUG] %s:%u: %s\n",__FILE__, __LINE__, #__VA_ARGS__);}
#define er(...) {kerr() << "[DEBUG] " << __FILE__ << ":" << __LINE__ << " " << #__VA_ARGS__ << "\n";}
#define die(...) {{er(__VA_ARGS__);} abort();}
#define DO(n,...) {I i=0,_i=(I)(n);for(;i<_i;++i){__VA_ARGS__;}}
#define DO2(n,...){I j=0,_j=(I)(n);for(;j<_j;++j){__VA_ARGS__;}}
#define DO3(n,...){I k=0,_k=(I)(n);for(;k<_k;++k){__VA_ARGS__;}}

// Observation. 2021.10.17 So, popping workstacks before jmping solved a certain problem where (I think) stack data was getting corrupted from the frame around where the ERROR(.) was called, so the thinking here [in adding the workstack pop] was to do it as soon as possible before anything could get corrupted. Technically, leaving any automatic objects on the stack before jmping is supposed to be undefined behavior in C++, so perhaps it's worth considering switching to try/catch, or offering that as a compile-time toggle. 
#define ERROR(x) ({if(TEST_MACROS)er(<-- err loc); pop_workstacks_for_normalized_thread_id(); siglongjmp(*The_Soft_Jmp_Envs[kerf_get_cached_normalized_thread_id()], x); NULL;})

namespace KERF_NAMESPACE {

// forward declarations
struct SLAB;
struct SLOP;
struct LAYOUT_BASE;
struct PRESENTED_BASE;
struct ITERATOR_LAYOUT;
struct ITERATOR_PRESENTED;
struct A_MEMORY_MAPPED;
struct REGISTERED_FOR_LONGJMP_WORKSTACK;
struct FILE_REGISTRY;

typedef void* V;
typedef char C;
typedef unsigned char UC;
typedef signed char SC;
typedef __uint128_t     UI4;    // or unsigned _ExtInt(128)
typedef uint64_t    UI, UI3;
typedef __int128_t       I4;    // or   signed _ExtInt(128)
typedef int64_t      I,  I3;
typedef int32_t          I2;
typedef int16_t          I1;
typedef int8_t           I0;
typedef double       F,  F3;
typedef float            F2;
typedef _Float16         F1;    // we want IEEE with inf and nan guaranteed, so prob. not __fp16
typedef union{V V;I*I;C*C;F*F;SLAB*SLAB;SLOP*SLOP;} POINTER_UNION;

bool The_Language_Initialized_Flag = false;
} // namespace

/////////////////////////////////////////
#include "enums.h"
#include "bitops.h"
#include "jump.h"
#include "sutex.h"
#include "slab.h"
#include "memory-pool.h"
#include "cores.h"
#include "thread.h"
#include "file.h"
#include "grade.h"
#include "accessor.h"
#include "layout.h"
#include "presented.h"
#include "verbs.h"
#include "hash.h"
#include "slop.h"
#include "iterator.h"
#include "bus.h"
#include "adverbs.h"
#include "horology.h"
#include "templates.h"
#include "lexer.h"
#include "parser.h"
#include "rng.h"
#include "init.h"
#include "interpreter.h"

#include "jump.cc"
#include "cores.cc"
#include "slab.cc"
#include "thread.cc"
#include "file.cc"
#include "sutex.cc"
#include "memory-pool.cc"
#include "accessor.cc"
#include "layout.cc"
#include "presented.cc"
#include "slop.cc"
#include "verbs.cc"
#include "bus.cc"
#include "iterator.cc"
#include "grade.cc"
#include "lexer.cc"
#include "parser.cc"
#include "hash.cc"
#include "rng.cc"
#include "init.cc"
#include "interpreter.cc"
#if DEBUG
  #include "test.cc"
#endif
