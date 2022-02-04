namespace KERF_NAMESPACE {

#pragma mark - Globals

volatile bool The_Can_Jump = false;
volatile int The_Did_Interrupt_Flag = false;
volatile bool The_Intentional_Midstream_Exit_Flag = false;

sigjmp_buf *The_Soft_Jmp_Envs[KERF_MAX_NORMALIZABLE_THREAD_COUNT]; // the nested one
sigjmp_buf *The_Hard_Jmp_Envs[KERF_MAX_NORMALIZABLE_THREAD_COUNT]; // ignore nesting
#if CPP_WORKSTACK_ENABLED
  // TODO haven't looked where these should actually go yet, possibly where the setjmp is called, probably on a per-thread basis
  // TODO guessing this is really like a frame height stored in the kervm stack
  I The_Cpp_Slop_Workstacks_Prior_Heights[KERF_MAX_NORMALIZABLE_THREAD_COUNT];
  I The_Cpp_Generic_Workstacks_Prior_Heights[KERF_MAX_NORMALIZABLE_THREAD_COUNT];
#endif
std::atomic<I> The_Mmap_Total_Byte_Counter = 0;
std::atomic<I> The_Munmap_Leak_Tracker = 0;

#pragma mark -

void kerf_exit(int exit_code);
int signal_init();
void handle_SIGINT(int sig, siginfo_t *info, void *context);

#pragma mark - Wrappers

template<typename L> void hard_jmp_wrapper(L &&lambda);
template<typename L>  int signal_safe_wrapper(L &&lambda);
template<typename L>  int thread_cancel_safe_wrapper(L &&lambda); // We determined that pthread_kill > pthread_cancel, so I don't think we use cancel.
template<typename L> void critical_section_wrapper(L &&lambda) noexcept;
#pragma mark -

struct REGISTERED_FOR_LONGJMP_WORKSTACK
{
#if CPP_WORKSTACK_ENABLED

  // Remark. We would rather throw C++ exceptions from signal handlers; however, because
  // clang++ does not currently (2021.08.10) support this on Linux, we must use
  // sigsetjmp+siglongjmp and unwind a stack of our own creation. When Linux+Clang
  // does support this the impact should be less than a 15% increase in binary size
  // with an upside of a significantly simpler codebase. See more in notes (readme.txt).

  // Warning. It's tempting to subclass SLOP from this, but I'm not sure that we can because we
  // use offsetof and this is possibly non-portable, though I'm not sure, when subclassing.
  // example: ./presented.cc:11:76: warning: offset of on non-standard-layout type 'kerf::SLOP' [-Winvalid-offsetof] 
  // Idea. We could potentially use Abstract class, then below that SLOP and a realized class which
  // our other new C++ classes can derive from.
  // Warning. There are other issues though, like mixing SLOPs with non-SLOPs
  // in the stack (literally, two different object classes which we have to
  // differentiate), which we don't necessarily want to do for debugging
  // purposes, so maybe we should keep them separate, which is low overhead.

  // Warning. I think you have to be careful not to have any objects A which
  // subclass this have descendent member objects B which are also subclasses
  // of this, or you will trigger a double destruct when both of them are
  // popped off the workstack. What is OK is to have non-member variables, such
  // as those on a method's stack, see MUTEX/SILLY_GUARD (SILLY_GUARD and
  // THREAD_POOL are both subclasses of this, and MUTEX lives on THREAD_POOL).

  // TODO lock guard class

  I index_of_myself_in_cpp_workstack = -1; // call the destructor after ctrl-c. compile-time option.

  auto get_my_cpp_workstack();
  void register_in_cpp_workstack();
  void deregister_in_cpp_workstack();

  // POP. We don't need ready() or the (-1==...) checks unless we're handling statically allocated globals that descend from this, so we can rewrite any such children to init in the main kerf init function instead. Other potential solns include, changing to __attribute__((init_priority(65535))) (<-- this didn't seem to work), using new/delete, etc.
  // Warning. Global items may allocate before workstack or thread mechanisms  exists, causing crash
  // Warning. Global items may destruct after workstack is gone, which would cause a crash
  const bool SAFE_FOR_GLOBALS = true;
  bool ready() { return The_Language_Initialized_Flag; }  // technically, we need only need threads init'd and the workstacks 

  REGISTERED_FOR_LONGJMP_WORKSTACK()
  {
    if(SAFE_FOR_GLOBALS && !ready()) return;
    register_in_cpp_workstack();
  }

  void registered_destructor_helper()
  {
    // Must be `virtual` so we can call destructor of children without knowing class
    // Additionally, if we're calling the destructor, we don't need this deregister here [in the "ancestor"]
    if(SAFE_FOR_GLOBALS && -1 == index_of_myself_in_cpp_workstack) return;
    deregister_in_cpp_workstack();
  }

  virtual ~REGISTERED_FOR_LONGJMP_WORKSTACK()
  {
    registered_destructor_helper();
  }

  REGISTERED_FOR_LONGJMP_WORKSTACK(REGISTERED_FOR_LONGJMP_WORKSTACK const& x)
  {
    register_in_cpp_workstack();
    assert(index_of_myself_in_cpp_workstack != -1);
    assert(index_of_myself_in_cpp_workstack != x.index_of_myself_in_cpp_workstack);
    return;
  }
  REGISTERED_FOR_LONGJMP_WORKSTACK(REGISTERED_FOR_LONGJMP_WORKSTACK && x)
  {
    // POP you can maybe save some cycles and workstack space by claiming the 
    // other guy's (x's) position in the workstack, and somehow disabling his deregister
    // potentially by reassigning his index to -1 and adding a check in destruct/deregister for this
    // (rewriting the position in the workstack with your pointer)
    // but i am not going to test it now
    register_in_cpp_workstack();
    return;
  }
  REGISTERED_FOR_LONGJMP_WORKSTACK& operator=(REGISTERED_FOR_LONGJMP_WORKSTACK const& x)
  { 
    assert(index_of_myself_in_cpp_workstack != -1);
    assert(index_of_myself_in_cpp_workstack != x.index_of_myself_in_cpp_workstack);
    return *this; 
  }
  REGISTERED_FOR_LONGJMP_WORKSTACK& operator=(REGISTERED_FOR_LONGJMP_WORKSTACK && x)
  { 
    assert(index_of_myself_in_cpp_workstack != -1);
    assert(index_of_myself_in_cpp_workstack != x.index_of_myself_in_cpp_workstack);
    return *this; 
  }

#endif
};

#pragma mark -



const char* error_string(int val)
{
  //As a matter of policy, it's fine to do the slower
  //version of error throwing where the putative
  //returned structure is allocated first
  //(provided its freed before the error is thrown,
  // or managed in a safe way, as with KVM->workspace)
  //The reason is that errors are not usually part
  //of a normal execution flow, so doing this won't
  //affect any performance that matters.

  switch(val)
  {
    case ERROR_DISK:              return "Disk error";
    case ERROR_FILE:              return "File error";
    case ERROR_VMEM:              return "Virtual memory error";
    case ERROR_DEPTH:             return "Depth limit exceeded error";
    case ERROR_CTRL_C:            return "Caught interrupt signal";
    case ERROR_REMOTE_HUP:        return "Remote socket closed during execution";
    case ERROR_SIZE:              return "Size error";
    case ERROR_SIGN:              return "Sign error";
    case ERROR_LENGTH:            return "Length error";
    case ERROR_REFERENCE:         return "Reference error";
    case ERROR_VARIABLE:          return "Undefined token error";
    case ERROR_RANK:              return "Rank error";
    case ERROR_INDEX:             return "Index error";
    case ERROR_ARITY:             return "Arity error";
    case ERROR_VALENCE:           return "Valence error";
    case ERROR_REPEAT:            return "Repeat error";
    case ERROR_ARGS:              return "Argument error";
    case ERROR_CONFORMABLE:       return "Conformable error";
    case ERROR_OBJECTTYPE:        return "Type error";
    case ERROR_TIME:              return "Time error";
    case ERROR_STRING:            return "String error";
    case ERROR_ARRAY:             return "Array error";
    case ERROR_MAP:               return "Map error";
    case ERROR_TABLE:             return "Table error";
    case ERROR_KEYS:              return "Key mismatch error";
    case ERROR_COLUMN:            return "Column error";
    case ERROR_ROW:               return "Row error";
    case ERROR_RAGGED:            return "Ragged table error";
    case ERROR_LEX_UNKNOWN:       return "Unknown token error";
    case ERROR_LEX_INCOMPLETE:    return "Incomplete token error";
    case ERROR_PARSE_UNKNOWN:     return "Unknown parse group error";
    case ERROR_PARSE_INCOMPLETE:  return "Incomplete parse group error";
    case ERROR_PARSE_UNMATCH:     return "Unmatched parse group error";
    case ERROR_PARSE_OVERMATCH:   return "Parse group overmatch error";
    case ERROR_PARSE_MISMATCH:    return "Parse group mismatch error";
    case ERROR_PARSE_NESTED_SQL:  return "Improperly nested SQL parse error";
    case ERROR_PARSE_DEPTH:       return "Parse group depth error";
    case ERROR_PARSE_DERIVED:     return "Derived verbs disallowed in this context error";
    case ERROR_PARSE_LAMBDA_ARGS: return "Function arguments contained disallowed name error";
    case ERROR_PARSE_SQL_VALUES:  return "Malformed SQL INSERT syntax for VALUES error";
    case ERROR_PARSE_NUM_VAR:     return "Variables cannot start with numbers error";
    case ERROR_SUBERROR:          return "Inherited error";
    case ERROR_NET:               return "Network error";
    case ERROR_HOST:              return "Host error";
    case ERROR_DYLIB:             return "Dynamic library error";
    case ERROR_PARALLEL:          return "Parallel execution error";
    case ERROR_ATLAS_MAP:         return "Atlas passed non-map error";
    case ERROR_JSON_KEY:          return "Bad JSON key error";
    case ERROR_RADIX:             return "Invalid radix error";
    case ERROR_FORMAT_STRING:     return "Invalid format string error";
    case ERROR_ZIP:               return "Compression error";
    case ERROR_FORKED_VERB:       return "Forked verb write error";
    case ERROR_MISSING:           return "Missing feature error";
    case ERROR_MEMORY_MAPPED:     return "Memory mapped error";
    case ERROR_CAPPED_APPEND:     return "Capped append width exceeded error";
  }

  return "Unspecified error";
}



}

