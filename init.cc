
namespace KERF_NAMESPACE {

  void main_thread_init()
  {
    /////////////////////////////////////////////////////////////////////////////
    // Warning. Some how this is necessary as of macos 11.5.2 on 2021.08.24 in
    // order to get other threads, while in their signal/critical safe wrappers,
    // to block pthread_kill(SIGUSR2) from the ctrl-c SIGINT handler. I have no
    // explanation for this.
    sigset_t e;
    sigemptyset(&e);
    sigaddset(&e, SIGUSR2);
    pthread_sigmask(SIG_BLOCK, &e, NULL);
    /////////////////////////////////////////////////////////////////////////////

    // sigfillset(&e);
    // pthread_sigmask(SIG_BLOCK, &e, NULL);
    // sigemptyset(&e);
    // sigaddset(&e, SIGINT);
    // pthread_sigmask(SIG_UNBLOCK, &e, NULL);

    kerf_suggest_bind_thread_to_cpu(0);
    The_Main_Thread_Normalized_ID = kerf_acquire_normalized_thread_id(); // main thread get id
    assert(0 == The_Main_Thread_Normalized_ID);
    The_Normalized_Thread_Id_Table_Joinable[0] = true; // main thread is default joinable.
  }

  void kerf_tree_init()
  {
    The_Kerf_Tree_Parent_Sutex.sutex_safe_wrapper_exclusive([&]{
      SLOP b(MAP_YES_YES); // MAP_YES_YES so we don't have to worry about a slab pointer change (no parent)
      b.coerce_parented();
      b.legit_reference_increment();
      The_Kerf_Tree = b.slabp;
    });
  }

  void kerf_tree_deinit()
  {
    The_Kerf_Tree_Parent_Sutex.sutex_safe_wrapper_exclusive([&]{
      SLOP a(The_Kerf_Tree);
      // Idea. We could also make a thing to xfer from PARENTED back to CPP_WORKSTACK before freeing, shrug
      a.legit_reference_decrement(true);
      a.neutralize();
      The_Kerf_Tree = nullptr;
    });
  }

  void cleanup()
  {
    // Remark. In cpp, if you build the new/delete pair inside a custom [automatic-memory] object, then for instance [any] `delete` cleanup is handled automatically, as cpp calls the object's destructor on exit. OTOH, some small amount of `delete` cleanup is fine, since we're going to have a cleanup block anyway for certain ... You could, however, bundle [at least some of] those inits and cleanups inside of custom object constructor and destructor pairs as well.

    // Feature. we should verify cpp workstacks are 0 height before popping them in any cleanup routines such as cleanup_threads, and record them as a memory leak if not

    The_Console_Lexer.reset();

    cleanup_threads(); 

    kerf_tree_deinit();
    
    libevent_global_shutdown();

#if DEBUG
    if(0 < The_Mmap_Total_Byte_Counter) std::cerr << "Error 0 < The_Mmap_Total_Byte_Counter: " << (The_Mmap_Total_Byte_Counter) << "\n";
    if(!The_Did_Interrupt_Flag) assert(0==The_Mmap_Total_Byte_Counter);

    if(0 < The_Munmap_Leak_Tracker) std::cerr << "Error: 0 < The_Munmap_Leak_Tracker: " << (The_Munmap_Leak_Tracker) << "\n";
    if(!The_Did_Interrupt_Flag) assert(0==The_Munmap_Leak_Tracker);
#endif


    // Warning. Ah, I see. We're using new/delete here so we can control when objects are released. (Also, to keep an inventory of what's active.) I don't think this matters as of this writing but it's bound to matter in the future. Was it also possibly so that we didn't [inadvertantly] make copies when accessing the contents?
#if CPP_WORKSTACK_ENABLED
    delete [] The_Cpp_Generic_Workstacks;
    delete [] The_Cpp_Slop_Workstacks;
#endif
    delete [] The_Thread_VMs;
    delete [] The_Thread_Memory_Pools;
    delete [] The_Thread_RNGs;
    kerf_release_normalized_thread_id();
  }

  void kerf_atexit()
  {
    // You can put cleanup() other places besides here, but then you need to control the exit
    // maybe switch to "kerf_exit(bool cleanup)" or something
    // (reason is ctrl+d inside select causes exit there)
    cleanup();
  }

  int kerf_init(bool force_reinit = false)
  {
    if(The_Language_Initialized_Flag && !force_reinit)
    {
      // Remark. Formulating this as a pthread_once call or equivalent would make this more thread-safe for the caller at the expense of what I suspect is too severe a performance penalty for the nature of the call. We could instead offload the calling of pthread_once/* to the caller.
      return 1;
    }

    main_thread_init();

    signal_init();      // call early

    hash_init();        // no dependencies

    kerf_tree_init();

    LEXER::init();       

    atexit(kerf_atexit);

    The_Language_Initialized_Flag = true;

    return 0;
  }

  template<typename L>
  void library_call_wrapper(L &&lambda)
  {
    bool reinit;
    kerf_init(reinit = false);
    lambda();
  }

  void example_library_call_template()
  {
    library_call_wrapper([]{sleep(1);});
  }

} // namespace
