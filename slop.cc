namespace KERF_NAMESPACE {
#pragma mark - Accessors

SLAB* SLOP::auto_slab() const
{
  return (SLAB*)this->slab_data;
}

LAYOUT_BASE* SLOP::layout() const
{
  return (LAYOUT_BASE*)this->layout_vtable;
}

PRESENTED_BASE* SLOP::presented() const
{
  return (PRESENTED_BASE*)this->presented_vtable;
}

#pragma mark - Hash

HASH_CPP_TYPE SLOP::hash(HASH_METHOD_MEMBER method, HASH_CPP_TYPE seed) const
{
  return presented()->hash(method, seed);
}

#pragma mark -

MEMORY_POOL& SLOP::get_my_memory_pool()
{
  return The_Thread_Memory_Pools[kerf_get_cached_normalized_thread_id()];
}

#pragma mark -

#if CPP_WORKSTACK_ENABLED
  auto SLOP::get_my_cpp_workstack()
  {
    auto i = kerf_get_cached_normalized_thread_id();
    return &The_Cpp_Slop_Workstacks[i];
  }
  
  void SLOP::register_in_cpp_workstack()
  {
    auto& i = this->index_of_myself_in_cpp_workstack;
    assert(-1 == i);
    auto w = get_my_cpp_workstack();
    auto c = w->size();
    w->emplace_back(this);
    i = c;
    assert(w->size() == c + 1);
  }
  
  void SLOP::deregister_in_cpp_workstack() noexcept
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

    // TODO POP. Compiler flag for "shrinking" the stack height while() the top is zeroed (nullptr). See also also registerable class.
  }
#endif

#pragma mark -

void SLOP::cow() // coerce cowed
{
  // QUESTION does this handle PRESENTED/GROUPED???? I'm not sure it does. you may be able to do a simple iterate/append on layout() into a new grouped. Answer. Oh, I guess it does. The ram copy increments children. I'm coming back after a hiatus 2021.03.27

  // Rule. cow reference swapping should always happen immediately (rewriting parents, fixing refcount owner, etc.) vs. delaying to end of C++/Kerf frame.

  I rc = this->slabp->r_slab_reference_count;
  I arena = this->slabp->reference_management_arena;

  // if(layout()->header_get_slab_is_on_drive())
  {
    // TODO Question Anything to do here? Idea. I think maybe reference counting should be handled at the MEMORY_MAPPED object level (its layout). 
    // Disk is always going to be shared even with mult reference, unless we do something weird like remapping our own MAP_PRIVATE, in which case, why, I mean, we could do that I guess (some kind of "read-only" mapping of tables into memory?)
  }

  switch(arena)
  {
    case REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT:
      // We can always write these
      return;
    case REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED:
      assert(is_tracking_parent_rlink_of_slab() || this->slabp == The_Kerf_Tree);
      [[fallthrough]];
    case REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK:
    case REFERENCE_MANAGEMENT_ARENA_KERFVM_WORKSTACK:
      switch(rc)
      {
        case 0:
          die(Attempted to copy-on-write a reference-count zero object)
        case 1:
          return; // We can write these
        default:
          break;
      }
    default:
      break;
  }

  if(presented()->allow_to_copy())
  {
    this->coerce_to_copy_in_ram(true);
  }
  else
  {
    // Remark. 2021.10.17 for now, only MEMORY_MAPPED has !allow_to_copy() and it's OK to write on MEMORY_MAPPED anyway. You could perhaps imagine other control flows: error or something, for other objects if you try to cow() them, etc.

    // no-op
    return;
  }
}

void SLOP::coerce_to_copy_in_ram(bool reference_increment_children)
{
  assert(presented()->allow_to_copy());
  assert((!is_tracking_parent_rlink_of_slab()) && (!is_tracking_parent_slabm_of_slab())); // I'm not sure what this is for. figure it out. // NB. Laughably, sadly, probably what we should do is fork this method into two separate nearly identically ones (dropping this parent tracker line) so that we can go ahead and use similar stuff where we know it's safe. I can't prove yet that this "old" function isn't blocking some disallowed application of copy (say in iterators or what not). Possibly even this line was added by an entire line paste typo, things like that happened around that time. Answer. Well, one use of it is in legit_reference_increment, when you cow you want the parent to stay fixed. The o̶n̶l̶y̶ other use of it (c03rc3_parented) says we can obviate it. So this problem is not as deep as previously thought. There is a use also in cow(). I'm not so sure I understand this now, but it seems related to being parented and cow(). It may be best to just fork it. Question. I think the problem is maybe, you're copying C to RAM, if C is in RAM you should be OK (and we can return here? having met the coerce requirement?), if not, why is it parent_tracking? (Question. were we allow to parent track a TRANSIENT thing? or only PARENTED arenas??) Alternatively, perhaps the issue is, if we copy this to RAM, we need to update the parent pointer, but we can't guarantee the parent is refcount==1 without having the origin pointer for the parent, which we don't have? Remark. This function is pretty weird anyway because it does a force_copy but never checks whether it needs to, yet it's called "coerce"...i guess the coerce part is the "copy".

  SLAB *new_slab = force_copy_slab_to_ram(reference_increment_children, false);
  assert(this->slabp != new_slab);

  bool decrement_subelements = false;

  if(this->slabp == this->auto_slab() && this->layout()->layout_payload_chunk_type() == CHUNK_TYPE_RLINK3 && this->countI() == 1)
  {
    // reference_increment_children = false; // shouldn't need to increment 
    if(reference_increment_children) decrement_subelements = true;
    // Note. Not incrementing above should be equivalent to decrementing below [in c03rc3_to_copy_in_ram]
  }

  this->release_slab_and_replace_with(new_slab, decrement_subelements, true); // true==check for layout change, while goofy TAPE_HEAD stuff in force_copy
}

void SLOP::legit_reference_increment(bool copy_on_overflow)
{
  UI old = this->slabp->r_slab_reference_count;
  this->slabp->r_slab_reference_count++;

  // Question. Anything special to do for disk increments?  if(layout()->slab_is_on_disk())

  bool overflow = slabp->r_slab_reference_count < old;

  if(overflow)
  {
    slabp->r_slab_reference_count = old; // wind it back

    if(copy_on_overflow && presented()->allow_to_copy())
    {
      this->coerce_to_copy_in_ram(true); // copy on overflow and assign
    }
    else
    {
      // TODO reference error
    }
  }
}

