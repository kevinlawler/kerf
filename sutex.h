#pragma once

namespace KERF_NAMESPACE {

#pragma mark - Sutex
// Sutex: Slab mUTEX or Signed mUTEX or your fav here
// spin rwlock with recursion/errorcheck [readers/writer] and bzero = safe [re]init/destroy in 2+ bits
// unfair, unticketed, writer-preferring, exponential PAUSE backoff into sched_yield
// ERRORCHECK: writers are stored as their normalized id (negative, one-indexed)
// RECURSIVE: reader locks are stored as an atomic count (non-negative)
//
// NB. We use readers/writers & shared/exclusive terminology interchangeably.
//
// Remark. It's tempting to want to do a #define to refactor to have the
// items from SUTEX be top-level in SLAB, but we probably don't want them
// exposed since you'll actually have to copy out[/in] SUTEX for the locking
// algorithms
//
// Remark. 16 bits is overkill for our mutex, around 12-13 should be enough.
// Reclaiming those bits is possible by creating a union on the second byte
// here and then marking the alternatives for Atoms, Vectors, etc. See
// example. Similarly, attribute bits for PRESENTED types can also be
// reclaimed.
//
// Idea. Feature. You can also change [hand off] the sutex write owner by changing it
// from your thread id to the other thread's id. (assert() your ownership first).

#ifdef _ExtInt
  typedef signed _ExtInt(round_up_nearest_power_of_2(SUTEX_BIT_LENGTH)) SUTEX_CONTAINER;
#else //only clang++ has _ExtInt currently 2021.08.13
  #if   SUTEX_BIT_LENGTH <=  8
  typedef int8_t SUTEX_CONTAINER;
  #elif SUTEX_BIT_LENGTH <= 16
  typedef int16_t SUTEX_CONTAINER;
  #elif SUTEX_BIT_LENGTH <= 32
  typedef int32_t SUTEX_CONTAINER;
  #elif SUTEX_BIT_LENGTH <= 64
  typedef int64_t SUTEX_CONTAINER;
  #else
    #error "SUTEX_BIT_LENGTH set too large"
  #endif
#endif

union SUTEX {
  SUTEX_CONTAINER i; 
  struct{
  #if SUTEX_BIT_OFFSET > 0
    SUTEX_CONTAINER mutex_offset:SUTEX_BIT_OFFSET;
  #endif
    SUTEX_CONTAINER counter:SUTEX_BIT_LENGTH - SUTEX_FLAG_BITS;
    SUTEX_CONTAINER writer_waiting:SUTEX_FLAG_BITS;
  #if SUTEX_BIT_REMAIN > 0
    SUTEX_CONTAINER mutex_unused:SUTEX_BIT_REMAIN;
  #endif
  };

  static const int IGNORED_THREAD_PLACEHOLDER_ONE_ID = 1;
  static const int DEFAULT_ALLOWED_COUNTER = 0;

  bool in_use()
  {
    return writer_waiting | counter;
  }

  void zero_sutex_claimed_subset_inside_container()
  {
    // equivalent to reinitializing, deinitializing, releasing, destroying, initializing, ...
    counter = 0;
    writer_waiting = 0;
    assert(!in_use());
  }

  static SUTEX_CONTAINER sutex_counter_from_normalized_thread_id(I x)
  {
    return -(x+1);
  }

  static I normalized_thread_id_from_sutex_counter(SUTEX_CONTAINER x)
  {
    return -(x+1);
  }

  static const int SUCCESS = 0;

  struct SUTEX_GUARD : REGISTERED_FOR_LONGJMP_WORKSTACK
  {
    // NB. We want sutex to be initialized non-null first, and then should_zero
    // to switch from false to true at the very end of construction
    SUTEX *sutex = nullptr;
    const bool exclusive;
    bool should_unlock = false;
    bool ignores_thread = false;

  #if DEBUG
    std::atomic<bool> indeterminate = /* sic */ false /* sic */; 
  #endif
  
