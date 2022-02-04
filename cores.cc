#pragma once

namespace KERF_NAMESPACE {

#pragma mark - Thread ID Utils

I kerf_count_hardware_threads()
{
  return std::thread::hardware_concurrency();
}

auto kerf_get_unnormalized_thread_id()
{
  return pthread_self();

  // auto i = std::this_thread::get_id(); // Assumption. this is fast, eg no semaphore
  // // Remark. migrating off this method/HACK probably requires us converting the normalized table into a bool table, with an associated std::thread:id array table that contains std::this_thread::get_id(), because in general that value is not standardized cross-platform
  // assert(sizeof(I)==sizeof(i)); // less would be OK but takes addl work here
  // I *j = (I*)&i;
  // return *j;
}

I kerf_acquire_normalized_thread_id(bool checks_existing)
{
  I error = -1;
  auto u = kerf_get_unnormalized_thread_id();

  if(checks_existing) { // optional. won't affect perf, but may obscure bugs
    I check = kerf_retrieve_acquired_normalized_thread_id_bypass_cache();
    if(error != check) {
      return check;
    }
  }

  // NB. nonatomic isn't going to work if you spawn a bunch of threads at once wo/ letting them each register first
  // which brings up: an alternative way to acquire these ids is to do it in advance, then pass the index to the thread,
  // (as local data to the pthread) and they just [non-atomically] assign their thread_id to that index once they start running

  bool atomically_acquires = true;

  if (!atomically_acquires) {
    DO(KERF_MAX_NORMALIZABLE_THREAD_COUNT, 
        if(!The_Normalized_Thread_Id_Table_Bool[i]) {
          The_Normalized_Thread_Id_Table_Bool[i] = true;
          The_Normalized_Thread_Id_Table[i] = u;
          return i; 
        }
    )
  }
  else {
    I capture = error;
    The_Normalized_Thread_Id_Mutex.lock_safe_wrapper([&]{ // locking actually obviates the __sync_bool_compare_and_swap below.
      DO(KERF_MAX_NORMALIZABLE_THREAD_COUNT, 
           // #include <stdatomic.h>
           // _Atomic(I) *existing = &The_Normalized_Thread_Id_Table[i];
           // if(atomic_compare_exchange_strong(existing, &expected, u))

           // NB. I chose to use the __sync version here because we expect atomic writes to be rare,
           // and non-atomic reads to be common, and I didn't have time to look whether declaring
           // datatypes as _Atomic slowed down reads or not

           auto *existing = &The_Normalized_Thread_Id_Table_Bool[i];
           auto expected = 0;
           if(__sync_bool_compare_and_swap(existing, expected, true))
           {
             The_Normalized_Thread_Id_Table[i] = u;
             capture = i;
             break;
           }
      )
    });
    return capture;
  }

  return error;
}

I kerf_retrieve_acquired_normalized_thread_id_bypass_cache()
{
  // The nice thing about this is the main thread (0) requires only one comparison, the next thread 2 comparisons, and so on
  // This correctly prioritizes earlier threads over later.
  // Presumably this should all be faster than thread-local storage, std::map, and so on.
  // POTENTIAL_OPTIMIZATION_POINT if we limit ourselves to pthreads, pthread_key_create is [I'm guessing] probably sufficient to get our O(1) fast-as-possible cache of the normalized thread id, assuming it really uses a thread-specific stack pointer
  // POTENTIAL_OPTIMIZATION_POINT If _Thread_local and family ever get sped up, then we can trivially cache this value using them.
  // POTENTIAL_OPTIMIZATION_POINT if you intend to circumvent normalized ids, then you could probably use pthread_key to have individualized pointers [one set for each thread] to VMs and memory-pools and such, OTOH, if you want to reuse or cache or pre-initialize VMs or mempools you're still going to need a pool-of-pools solution similar to what our normalized id system provides. My guess is that "caching" the normalized thread id gives us everything that we care about (and caching itself may not even be necessary) and that the circumventing solution indicated here is a bad path.

  I error = -1;
  auto u = kerf_get_unnormalized_thread_id();
  DO(KERF_MAX_NORMALIZABLE_THREAD_COUNT, if(pthread_equal(u, The_Normalized_Thread_Id_Table[i])) return i)
  return error;
}

void kerf_set_cached_normalized_thread_id(I id)
{
  // POTENTIAL_OPTIMIZATION_POINT
  // no-op currently. stub
  // I cached_value = kerf_retrieve_acquired_normalized_thread_id_bypass_cache();
  // POP Idea. You can try __Thread and also pthread_key for caches to see if those improve anything. I would guess not.
}

I kerf_get_cached_normalized_thread_id()
{
  // POTENTIAL_OPTIMIZATION_POINT later, return the cached value
  return kerf_retrieve_acquired_normalized_thread_id_bypass_cache();
}

void kerf_release_normalized_thread_id()
{
  auto u = kerf_get_unnormalized_thread_id();
  decltype(u) erase = NULL_PTHREAD_T;

  DO(KERF_MAX_NORMALIZABLE_THREAD_COUNT, 
      if(pthread_equal(u, The_Normalized_Thread_Id_Table[i])) {
        The_Normalized_Thread_Id_Table_Bool[i] = false;
        The_Normalized_Thread_Id_Table_Joinable[i] = false;
        The_Normalized_Thread_Id_Table[i] = erase; // this should happen last
        return; // optional: you could continue & walk the table before the thread exits but this might obscure bugs
      }
  )

  // unset cached value
  kerf_set_cached_normalized_thread_id(-1);
}

#pragma mark - CPU Utils

I kerf_get_currently_executing_cpu()
{
  er(TODO current cpu);
  return 0;
  // POTENTIAL_OPTIMIZATION_POINT this is all copy-pasta and I haven't gone over it in depth
  // an alternate method is __cpuid (gnu?)
  // whatever we're doing should be as fast as a single branchless instruction (the `CPUID` instruction)
  // just looking at it we could probably remove some of the branching below

  // more gnu stuff
    // int eax, ebx, ecx, edx;
    // char vendor[13];
    // __cpuid(0, eax, ebx, ecx, edx);

  // linux seems to have for instance: https://stackoverflow.com/a/2215105
  
  //  this works for intel macs but not arm as of 2021.07.20
  
  // #include <cpuid.h>
  // #ifdef __MACH__ //Apple OSX
  // 
  // #define CPUID(INFO, LEAF, SUBLEAF) __cpuid_count(LEAF, SUBLEAF, INFO[0], INFO[1], INFO[2], INFO[3])
  // 
  // #define GETCPU(CPU) {                              \
  //         uint32_t CPUInfo[4];                           \
  //         CPUID(CPUInfo, 1, 0);                          \
  //         [> CPUInfo[1] is EBX, bits 24-31 are APIC ID <] \
  //         if ( (CPUInfo[3] & (1 << 9)) == 0) {           \
  //           CPU = -1;  [> no APIC on chip <]             \
  //         }                                              \
  //         else {                                         \
  //           CPU = (unsigned)CPUInfo[1] >> 24;                    \
  //         }                                              \
  //         if (CPU < 0) CPU = 0;                          \
  //       }
  //   int c = 0; 
  //   GETCPU(c);
  //   return c;
  // #endif
}

void kerf_suggest_bind_thread_to_cpu(I cpu_num)
{
  if(-1 == cpu_num)
  {
    I i = __sync_add_and_fetch(&The_Suggested_Core_Counter, 1);
    cpu_num = i % The_Cores_Count;
  }

  // POTENTIAL_OPTIMIZATION_POINT (this real time is the "bounded duration but may be slow" type not the "really fast" type). indicate in thread attributes that it needs "real-time" priority or some other high and appropriate priority
  // on macos see set_realtime https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/KernelProgramming/scheduler/scheduler.html#//apple_ref/doc/uid/TP30000905-CH211-BABCHEEB
  // thread affinity:
  // https://developer.apple.com/library/archive/releasenotes/Performance/RN-AffinityAPI/#//apple_ref/doc/uid/TP40006635-CH1-DontLinkElementID_2
  // thread_policy_set(pthread_mach_thread_np(pthreadId))
#ifdef __MACH__ //Apple OSX

#endif

  // TODO. this

  // pthread_t threads[numberOfProcessors];
  // 
  // pthread_attr_t attr;
  // cpu_set_t cpus;
  // pthread_attr_init(&attr);
  // 
  // CPU_ZERO(&cpus);
  // CPU_SET(cpu_num, &cpus);
  // pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
  // pthread_create(&threads[i], &attr, DoWork, NULL);
}

#pragma mark - Mutex

MUTEX_UNTRACKED::MUTEX_UNTRACKED(bool recursive)
{
  alloc_mutex(recursive);
}

MUTEX_UNTRACKED::~MUTEX_UNTRACKED()
{
  dealloc_mutex();
}

void MUTEX_UNTRACKED::handle_error_en(int en, const char* msg)
{
  errno = en;
  perror(msg);
}

template<typename L> 
int MUTEX_UNTRACKED::lock_safe_wrapper(L &&lambda)
{
  // Since we handle EDEADLK here you can call this method "recursively" whereas you can't
  // really with [try]lock/unlock, at least automatically
  const int SUCCESS = 0;
  int s;

  // NB. there is probably a neglible race condition here which we can ignore,
  // namely that between the call to lock() and the storing in the guard with
  // should_unlock, we are exposed. Moving this logic into the lock guard does
  // not solve the problem by itself. Heavy-duty or slow solns are not worth
  // it, given the limited applicability. (The minor risk is that you will be
  // hit by an outside signal.) Update. OK, so now we're solving this by having
  // SILLY_GUARD default to unlocking, so the risk is that you perform an
  // extraneous unlock during cleanup and maybe the error-checking in
  // MUTEX::unlock complains to stderr about an unnecessary unlock.

  SILLY_GUARD g(this);

  s = lock();

  bool should_unlock = true;

  switch(s)
  {
    case SUCCESS: should_unlock =  true; break;
    case EDEADLK: should_unlock = false; break;
    default: return s;
  }

  g.should_unlock = should_unlock;

  lambda();

  // Handled by SILLY_GUARD now
  // if(should_unlock)
  // {
  //   this->unlock();
  // }

  return SUCCESS;
}

int MUTEX_UNTRACKED::lock()
{
  return locker(pthread_mutex_lock);
}

int MUTEX_UNTRACKED::trylock()
{
  return locker(pthread_mutex_trylock);
}

int MUTEX_UNTRACKED::unlock()
{
  int s = this->unlock_without_pop();

  return s;
}

int MUTEX_UNTRACKED::locker(decltype(pthread_mutex_trylock) arg_func)
{
  // TODO maintain thread-based stack [of currently locked mutexes]?
  // TODO rotate unsalvageable locks https://stackoverflow.com/questions/29882647 - may help to push onto vector constantly [as if using a stack. can use `I count` index into array then or pointer if you prefer.]. may need a separate [non-recursive] mutex in order to do this! (which should be protected with p̶t̶h̶r̶e̶a̶d̶_̶c̶l̶e̶a̶n̶u̶p̶_̶p̶u̶s̶h̶/̶p̶o̶p̶ ̶p̶a̶i̶r̶ (+ signal guard AND cancel guard), which should be OK b/c of the simple content) -  signal_safe_wrapper thread_cancel_safe_wrapper
  // Question. Er, how do we know that a mutex is locked and in an unusable state, because its owning thread is dead, in situations when we don't have access to PTHREAD_MUTEX_ROBUST [and hence cannot recover the mutex using pthread_mutex consistent]? Answer. This is detected using a trylock() sweep of known mutexes by a separate thread (likely main thread) during a cleanup routine when all other threads are known canceled and mutexes are understood not to be owned by the thread performing the sweep.
  int s;

  s = arg_func(&pmutex);

  const int SUCCESS = 0;

  switch(s)
  {
    case SUCCESS:
    {
      return SUCCESS;
    }
    case EDEADLK:
    {
      // current thread already owns mutex, nothing to do.
      // This is OK, however, if we were to have a matching [inner] `unlock()` call, it isn't really,
      // since it's liable to unlock early, before the "outer" call completes.
      // probably want it silent
      // printf("pthread_mutex_lock returned EDEADLK\n");
      return EDEADLK;
    }
  #ifdef PTHREAD_MUTEX_ROBUST
    // If we have ROBUST mutexes, go ahead and try to recover it instead of rotating 
    case EOWNERDEAD:
    {
      printf("pthread_mutex_lock returned EOWNERDEAD\n");

      s = pthread_mutex_consistent(&pmutex);
      if (s != 0)
      {
        handle_error_en(s, "pthread_mutex_consistent");
        return s;
      }
      
      s = this->unlock_without_pop();
      if (s != 0)
      {
        handle_error_en(s, "EOWNERDEAD pthread_mutex_unlock");
        return s;
      }

      return this->locker(arg_func); // return without fallthrough to try again
    }
  #endif
    case EINVAL:
    {
      handle_error_en(s, "pthread_mutex_lock EINVAL");
      return s;
    }
    case EBUSY:
    {
      // silent for pthread_mutex_trylock()
      // handle_error_en(s, "pthread_mutex_lock EBUSY error");
      return s;
    }
    case EAGAIN:
    {
      handle_error_en(s, "pthread_mutex_lock EAGAIN");
      return s;
    }
    case ENOTRECOVERABLE:
    {
      handle_error_en(s, "pthread_mutex_lock ENOTRECOVERABLE");
      return s;
    }
    default:
    {
      handle_error_en(s, "pthread_mutex_lock general");
      return s;
    }
  }

  return SUCCESS;
}

int MUTEX_UNTRACKED::unlock_without_pop()
{
  const int SUCCESS = 0;
  int s;
  s = pthread_mutex_unlock(&pmutex);

  switch(s)
  {
    case SUCCESS:
    {
      // OK
      break;
    }
    case EPERM:
    {
      // I'm guessing we want to do this silently if we're attempting to unlock mutexes we don't own
      // handle_error_en(s, "pthread_mutex_unlock EPERM error: The current thread does not own the mutex.");
      break;
    }
    case EINVAL:
    {
      handle_error_en(s, "pthread_mutex_unlock EINVAL error");
      return s;
    }
    case EAGAIN:
    {
      handle_error_en(s, "pthread_mutex_unlock EAGAIN error");
      return s;
    }
    default:
    {
      handle_error_en(s, "pthread_mutex_unlock general error");
      break;
    }
  }

  return s;
}

void MUTEX_UNTRACKED::alloc_mutex(bool recursive)
{
  if(recursive)
  {
    mtype = PTHREAD_MUTEX_RECURSIVE;
  }

  int s;

  s = pthread_mutexattr_init(&attr);
  if(s!=0)
  {
    handle_error_en(s, "pthread_mutexattr_init");
  }

  s = pthread_mutexattr_setpshared(&attr, pshared);
  if(s!=0)
  {
    handle_error_en(s, "pthread_mutexattr_setpshared");
  }

  s = pthread_mutexattr_settype(&attr, mtype);
  if(s!=0)
  {
    handle_error_en(s, "pthread_mutexattr_settype");
  }

  s = pthread_mutexattr_setprotocol(&attr, protocol);
  if(s!=0)
  {
    handle_error_en(s, "pthread_mutexattr_setprotocol");
  }

#ifdef PTHREAD_MUTEX_ROBUST
  // Not avail on macOS as of 2021.04.07
  s = pthread_mutexattr_setrobust(&attr, robust);
  if(s!=0)
  {
    handle_error_en(s, "pthread_mutexattr_robust");
  }
#endif 

  // Not used currently
  // pthread_mutexattr_setprioceiling(&attr, prio);
  
  s = pthread_mutex_init(&pmutex, &attr);
  if(s!=0)
  {
    handle_error_en(s, "pthread_mutex_init");
  }
}

void MUTEX_UNTRACKED::dealloc_mutex()
{
  int s;

  if(DEBUG)
  {
    // mutex should be unlocked and clean
    s = pthread_mutex_trylock(&pmutex);
    assert(0 == s);
    pthread_mutex_unlock(&pmutex);
  }

  s = pthread_mutex_destroy(&pmutex);
  if(s!=0)
  {
    handle_error_en(s, "pthread_mutex_destroy");
  }

  s = pthread_mutexattr_destroy(&attr); 
  if(s!=0)
  {
    handle_error_en(s, "pthread_mutexattr_destroy");
  }
}


} //namespace