void SLOP::legit_reference_decrement(bool decrement_subelements_on_free) const noexcept
{
  // SLAB *s = this->slabp;
  
#if TEST_TRACK_ALLOCATIONS
    // if(0 >= s->r_slab_reference_count)
    if(0 >= this->layout()->header_get_slab_reference_count())
    {
      // inspect_slab(s);
      this->inspect();
      er(Slab reference count is already 0 or below somehow.);
      die();
    }
#endif

  this->layout()->header_decrement_slab_reference_count();

  if(this->layout()->header_get_slab_reference_count() <= 0) // NB. If the rc var is unsigned, it never actually will drop below 0
  {

#if TEST_TRACK_ALLOCATIONS
    if(0 > this->layout()->header_get_slab_reference_count())
    {
      er(Slab reference count below 0 somehow.);
    }
#endif

    MEMORY_POOL &my_pool = this->get_my_memory_pool();

    // POP. Can probably bundle pre/in/post into one presented call, probably not much of an improvement
    presented()->dealloc_pre_subelt();

    if(decrement_subelements_on_free)
    {
      // P_O_P cache the memory pool and pass it down

      // only certain arrays should have this non-void
      // Idea. Move to presented as class virtual method?
      switch(layout()->layout_payload_chunk_type())
      {
        case CHUNK_TYPE_VARSLAB:
          if(layout()->known_flat())
          {
            break;
          }
          [[fallthrough]];
        case CHUNK_TYPE_RLINK3:
          this->layout_link_parents_lose_references();
          break;
        default:
          break;
      }
    }


    // TODO ??? send to pool and let pool unmap???
    // if(layout()->header_get_slab_is_on_drive()) { }
    presented()->dealloc_post_subelt();

    my_pool.pool_dealloc(this->layout()->slab_pointer_begin());

    // SLOP * p = (SLOP*)(void*)this;
    // p->slabp =  (SLAB*)0x0123456789ABC;
    // p->slabp = p->auto_slab();
    // *p->auto_slab() = {0};
  }
}

void SLOP::layout_link_parents_gain_references()
{
  auto g = [&](SLOP x) { x.parent_gain_reference(); };
  this->iterator_uniplex_layout_subslop(g, nullptr, false, false);
}

void SLOP::layout_link_parents_lose_references() const
{
  // Question. maybe this is better if it goes somewhere else, like PRESENTED?

  // Remark. How we're proceeding here: since it looks like we're going to have to recurse/SLOP on this all anyway, do a reverse iterator, SLOP each one, handle the decrement if it's PARENT arena, RECURSE into the slop if you're going to be freeing the thing (this is why it's necessary to have a SLOP in the first place)

  // P_O_P?
  // case RLINK3_UNIT:
  // case RLINK3_ARRAY:
  // case SLABM_UNIT:
  // case SLAB4_ARRAY:

  // P_O_P: we can reverse on SLAB4_ARRAY (soon to be LIST) *only* when we have the guaranteed regular-stride of the subelements (viz. we can reverse at all)
  //        when we can, it's better to (?), because it keeps the memory pool linked-list stacked in the same order
  bool reverse = false;
  switch(layout()->layout_payload_chunk_type())
  {
    case CHUNK_TYPE_VARSLAB:
      assert(!layout()->known_flat());
      if(layout()->payload_known_regular_width())
        reverse = true;
      break;
    case CHUNK_TYPE_RLINK3:
      reverse = true;
      break;
    default:
      break;
  }

  auto g = [&](const SLOP& x) { x.parent_lose_reference(true); };
  this->iterator_uniplex_layout_subslop(g, nullptr, reverse, false);
}

SLAB* SLOP::force_copy_slab_to_ram(bool reference_increment_children, bool preserve_tape_heads_literal)
{
  // Compare FILE_OPERATIONS::write_to_drive_path_header_payload
  assert(presented()->allow_to_copy());

  C current_m = this->layout()->header_get_memory_expansion_log_width();
  C old_layout = this->layout()->header_get_slab_object_layout_type();

  MEMORY_POOL &my_pool = this->get_my_memory_pool();
  SLAB* dest = (SLAB*)my_pool.pool_alloc_struct(POW2(current_m)); // POP See below: POW2(m) can be oversized

  C revised_layout = old_layout;

  I preserve_front_bytes = LAYOUT_BASE::header_count_memory_bytes_at_front();
  I skip = preserve_front_bytes;

  // P_O_P: won't a lot of these slabs be auto_slab/slab4's that we can copy with a direct access?
  // P_O_P: (SLOP::)layout()->slab_bytes_current_used() is superior to POW2(m) if we have the slab boxed in a SLOP
  // Remark. possibly this is better done as a SLOP-to-SLOP method instead of operating raw on input slabs and returning them
  // memcpy(((char*)dest) + skip, ((char*)layout()->slab_pointer_begin()) + skip, layout()->slab_bytes_current_used() - skip);

  if(preserve_tape_heads_literal)
  {
    SLAB *s = this->slabp;
    memcpy(((char*)dest) + skip, ((char*)s) + skip, POW2(current_m) - skip);
  }
  else
  {
    I header_size = this->layout()->header_bytes_total_capacity();

    // copy header
    memcpy(((char*)dest) + skip, ((char*)this->layout()->header_pointer_begin()) + skip, header_size - skip);

    switch(old_layout)
    {
      case LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM:
      {
        revised_layout = LAYOUT_TYPE_UNCOUNTED_ATOM;
        // HACK? getting tape heads to work when appending to RLINK3_ARRAY, representationally
        // I fun_int = this->presented()->integer_access_raw(this->layout()->payload_pointer_begin());
        I magic = 8;
         // *(I*)(((char*)dest) + magic) = fun_int; 
        SLOP hack = this[0][0]; // P_O_P this is probably slow.
        *(I*)(((char*)dest) + magic) = (I)hack.slabp->n; 
        break;
      }
      case LAYOUT_TYPE_TAPE_HEAD_COUNTED_VECTOR:
      {
        revised_layout = LAYOUT_TYPE_COUNTED_VECTOR;
        I magic = -8;
        memcpy(((char*)dest) + header_size + magic, this->layout()->payload_pointer_begin(), this->layout()->payload_bytes_current_used());
        break;
      }
      default:
        // copy payload
        memcpy(((char*)dest) + header_size, this->layout()->payload_pointer_begin(), this->layout()->payload_bytes_current_used());
        break;
    }
  }

  dest->t_slab_object_layout_type = revised_layout;
  dest->zero_sutex_values();

  if(reference_increment_children)
  {
    layout_link_parents_gain_references();
  }

  return dest;
}

