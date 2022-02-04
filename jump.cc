namespace KERF_NAMESPACE {

void kerf_exit(int exit_code)
{ 
  //wrapper to notify debug that we may have left things in unclean state
  The_Intentional_Midstream_Exit_Flag = true;
  exit(exit_code);
}

int signal_init()
{
  // TODO: implement remarks in readme about making this suitable for library calls

  // TODO: I think sigaction can return error values which we should check

  // SIGABRT
  struct sigaction aact = {0};
  aact.sa_handler = THREAD::handle_SIGABRT;
  aact.sa_flags = SA_RESTART;
  sigemptyset(&aact.sa_mask);
  sigaction(SIGABRT, &aact, NULL);

  // SIGINT
  struct sigaction sact = {0};
  sact.sa_sigaction = handle_SIGINT;
  // sact.sa_handler = handle_SIGINT;
  // Observation: An SA_SIGINFO version stops a doubly-triggered ctrl-c handler inside macvim
  sact.sa_flags = SA_RESTART | SA_SIGINFO;
  sigemptyset(&sact.sa_mask);
  sigaction(SIGINT, &sact, NULL);

  // SIGPIPE ignore
  struct sigaction iact = {0};
  iact.sa_handler = SIG_IGN;
  iact.sa_flags = SA_RESTART;
  sigemptyset(&iact.sa_mask);
  sigaction(SIGPIPE, &iact, NULL);

  // //SIGWINCH ignore
  // struct sigaction wact = {0};
  // wact.sa_handler = SIG_IGN;
  // wact.sa_flags = SA_RESTART;
  // sigemptyset(&wact.sa_mask);
  // sigaction(SIGWINCH, &wact, NULL);

  // // SIGUSR1
  // struct sigaction uact = {0};
  // uact.sa_handler = handle_SIGUSR1;
  // uact.sa_flags = SA_RESTART;
  // sigemptyset(&uact.sa_mask);
  // sigaction(SIGUSR1, &uact, NULL);

  // SIGUSR2
  struct sigaction uact = {0};
  uact.sa_handler = THREAD::handle_SIGUSR2;
  uact.sa_flags = SA_RESTART;
  sigemptyset(&uact.sa_mask);
  sigaction(SIGUSR2, &uact, NULL);

  return 0;
}

void handle_SIGINT(int sig, siginfo_t *info, void *context)
{
  // The policy for CTRL+C is that memory leaks
  // are acceptable, but try to avoid them.
  // The policy for other kinds of sigsetjmp
  // handling is that memory leaks must be
  // avoided.

  ucontext_t *ucontext = (ucontext_t*) context;
  // __builtin_dump_struct(info, &printf);
  // __builtin_dump_struct(ucontext, &printf);

  // Question. why is this ctrl-c method being called twice, seemingly? macvim terminal? hung child process? Answer. Quite possibly macvim terminal. doesn't seem to happen in normal terminal, wouldn't worry about it. We gracefully handle it anyway.
  // fprintf(stderr,"\nGot signal: %d\n", sig);
  fprintf(stderr,"\nCaught Ctrl-c SIGINT interrupt signal\n");
  
  The_Did_Interrupt_Flag = true; 
  
  //In our error-throwing method we just cause children to come back here via raise()
  //maybe we should refactor this into a method though that gets called in both places?

  // Remark. This is the old kerf1.0 fork/raise error handling stuff
  // not sure yet how to rework for pthreaded version

  // if(The_Process_is_Child_Flag)
  // {
  //   fully_recover_from_hard_jmp();
  //   //fprintf(stderr, "Child %2lld terminating in response to CTRL+C.\n", The_Process_is_Child_Number);
  //   if(The_Process_Shm_Handle>2)
  //   {
  //     close(The_Process_Shm_Handle);
  //     The_Process_Shm_Handle = 0;
  //   }
  //   _exit(CTRL_C_EXIT_CODE);
  //   return;
  // }
  // else if(The_Process_is_Active_Parent)
  // {
  //   while(-1 != wait(NULL)) { }
  // 
  //   DO(sizeof(The_Process_Shm_Handles)/sizeof(The_Process_Shm_Handles[0]), 
  //     if(The_Process_Shm_Handles[i]>2)
  //     {
  //       close(The_Process_Shm_Handles[i]);
  //       The_Process_Shm_Handles[i]=0;
  //     }
  //   )
  // }

  // Rule. "But you cannot lock mutexes from a signal-catching function." You're not supposed to acquire mutexes and so on in signal handlers. Why? Could we though? One of the concerns is that the, say, main thread handling the signal may have already acquired the lock, creating a deadlock (we could potentially detect his), possibly the second concern is that this may be undefined behavior per the standard (could we ignore it? ie, maybe you're not supposed to but it's actually OK).

  if(!The_Can_Jump)
  {
    //jmp_buf uninitialized
    //(or [re-]initializing function has returned
    // and set The_Can_Jump=false)
    fprintf(stderr, "\nTerminating in response to CTRL+C.\n");
    kerf_exit(CTRL_C_EXIT_CODE);
    return;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Warning: any non-zero cpp_workstack popping must take place before the longjmp
  //          because SLOP data will be overwritten along with the frame
  cleanup_threads();
  /////////////////////////////////////////////////////////////////////////////

  auto normalized_id = kerf_get_cached_normalized_thread_id();

  if(SECURITY_ALLOW_LONGJMP_FROM_SIGNAL)
  {
    siglongjmp(*The_Hard_Jmp_Envs[normalized_id], ERROR_CTRL_C);
  }
}


#pragma mark - Wrappers

template<typename L>
void hard_jmp_wrapper(L &&lambda)
{
  auto normalized_id = kerf_get_cached_normalized_thread_id();

  int val;
  sigjmp_buf jmp_env;

  switch(val = sigsetjmp(jmp_env, true))
  {
    case 0:
    {
      The_Hard_Jmp_Envs[normalized_id] = &jmp_env;
      The_Soft_Jmp_Envs[normalized_id] = The_Hard_Jmp_Envs[normalized_id];
  
    #if CPP_WORKSTACK_ENABLED
      // Note. Any SLOP that gets stack-allocated in the function scope after this can be cleared during ctrl-c, so make sure you jump clean and land clean.
      The_Cpp_Slop_Workstacks_Prior_Heights[normalized_id] = The_Cpp_Slop_Workstacks[normalized_id].size();
      The_Cpp_Generic_Workstacks_Prior_Heights[normalized_id] = The_Cpp_Generic_Workstacks[normalized_id].size();
    #endif
  
      break;
    }
    default:
    {
      // Note. lambda will switch on `val` below
      break;
    }
  }

  lambda(val);
}

template<typename L>
int signal_safe_wrapper(L &&lambda)
{
  // Question. Is the below true? pthread_kill is specified to execute synchronously but it does not speak on whether the signal mask is respected Answer. No, it wasn't true. We evidently as of 2021.08.24 got the mask to be respected everywhere.
  // N̶B̶.̶ ̶T̶h̶i̶s̶ ̶s̶t̶o̶p̶s̶ ̶p̶r̶o̶c̶e̶s̶s̶ ̶s̶i̶g̶n̶a̶l̶s̶ ̶f̶r̶o̶m̶ ̶r̶e̶a̶c̶h̶i̶n̶g̶ ̶t̶h̶e̶ ̶t̶h̶r̶e̶a̶d̶ ̶b̶u̶t̶,̶ ̶p̶e̶r̶ ̶s̶t̶a̶n̶d̶a̶r̶d̶,̶ ̶n̶o̶t̶ ̶p̶t̶h̶r̶e̶a̶d̶_̶k̶i̶l̶l̶ ̶s̶i̶g̶n̶a̶l̶s̶,̶ ̶s̶y̶s̶ ̶e̶r̶r̶o̶r̶ ̶s̶i̶g̶n̶a̶l̶s̶,̶ ̶e̶t̶c̶.̶
  // S̶e̶e̶ ̶T̶a̶b̶l̶e̶ ̶5̶-̶1̶ ̶i̶n̶ ̶"̶P̶t̶h̶r̶e̶a̶d̶s̶ ̶P̶r̶o̶g̶r̶a̶m̶m̶i̶n̶g̶"̶ ̶N̶i̶c̶h̶o̶l̶s̶,̶ ̶e̶t̶ ̶a̶l̶.̶
  // See also void* THREAD::kerf_init_wrapper(void *arg)
  // Observation 2021.08.24 macos 11.5.2, (both terminal and macvim), thread A raise(SIGUSR2) on itself while sigmask ignoring SIGUSR2 does nothing. *NO, DOES _NOT_ INVOKE*
  // Observation 2021.08.24 macos 11.5.2, (both terminal and macvim), main thread pthread_kill(SIGUSR2) *from the main() function NOT a handler* *does NOT* deliver SIGUSR2 to thread A, A!=main, while A is sigmask ignoring SIGUSR2. *NO, DOES _NOT_ INVOKE*
  // Observation 2021.08.24 macos 11.5.2, (both terminal and macvim), main_thread pthread_kill(SIGUSR2) from the ctrl-c handler on to thread A (A!=main) while A sigmask ignoring SIGUSR2 *̶D̶O̶E̶S̶ ̶I̶N̶V̶O̶K̶E̶*̶ ̶t̶h̶e̶ ̶s̶i̶g̶u̶s̶r̶2̶ ̶s̶i̶g̶n̶a̶l̶ ̶h̶a̶n̶d̶l̶e̶r̶ ̶o̶n̶ ̶A̶ ̶*̶Y̶E̶S̶,̶ ̶D̶O̶E̶S̶ ̶I̶N̶V̶O̶K̶E̶*̶ ̶-̶ ̶o̶k̶ ̶_̶ ̶I̶ ̶r̶a̶n̶ ̶t̶h̶i̶s̶ ̶a̶g̶a̶i̶n̶ ̶b̶u̶t̶ ̶t̶h̶e̶n̶ ̶i̶t̶ ̶d̶i̶d̶_̶n̶o̶t̶_̶i̶n̶v̶o̶k̶e̶?̶ ̶w̶t̶f̶?̶ ̶d̶i̶d̶ ̶I̶ ̶s̶c̶r̶e̶w̶ ̶i̶t̶ ̶u̶p̶ ̶t̶h̶e̶ ̶f̶i̶r̶s̶t̶ ̶t̶i̶m̶e̶?̶ ̶O̶K̶,̶ ̶s̶o̶ ̶I̶ ̶t̶h̶i̶n̶k̶  At first I ran it, and it was invoking, *provided* you allowed the process to live long enough without exiting. After a "bugfix" (?) where i explictly set the pthread_sigmask on the main thread during kerf init, seemingly the problem went away, and now "NO, DOES _NOT_ INVOKE" seems to be the case as desired; however, I can't rule out the possibility we're corrupting something and producing a heisenbug.

  int t = 0;
  sigset_t save;

  {
    sigset_t replace;
    sigfillset(&replace);         // ignore all ignorable signals

  #if PERMIT_SEGV_LIKE_IN_CRITICAL_SECTION
    // As we currently disable all non-SIGUSR2 signals in non-main threads,
    // technically, the following should only transpire if the current thread
    // is the main thread, but I don't think it matters
    sigdelset(&replace, SIGSEGV);
    sigdelset(&replace, SIGFPE);
    sigdelset(&replace, SIGILL);
    sigdelset(&replace, SIGBUS);
  #endif
    
    t = pthread_sigmask(SIG_SETMASK, &replace, &save);

    if (t != 0)
    {
      perror("signal safe pthread_sigmask SIG_SETMASK");
      return t;
    }
  }

  /////////////////////////////////////////////////////
  lambda();
  /////////////////////////////////////////////////////

  {
    t = pthread_sigmask(SIG_SETMASK, &save, NULL); // restore old sigmask, including ctrl-c handling

    if (t != 0)
    {
      perror("signal safe pthread_sigmask SIG_SETMASK");
      return t;
    }
  }

  return 0;
}

template<typename L> void critical_section_wrapper(L &&lambda) noexcept
{
  // Primarily this is so we don't get side-swiped by ctrl-c at an inopportune time. (This is a strong way to shield ourselves from ctrl-c when need to temporarily produce an inconsistent state before making it consistent).

  // Our assumptions for critical sections are:
  // Assumption 1. Will not be interrupted by a SIGNAL, whether originating internally or externally from the process or from this thread or any other. Excepting un-ignorable signals like SIGKILL, SIGABRT, SIGSTOP. Additionally, will not on this thread generate SIGSEGV, SIGFPE, SIGBUS, SIGILL, and the like, which would "put the program in an undefined state". We've since added a compiler toggle to crash on SIGSEGV et al. instead of silently hanging.
  // Assumption 2. Will not throw a kerf error and so siglongjmp out of a critical section. TODO enforce this via assertions when in DEBUG mode (set a global that only exists during DEBUG and assert() at the error throw mechanism) Remark. While it may be possible to store/restore the sigmask on jmps (assuming we verify the mask doesn't change a third time in between the setjmp and the critical section), this ignores the overall point that jumping during a critical section "should" leave things inconsistent. I suppose it's possible to ensure consistency and then jmp... It's less complicated to disallow jumps? Note. We've added `noexcept` to indicate the current thinking.
  // Assumption 3. As a rule of thumb, we don't want any critical section to last longer than the threshold of human perceptibility, which is about 30 milliseconds. In other words, we want ctrl-c to be responsive.

  // Warning. You cannot create a critical section guard, see Assumption 2. Critical sections ignore signals, so the guard can never be used in response to a signal. Similarly, we could use a guard to allow us to longjmp from a critical section. This would let us handle kerf-error-throwing functions from within critical sections. But note: if you can jump at that point in the critical section, it isn't a critical section, and you should split it there instead. There may be some cases where splitting is infeasible (eg, a nested call as part of a larger callback), but I have not yet seen them. Generally, error-throwing should be replaced by bool-returning. Return true on error.

  // Timing. As of 2021.08.24 on macOS 11.5.2 the signal_safe_wrapper with pthread_sigmask seems to add about 5 microseconds per call. Simple benchmark.

  // POP Instead of using signal_safe_wrapper for critical sections, we can use a per-thread atomic boolean "critical_section" flag that says "don't signal me via pthread_kill yet", and then the main thread has to wait to acquire it before sending pthread_kill(SIGUSR2) to end the thread (but compare the 5 μs benchmark above). Additionally, because the main thread could be interrupted by ctrl-c, you'd either have to A. make sure the main thread is special in not-receiving ctrl-c during its critical sections (say by defaulting to a signal mask, which defeats our assumption that it's performant) or B. putting the ctrl-c handler in a separate, non-normalized bare-bones pthread running in a sched_yield loop which itself waits to acquire the "main_thread"'s atomic critical section flag before signalling it. Note that, because main_thread is no longer reactive to SIGINT, this would additonally consume SIGUSR1 on top of SIGUSR2, or some other scheme for sending SIGUSR2 to main_thread and then reregistering somthing else.
  // Remark. See also https://www.gnu.org/software/libc/manual/html_node/Remembering-a-Signal.html
  // Alternative Idea. Another exotic thing we can do is to change the way our sutex works. What we can do is have shared_unlocks block (slower implementation) block while the counter is non-positive, then a shared_lock can leave the sutex as a writelock until it reaches a determinate state (the lock guard has the pointer to the sutex, a boolean value for being determinate, and apparently now a temporary storage variable for the read - ah, that thing can become indeterminate?) at which point it can restore the shared-counter and proceed (exclusive_locks don't have the indeterminate problem because you can check the sutex for your own thread id). I think the issue here is the read counter can become indeterminate in the same way. Nice try though. I don't think we're getting around this without some kind of "double-operation atomic", of any sort really (update two pointers at once), which I don't think is available.

  // POP If ctrl-c is disabled, eg on OSes where they are unavailable (iOS) or when it is intentionally user-disabled, then critical sections aren't needed. Do this as a compile-time define/flag or perhaps a global variables. | POP Compiler-time option to disable protecting critical sections. This speeds things up but may cause ctrl-c to rarely cause the program to enter unfixable/indeterminate states. Either leave ctrl-c on with this disclaimer or disable. "Remark. We should have a compiler flag to turn critical section wrappers into a "no-op" where's there's no overhead and we just perform the contained operations unprotected." 

  // Remark. Some writes (in a known-active-multithreaded context, eg on the kerf tree) will likely need this since it's often that you can't just interrupt a write in-process and have a recoverable state (i dunno, swapping pointers around and such, for instance). 
  // Remark. It may be helpful to simply mark critical sections and then worry about what kind of method we're going to use later; there is also that lingering question of whether pthread_sigmask defers pthread_kill(SIGUSR1) or not.

  auto i = signal_safe_wrapper(lambda);
}

template<typename L>
int thread_cancel_safe_wrapper(L &&lambda)
{
  int restore, junk;

  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &restore);
  lambda();
  pthread_setcancelstate(restore, &junk);

  return 0;
}


#pragma mark -

#if CPP_WORKSTACK_ENABLED
  auto REGISTERED_FOR_LONGJMP_WORKSTACK::get_my_cpp_workstack()
  {
    auto i = kerf_get_cached_normalized_thread_id();
    return &The_Cpp_Generic_Workstacks[i];
  }
  
  void REGISTERED_FOR_LONGJMP_WORKSTACK::register_in_cpp_workstack()
  {
    auto& i = this->index_of_myself_in_cpp_workstack;
    assert(-1 == i);
    auto w = get_my_cpp_workstack();
    auto c = w->size();
    w->emplace_back(this);
    i = c;
    assert(w->size() == c + 1);
  }
  
  void REGISTERED_FOR_LONGJMP_WORKSTACK::deregister_in_cpp_workstack()
  {
    auto& i = this->index_of_myself_in_cpp_workstack;

    assert(-1 != i);
    auto w = get_my_cpp_workstack();
    assert(w->size() > 0);
    assert(i >= 0);
    assert(i < w->size());
    assert(w->operator[](i) == this);
    w->operator[](i) = nullptr;
    i = -1;

    // TODO POP. Compiler flag for "shrinking" the stack height while() the top is zeroed (nullptr). See also also SLOP class.
  }

#endif

#pragma mark -

} // namespace
