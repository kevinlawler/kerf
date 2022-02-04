namespace KERF_NAMESPACE {
template<typename L>
void SLOP::iterator_uniplex_layout_subslop(L &lambda, bool* early_break_flag, bool reverse, bool memory_mapped_descend) const
{
  //lambda ex: auto g = [&](const SLOP& x) {r.cow_layout_append(x);};

  // Remark. This iterator thing is not just TAPE_HEADs, but also yields amortized O(1)-indexing/O(n)-iteration on LISTs, who would otherwise have O(n^2) iteration (if we do choose to allow LISTs in the mainline processing code). Remark. The `reverse` iteration isn't going to work so well on LISTs, but presumably we don't need to
  // Remark. The iterator is now essential if we have implemented LISTs. Note that using payload_used_chunk_count in isolation is still OK (eg for `n`) if we don't mind the linear hit.

  I inc = reverse ? -1 : 1;

  ITERATOR_LAYOUT z(*this, reverse, true, memory_mapped_descend);
  if(z.known_item_count != 0)
  do
  {
    lambda(*z);
    if(!z.has_next(reverse) || (early_break_flag && *early_break_flag)) break;
    z.next(inc);
  } while(1);

  return;

  // Observation. Specialty iterator above buys somewhere between 10%-25%.
  // P_O_P this n is going to be "wrong", because it's linear for LIST (EXCEPT when regular-stride). we need to use has_next, fix payload_used_chunk_count for LIST, reg stride LIST, JUMP_LIST
  LAYOUT_BASE &r = *layout();
  I n = r.payload_used_chunk_count();
  // P_O_P you could optimize this for certain vectors, like INT3_VEC, by making one slop, doing a simple int3* increment into the vector address space, and then replacing the ->i value. put the major types in a switch statement P_O_P consider instead the TAPE_HEAD pattern.
  DO(n, I j = reverse ? n - 1 - i : i; lambda(r[j]);)
  return;
}

template<typename L>
SLOP SLOP::iterator_uniplex_layout_subslop_accumulator(L &lambda) const
{
  //lambda ex: auto g = [&](const SLOP& x) -> SLOP {r.cow_layout_append(x); return r;};

  SLOP a(UNTYPED_ARRAY); // P_O_P pass hints on end-size (->m) to accumulator
  auto g = [&](const SLOP& x) {a.layout()->cow_append(lambda(x));};
  this->iterator_uniplex_layout_subslop(g);
  return a;
}

template<typename L>
void SLOP::iterator_uniplex_presented_subslop(L &&lambda, bool* early_break_flag, bool reverse) const
{
  if(presented()->is_simply_layout())
  {
    iterator_uniplex_layout_subslop(lambda, early_break_flag, reverse);
    return;
  }

  // TODO PRESENTED versions of efficient iterators, like layout has?
  //      ???? how do we iterate presented? I guess the idea i had was to implement a per-class [PRESENTED-CLASS] method
  //      figure out what to do for FOLIO, UNHASHED_MAP, see readme.txt

  PRESENTED_BASE &p = *this->presented();
  I n = this->countI();
  DO(n, I j = reverse ? n - 1 - i : i; lambda(p[j]); if(early_break_flag && *early_break_flag) break;)
}

template<typename L>
SLOP SLOP::iterator_uniplex_presented_subslop_accumulator(L &lambda) const
{
  //lambda ex: auto g = [&](const SLOP& x) -> SLOP {r.cow_layout_append(x); return r;};
  SLOP a(UNTYPED_ARRAY); // P_O_P pass hints on end-size (->m) to accumulator
  auto g = [&](const SLOP& x) {a.layout()->cow_append(lambda(x));};
  this->iterator_uniplex_presented_subslop(g);
  return a;
}

template<typename L>
void SLOP::iterator_duplex_layout_subslop(L &lambda, const SLOP& rhs, bool* early_break_flag) const
{
  // Idea. How to do arbitrary (triplex, quadruplex, ...) iterators in C++: https://stackoverflow.com/a/34315532 use a `...` lambda and (optionally?) function folding. Then, probably, an array `a[i]` of iterating-slops instead of named `x,y,z` 

  // lambda ex: auto g = [&](const SLOP& x, const SLOP& y) {a.cow_layout_append(lambda(x,y));};

  LAYOUT_BASE &p = *this->layout();
  LAYOUT_BASE &q = *rhs.layout();

  // P_O_P: payload_used_chunk_count() is going to be linear here for LISTs. In a sense, that's OK, since this is already O(n), however,
  //        to fix this, implement a method that gets item-count but stops at `2`, so in {0,1,2}, then use that to `reconcile`, and
  //        [ update: the method exists now: LAYOUT_BASE::payload_count_zero_one_or_two_plus_chunks() ]
  //        instead of using `b` count below, use while(has_next()) on the two iterators.
  //        you also need `|| !d.has_next()` or something like that: you need to check that one [of c,d] doesn't end early, and then throw Length Error

  I m = p.payload_used_chunk_count();
  I n = q.payload_used_chunk_count();

  if(0==m || 0==n) return;

#if LENGTH_1_LISTS_RECONCILE_WITH_N
  // NB. Factored with PRESENTED version
  // Remark. Units should be handled higher [if we don't want them to work here], eg, in general_unit_atomic_duplex_layout_combiner 

  // P_O_P if this is slow, it's almost easier to munge it into the `DO` below, locking the respective index values to 0.

  if(1 != m && 1 == n)
  {
    const SLOP &s = q[0];
    auto g = [&](const SLOP& x) {lambda(x,s);};
    this->iterator_uniplex_layout_subslop(g);
    return;
  }
  else if(1 == m && 1 != n)
  {
    const SLOP &r = p[0];
    auto g = [&](const SLOP& x) {lambda(r,x);};
    rhs.iterator_uniplex_layout_subslop(g);
    return;
  }
#endif

  if(m!=n) die(Must implement length error in iterator); // TODO: Length Error

  I b = MAX(m,n);

  /////////////////////////////////////////
  ITERATOR_LAYOUT c(*this);
  ITERATOR_LAYOUT d(rhs);

  do
  {
    lambda(*c,*d);
    if(!c.has_next() || (early_break_flag && *early_break_flag)) break;
    c.next();
    d.next();
  } while(1);

  return;
  /////////////////////////////////////////

  DO(b, I j = i; I k = i; 
      const SLOP &r = p[j];
      const SLOP &s = q[k];
      lambda(r,s);)
  return;
}

template<typename L>
SLOP SLOP::iterator_duplex_layout_subslop_accumulator(L &lambda, const SLOP& rhs) const
{
  // lambda ex: auto g = [&](const SLOP& x, const SLOP& y) -> SLOP {return general_unit_atomic_duplex_layout_combiner(unit_pairwise_op, x, y);};

  // Remark. It's fairly simple to add unit handling here (eg, test and return lambda(this,rhs), instead of the same thing enlisted) but seems improper to do so. These things should only know about LAYOUT.

  SLOP a(UNTYPED_ARRAY); // P_O_P pass hints on end-size (->m) to accumulator. anything works, maybe min of the two args
  auto g = [&](const SLOP& x, const SLOP& y) {a.layout()->cow_append(lambda(x,y));};
  this->iterator_duplex_layout_subslop(g, rhs);
  return a;
}

template<typename L>
void SLOP::iterator_duplex_presented_subslop(L &lambda, const SLOP& rhs, bool* early_break_flag) const
{
  PRESENTED_BASE &p = *this->presented();
  PRESENTED_BASE &q = *rhs.presented();

  auto sl = p.is_simply_layout();
  auto sr = q.is_simply_layout();
  if(sl && sr)
  {
    return iterator_duplex_layout_subslop(lambda, rhs, early_break_flag);
  }

  I m = p.count();
  I n = q.count();

  if(0==m || 0==n) return;

#if LENGTH_1_LISTS_RECONCILE_WITH_N
  // NB. Factored with LAYOUT version
  if(1 != m && 1 == n)
  {
    const SLOP &s = q[0];
    auto g = [&](const SLOP& x) {lambda(x,s);};
    return this->iterator_uniplex_presented_subslop(g);
  }
  else if(1 == m && 1 != n)
  {
    const SLOP &r = p[0];
    auto g = [&](const SLOP& x) {lambda(r,x);};
    return rhs.iterator_uniplex_presented_subslop(g);
  }
#endif

  if(m!=n) die(Must implement length error in iterator); // TODO: Length Error

  I b = MAX(m,n);

  // TODO PRESENTED versions of efficient iterators, like layout has?

  DO(b, I j = i; I k = i; 
      const SLOP &r = p[j];
      const SLOP &s = q[k];
      lambda(r,s);
      if(early_break_flag && *early_break_flag) break;
  )
}

template<typename L>
SLOP SLOP::iterator_duplex_presented_subslop_accumulator(L &lambda, const SLOP& rhs) const
{
  SLOP a(UNTYPED_ARRAY); // P_O_P pass hints on end-size (->m) to accumulator. anything works, maybe min of the two args
  auto g = [&](const SLOP& x, const SLOP& y) {a.layout()->cow_append(lambda(x,y));};
  this->iterator_duplex_presented_subslop(g, rhs);
  return a;
}

#pragma mark - Mmap Early Remove Queue

template<typename T = SLAB>
struct EARLY_QUEUE
{
  // Rule. We must have as many available EARLY_QUEUE slots as we have temporary values that require maps (eg, 2 for a dyadic operation).

  // Assumption. One of our concerns with the EARLY_QUEUE is that add-remove or remove-add is greatly contracted and the same MEMORY_MAPPED object encounters an exotic race condition where, for instance, it's registering with the EARLY_QUEUE but being immediately unmapped at the same time. I think we can ignore this for now. (I think we're handling this case now.)
  // Assumption. Various overuses of the EARLY_QUEUE can create deadlocks. Eg, I dunno, maybe you're near capacity and everything is sutex-locked and trying to capture a lock on each other or something. The EARLY_QUEUE is not responsible for that.

  // POP./Idea. When you're doing a pop b/c you ran out of space, you might want to go through and check which sutexes the current thread owns (you can only check write locks easily this way, without doing something exotic like examining sutex_guard stack), and then move them all to the front.
  // Remark./Feature./POP. In the EARLY_QUEUE we want to unmap MAP_PRIVATE & ~PROT_WRITE first, before we unmap MAP_SHARED & PROT_WRITE, because those are going to be slower unmaps (MAP_SHARED), and as we specified MAP_PRIVATE & PROT_WRITE are not going to be in the EARLY_QUEUE at all because we'd lose those changes.
  // POP. Similarly, we may want to skip over MEMORY_MAPPED where the READ lock is set or the WRITE lock is set. Not sure which to skip first. | I've commented on things like this before. We may want to do multiple passes looking to cherry pick the best, eg not-at-all-locked plus MAP_PRIVATE & ~PROT_WRITE, then begin relaxing conditions. First locks would be locks we own [so I guess necessarily writes we own], then locks others own [maybe reads or writes first]. Eventually you'll have to block-wait on something, which could produce a deadlock (we accept this as an ASSUMPTION).

  // Question. What if you're trying to pop something and you have the sutex read lock on the MMAPPED_OBJECT but it's actually you and you don't know it? Is that a deadlock condition? Idea. Perhaps we have a nested-mmap read/write condition that you're not supposed to violate which is based on default MAXIMUM DEPTH. Then you need an assertion that allowable open/mmapped file is greater than the depth. Idea. We could also have a condition that the read-sutex on the MEMORY_MAPPED object, you're not allowed to do any write locks within it, i dunno if this is possible. Idea. Maybe the check is while you're trying to pop, and the read OR the write lock is set on the sutex, you just move all those to the front, at least for the first loop, then you wait on readers, and if that doesn't work then you wait on writers. Or you can just wait on readers and call that a known-acceptable-intentional deadlock condition, like maxing a read-count in a sutex. *The thing is though, they can't unlock if they're waiting and you have the earlyq mutex!* So maybe you unlock the earlyq, return a failure code, and try again infinitely, and that's our version of the "readers-are-maxed-wait-indefinitely" thing.

  std::deque<I> available;
  std::deque<I> used;

  I current_residents = 0;
  I lifetime_pushed = 0;
  I lifetime_popped = 0;

  T **resident = nullptr;
  I *resident_bdays= nullptr;
  I capacity;

  friend std::ostream& operator<<(std::ostream& o, EARLY_QUEUE& x) {return o << x.to_string();} 

  std::string to_string()
  {
    std::string s = "";

    s += "{\n";

    s += "available: ";
    for(auto x : available) s += std::to_string(x) + ", "; 
    s += "\n";

    s += "used: ";
    for(auto x : used) s += std::to_string(x) + ", "; 
    s += "\n";

    s += "resident: ";
    DO(capacity, s += std::to_string((I)resident[i]) + ", ")
    s += "\n";

    s += "resident_bdays: ";
    DO(capacity, s += std::to_string((I)resident_bdays[i]) + ", ")
    s += "\n";

    s += "current_residents: " + std::to_string(current_residents) + "\n";
    s += "lifetime_pushed: "   + std::to_string(lifetime_pushed) + "\n";
    s += "lifetime_popped: "   + std::to_string(lifetime_popped) + "\n";
    s += "capacity: "          + std::to_string(capacity) + "\n";

    s += "}";
    s += "\n";
    return s;
  }

  static const I UNSET_INDEX_SENTINEL = -1;

  void (T::*callback_inform_removed)() = &T::callback_from_mapped_queue_to_inform_removed;
  void (T::*callback_set_index)(I) = &T::callback_from_mapped_queue_to_store_index;

  // Remark. If we do lock the mutex on this object, we should be fairly sure, as we kind of are now, that we aren't going to be doing anything "complicated" that could result in a tight loop [that we need to exit from], otherwise we need to rewrite our ctrl-c method to be more aggressive, because it's going to wait on the lock
  MUTEX_UNTRACKED mutex;

  EARLY_QUEUE(I desired_capacity = EARLY_QUEUE_DEFAULT_CAPACITY) : capacity(desired_capacity)
  {
    capacity = MAX(capacity,1);
    assert(capacity >= 1);
    resident = new T*[capacity];
    resident_bdays = new I[capacity];
    DO(capacity, available.emplace_back(i)) 
  }

  ~EARLY_QUEUE()
  {
    delete[] resident;
    delete[] resident_bdays;
  }

public:

  void test()
  {

  }

  void push_back(T* x)
  {
    mutex.lock_safe_wrapper([&]{
      unsafe_push_back(x);
    });
  }

  // auto pop_front()
  // {
  //   decltype(unsafe_pop_front()) i;
  // 
  //   mutex.lock_safe_wrapper([&]{
  //     i = unsafe_pop_front();
  //   });
  // 
  //   return i;
  // }

  void early_erase(I resident_index)
  {
    mutex.lock_safe_wrapper([&]{
      unsafe_early_erase(resident_index);
    });
  }

private:

  void unsafe_push_back(T* x)
  {
    I resident_index = -1; // eq pop()

    if(available.size() == 0)
    {
      assert(current_residents >= capacity);
      resident_index = this->unsafe_pop_front(); // Warning. Putting this in a critical section puts the munmap callback inside one
    }

    critical_section_wrapper([&]{

      if(available.size() > 0)
      {
        assert(-1 == resident_index);
        resident_index = available.front();
        available.pop_front();
      }

      assert(-1 != resident_index);

      while(used.size() > 0 && used.back() == -1)
      {
        used.pop_back();
        lifetime_pushed--;
        assert(lifetime_pushed >= 0);
      }

      if(DEBUG)
      {
        for(auto x : available) assert(x != resident_index);
        for(auto y : used) assert(y != resident_index);
      }

      used.emplace_back(resident_index);
      current_residents++;
      resident[resident_index] = x;
      assert(lifetime_pushed >= 0);
      resident_bdays[resident_index] = lifetime_pushed++;

      if(II == lifetime_pushed)
      {
        die(Thread Safe Early Remove Queue lifetime maximum reached);
        // Idea. you can actually walk the whole data structure and subtract out a large value, say at II/2 for instance, to take everything back down to near 0, as many times as you need. (This is easy, you have the lock on this EARLY_QUEUE already, the remote MEMORY_MAPPED's only store indexes into it, so you can modify everything as needed. You can trigger it as often as necessary, either at II/2 or every 10k, or whatnot.)
      }

      // communicate resident_index to object
      (x->*callback_set_index)(resident_index);
    });

  }

  I unsafe_pop_front()
  {
    // See remarks at A_MEMORY_MAPPED::good_maybe_munmap

    if(used.size() <= 0) return -1;

    I next_index = used.front();

    if(-1 == next_index)
    {
      // Improvement. should probably assert tail recursion here, so that we don't get stack overflow, even though our queue size limit may come under. Alternatively you have to build this so it doesn't recurse.
      critical_section_wrapper([&]{
        // sic, we duplicate these modification to put the second use after the x->callback
        used.pop_front();
        lifetime_popped++;
      });
      return unsafe_pop_front();
    }

    if(DEBUG)
    {
      for(auto x : available) assert(x != next_index);
    }

    T *x = resident[next_index];
    I bday = resident_bdays[next_index]; 

    (x->*callback_inform_removed)();

    critical_section_wrapper([&]{
      used.pop_front();
      lifetime_popped++;
      resident[next_index] = nullptr;
      resident_bdays[next_index] = -1;
      current_residents--;
    });

    if(DEBUG)
    {
      for(auto y : used) assert(y != next_index);
    }


    // Remark. When we, say, catch ctrl-c (or catch a KERF_ERROR?), "reset" all the threads however we're going to do that, then reset this mutex, then go through the list of MAPPED_OBJECTs and reset all their mutexes. Maybe even need to go through Kerf_Tree and reset all the read/write locks. WARNING you may not be able to do this: often unlocking a mutex is undefined behavior if you weren't the one who locked it. it IS possible to get it to only error say with PTHREAD_MUTEX_RECURSIVE better: PTHREAD_MUTEX_ERRORCHECK, in which case, we can have every thread attempt this in its pthread_cleanup_push routine. However, see below for an alternative. Also, that doesn't ensure ctrl-c signal safety inside the main thread (because it's not going to cancel itself).

    // TODO/Question. Well now the tricky part is, do i need a mutex lock every time I do a read on the object to ensure I'm not about to get early released by an external thread? Possibly we need to wait until a thread is idle to do this releasing! Think about this. Maybe it's enough to have an object unmap itself as it goes out of view...?? I suppose you could put a mutex directly on the MAPPED_OBJECT and that would solve things (it's a lot of locking though). Locking+unlocking a mutex is actually pretty fast https://stackoverflow.com/a/49712993/

    // vvvvv this may be junk b/c it's unlikely/impossible to reach this case here
    // if(0 == current_residents)
    // {
    //   assert(available.size() == capacity);
    //   assert(used.size() == 0);
    //   // reset counter
    //   lifetime_pushed = 0;
    //   lifetime_popped = 0;
    // }

    // Feature. returning this outside of the critical section means that we might leak it [on ctrl-c]. Probably what we should do is store it locally on the EARLY_QUEUE, and then have the "reclamation" process be wrapped in a critical section. Actually, because we never call pop_front() (vs. unsafe_pop_front), it should be sufficient to modify an integer passed by reference. The way to do this is to make unsafe_push_back use for what it currently has as `resident_index` instead an integer member of EARLY_QUEUE, which can be -1 at times. `unsafe_push_back` can then be modified to check if the member is available (not -1). in this way, popped values would no longer be leaked.

    return next_index;
  }

  void unsafe_early_erase(I resident_index)
  {
    
    critical_section_wrapper([&]{
      if(DEBUG)
      {
        for(auto y : available) assert(y != resident_index);
      }

      I& i = resident_index;

      assert(i >= 0);
      I bday = resident_bdays[i];

      I ui = bday - lifetime_popped;

      assert(bday >= 0);
      assert(bday >= lifetime_popped);
      assert(ui >= 0);
      assert(ui < used.size());

      used[ui] = -1;

      auto x = resident[i];

      resident[i] = nullptr;
      resident_bdays[i] = -1;
      current_residents--;

      available.emplace_back(i);

      if(DEBUG)
      {
        for(auto y : used) assert(y != resident_index);
      }

    });

    // (x->*callback_set_index)(UNSET_INDEX_SENTINEL); // optional, but see HACK for file origin pointer inside safe munmap for MEMORY_MAPPED
  }

};

template<typename T = SLAB>
struct EARLY_QUEUE_UNTRACKED : EARLY_QUEUE<T>
{
  using EARLY_QUEUE<T>::EARLY_QUEUE; // inherit constructors
};

template<typename T = SLAB>
struct EARLY_QUEUE_TRACKED : EARLY_QUEUE_UNTRACKED<T>, REGISTERED_FOR_LONGJMP_WORKSTACK
{
  using EARLY_QUEUE_UNTRACKED<T>::EARLY_QUEUE_UNTRACKED; // inherit constructors
};

EARLY_QUEUE<SLAB> The_Thread_Safe_Early_Remove_Queue;

}