void SLOP::coerce_parented(bool reference_increment_children)
{
  // What this is is a function that moves any other arena to the TREE_OR_PARENTED arena

  // Note. this->slabp may alter through this function, so should not be memoized

  switch(this->slabp->reference_management_arena)
  {
    case REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT:
      // P_O_P: as of writing, we could instead do only force_copy_slab_to_ram here (we had it like that before refactoring. it had enough guarantees at this line ). we have it this way for semantics.
      this->coerce_to_copy_in_ram(reference_increment_children);
      [[fallthrough]];
    case REFERENCE_MANAGEMENT_ARENA_KERFVM_WORKSTACK:
      [[fallthrough]];
    case REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK:
    #if CPP_WORKSTACK_ENABLED
      if (-1 != index_of_myself_in_cpp_workstack)
      {
        deregister_in_cpp_workstack();
      }
    #endif

      {
        // State as of 2021.09.20 Apple clang version 12.0.5 (clang-1205.0.22.11)
        // Remark. So, we had an issue where we had SLOPs---let's say a,b,c,
        // which are destructed c,b,a---were CPP_WORKSTACK allocated values.
        // SLOP c remained CPP_WORKSTACK, but had `a` appended as a child,
        // making the `a` slab parented with reference count 1, but SLOP `a`
        // still pointed to the slab, but c's slop destructed and freed its `c`
        // CPP_WORKSTACK slab first causing it to first free the child parented
        // `a` slab, so when SLOP `a` goes to check itself on the dtor, it checks
        // freed memory (instead of a TREE_OR_PARENTED that it would ignore).
        // Normally, this couldn't happen, but I suspect something is
        // screwy/unexpected with the way C++ dtors nested rvalues.
        // Example: SLOP(1,SLOP(2, SLOP(3,4))) 
        // We would expect the deepest rightmost rvalues to free first, but
        // that doesn't seem to be the case?? (Commas of course, don't
        // typically come with order guarantees, but shouldn't depth?)
        // I think there is also something with "temporary objects" which
        // die at the end of the semicolon.
        // There are a couple ways around this:
        // 2021.09.24 This is fixed now and we use method 1 with flags.
        // 1. Done. Feature. Don't use nested rvalue slops like that, eg, fix
        //    our use of template parameter packs with SLOP(...), and then get
        //    rid of the cow() below. I'd like to do this if I could but I
        //    don't understand parameter packs that well, or even if the
        //    order-of-destruction is the problem here. I didn't
        //    succeed with std::forward but again not something I have
        //    experience with. I suspect the way it is now, we're doing a
        //    lot of unnecessary, heavy copies at the 2 instances of `SLOP c =
        //    rhs` in layout.cc, because of this cow(). This is bad enough that
        //    we really either want to fix how SLOP(...) works or get rid of
        //    it, or do #2 below which is probably harder. It could also be a
        //    problem with how we're freeing stuff inside the constructor after
        //    expanding via the `...` variadic parameter pack template.
        //    Idea. The issue I think is the anonymous SLOP(SLOP(),SLOP())
        //    construction, it's fine if you save it in SLOP a = SLOP(...); as
        //    an lvalue. So potentially we could focus on a fix for just that.
        //    Or maybe, make it so that's where we enforce persistence? The root
        //    here is in the constructor, SLOP(UNTYPED_ARRAY), we could
        //    also inject there and possibly cause it to not return until
        //    the end of the containing scope (contrasting to returning as
        //    a temporary object at the end of the semicolon).
        //    Idea. Another plan of attack is perhaps std::make_tuple
        //    will wrangle this stuff better. auto argtuple = std::make_tuple(arg...);  
        //    Idea. It might be better to enlist the help of a C++ type magic
        //    expert instead of banging my head against this
        //    Idea. We might want to set a bit on SLOPs that says "we stole
        //    your slab so don't destruct." (SLOPs of temporary object
        //    semicolon duration want this). This could happen at eg
        //    layout()->cow_append where we do the parented append(s)
        //    **Idea. We can set a bit on the auto_slab (some unused memory or
        //    layout attribute) aka "flags"**
        //    Idea. Alternatively, a bit that makes SLOPs free parented. Even a
        //    subclass of SLOP. Then that root untyped_array alloc (parent
        //    init) should become a parented allocation, and we eventually
        //    set the refcount to 2 on the slab.
        //    Idea. Capturing SLOP(...) as a *macro* (preprocessor define)
        //    might give us better introspection into what is an rvalue,
        //    xvalue, temporary, or not. In the templated/parameter-packed
        //    case, everything is an lvalue because it's captured as a
        //    reference by the constructor signature.
        // 2. Change our "PARENTED" regime so that children of CPP_WORKSTACK
        //    objects are treated with the appropriate reference counts (the
        //    intention of PARENTED was that it would outlast the scope)
        // 3. Use this cow method here below. This kind of puts a damper on
        //    some of our C++ fun, but what are you going to do if you don't
        //    have guarantees that it will outlive the scope?
        // See: https://stackoverflow.com/questions/21273570
        // Perhaps std::forward can help
        ////////////////////////////////////////////////////////////////////////
        // 2021.09.24
        // Warning. One remaining large issue we're dealing with is that,
        // currently, you can't declare `SLOP a` and then on the next line
        // `SLOP b` but b.append(a), because a.slabp will coerce
        // parented, becoming `a` parented under `b`, and `b` is a cpp
        // workstack object, but `b` frees before `a`, and so `b` destructs
        // freeing the parented slab, but then `a` points to corrupt memory. So
        // you can either always ensure that `b` is allocated before `a` in the
        // C++ code, or you can set a flag on `a` which causes it not to look
        // at its pointer after b is destructed: it may be sufficient to not
        // look at the pointer inside `a` when `a` is destructed.
        // Remark. We have it solved one way, currently, for SLOP(x,y,...) but
        // we need it solved for the above mentioned case with `a` and `b`
        // Rule. 2021.09.25 For now we will do #1.  
        //   1. You either insist on the user respecting C++'s LIFO destructor
        //      order (SLOP a; and SLOP b; lines declared in the right order.
        //      This should work nicely even when the refcount is >=1.
        //   OR
        //   2. If SLAB cpp arena refcount is 1, you can make the SLAB parented
        //      arena, steal it from the SLOP, and set a flag on the SLOP, much
        //      like we did in the case of SLOP(x,y,...) above. If the refcount
        //      is  >=2 you can
        //      Option. Throw a runtime kerf userland error
        //      Option. Throw a run-time crash error/abort/assert
        //      Option. COW the >=2 cpp arena slab, which would create a copy
        //      you could steal for the PARENTED version. Consider A and B
        //      SLOPs were pointing at this CPP arena version, beginning with
        //      refcount two. Option. You could leave both of those SLOPs as
        //      is, and this is guaranteed to work but may eventually make more
        //      copies than you intend. Option. Or you could steal from SLOP A
        //      (assuming it's the one you're `appending` or what not) since
        //      you know where it is, but you don't necessarily know where B is
        //      (since it's not present in the operation). In the steal case,
        //      you may make fewer copies if you re-use A for appending,
        //      however, you must ensure that A is not freed before the
        //      acquirer by flagging A in the appropriate way. There could be
        //      additional machinery that needs to be built here, for instance
        //      possibly transferring/considering flags at the SLOP copy
        //      constructor or things of that nature, since in this case SLOP A
        //      has a longer lifetime than in the SLOP(x,y,...) case, where we
        //      know it dies immediately after because it is a C++ temporary
        //      value.
        //
        //      The theft takes place at 
        //      layout.cc @ 1158 (one version at CHUNK_TYPE_RLINK3, another at CHUNK_TYPE_VARSLAB)
        //        "SLOP c = rhs;"
        //        "c.coerce_parented();"
        //
        // cow();

      }

      this->slabp->reference_management_arena = REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED;
      this->slabp->r_slab_reference_count = 0;
      break;
    case REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED:
      [[fallthrough]];
    default:
      break;
  }

}

