#pragma once

namespace KERF_NAMESPACE {

// Remark. the issue with thread-local storage is that [reportedly, on older versions] every access requires a semaphore capture and release and some other OS calls. I haven't tested whether it's still slow. What we want is simply a way for a thread to QUICKLY get an integer in [0,...,31] that represents its NORMALIZED thread id (in this case for 32 threads). One way we might try is to create a global static array of pthread values (32) and then check for equality to the existing thread (currently active). this takes #thread accesses, but is lock free (right?) and presumably is going to be faster than using thread local storage, provided perhaps that the equality check is not bogus and slow (eg, needs semaphores). I suppose one problem to take into consideration is, if you ever decide to spawn sub-threads on the main threads (we may! parallel algorithms, etc.) then our system here may break - we need some way to performantly figure out what the master threads are, which have their own lock-free memory pools. There also seem to be some implementation dependent methods for getting thread ids (I'm guessing they're not normalized) which we may be able to combine into a portable function: pthread_getthreadid_np, pthread_self, syscall(SYS_gettid), gettid(), getpid(), syscall(__NR_gettid), and so on. | Perhaps we can sidestep all this by tying the memory-pools to PROCESSORS (or processor threads) or CORES and then we could rely on something like pthread_getaffinity_np or cpu_set_t. See std::thread::hardware_concurrency. See https://stackoverflow.com/questions/280909 and https://eli.thegreenplace.net/2016/c11-threads-affinity-and-hyperthreading/  Linux has sched_getcpu and MacOS has this (CPUID): https://stackoverflow.com/questions/33745364  exploiting the CPUID processor instruction seems to be the way to go here. everything fits. the instruction is performant, it's all lock free, you get the minimal number of pools necessary, it works with multiple threads per cpu, etc.
// Note. In the case of my macbook air, i have 2 cores with 2 hardware threads each, for a total of 4 "cpus". I think the differentiator is that threads inside a core share cache lines, so you may want to take that into consideration in later architecture. 
// Question. Do we actually get a guarantee that software threads on a single hardware thread won't clobber each other (eg, contending for a memory pool)? I don't think that we do? I suppose as long as we're doing this on a per-kerf-stack basis, and that subsequently spawned sub-threads (from stack threads) don't allocate kerf memory, then we'd be OK. (It's a separate problem, I think. It shouldn't be that hard to make the pools lock-free, at least on allocation. On repooling, I dunno. But if that was the case then we'd already have a working lock-free global memory pool...) 
// Remark. To set affinity before creating a pthread use pthread_attr_setaffinity_np | You may also want to specifically set the affinity of the PROCESS versus the threads, which on linux is sched_setaffinity
// Remark. Seems like MacOS is a punk about setting affinity. So that path may not work. We could still perhaps return to a per-thread memory allocator pattern... Or always depend on a global thread-safe allocator.....
// Idea. We could also have threads capture a position in a normalized static array {[0]=>4584734, [1]=>0/-1, ...} provided we make WRITES to the threadid int atomic when it's not 0 or -1. When a thread is created it captures its place, when destroyed it cleans it up. See for instance pthread_cleanup_push NOTE: this assumes getting an unnormalized threadid is not a slow operation ! like one that depends on a semaphore or thread-local memory (generally, it appears cheap, fortunately https://stackoverflow.com/a/15008330 ) Someone else used `std::map<std::thread::id, pid_t> threads;` also std::this_thread::get_id() for maps and such Note. We can special case thread zero. See std::atomic::compare_exchange_strong
// Idea. We can just build one big pool that has (as minimum attributes) {slab_width} x {normalized_thread_id} as lane identifiers, and then we never have contention issues
// Remark. If each thread has its own pool, and you can't reclaim atoms properly [because they're fragmenting allocations to below the page size], you may leak a lot of memory if you spawn a lot of threads. We can skirt this issue by making a page (PAGE_SIZE_BYTES) the minimum allocation size (assuming you're simply using munmap). This may gain in the end, especially if the creation of vectors takes place against the atom, then you don't ever have to step through the minimal sub-page allocations. Note that we can still mmap a lot of "atoms" at once in a single allocation (#atoms X PAGE_SIZE_BYTES), slice em up, and depool them to the application, and then *we are allowed* to munmap them one at a time [the sliced ones] when we are ready to shut down the pool (because the pool is thread-based or something), because you are allowed to munmap individual pieces like that. This offloads the reassembly of the fragmented memory to the operating system. | If we save a constant set of memory pools for normalized threads ids, then fragmentation really doesn't matter since we assume some thread will come as use it "later." If it really bothers you, or if we detect it happening over time, you can lock several pools and then rebalance them.
// Idea. We could also use malloc for sub-page-sized allocations. In that case, you don't split it up for the pool lane, you just ask for the size you need and then `free`() it later
// Note. See also google's tcmalloc
// Remark. When a thread acquires its normalized id, you can use that id for memory pool, kerf VM, etc.
// Remark. List of multithreading stuff.
//  C11 _Atomic and friends, <stdatomic.h> atomic_compare_exchange_strong
//  C11 threading additions https://en.cppreference.com/w/c/thread
//  C++11 <Atomic> and friends, <mutex>, <future>, <thread> etc.
//  C++11 threading additions
//  clang/GNU __sync and __atomic built-ins
//  __thread/thread_local/_Thread_local (historically slow, at one time needed semaphore, unknown today)
//  linux (eg <asm-x86_64/atomic.h>) /osx (GCD/MACH/OSAtomic.h)/bsd specific operations
//  (posix) pthread specific ops, mutex and so on
// Idea. Thread spawing: 
//                      -1. once globally: initialize/warm cache on all projected vms/pools/* (could be implemented using pthreads' `once` mechanism for items beyond the main/first/zeroeth)
//                       0. suggest CPU affinity (probably +1 offset round-robin from spawning thread's CPU) 
//                       0. set priority to real-time or whatever. pthread_setschedparam, + system specific stuff
//                       0. capture normalized id, VM/workstack/memory pool (as needed), sigsetjmp envs, 
//                       0. set cleanup function (de-register normalized id, clean up vm?)
// Question. How do we wrangle/marshall/* these threads once created? It may help to lay out our threading model: mapcores in parallel, parallel socket reading, parallel algorithms, parallel disk reads?, spawn_async_thread({lambda}), any others? It would be nice to employ/expose std::future (is this really any different from thread.join()?) and so on. Employing it...there are probably applications/initializations from kerf1 we missed. Exposing it: the spawn_async method can return a lambda that can be used (called) to wait/block on the promise.
// Note. You can pre-allocate threads and then have them "wake up" for specific tasks using pthread_cond_signal (thread pool) same as std::condition_variable::wait()
// Remark. I suspect that a thread pool will be significantly faster [than creating one-off threads] (one estimate I saw was 300x, whether it factors into [overall] perf later or not I don't know). One thing to consider is "per-core" (suggested or re-suggestible) threads so we can take advantage of processor layout on systems that support it. IOW, core lanes in the thread pool.
// Remark. Probably a case of do it both ways: threadpooled and not-threadpooled, kerf-vm and non-kerf-vm
// Remark. We could also do kind of a goofy thread pool (stand-in object) that just spawns new threads every time, and revisit it 1. if the perf ever matters 2. once we have a better idea of the mthreading requirements
// Question. Will the main thread participate in performing work or will it always offload it in order to perform management tasks? Should this be a toggle?
// Question. How do signals and siglongjmp work in the context of pthreads?
// Idea. What if we had some sort of generalizable way to call parallel operations on FOLIOs? Note. A FOLIO is essentially a simple version of a database "b-tree" structure (a segmented array) and we can play some fun tricks them them: subclassing FOLIO to provide cached values such as "overall length" (sum of lengths of subsequences; *having finer-grained write locks* (note that we could have array with this sort of thing that aren't folios, just subclassed, we can even have a special one that maintains a separate group element which tracks multithreaded write locks on an specified "interval" basis (per thread) so that you don't have to lock the entire slab; compressing these unexposed sub-arrays so that we speed up writes in the context of ZIP compression (to something closer to O(n/s) instead of O(n);  
// Remark. The upshot of this is that we should write our write-lock acquiring method in a way that accepts arguments like the kind we would pass to amend (indexes to amend, values to insert, the object in question, perhaps its parent) so that we have stubs for improving the granularity of our write-lock acquisitions later.
// Remark. C++ has a <function> header which would allow you to overload `()` for SLOPs. May have some weird side effects, not sure how useful it is.
// Remark. You can't fork() on iOS, and it seems like an undesirable thing to have in a "library" (such as we expect many kerf binaries to be) so perhaps we should avoid the use of fork() entirely in our app/threading model 
// Note. pthreads and signals:
// https://stackoverflow.com/questions/2575106/posix-threads-and-signals
// https://stackoverflow.com/questions/26198435/signal-handling-among-pthreads
// Remark. Seems like the basic option is capture the signal in one main pthread 
// [or perhaps a side thread dedicated to the purpose], blocking it in others,
// then use the main thread to communicate with all, possibly using pthread_kill to pass along the
// ctrl-c directly. (When we were using fork(), every child process received the signal.)
// alternatively, seems like some ppl use pthread_cancel
// Remark. Seems like the ctrl-c cleanup will differ depending on whether the thread has a VM/* or not.
// Question. Which threads are allowed to resume? (i.e., who has their own soft/hard_jmp_env?) (socket thread?)
// Note. We have some pthread_sigmask stuff, see $D/stuff/tech/source/kerf/src/jump.c
// Remark. We're going to want a single-threaded option too - maybe we can easily limit thread pool size to 1 (may still need extra utility threads)
// Idea. We can use mapcores to parallelize application of lambdas {[x]x+1} as before, but we can also use it to do things like primitive verbs '+' (did we already do this in the bytecode??), actually what I'm trying to get at is a sort of better thing, which is the parallel application of C functions with the appropriate signature (SLOP,SLOP)->(SLOP) as so on, such as `sum` style accumulator, we can trivially pass these as function pointers to a wrapper which parellelizes them. Of course we can choose {segmented, interleaved} application as before (which was faster? kerf1 set interleaved=false and never changed it, so presumably interleaved was slower).  (2021.03.30 the return can be a FOLIO)
// Remark. This file should be changed so that we encapsulate these thread methods [and the thread_id table?] inside a class with public/private methods


#pragma mark -

#undef fork
// fork 1. not availaible on iOS, 2. not nice for a library method to call, 3. legacy parallelism. as a result, we didn't consider `fork` in the design of our concurrency model. this likely simplified things.
pid_t fork(void) { die(fork disallowed in kerf2) }

#pragma mark - Globals
// threads that want a VM or a memory pool need to acquire a normalized id
bool The_Normalized_Thread_Id_Table_Bool[KERF_MAX_NORMALIZABLE_THREAD_COUNT];
pthread_t The_Normalized_Thread_Id_Table[KERF_MAX_NORMALIZABLE_THREAD_COUNT];
bool The_Normalized_Thread_Id_Table_Joinable[KERF_MAX_NORMALIZABLE_THREAD_COUNT];
pthread_t NULL_PTHREAD_T = {};

SUTEX The_Kerf_Tree_Parent_Sutex = {0}; // TODO um, maybe wherever this is currently present in the source, we may need locks up and down the kerf tree instead of just at this root/parent node.
SLAB* The_Kerf_Tree = nullptr;
I The_Main_Thread_Normalized_ID = 0;

// Remark. May want to bundle these into a THREAD_PACKAGE or somesuch, or move pools onto KVMs
// POP. If this gets too big on startup (the per-thread storage) we can change it to not allocate until a thread is created, say when a given normalized id is returned for the first time.
THREAD_SAFE_MALLOC_POOL* The_Thread_Memory_Pools = new THREAD_SAFE_MALLOC_POOL[KERF_MAX_NORMALIZABLE_THREAD_COUNT];
SLAB* The_Thread_VMs = new SLAB[KERF_MAX_NORMALIZABLE_THREAD_COUNT]; // this type will change to whatever it is we end up using for KVMs
#if CPP_WORKSTACK_ENABLED
  std::vector<SLOP*> *The_Cpp_Slop_Workstacks = new std::vector<SLOP*>[KERF_MAX_NORMALIZABLE_THREAD_COUNT]; // TODO move these onto Thread_VMs, or maybe not if we don't intend to transmit them
  std::vector<REGISTERED_FOR_LONGJMP_WORKSTACK*> *The_Cpp_Generic_Workstacks = new std::vector<REGISTERED_FOR_LONGJMP_WORKSTACK*>[KERF_MAX_NORMALIZABLE_THREAD_COUNT]; // TODO move these onto Thread_VMs, or maybe not if we don't intend to transmit them
#endif

#pragma mark - Thread ID Utils

I kerf_count_hardware_threads();
I The_Cores_Count = kerf_count_hardware_threads();
I The_Suggested_Core_Counter = 0;

auto kerf_get_unnormalized_thread_id();
I    kerf_acquire_normalized_thread_id(bool checks_existing = false);
I    kerf_retrieve_acquired_normalized_thread_id_bypass_cache();
void kerf_set_cached_normalized_thread_id(I id);
I    kerf_get_cached_normalized_thread_id();
void kerf_release_normalized_thread_id();

#pragma mark - CPU Utils

I kerf_get_currently_executing_cpu();
void kerf_suggest_bind_thread_to_cpu(I cpu_num = -1);

// Backoff/relax/yield inspired by oneTBB Intel

static inline void kerf_yield()
{
  sched_yield(); // scheduler yield, not ARM YIELD
  // std::this_thread::yield()
  // windows has SwitchToThread();
  // Question. Does usleep/nanosleep/udelay have any application here? Answer. I think no, though I used it in the past for similar purposes, this seems frowned upon/ineffective.
}

static inline void kerf_cpu_relax(I delay = 1)
{

#if SCHEDULER_YIELD_OVER_CPU_PAUSE
  kerf_yield();
  return;
#endif

  // Execute a PAUSE[/YIELD on arm] instruction `delay` times on a given arch

  while(delay-- > 0)
  {
  #ifdef _mm_pause
    _mm_pause();
  #elif __has_builtin(__builtin_ia32_pause)
    __builtin_ia32_pause();
  #elif __ARM_ARCH_7A__ || __aarch64__
    __asm__ __volatile__("yield" ::: "memory");
  #else
    (void)delay;
  #endif
  }

  return;
};

struct BACKOFF {
  // Exponential backoff
  static constexpr char YIELD_AT_COUNT = 5; 
  // at a max of m, you've done -1+2^m. So m==5 -> 31 here, plus overhead.
  // this count is from the Intel code, but see https://news.ycombinator.com/item?id=21962077
  // or https://webkit.org/blog/6161/locking-in-webkit/ which corroborate
  // the constant 50 used in linux ticket proportional backup also corroborates
  // https://lwn.net/Articles/531254/
  char count = 0;
  static_assert(YIELD_AT_COUNT <= std::numeric_limits<decltype(count)>::max());

