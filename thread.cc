#pragma once

namespace KERF_NAMESPACE {

#pragma mark - Thread

void pop_workstacks_for_normalized_thread_id(I normalized_thread_id = -1)
{
#if CPP_WORKSTACK_ENABLED

  // TODO/Question Looking at this, I think the "prior heights" are eventually going to come off the stack, and so we may need some rewriting here and/or everywhere this function is [2021.10.24 they may actually live in the cpp-function-stack (literally, "the stack") in the vein of jmp structs, as a series of linked integer heights. or they may live inside the kerf-vm-stack.]

  if(-1 == normalized_thread_id)
  {
    normalized_thread_id = kerf_get_cached_normalized_thread_id();
  }

  I &i = normalized_thread_id;

  // Warning. It's better to pop the generic stack first, because this will
  // release locks on the SUTEXes on SLAB object before the SLABS are ever
  // freed by releasing the SLOP workstack. See remarks at SUTEX.

  auto a_Jump_Generic_Workstack = &The_Cpp_Generic_Workstacks[i];
  auto prior_height = The_Cpp_Generic_Workstacks_Prior_Heights[i];
  while(a_Jump_Generic_Workstack->size() > (prior_height))
  {
    I place = a_Jump_Generic_Workstack->size() - 1;
    REGISTERED_FOR_LONGJMP_WORKSTACK *p = a_Jump_Generic_Workstack->back();

    if(nullptr != p)
    {
      assert(-1 != p->index_of_myself_in_cpp_workstack);
      assert(place == p->index_of_myself_in_cpp_workstack);

      p->~REGISTERED_FOR_LONGJMP_WORKSTACK(); // Warning: you literally need to call this and not a helper, unless you can somehow figure out how to get the ancestor helper to call the descendant "helper" (which doesn't exist, unless you implement it on every class, and even then isn't called - not that hard to solve though, make an abstract virtual class on the base). Destructors have special privileges to call descendants. OTOH, calling the destructor directly I think runs afoul of certain undefined behavior rules. The other workstack cpp_slop_workstack gets away with it because SLOP has no ancestors or descendants
      // p->registered_destructor_helper();
    }

    // presumably, p has also zeroed its position in the workstack
    // prior to us popping it down here

    // assert(place + 1 == a_Jump_Generic_Workstack->size()); // see remarks below
    a_Jump_Generic_Workstack->pop_back();
  }

  // SLOP
  auto a_Jump_Slop_Workstack = &The_Cpp_Slop_Workstacks[i];
  prior_height = The_Cpp_Slop_Workstacks_Prior_Heights[i];
  // fprintf(stderr, "Popping CPP workstack. Height: %lu (Prior: %lld)\n", a_Jump_Cpp_Workstack->size(), prior_height);
  while(a_Jump_Slop_Workstack->size() > (prior_height))
  {
    I place = a_Jump_Slop_Workstack->size() - 1;
    SLOP *p = a_Jump_Slop_Workstack->back();

    if(nullptr != p)
    {
      assert(-1 != p->index_of_myself_in_cpp_workstack);
      assert(place == p->index_of_myself_in_cpp_workstack);
      // p->~SLOP();

      if(p->layout()->header_get_memory_reference_management_arena() == REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK)
      {
        if(p->slabp->sutex.in_use())
        {
          er(p->slabp->sutex.in_use(). Question. How are we reaching this case?)
          std::cerr <<  "p->slabp->sutex.writer_waiting: " << ((I)p->slabp->sutex.writer_waiting) << " p->slabp->sutex.counter: " << ((I)p->slabp->sutex.counter) << " my val would be: " << -(1+normalized_thread_id) << "\n";
          assert(!p->slabp->sutex.in_use());
          p->slabp->sutex = {0};
        }
      }
      p->slop_destructor_helper();
    }

    // p->~SLOP() may call back to here - that's fine if so, but if so we must postpone the workstack pop until after.
    // alternatively, you can set p->index_of_myself_in_cpp_workstack = -1 to avoid calling back

    // assert(place + 1 == a_Jump_Slop_Workstack->size()); // This condition may trigger if an object (SLOP) pushes to the stack during destruction. It's OK, I guess, for it to do that, as long as the process converges? In this case, a MEMORY_MAPPED object pushes six more (simpler) things on the stack while it's being freed. I suppose we could...stop it from doing that...but why?
    a_Jump_Slop_Workstack->pop_back();
  }

#endif
}

void cleanup_threads()
{
  // Notify/cancel/handle/whatever all threads with ids

  // signal wrapper is less important now that KERF_MUTEX invisibly handles deadlocks (self-acquisition of mutexes)
  // 2021.04.29 also, seems like it's preventing segfaults somehow, so I disabled it.
  // 2021.10.24 also, shouldn't the temporary sigmask in the signal handler be handling this instead?
  // signal_safe_wrapper([&]{

  bool this_is_main_thread = pthread_equal(The_Normalized_Thread_Id_Table[The_Main_Thread_Normalized_ID], pthread_self());
  assert(this_is_main_thread); // not sure if this is necessary, but I haven't verified the alternative for correctness
  int s;

  // NB. Getting the thread_id lock is necessary because we can't allow new threads to be generated while we're trying to clean them up
  The_Normalized_Thread_Id_Mutex.lock_safe_wrapper([&]{ 

    // Remark. Iterating in reverse won't buy any "stack"-like guarantees: b/c threads can exit any time, the array can fragment

    for(I i = 0; i < ARRAY_LEN(The_Normalized_Thread_Id_Table); i++)
    {
      bool index_of_main_thread = (The_Main_Thread_Normalized_ID==i);
      auto in_use = The_Normalized_Thread_Id_Table_Bool[i];
      auto thread_id = The_Normalized_Thread_Id_Table[i];

      // std::cerr <<  "================== THREAD " << i << " [" << thread_id << "] ========================\n";

      if(index_of_main_thread) continue; // don't cancel the main thread
      // I didn't check if these [in_use / null_pthread] truly need cleanup so this may be redundant
      if(!in_use) continue;
      if(pthread_equal(thread_id, NULL_PTHREAD_T)) continue;

      // Remark. There's actually a pretty crazy thing where threads can reuse IDs [creating exotic race conditions], accounting for this is not worthwhile
      // Remark. We want this to be async (all threads receive the signal async before main continues and later joins) so that threads can unwind their lock-guard-stacks and their regular-slop-stacks in parallel. Doing this asynchronously allows potential deadlocks to unwind what would otherwise be created in a synchronous passthrough.
      kerr() << "attempting to pthread_kill SIGUSR2 thread_id: " << (thread_id) << "\n";
      s = pthread_kill(thread_id, SIGUSR2);
      if (s != 0)
      {
        THREAD::handle_error_en(s, "cleanup_threads pthread_kill");
      }
    }

    /////////////////////////////////////////////////////////////////////////
    assert(this_is_main_thread); // not sure if this is necessary, but I haven't verified the alternative for correctness
    pop_workstacks_for_normalized_thread_id(The_Main_Thread_Normalized_ID);
    /////////////////////////////////////////////////////////////////////////

    for(I i = 0; i < ARRAY_LEN(The_Normalized_Thread_Id_Table); i++)
    {
      bool index_of_main_thread = (The_Main_Thread_Normalized_ID==i);
      auto in_use = The_Normalized_Thread_Id_Table_Bool[i];
      auto thread_id = The_Normalized_Thread_Id_Table[i];
      auto joinable = The_Normalized_Thread_Id_Table_Joinable[i];

      // std::cerr <<  "================== THREAD " << i << " [" << thread_id << "] ========================\n";

      if(index_of_main_thread) continue; // don't cancel the main thread
      // I didn't check if these [in_use / null_pthread] truly need cleanup so this may be redundant
      if(!in_use) goto cleanup;
      if(pthread_equal(thread_id, NULL_PTHREAD_T)) goto cleanup;

      // NB. Detaching a detached thread results in undefined behavior, so we need to track this attribute (joinable vs. detached). (Joining a detached thread is also undefined.)
      // NB. Not joining joinable threads creates zombie processes and wastes resources. Hence you do need to track joinability in the thread thing.
      if(joinable)
      {
        void *result;
        s = pthread_join(thread_id, &result); 
        // Alternatively, we could instead detach the joinable threads, I don't think it matters

        if (s != 0)
        {
          // NB. In some cases, this competes with THREAD_POOL so that they both attempt to join the same worker thread(s) during a ctrl-c event, resulting in an EINVAL error. Feature. Perhaps we should alter THREAD_POOL to not wait on its threads (re: join()) when a ctrl-c global is set.
          // THREAD::handle_error_en(s, "jump pthread_join");
        }

        if (result != PTHREAD_CANCELED)
        {
          // printf ("Jump: Thread was not canceled\n");
        }
      }

    cleanup:
      // Remember. Cleaning must happen after pthread_kill signal because we use the thread table inside the handler
      // Note: calling kerf_release_normalized_thread_id() is cleaner but would result in O(t^2) algo, which may/may not be fine
      The_Normalized_Thread_Id_Table_Bool[i] = false;
      The_Normalized_Thread_Id_Table[i] = NULL_PTHREAD_T;
      The_Normalized_Thread_Id_Table_Joinable[i] = false;
    }
  });
  // }); // signal safe
}

void* THREAD::thread_start(void *arg)
{
  while(1)
  {
    std::cerr <<  "thread started: " << (pthread_self()) << "\n";
    usleep(500*1000);
  }
  return nullptr;
}

void* THREAD::thread_noop(void *arg)
{
  return nullptr;
}

void THREAD::handle_SIGUSR2(int sig)
{
  kerr() <<  "SIGUSR2 for thread: " << (pthread_self()) << "\n";
  
  // if we are waiting on the following in main's cleanup function:
  //     T̶h̶e̶_̶T̶h̶r̶e̶a̶d̶_̶S̶a̶f̶e̶_̶E̶a̶r̶l̶y̶_̶R̶e̶m̶o̶v̶e̶_̶Q̶u̶e̶u̶e̶.̶l̶o̶c̶k̶(̶)̶;̶
  //     The_Normalized_Thread_Id_Mutex.lock();
  // then [so far] there's nothing for us to unlock other than the kerf tree items...
  // (because these mutexes have "guarantees" in their corresponding uses that they 
  //  will do short operations only and won't spin while owning lock. On top of that
  //  we want the associated data in the Early Remove Queue to remain consistent.)
  // Anyway, silently attempt to unlock them whether we have them or not.
  // Actually, we do need this kind of functionality, maybe not here, but before
  // a siglongjmp for kerf error handling.

  // Remark. We're handling cpp workstack here instead of in batch at the end
  // of cleanup_threads because I'm not sure whether function stack frame data
  // for a thread will persist after pthread_exit() or other forms of cancel.
  // If it does not, then we need to do it before where the thread would end,
  // i.e., here above what is now pthread_exit()
  // (I'm guessing it won't survive. We'll be covered either way.)
  // Also, a thread should release structures to its own pool. 
  auto normalized_id = kerf_get_cached_normalized_thread_id();

  // Remark. there's some weird condition where if we're waiting on a .join() in the main thread (see simple THREAD test in test.cc) because say the thread is sleeping and then we ctrl-c somehow we get a -1 at this point, I don't know why, maybe that thread somehow finishes first and then clears its own id
  if(-1 != normalized_id)
  {
    pop_workstacks_for_normalized_thread_id(normalized_id);
  }

  // std::cout <<  "handle sigusr2 pthread_self(): " << (pthread_self()) << "\n";
  // std::cout <<  "print your sigmask: \n";
  // sigset_t save;
  // sigset_t replace;
  // sigfillset(&replace); // ignore all ignorable signals
  // pthread_sigmask(SIG_SETMASK, &replace, &save);
  // std::cerr <<  "sigismember(&save, ): " << (sigismember(&save, SIGUSR2)) << "\n";

  // pause();

  // Observation. There's three options. 1. allow threads to finish any tight loops (slow ctrl-c, safe consistency). 2. exit immediately (fast ctrl-c, bad consistency) 3. program only atomic write operations (undesirable, traditional rdbms). We choose (2).

  // NB. pthread_exit is not a async-signal-safe so technically is disallowed to be called from signal handler
  pthread_exit(nullptr);
}

void THREAD::handle_SIGABRT(int sig)
{
#if defined(__has_feature)
#if __has_feature(address_sanitizer) // code that builds only under AddressSanitizer
  const bool stack_dump_when_abort = true;
  char *m = (char*)malloc(1);
  free(m);
  if(stack_dump_when_abort) m[0] = 1; // trigger asan stacktrace. i'd rather use anything cleaner. having trouble getting -lunwind on macos
#endif
#endif
}

void* THREAD::kerf_init_wrapper(void *arg)
{
  // the passed `arg` was a `new` clone of the object
  // doing this new/delete this is necessary to prevent thread race conditions I think
  auto *copy = (THREAD*)arg;
  auto should_init_deinit_kerf_vm_thread_stuff = copy->should_init_deinit_kerf_vm_thread_stuff;
  auto function_to_call = copy->function_to_call;
  auto function_data = copy->function_data;
  auto prio = copy->param.sched_priority;
  auto cancel_type = copy->cancel_type;
  auto cancel_state = copy->cancel_state;
  delete copy;

  void *ret = nullptr;

  kerf_suggest_bind_thread_to_cpu(); // TODO: probably moves to where attr are set

  assert(!pthread_equal(pthread_self(), NULL_PTHREAD_T));

  int s, oldtype, oldstate;

  s = pthread_setconcurrency(prio);
  if (s != 0)
  {
    THREAD::handle_error_en(s, "pthread_setconcurrency");
  }

  s = pthread_setcanceltype(cancel_type, &oldtype);
  if (s != 0)
  {
    THREAD::handle_error_en(s, "pthread_setcanceltype");
  }

  s = pthread_setcancelstate(cancel_state, &oldstate);
  if (s != 0)
  {
    THREAD::handle_error_en(s, "pthread_setcancelstate");
  }

  // Remark. Here is where we might belatedly alter the pthread sigmask to accept other signals.
  // See also remarks at `int signal_safe_wrapper(L &&lambda)`, and its cited sources.
  sigset_t save;
  {
    sigset_t replace;

    sigfillset(&replace);         // ignore all ignorable signals
    sigdelset(&replace, SIGUSR2); // except SIGUSR2, so we can cancel threads via pthread_kill
    s = pthread_sigmask(SIG_SETMASK, &replace, &save);

    if (s != 0)
    {
      THREAD::handle_error_en(s, "signal safe pthread_sigmask SIG_SETMASK");
    }
  }

  if(should_init_deinit_kerf_vm_thread_stuff)
  { 

  }

  hard_jmp_wrapper([&] (int jmped) {
    switch(jmped)
    {
      case 0:
      {
        ret = function_to_call(function_data);
        break;
      }
      case 1: [[fallthrough]];
      default:
      {
        // TODO: ret should point to kerf error object based on `jmped`
        SLOP err((ERROR_TYPE)jmped);

        fprintf(stderr, "\nSiglongjmp inside of thread.\n");
        break;
      }
    }
  });

  if(should_init_deinit_kerf_vm_thread_stuff)
  {
  #if CPP_WORKSTACK_ENABLED
    auto i = kerf_get_cached_normalized_thread_id();
    pop_workstacks_for_normalized_thread_id(i);
    assert(0 == The_Cpp_Generic_Workstacks_Prior_Heights[i]);
    assert(0 == The_Cpp_Slop_Workstacks_Prior_Heights[i]);
  #endif
  }

  kerf_release_normalized_thread_id();

  return ret;
}

void THREAD::handle_error_en(int en, const char* msg)
{
  errno = en;
  perror(msg);
}

int THREAD::init()
{
  this->normalized_id = kerf_acquire_normalized_thread_id(false); // acquiring for the new thread, not this thread
  if(-1 == this->normalized_id)
  {
    perror("Error acquiring normalized thread id for new thread (are threads over capacity?)");
    return -1;
  }

  int s;

  s = pthread_attr_init(&this->attr);
  if (s != 0)
  {
    this->handle_error_en(s, "pthread_attr_init");
  }

  s = pthread_attr_setdetachstate(&this->attr, this->detach_state);
  if (s != 0)
  {
    this->handle_error_en(s, "pthread_attr_setdetachstate");
  }

  if(PTHREAD_CREATE_JOINABLE == this->detach_state)
  {
    // NB. need to always set the variable to either true or false here to clear the old value
    The_Normalized_Thread_Id_Table_Joinable[this->normalized_id] = (PTHREAD_CREATE_JOINABLE == this->detach_state ? true : false);
  }

  s = pthread_attr_setinheritsched(&this->attr, this->inherit_sched);
  if (s != 0)
  {
    this->handle_error_en(s, "pthread_attr_setinheritsched");
  }

  s = pthread_attr_setscope(&this->attr, this->scope);
  if (s != 0)
  {
    this->handle_error_en(s, "pthread_attr_setscope");
  }

  s = pthread_attr_setschedpolicy(&this->attr, this->sched_policy);
  if (s != 0)
  {
    this->handle_error_en(s, "pthread_attr_setschedpolicy");
  }

  s = pthread_attr_setschedparam(&this->attr, &this->param);
  if (s != 0)
  {
    this->handle_error_en(s, "pthread_attr_setschedparam");
  }

  // 2021.03.30 not needed currently
  // pthread_attr_setstack
  // pthread_attr_setstackaddr
  // pthread_attr_setguardsize

  return 0;
}

void THREAD::deinit()
{
  int s;

  s = pthread_attr_destroy(&this->attr);
  if (s != 0)
  {
    this->handle_error_en(s, "pthread_attr_destroy");
  }
}

int THREAD::start()
{
  int s = 0;

  // Remark. This disables all the signals in parent so they aren't inherited by the child thread. Additionally, because we're blocking all signals here, and that includes ctrl-c, this [block of code in this function] is async safe. Well, assuming you don't do anything wild in the other threads like pthread_kill (force raise signal in, on macOS) this one. | I'm not sure how much of the previous remark is actually true, since I had to block SIGUSR2 in the main thread first, to get these assumptions to work.

  signal_safe_wrapper([&]{
    decltype(this) copy_must_delete;

    s = this->init(); // alloc attr

    if(s != 0)
    {
      goto fail;
    }

    copy_must_delete = new auto(*this);

    s = pthread_create(&The_Normalized_Thread_Id_Table[this->normalized_id], &this->attr, &this->kerf_init_wrapper, copy_must_delete); // deleted inside

    if (s != 0)
    {
      this->handle_error_en(s, "pthread_create");
      delete copy_must_delete;
      goto fail;
    }

    succeed:
      this->deinit(); // free attr
      return;

    fail:
      this->deinit(); // free attr
      if(-1 != this->normalized_id) // release normalized thread id
      {
        The_Normalized_Thread_Id_Table_Bool[this->normalized_id] = false;
        The_Normalized_Thread_Id_Table[this->normalized_id] = NULL_PTHREAD_T;
      }
      return;
  });

  return s;
}

int THREAD::join(void **retval)
{
  return pthread_join(The_Normalized_Thread_Id_Table[this->normalized_id], retval);
}

} // namespace