void SLOP::parent_gain_reference()
{
  SLAB* s = this->slabp;

  switch(s->reference_management_arena)
  {
    default:
    case REFERENCE_MANAGEMENT_ARENA_KERFVM_WORKSTACK:
    case REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK:
      // TODO/P_O_P assert: unreachable, we should be able to get rid of this switch statement, right?
      // TODO Actually, this is nice to keep because if our stuff goes wonky (race conditions/interrupts) it can prevent weirdness?
      break;
    case REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT:
      // This I suppose is possible if we're SLAB4_ARRAY, not truly, but we can exploit it when iterating over their elements in reverse  
      // Remark. What's going to happen is the inline TRANSIENT RLINK3_UNITs get ignored, because the indexing SLOP skips them and goes right to the slab in question. So in this `case` right here, you really don't want to do anything.
      // this->layout_link_parents_gain_references(); // Question. RLINK3_ARRAY needs this for auto memory???
      // Question. If this is an MMAP sub-element, viz. the DRIVE attribute is set, should we RE-MMAP a new map here (preferred), or do a RAM copy, or ... ?
      break;
    case REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED:
      legit_reference_increment();
      break;
  }
}

void SLOP::parent_lose_reference(bool decrement_subelements_on_free) const
{
  switch(this->layout()->header_get_memory_reference_management_arena())
  {
    default:
    case REFERENCE_MANAGEMENT_ARENA_KERFVM_WORKSTACK:
    case REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK:
      // TODO/P_O_P assert: unreachable, we should be able to get rid of this switch statement, right?
      // TODO Actually, this is nice to keep because if our stuff goes wonky (race conditions/interrupts) it can prevent weirdness?
      break;
    case REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT:
      // This I suppose is possible if we're SLAB4_ARRAY, not truly, but we can exploit it when iterating over their elements in reverse  
      // Remark. What's going to happen is the inline TRANSIENT RLINK3_UNITs get ignored, because the indexing SLOP skips them and goes right to the slab in question. So in this `case` right here, you really don't want to do anything.
      // Idea. Also, this sort of thing is nice to have if we point RLINK3s at a disk array that we're managing elsewhere (say we do a version of MEMORY_MAPPED in a way that one of the layout elements is a dummy pointer to an mmapped disk vector (it gets freed last), then after that is a PREFERRED_MIXED_ARRAY with links to the disk's long sub-slabs. If those are marked TRANSIENT, then this scheme works.)
      // this->layout_link_parents_lose_references(); // Question. RLINK3_ARRAY needs this for auto memory???
      break;
    case REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED:
      assert(this->layout()->header_get_slab_reference_count() >= 1);
      legit_reference_decrement(decrement_subelements_on_free);
      break;
  }
}

void SLOP::slop_gain_reference(SLAB* s)
{
  assert(s != nullptr);

  // (Regarding reassigning this->slabp as s)
  // Remark. Again, similar to slabp=nulltpr in _lose_reference, this isn't strictly necessary, but the alternative is managing it in all the callers
  // Remark. Also, it's safer to do this at the bottom, but we'd like this function to take place atomically, with this at the top
  this->slabp = s;

  // Warning. Technically, the direct arena access is not right, and you'd need to round trip layout() in case layout type specifies arena lives elsewhere.
  switch(slabp->reference_management_arena)
  {
    case REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK:
    #if CPP_WORKSTACK_ENABLED
      if (-1 == index_of_myself_in_cpp_workstack)
      {
        register_in_cpp_workstack();
      }
    #endif
      this->legit_reference_increment();
      break;
    case REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT:
    case REFERENCE_MANAGEMENT_ARENA_KERFVM_WORKSTACK:
    case REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED:
    default:
      break;
   }
}