  auto pause() {
    if (count < YIELD_AT_COUNT)
    {
      // POP you can add jitter by doing a trivial hash of the pointer address
      // for `this` and then say ORing all hash bits of lower order than the
      // pow2 bit produced by the `count` left-shift. Or even hashing into
      // that sized field.
      kerf_cpu_relax(1LL << count++);
    } else {
      // Pause is so long that we might as well yield CPU to scheduler.
      kerf_yield();
    }
    return *this;
  }
};

#pragma mark - Mutex

struct MUTEX_UNTRACKED
{
public:
  pthread_mutex_t pmutex;
  pthread_mutexattr_t attr;

  decltype(PTHREAD_PROCESS_SHARED) pshared = PTHREAD_PROCESS_PRIVATE;
  decltype(PTHREAD_MUTEX_RECURSIVE) mtype = PTHREAD_MUTEX_ERRORCHECK; // PTHREAD_MUTEX_NORMAL PTHREAD_MUTEX_DEFAULT
  decltype(PTHREAD_PRIO_PROTECT) protocol = PTHREAD_PRIO_NONE; // PTHREAD_PRIO_INHERIT
#ifdef PTHREAD_MUTEX_ROBUST
  decltype(PTHREAD_MUTEX_STALLED) robust = PTHREAD_MUTEX_ROBUST; // Question. reports that this needs PTHREAD_PROCESS_SHARED ?
#endif 

public:

  MUTEX_UNTRACKED(bool recursive = false);
  ~MUTEX_UNTRACKED();
  MUTEX_UNTRACKED(const MUTEX_UNTRACKED &) = delete;

  static void handle_error_en(int en, const char* msg);

  // Use lock_safe_wrapper over [try]lock/unlock whenever possible because it handles "recursion" transparently
  template<typename L> int lock_safe_wrapper(L &&lambda);
  int unlock();

  struct SILLY_GUARD : REGISTERED_FOR_LONGJMP_WORKSTACK
  {
    MUTEX_UNTRACKED *mutex;
    bool should_unlock = true;

    SILLY_GUARD(MUTEX_UNTRACKED *m, bool should = true) : mutex(m), should_unlock(should) { }
    ~SILLY_GUARD()
    {
      if(should_unlock)
      {
        mutex->unlock();
      }
    }
  };

protected:
  /////////////////////////////////////
  // Remark. Force developers to use wrapper or lock-guard,
  // as maintaining concurrency correctness with lock/unlock family is too hard.
  // We're cheating a little bit currently as `unlock` is exposed so that we
  // can handle it the old way, but we should look at making `unlock` protected too.
  // Similarly, THREAD_POOL is correct (enough) but doesn't need to be changed,
  // though it could be improved with lock-guards as well.
  template <typename> friend struct THREAD_POOL;
  int lock();
  int trylock();
  /////////////////////////////////////

  int locker(decltype(pthread_mutex_trylock) arg_func = pthread_mutex_lock);
  int unlock_without_pop();
  void alloc_mutex(bool recursive = false);
  void dealloc_mutex();

};

struct MUTEX_TRACKED : MUTEX_UNTRACKED, REGISTERED_FOR_LONGJMP_WORKSTACK
{
  using MUTEX_UNTRACKED::MUTEX_UNTRACKED; // inherit constructors
};

MUTEX_UNTRACKED The_Normalized_Thread_Id_Mutex;


} // namespace