    SUTEX_GUARD (SUTEX *u, const bool like_writer = true, const bool will_ignore = false) : sutex(u), exclusive(like_writer), ignores_thread(will_ignore)
    { 
      critical_section_wrapper([&]{ // Theory. It may be ok to enter a critical section while waiting on a lock - you're not doing anything - so the guy doing a write likely won't be in a critical section, and he'll be hit by ctrl-c first 
      #if DEBUG
        indeterminate.store(true);
      #endif

        auto s = sutex_lock(sutex, exclusive, ignores_thread);
        switch(s)
        {
          default:
          case SUCCESS: should_unlock =  true; break;
          case EDEADLK: should_unlock = false; break;
        }

      #if DEBUG
        indeterminate.store(false, std::memory_order_release);
      #endif
      });

      // NB. t̶e̶c̶h̶n̶i̶c̶a̶l̶l̶y̶,̶ [without the new atomic should_zero that solves the
      // problem,] there's a minor race condition between getting the return
      // value from the lock call and updating should_unlock (in both excl. and
      // shared) You could partially ameliorate this by walking the kerf tree
      // after ctrl-c, though that ignores non-kerf-tree locks.
      // Idea. One way to solve the exclusive side is to make a slower version
      // lock-exclusive which tests for ownership before actually unlocking,
      // and call that version via a flag/toggle. You must also default
      // should_unlock to true. This doesn't solve the `shared` case though.
      // Idea. Some of this you'll have to set to "warn to stderr" not abort().
      // Idea. The way to get both excl./shared, after ctrl-c, in the presence
      // of the mentioned race condition, is not really to unlock these with
      // the guard, but to zero/re-init the sutexes they point to. You get a
      // complete list, kerf-tree and non, via the SUTEX_GUARD->sutexes stored
      // on the cpp workstack via REGISTERED_FOR_LONGJMP_WORKSTACK.
      // Idea. defaults: should_zero=true, should_unlock=false. Then after
      // setting should_unlock or not, always set should_zero = false. Then
      // in the destructor you can handle it without doing anything special.

      // Note. however, if you unwind the per-thread stacks in the wrong order,
      // this could cause problems. (thread A frees a SLAB S but you haven't
      // popped the SUTEX_GUARD in thread B that points to S's sutex yet.)
      // Answer. With separate SLAB and GENERIC workstacks, this is OK, because
      // you do the separate workstacks first. Alternatively. Walk all the
      // stacks looking for SUTEX_GUARDs, unlock or zero their locks if
      // indicated and desired, and set each guard to should_unlock = false
      // before proceeding.
    }

    ~SUTEX_GUARD();
  };

public:
  SUTEX_GUARD guard_exclusive(const bool ignores_thread = false) { return SUTEX_GUARD(this, true, ignores_thread);} 
  SUTEX_GUARD guard_shared(const bool ignores_thread = false){     return SUTEX_GUARD(this, false, ignores_thread);}
  template<typename L> auto sutex_safe_wrapper(L &&lambda, const bool exclusive = true, const bool ignores_thread = false)
  {
    SUTEX_GUARD g(this, exclusive, ignores_thread);
    return lambda();
  }
  template<typename L> auto sutex_safe_wrapper_exclusive(L &&lambda, const bool ignores_thread = false) { return sutex_safe_wrapper(lambda, true, ignores_thread); }
  template<typename L> auto sutex_safe_wrapper_shared(L &&lambda, const bool ignores_thread = false) {    return sutex_safe_wrapper(lambda, false, ignores_thread);}

public:
  static int sutex_lock_exclusive(SUTEX *u, const bool try_only = false, const I allowed_counter = DEFAULT_ALLOWED_COUNTER, const bool ignores_thread = false);
  static int sutex_try_lock_exclusive(SUTEX *u, const bool ignores_thread = false);
  static void sutex_unlock_exclusive(SUTEX *u, const bool ignores_thread = false);
  
  static int sutex_lock_shared(SUTEX *u, const bool try_only = false);
  static int sutex_try_lock_shared(SUTEX *u);
  static void sutex_unlock_shared(SUTEX *u);
  
  static int sutex_downgrade_lock(SUTEX *u, const bool ignores_thread = false);
  static int sutex_try_downgrade_lock(SUTEX *u, const bool ignores_thread = false);
  static int sutex_upgrade_lock(SUTEX *u, const bool try_only = false, const I allowed_counter = 1, const bool ignores_thread = false);
  static int sutex_try_upgrade_lock(SUTEX *u, const I allowed_counter = 1, const bool ignores_thread = false);

  static int sutex_lock(SUTEX *u, const bool exclusive = true, const bool ignores_thread = false);
  static void sutex_unlock(SUTEX *u, const bool exclusive = true, const bool ignores_thread = false);

};

struct kerr
{
  // Thread-safe error logging (logical messages will not intersperse cross-thread)
  // apparently std::osyncstream(std::cerr) does this but isn't present on my system 2021.10.25
  // you can make this work like `std::cerr` without parens by making a wrapper class that returns this as its operator<< and then storing the wrapper object in a global static  

  static SUTEX s;
  SUTEX::SUTEX_GUARD g = SUTEX::SUTEX_GUARD(&s);
  bool use_stderr = true;

  kerr(bool use_stderr = true) : use_stderr(use_stderr){}

  kerr& operator<<(auto x)
  { 
    (use_stderr ? std::cerr : std::cout) << x;
    return *this;
  }
};

struct kout : kerr
{
  kout() : kerr(0){}
};

SUTEX kerr::s = {0};

} // namespace