void SLOP::slop_lose_reference(bool decrement_subelements_on_free) noexcept
{
  switch(this->layout()->header_get_memory_reference_management_arena())
  {
    default:
    case REFERENCE_MANAGEMENT_ARENA_KERFVM_WORKSTACK:
      break;
    case REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED:
      break;
    case REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT:
    {
      if(auto_slab() == this->slabp)
      {
        presented()->dealloc_pre_subelt(); // I assume we need this and the matching below, but haven't tested thoroughly
        switch(layout()->layout_payload_chunk_type())
        {
          // Question. ...can we have [auto] slab as PARENT here, to heap allocated? I hope not
          // Answer. We do this and it's OK, for instance, passing RLINK3_UNIT to SLAB4_ARRAY for append. In this case, reference counting does *not* trigger.
          // TODO Question BUG. OK, well, what if we have a PACKED RLINK3_ARRAY in an auto_slab, say via SLOP constructor...shouldn't we free its pointers? We can kind of cheat that by forcing it to be a COUNTED VECTOR. Compare, SLAB4_ARRAY dodges the issue because 1 element is always too big. | We do trigger a memory leak on a transient packed RLINK3_ARRAY with size==1 as predicted. Another problem is, we could have that packed array appear inline in something else!
          // Question. Maybe we do want auto memory to free pointers on release???
          // Idea. Alternatively, transient memory does nothing (neither increment nor decrement) to references, BUT, when it changes arenas, embedded RLINK3_UNITs will activate and reference increment. This sounds like it works closer to what we want.
          // Remark. [dupe] What's going to happen is the inline TRANSIENT RLINK3_UNITs get ignored, because the indexing SLOP skips them and goes right to the slab in question. So in this case right here, you really don't want to do anything.
          // Remark. What a mess this is.
          // Idea. I think perhaps on this issue we should make a ruling that that's an invalid way to "persist" PARENTED arena objects. They need a chain that ends in a MANAGED (heap) arena (ie not UNMANAGED_TRANSIENT), either the kerf tree, or a cpp-workstack thing, whatever. It's OK if it's in a RLINK3_UNIT that's locally unmanaged, inline in a MANAGED SLABP4_ARRAY, for instance, because that has something managed at the top. We could also perhaps special-case auto_slab RLINK3s.
          // NB. nm i think i fixed it enough for now. In a way, we are creating a SLOP as the new legit way to "persist" PARENTED  arena objects. 
          // Remark. TODO Another thing to consider is how this changes our cpp workstack semantics. Now these things effectively need to appear in the cpp_workstack (SLOP's with slabp==auto_slab and RLINK3_{UNIT,ARRAY} presented type)
          // Question. OK but now what if these things are present inline in a SLAB4, won't they trigger a bad decrement?
          // Answer. No, because in that case (auto_slab() != s) because the indexing SLOP will point outside to the inline payload instead
          // Idea. We could also move such things onto the ~PRESENTED_BASE() destructor in the ~SLOP() destructor, where they may more properly belong. (eg, ~A_RLINK3_ARRAY() handles it when its destructor's called)
          // Rule. A SLOP can't point to another SLOP's auto_slab

          // NB. These are only necessary if we allow RLINK3 in m<=4, necessarily packed, and in transient memory
          case CHUNK_TYPE_RLINK3:
            // Warning. below test is strong evidence we should move these methods to class methods on LAYOUT
            if(this->slabp->t_slab_object_layout_type == LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM) break;
            if(decrement_subelements_on_free) this->layout_link_parents_lose_references();
            break;
          default:
            break;
        }
        presented()->dealloc_post_subelt();

        *auto_slab() = {0};
        // Remark. note that if we have an auto_slab() atom, especially a TAPE_HEAD_*, and we switch the slabp to something else (away from &auto_slab()), we should zero AT LEAST PART of the auto_slab (I'm guessing LAYOUT type and PRESENTATION type, safer to do all, maybe slower maybe not), if for no other reason than to prevent t̶h̶e̶ ̶d̶e̶s̶t̶r̶u̶c̶t̶o̶r̶ other refcounting pieces from mistakenly considering the auto_slab to be storing an external reference we need to memory manage. 
      }

      break;
    }
    case REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK:
      // Idea. POTENTIAL_OPTIMIZATION_POINT Alternatively, [for cpp-workstack items, when reference decrement would free them] transfer to kerfvm-workstack for freeing at vm-stack-frame-pop time instead of cpp-function-stack-time. This may or may not improve speed globally. This should be a compile-time flag/toggle.
      legit_reference_decrement(decrement_subelements_on_free);
      break;
  }

  // Remark. This isn't strictly necessary so long as we're handling management of ->slabp elsewhere, in the callers
  // Remark. We'd may instead want to set slabp=auto_slab() and null out auto_slab, however, that adds a lot of extra ops during swaps, AND it clobbers overloaded PARENT storage if we're reassigning values. You know, it actually is safer I think, to assign it to auto_slab but NOT clobber it. We must guarantee autoslab stays clean enough in the memory header, which we're doing. However, this IS an issue if pointing to overloaded parents in auto_slabs causes a weird free event
  this->slabp = nullptr;
  // this->slabp = auto_slab();
}

bool SLOP::is_tracking_parent_rlink_of_slab() const
{
  SLAB *d = (SLAB*)this->slab_data;
  if(this->slabp == d || d->t_slab_object_layout_type != LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM || d->presented_type != RLINK3_UNIT) return false;
  assert(this->slabp->reference_management_arena == REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED);
  return true;
}

bool SLOP::is_tracking_parent_slabm_of_slab() const
{
  SLAB *d = (SLAB*)this->slab_data;
  if(this->slabp == d || d->t_slab_object_layout_type != LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM || d->presented_type != SLABM_UNIT) return false;
  return true;
}

bool SLOP::is_tracking_memory_mapped() const
{
  SLAB *d = (SLAB*)this->slab_data;
  if(this->slabp == d || d->t_slab_object_layout_type != LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM || d->presented_type != MEMORY_MAPPED) return false;
  return true;
}

SLOP SLOP::literal_memory_mapped_from_tracked()
{
  if(!is_tracking_memory_mapped()) return NIL_UNIT;
  return self_or_literal_memory_mapped_if_tracked();
}

SLOP SLOP::self_or_literal_memory_mapped_if_tracked() const
{
  if(!is_tracking_memory_mapped()) return *this;

  // NB. the return value won't itself be parented, of course, because that info is lost
  SLAB* sp = this->auto_slab()->a;
  return SLOP(sp, 0, true, false, false);
}

bool SLOP::is_tracking_parent() const
{
  return is_tracking_parent_rlink_of_slab() || is_tracking_parent_slabm_of_slab();
}

SLAB** SLOP::tracking_parent_address() const
{
  if(is_tracking_parent_rlink_of_slab())
  {
    // based on:
    //   .t_slab_object_layout_type=LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM,
    //   .presented_type=RLINK3_UNIT,
    //   .v=spp // address_of_old_slab_pointer == spp;
    return (SLAB**)this->auto_slab()->v;
  }

  if(is_tracking_parent_slabm_of_slab())
  {
    // based on:
    // *p.auto_slab() = (SLAB){
    //     .t_slab_object_layout_type=LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM,
    //     .presented_type=SLABM_UNIT,
    //     .a=s
    SLAB *s = this->auto_slab()->a;

    // we believe these things are true, and we depend on them being true
    // downstream of this function in order to modify the pointer values indicated by the returning addresses
    assert(s->t_slab_object_layout_type == LAYOUT_TYPE_UNCOUNTED_ATOM);
    assert(s->reference_management_arena == REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT);
    assert(s->r_slab_reference_count == 1);

    return (SLAB**)&s->v;
  }

  return nullptr;
}

void SLOP::release_slab_and_replace_with(SLAB* s, bool decrement_subelements_on_free, bool needs_check_for_changed_layout_or_presented)
{
  // Acquire the new slab*, set it to slabp, yes, but also set the rc arena, set rc count, set memory attributes, push on any workstacks, ... The thing is, we need to respect the previous arena and re-set it

  // TODO does this work if we pass s == this->slabp ???  this can happen I think if we for instance expand in place in some parent calls
  // maybe simply if(s == old) return;

  // NB. there are probably some simple race conditions here for an untimely ctrl-c, we can ignore
  // TODO: the way to fix I guess is to temporarily zero our position in the workstack, then to restore it after this block. (the restore portion needs to go ahead of gained_ref because it will set if unset)
  // TODO zero it here [at this line]

  // TODO we replace an slab4 inline transient atom [slabp will NOT ==auto_slab()] with a rlink in certain cases?

  SLAB* old = this->slabp;
  REFERENCE_MANAGEMENT_ARENA_TYPE old_arena = old->reference_management_arena;

  bool vtables_change = (old->t_slab_object_layout_type != s->t_slab_object_layout_type) || (old->safe_presented_type() != s->safe_presented_type());

  bool slabm_tracking = false; 

  switch(s->reference_management_arena)
  {
    default:
    case REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT:
    case REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED:
    case REFERENCE_MANAGEMENT_ARENA_KERFVM_WORKSTACK:
      break;
    case REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK: 
    {
      switch(old_arena)
      {
        default:
        case REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK:
          break;
        case REFERENCE_MANAGEMENT_ARENA_KERFVM_WORKSTACK:
          // TODO: push on kerfvm-workstack?
          s->r_slab_reference_count = 1; // TODO will kerfvm-workstack handle this instead?
          s->reference_management_arena = old_arena;
          break;
        case REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT:
          slabm_tracking = is_tracking_parent_slabm_of_slab();
          if(!slabm_tracking) break;
          else [[fallthrough]];
        case REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED:
          s->r_slab_reference_count = 0; // unmanaged or cpp_workstack become owned by TREE_OR_PARENTED
          s->reference_management_arena = REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED;
          break;
      }
    }
  }

  bool rlink_tracking = is_tracking_parent_rlink_of_slab();
  bool tracking = rlink_tracking || slabm_tracking;

  // TODO/P_O_P this looks incorrect to me, in the sense that it looks like we can combine these and this code was rough to begin with. See the parent pointer rewrite thing below, this doesn't look clean/integrated
  // Observation: we can push this above into the switch, as part of our continued refactoring process. Similarly, the cases below for RLINK3_UNIT/SLABM_UNIT, I believe.
  if(tracking)
  {
    assert(s != old);
    assert(old != auto_slab() && s != auto_slab() && this->slabp != auto_slab() && auto_slab()->t_slab_object_layout_type == LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM);
    assert(s->reference_management_arena == REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED);

    //update overloaded slop auto_slab parent tracker 
    SLAB *replacement = s;
    switch(+auto_slab()->safe_presented_type())
    {
      case RLINK3_UNIT:
      {
        assert(old->reference_management_arena == REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED);

        parent_lose_reference(decrement_subelements_on_free);
        this->slabp = replacement; // slop_gain_reference(s);
        parent_gain_reference();

        SLAB** spp = (SLAB**)auto_slab()->a;
        *spp = replacement;
        break;
      }
      case SLABM_UNIT:
      {
        if(DEBUG)
        {
          // You can support this later but see remarks elsewhere on supporting non-16-byte-length entries and what you need to do.
          assert(sizeof(SLAB) == this->layout()->width_aligned_claimed_in_mixed_parent());
        }

        assert(old->reference_management_arena == REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT);

        this->slabp = replacement; // slop_gain_reference(s);
        parent_gain_reference();

        *old = SLAB::good_rlink_slab(replacement);

        this->SLOP_constructor_helper((SLAB**)&old->a);
        return; // we can return and skip reconstitute_vtables below

        break;
      }
    }
  }
  else
  {
    assert(s!=old);
    slop_lose_reference(decrement_subelements_on_free);
    // TODO restore goes here because gained_ref will set if unset [this may be broken now, the recommendation, but this passage still needs looked at]
    slop_gain_reference(s);
  }


  // // POTENTIAL_OPTIMIZATION_POINT always reconstituting is safe and should be fast, if it's better to eliminate this branch
  // if(needs_check_for_changed_layout_or_presented && vtables_change)
  // {
    reconstitute_vtables();
  // }
}

#pragma mark - 

I SLOP::findI(const SLOP& needle) const
{ 
  // POP some items can avoid O(n) hunt based on type incompat, eg, a STRING will never be inside a INT3 vector

  bool break_flag = false;
  I i = 0;

  auto g = [&](const SLOP& x)
  {
    if(x == needle)
    {
      break_flag = true;
      return;
    }
    i++;
  };

  iterator_uniplex_presented_subslop(g, &break_flag);

  return i;
}
 
#pragma mark -

void SLOP::cowed_check_and_set_sorted_attributes_for_append_based_on_last_two()
{
  // P_O_P: wherever this is being called, there may be an opportunity to leverage existing variables (eg, already SLOP-wrapped last items and such). Similarly, if you're doing say integer1 vector appends, there's likely an opportunity to compare them directly without any of the unboxing here.
  // P_O_P: for array `join` and so on, can wait until the end and do batch test. maybe early return is faster? who knows. testing at-the-time is cache friendlier (single pass).

  auto L = layout();
  bool  asc = L->header_get_known_sorted_asc();
  bool desc = L->header_get_known_sorted_desc();
  I n = countI();

  if(!asc && !desc) return; // nothing to check
  if(n < 2) return; // Lists of length 0 or length 1 are always sorted

  SLOP &r = *this;
  // POP who knows if this is efficient
  auto c = r[n-2] <=> r[n-1];

  // POP can/should these `ifs` be removed?
  if( asc && std::is_gt(c)) L->header_set_known_sorted_asc(false);
  if(desc && std::is_lt(c)) L->header_set_known_sorted_desc(false);
}

void SLOP::cowed_rewrite_layout_type(UC layout_type)
{
  slabp->t_slab_object_layout_type = layout_type;
  reconstitute_layout_wrapper(&layout_vtable, layout_type); // or reconstitute_vtables();
}

void SLOP::cowed_rewrite_presented_type(PRESENTED_TYPE presented_type)
{
  layout()->header_set_presented_type(presented_type, true);
}

void SLOP::coerce_STRIDE_ARRAY()
{
  // POP instead of iterating, you can reuse certain FLAT array (LIST) for the values() portion (reference increment)
  // POP if the values portion is regular, you get indices() portion without counting. POP technically, it's a AFFINE.

  SLOP s(STRIDE_ARRAY);
  auto g = [&](auto x)
  {
    s.append(x);
  };
  this->iterator_uniplex_layout_subslop(g);

  *this = s;
}

void SLOP::coerce_FLAT_COMPLETE_JUMP_LIST_WITH_SUBLISTS()
{
  // Observation. If you ever embark on streaming this data, you can precompute the width of the maximum JUMP integer by looking at the size of the flattened `this`, or perhaps summing the sizes of "flattened" children / just computing what they would be (this is getting pretty crazy to just stream). Then you multiply that times the number of elements (also save that in case you were iterating a LIST) to know how big the integer vector will be (you need all this to be able to stream the byte counter in the front). Then you sum the following list widths. Then you stream 16 bytes of header with the counter, then you re-iterate and stream the integer sizes, then you re-iterate and stream the flattened children themselves, the actual flat representations. Jeez. What a project.

  // Remark: The way we are currently doing it already handles this case (if you change it remember to handle this): incomplete jumplists due to LIST-only (not +JUMP) append [ie, we append to the layout[1] LIST without updating/appending the layout[0] intvector and set false the "complete" attribute] need to be accounted for in COERCE JUMP LIST, etc. See attribute_zero_if_jump_list_is_complete_and_no_untracked_terms

  SLOP j(INT0_ARRAY,   LAYOUT_TYPE_COUNTED_VECTOR);
  SLOP u(UNTYPED_LIST, LAYOUT_TYPE_COUNTED_LIST);
  SLOP v(UNTYPED_LIST, LAYOUT_TYPE_COUNTED_LIST);

  // Observation. Because we use all COUNTED instead of PACKED, the overhead is > 3*16. With alignment, it's more. Each LIST element is >= 16 bytes (while SLAB4 is the minimal layout type).

  // P_O_P we're have two linear passes here, maybe we don't need to write to `u` (and just extract hypothetical flat sizes. although you still have to flatten them for `v`) so you may be stuck here.
  // P_O_P at least, the second write, of `u` to `v` could be a straight memcpy or Idea. optimized flat-to-flat join
  // POP The way we append to `u`, a counted non-jump list, this makes recursive counted non-jump lists, but it might make more sense in some instances to do counted jump list children. potentially as a toggle or companion function (make the sublists jumplists, potentially heuristically)

  assert(0 == u.layout()->payload_bytes_current_used());

  auto g = [&](auto x)
  { 
    u.layout()->cow_append(x);
    I c = u.layout()->payload_bytes_current_used();
    assert(SLAB_ALIGNED(c));
    j.append(c);
  };
  this->iterator_uniplex_layout_subslop(g);

  I jumps_size = j.layout()->slab_bytes_current_used_aligned();

  v.layout()->cow_append(j);

  // auto h = [&](auto x)
  // { 
  //   v.cow_layout_append(x);
  // };
  // u.iterator_  uniplex_layout_subslop(h); // Alternatively. Iterate `this` again, potentially re-flattening children

  // Remark. By appending flat `u` instead of re-iterating through its children, we accomplish this append as a single expand and memcpy.
  v.layout()->cow_append(u);

  assert(j.layout()->known_flat());
  assert(u.layout()->known_flat());
  assert(v.layout()->known_flat());
  // NB. If `v` isn't an "unpacked" COUNTED_LIST at this point, the LAYOUT_TYPE will be wrong for conversion to COUNTED_JUMP_LIST
  assert(j.slabp->t_slab_object_layout_type != LAYOUT_TYPE_COUNTED_VECTOR_PACKED);
  assert(j.slabp->t_slab_object_layout_type == LAYOUT_TYPE_COUNTED_VECTOR);
  assert(u.slabp->t_slab_object_layout_type != LAYOUT_TYPE_COUNTED_LIST_PACKED);
  assert(u.slabp->t_slab_object_layout_type == LAYOUT_TYPE_COUNTED_LIST);
  assert(v.slabp->t_slab_object_layout_type != LAYOUT_TYPE_COUNTED_LIST_PACKED);
  assert(v.slabp->t_slab_object_layout_type == LAYOUT_TYPE_COUNTED_LIST);

  v.cowed_rewrite_layout_type(LAYOUT_TYPE_COUNTED_JUMP_LIST);
  v.slabp->attribute_zero_if_jump_list_is_complete_and_no_untracked_terms = 0;
  v.slabp->attribute_zero_if_known_that_top_level_items_all_same_stride_width = 1; // Regime. Use the second nested item (here, `u`) to maintain such attributes, then pass through
  v.slabp->attribute_zero_if_known_children_at_every_depth_are_RLINK3_free = 0;
  // v.layout()->header_add_to_byte_counter_unchecked(-jumps_size);

  // See LAYOUT_BASE::cow_amend_one, see also coerce_UNFLAT
  if(this->presented()->is_grouped())
  {
    v.cowed_rewrite_presented_type(this->layout()->header_get_presented_type());
    v.slabp->presented_reserved = this->slabp->presented_reserved;
  }

  *this = v;

  return;
}

void SLOP::coerce_UNFLAT()
{
  // Remember. The negation of "known_flat" is not "known_unflat" but "unknown"

  bool offender = false;

  // Feature. Potentially this is a layout() method that says "should_be_unpacked_for_proper_ram_use" or something
  switch(layout()->header_get_slab_object_layout_type())
  {
    default:
      offender = false;
      break;
    case LAYOUT_TYPE_COUNTED_LIST:
    case LAYOUT_TYPE_COUNTED_LIST_PACKED:
    case LAYOUT_TYPE_COUNTED_JUMP_LIST:
      offender = true;
      break;
  }

  if(!offender) return;

  SLOP u(PREFERRED_MIXED_TYPE);
  auto g = [&](auto x)
  {
    x.coerce_UNFLAT();
    u.append(x);
  };
  this->iterator_uniplex_layout_subslop(g);

  // You have to handle PRESENTED_TYPE and also GROUPED as well: see coerce_FLAT, see LAYOUT_BASE::cow_amend_one, see SLOP::coerce_FLAT_COMPLETE_JUMP_LIST_WITH_SUBLISTS
  if(this->presented()->is_grouped()) 
  {
    u.cowed_rewrite_presented_type(this->layout()->header_get_presented_type());
    u.slabp->presented_reserved = this->slabp->presented_reserved;
  }

  *this = u;

}

void SLOP::coerce_FLAT()
{
  // NB. we probably need a companion function that will output STRIDE_ARRAYs instead of FLAT, for writing directories and directory-inline regular files.

  if(layout()->known_flat()) return;
  else coerce_FLAT_COMPLETE_JUMP_LIST_WITH_SUBLISTS(); // POP, see this func, you can maybe make the sublists jumplists
  return;

  // If the object is already known_flat(), you're ok (Note. that this may include sub-LAYOUT_TYPE_COUNTED_LISTs with bad performance) 
  // Otherwise, we want to convert to a JUMP_LIST, recursing on the children . (so the children are either known_flat() or we convert them to a JUMP_LIST, and so on).
  // (JUMP_LIST children are not guaranteed flat! ##WAit, coerce_JUMP_LIST claims they are, because list would flatten them recursively, so it may be simpler to do this than what I assumed - however, you may have "bad performance" non-JUMP_LIST LISTS mixed in there? wait, it seems you get the flat guarantee above, but do you always get that guarantee? it hinges on LAYOUT_TYPE_COUNTED_LIST having a flat guarantee, which it DOES NOT. To recap: in general JUMP_LIST does NOT guarantee recursive rlink-freeness, though you may get that guarantee from a utility function that produces JUMP_LISTS, such as coerce_FLAT_JUMP_LIST, which we just renamed. A jump_list is flat if known_flat() returns true, equivalent to 0== attribute_zero_if_known_children_at_every_depth_are_RLINK3_free. Otherwise, unknown flatness.)

  // Remark. For LAYOUT_TYPE_COUNTED_LISTs, if they are regular-stride-width they have good performance, they could have their stride broken, but this only happens on append(/amend), and JUMP_LIST isn't going to fix that, as it already breaks append.  POP. If they have irregular stride-width, and are long, then you may benefit from conversion to JUMP_LIST in some cases. though I would wonder how we arrived at that situation. 
  // Remark. If LAYOUT_TYPE_COUNTED_LISTs are short, irrespective of regular/irregular, they already have good performance (O(ε) ~ O(1)) without needing to be jumplists, and while multiple appends would break that, typically we're assuming these "flat" writes are read-only, though POP you could detect if you're doing a lot of writes (appends) and touch up the file. however, again, how did you get a LIST down if you didn't want that?
}

std::string SLOP::unquoted_string() const
{
  if(stringish())
  {
    // TODO this should be on PRESENTED
    // TODO we actually need unquoted (unticked), unescaped, and this needs to modify our "to_string" methods (note: A_FLOAT3_UNIT already has a signature with `last` that we likely need to incorporate)
    // once we have that, we probably want to blow away this ["unquoted_string"] function [or turn it into an alias], specify the to_string signature options inside the only place it's called (currently SLOP::operator>>)
    //  see /Users/kevin/Desktop/stuff/tech/source/kerf/src/puts.c
    //      K shout(K x, I depth, bool tick, bool escaped, bool full, bool indent)
    // Question. what's the point of printing "/" as "\/" for json purposes, if we're not going to maintain full compatibility and print maps using string quote names as specified in the json standard?
    switch(layout()->header_get_presented_type())
    {
      case STRING_UNIT:
      case CHAR0_ARRAY:
      {
        std::string b = "";
        const char *t = (const char *)layout()->payload_pointer_begin();
        DO(layout()->payload_bytes_current_used(), b+= (char)*t++)
        return b;
      }
      case CHAR3_UNIT:
      {
        std::string b = "";
        b.push_back((C)*this);
        return b;
      }
      default:
        break;
    }
  }

  return this->to_string();
}



} // namespace

