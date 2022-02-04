#pragma once

namespace KERF_NAMESPACE {

struct SLOP {

  // 1. Raw data needs struct interpretation so we have SLAB
  // 2. SLAB can't predict where vtable pointer goes so we need virtual class object wrapper (A_BASE)
  // 3. Class object can't 1. make virtualized calls by itself and can't 2. return genericized derived objects (typed references, slicing, varying sizeof, etc), so we need pointer to virtual class base (A_BASE*)
  // 4. Pointers can't be overloaded with operations so we need pointer wrapper class (SLOP, wraps A_BASE*)
  // C++ is truly ridiculous
  // 5. we can however bundle all of it into a single object (the A_BASE space and the pointer to A_BASE). this makes it less bad.
  // 6. SLOP cannot be subclassed if we put an [conventional] automatic SLAB member at the end (nice to have), because of the flexarray. [but a cheating data[] one is OK]
  // 7. A single embedded class isn't really sufficient for the "modularity" we need, so we split A_BASE into PRESENTED_BASE and LAYOUT_BASE. All of these must be single-inheritance or they expand the size of the vtable lol. Realistically we could do an ACCESSOR_BASE and perhaps a MEMORY_ARENA_BASE and so on instead of casing these.
  // Remark. You don't want to start by subclassing SLOP too hard. Subclassing it for the writer is ok, but if you do that kind of thing too often, you can't usefully subclass anything [any one particular object, because they're all split now] and retain all the functionality. This implies we should leave virtual methods/subclassing to the cheating vtables we store on the inside.

  // Observation. wrapping the virtualized object as an A_BASE inside of the SLOP allows us to "guarantee" where the vpointer exists (to inside a small fixed window)

  // Observation. The splitting into more than one virtual table, which is an admission that the C++ class hierarchy is not strong enough on its own to support the generality we need, is how we reconcile the need for swapping out certain elements of the vtable, the vtable being, contrary to our desire, embodied like a `const` type element. Each newly added vtable is supposed to represent a broad swath of `switch` statements that would otherwise appear in our code, which cannot be accounted for by some other vtable swath. The sensible separation and compartmentalization is I suspect wildly up to interpretation and editorial discretion. It may make sense for instance in some cases to retain a certain amount of `switch`ing in favor of not adding a separate vtable. At any rate, it additionally enforces a sensible object-oriented separation of the code.

  // Question. Should LAYOUT vtable only be visible to the BASE/PRESENTED vtable instead of both here in SLOP?

  // POTENTIAL_OPTIMIZATION_POINT: these vtable things don't need to round-trip the SLOP via ->parent() to get to each other, they can use each other directly using the same kind of pointer math from the parent() function. Eg, layout can call accessor directly

  // POTENTIAL_OPTIMIZATION_POINT: We don't need to store the 8==sizeof virtual pointers here, we can store smaller (1==sizeof?) indexes that return such objects. But then I guess they don't know where parent is and (nearly) all methods will need to be modified to take parent as an argument.

  // POTENTIAL_OPTIMIZATION_POINT: can likely move address_of_old_slab_pointer into slab_data

  // Remark. If we ever have to back down on/repair our use of the PRESENTED/LAYOUT layout_vtable/presented_vtable scheme using parent() (say because of undefined behavior calculating the pointer) you can just add a SLAB *parent to each of the base classes, it bloats SLOP a little and so we could do it if we had to.

  // /*  8 */ SLAB** address_of_old_slab_pointer = 0; // for "managed" links with writes (update pointer, reference manage pointees) - we've changed this to use the auto slab_data
  // /*  8 */ char accessor_mechanism_vtable[sizeof(ACCESSOR_MECHANISM_BASE)];

  /* -- */
  /*  8 */ SLAB *slabp = (SLAB*)&slab_data;
#if CPP_WORKSTACK_ENABLED
  /*  8 */ I index_of_myself_in_cpp_workstack = -1; // call the destructor after ctrl-c. compile-time option.
#endif
  /* -- */
  /*  8 */ char layout_vtable[sizeof(LAYOUT_BASE)];
  /*  8 */ char presented_vtable[sizeof(PRESENTED_BASE)];
  /* -- */
  /* 16 */ char slab_data[sizeof(SLAB)] = {0};
  /* -- */

  // POTENTIAL_OPTIMIZATION_POINT. I conjecture it's possible to combine the multiple vtable spaces here into one, and that even with some small combinatorial explosion in creating (combined) classes with vtables, you could maybe win in the end if you're creating lots and lots of SLOPs. But I dunno.

  // POTENTIAL_OPTIMIZATION_POINT. we can make the local auto slab data here bigger, to support bigger (>16byte, for strings and such) automatic slabs, just make sure we use sizeof(slab_data) everywhere, and then we can set the actual size as a compile time define to see how it affects performance
  // K header = &local;
  // POINTER_UNION data = {.V=&header->k};
  // SLAB local_slab = {0};// T̶O̶D̶O̶. Either 1. we keep this flexarray at the end and can never subclass/derive class this class
                           //              2. we replace the "flexarray" with a fake fixed data and figure out how to cast (try *(SLAB*)slab_data={ } ). we can do, see below

#pragma mark - Constructors

  void SLOP_constructor_helper(SLAB *sp, int depth = 0, bool reuse = false, bool rlink_descend = true, bool memory_mapped_descend = true)
  {
    if(depth > 1) die(SLOP Linking too deep);

    slop_gain_reference(sp);

    // Remark. In the copy-constructor we can catch and do the index-into-cpp workstack thing (for c++-workstack allocated objects only), but otherwise, you need to have the SLOP around and not just the SLAB* to inherit that, unless you want an O(n) search through the workstack.

    // Remark. Kind of a weird case, what if someone passes slop1=SLOP(slop2->slabp=slop2->auto_slab()), so that two SLOPs point to the same SLOP's automatic memory (slop2's)? slop2 shouldn't go out of scope before slop1, so it's OK on that basis. I mean...I guess it's ok (/ useful)? We could detect and force a slab-copy via *auto_slab() = *(SLAB*)slop2->slabp; and rewrite slop1's pointer to point to its own auto_slab (but then do we lose writing-in-place functionality down the road?).
    // Remark. We may want to subclass SLOP to handle reference-managed-or-not, but we can't subclass slop for aforementioned reasons (well we could I guess? with virtual? but then it's bigger by 8bytes, maybe slower). Can we pile on CHUNK_TYPE? (The concern here is modifying *(SLAB*) in place, I guess. This ptr-link situation is always going to be a tradeoff, like with TAPE_HEAD, between lots of dereferencing everywhere and pre-initialization w/ storage.)

    // POTENTIAL_OPTIMIZATION_POINT: if we disallowed links in mixed-lists (restrict to link vectors and have all atoms in list as separate SLABs instead of inline), SLOP initialization would be faster everywhere
    // POTENTIAL_OPTIMIZATION_POINT: because this is looking at layout (or presented), we can actually do this I think in the reconstitute_vtables thing (pass depth maybe, maybe rename it to s/t more general), and skip the if-branch hit most of the time, but catch the same during the `switch` for layout reconstitution. (You may wish to pass both the slabptr (passing already) and the ((SLOP&)this) (not yet passing) to the reconstitute_vtables methods in order to avoid a parent() call on the existing fast initializations, because this slow one is likely going to need parent(); also you're going to need to pass depth...yikes...) The check will be for RC_SLABPOINTER3_UNIT in PRESENTED (since we got rid of LAYOUT_TYPE_SLAB_REDIRECT_MANAGED to be simpler). so maybe we don't need to pass anything addl except depth. This check also needs to be "tail" because of the light reconstitute_vtables recursion.
    // if(sp->t_slab_object_layout_type == LAYOUT_TYPE_SLAB_REDIRECT_MANAGED)
    if(sp->t_slab_object_layout_type==LAYOUT_TYPE_UNCOUNTED_ATOM && sp->safe_presented_type() == RLINK3_UNIT && rlink_descend) 
    {
      assert(sp->r_slab_reference_count < 2); // If we're going to update parent, it needs to not be not referenced (or pre-cowed)

      // Remark. Seems like it's fine here to leverage a LIST's RLINK_UNIT as a regular RLINK [parent tracker] instead of as a LIST's SLABM. Motivation. This frees up `SLABM_UNIT` to deal with non-rlink units
      SLOP_constructor_helper((SLAB**)&sp->a, depth); // call "constructor" again
      // SLOP_constructor_helper(sp->a, depth + 1); // call "constructor" again deeper

      // // Idea. this makes me wonder if we shouldn't merge these parent trackers into returning a TAPE_HEAD iterator instead. This may or may not work.
      // *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH,
      //                 .t_slab_object_layout_type=LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM,
      //                 .presented_type=SLABM_UNIT,
      //                 .a=sp // address_of_old_slab_pointer == &(sp->a);
      //                 };
    }
    else 
    {
      if(reuse)
      {
        // NB. You can either always clear auto_slab here, or do it in the callers IF/WHEN you re-use a SLOP
        *auto_slab() = {0};
      }
      reconstitute_vtables();
    }

    if(sp->safe_presented_type() == MEMORY_MAPPED && memory_mapped_descend)
    {
      // POP this is probably not the optimal way to structure this if/switch setup

      // TODO POP only read/shared lock here, then upgrade to write/exclusive whenever you cow() `this` SLOP. NB. I already made `MEMORY_MAPPED_READONLY_SHARED_FLAG` enum to distinguish the presented_type for the lock - note that to save wasting one of those you could probably overload nearly anything in the autoslab instead: memory attributes or the TAPE_HEAD_UNCOUNTED_ATOM attributes.  Note that you prob. can't read-lock above the `get_good_slop` call because I think that sometimes needs a write lock, and in fact, you'll need to mirror the "full" read-locking version at A_MEMORY_MAPPED::rwlocker_wrapper_read and that will solve it. Alternatively. If for some reason this scheme doesn't work (it should work), then we had an alternate scheme which was to specify read/write at SLOP allocation time: write is default, but you can specify read for "p.o.p." optimizations everywhere, further `MAPPED_PRIVATE + ¬PROT_WRITE` would ALWAYS use the READ_ONLY SLOP optimization.
      // Warning. Once you add read/shared, so that you have both versions read/shared and write/exclusive, you need to be careful about attempting to mix the two within the same thread. If you already have the write lock, which you can detect, you should probably upgrade a "read-desiring" SLOP to use a write-lock instead. If you have the sutex read lock, which you can't detect unless you add a "redundant read thread id" attribute to MEMORY_MAPPED (mutatis mutandis in the style of REDUNDANT_WRITE_LOCK_COUNTER), normally, attempting to add a write lock will cause a deadlock from within the same thread. You could at least detect this and warn/abort. There are other ways around it, though I'm not sure if there are any we'd approve of, and it may be that we should simply deteect this as an error or even just silently deadlock, on the grounds that this is a cpp-land coding error.
      SUTEX::SUTEX_GUARD g = SUTEX::SUTEX_GUARD(&sp->sutex); // SUTEX::sutex_lock_exclusive(&sp->sutex);

      auto& m = *(A_MEMORY_MAPPED*)this->presented();
      I r = m.increment_redundant_write_lock_counter(); // For sutex write/exclusive locks, store a "redundant" counter on the MEMORY_MAPPED slab because thread-exclusive sutex locks don't have the notion of a count. Sutex read/shared locks already have a count notion so won't need this. A benefit of this is that we don't have to worry about single-thread SLOP-to-SLOP copies/moves or out-of-order freeing, say by `neutralize` or "cpp-userland automatic object lifetime out of order stack mismanagement". Feature. Er, technically this should go in the critical section below, but we can't move it past where we neutralize `this`, so maybe we need to wrap all of this in a critical section, but I haven't verified that's OK yet. Idea. Could store the pointer to the redundant count without incrementing it yet, then move that increment down below into the critical section
      auto h = m.get_good_slop();
      this->neutralize(); // don't free `sp`, the MEMORY_MAPPED slab pointer
      *this = h;
      assert(this->slabp != this->auto_slab()); // we need the space for below

      critical_section_wrapper([&]{
        g.should_unlock = false;
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH,
                        .t_slab_object_layout_type=LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM,
                        .presented_type=MEMORY_MAPPED,
                        .a=sp
                        };
      });
    }
  }

  void SLOP_constructor_helper(SLAB **spp, int depth = 0, bool reuse = false, bool memory_mapped_descend = true)
  {
    SLOP_constructor_helper(*spp, depth + 1, reuse, true, memory_mapped_descend);

    // TODO: here and in the (SLAB* sp) version, we need the containing vector of the slabpointer (or for (SLAB*), the parent mixed list with the SLAB containing the link) to be refcount 1 / to be cowed if our plan is to modify these things in place. How do we ensure this? We're not passing the parent so far.

    // TODO: when swapping in new ptrs, assert: reference count of parent is not >=2 (and not < 1) aka == 1

  if(!is_tracking_memory_mapped()) // don't overwrite our auto_slab that we already filled for MEMORY_MAPPED. POP not sure if this is the ideal way to situate all these checks
    *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH,
      .t_slab_object_layout_type=LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM,
      .presented_type=RLINK3_UNIT,
      .v=spp // address_of_old_slab_pointer == spp;
    };
  }

  void reconstitute_vtables() 
  {
    auto layout_type = slabp->t_slab_object_layout_type;
    // NB. It's probably more correct to use the wrapper but we hard assume layout type is at front, and the raw access can init (layout() has dependency on being init'ed). For similar reasons, it difficult to write a header_set_slab_object_layout_type method: you'd be changing the vtable of itself
    // auto layout_type = layout()->header_get_slab_object_layout_type(); // Warning: don't use, this is an example of what not to use
    reconstitute_layout_wrapper(&layout_vtable, layout_type); // Warning: this must precede the call to layout() below. This was caught by clang's UndefinedBehaviorSanitizer (UBSan)

    auto presented_type = layout()->header_get_presented_type();
    reconstitute_presented_wrapper(&presented_vtable, presented_type); // must be last
  }

  SLOP(SLAB* sp, int depth = 0, bool reuse = false, bool rlink_descend = true, bool memory_mapped_descend = true)
  {
    SLOP_constructor_helper(sp, depth, reuse, rlink_descend, memory_mapped_descend);
  }

  SLOP(SLAB** spp, int depth = 0, bool reuse = false)
  {
    SLOP_constructor_helper(spp, depth, reuse);
  }

  void SLOP_constructor_helper(SLAB slab4_unit, bool raw = true)
  {
    *auto_slab() = slab4_unit;

    // fix up the header. as slab header changes, this will change (dependency)
    auto_slab()->memory_header_wo_layout = SLAB::good_transient_memory_header();

    // Alternatively, cherry-pick version:
    // auto_slab()->first_four = SLAB::good_transient_first_four();
    // auto_slab()->t_slab_object_layout_type = slab4_unit.t_slab_object_layout_type;
    // auto_slab()->presented_type = slab4_unit.presented_type;
    // auto_slab()->slab_word2[1] = slab4_unit.slab_word2[1];
    // auto_slab()->slab_word3[1] = slab4_unit.slab_word3[1]; 

    // Remark. We're allowing TAPE_HEADs to get through here.

    // TODO: also, I think you have to make sure you don't take a link-pointer here somehow...? Unless you pop the pointer out and initialize SLOP(pointer) instead.

    // Question. Should we do this, and [potentially] populate as parented link, or accept the SLAB values raw and simply reconstitute_vtables() ?
    if(!raw)
    {
      SLOP_constructor_helper((SLAB*)auto_slab());
    }

    if(raw)
    {
      // P_O_P: if you're stealing a slab from another slop (say to clone an inline slab4), you can steal its vtables too
      reconstitute_vtables();
    }

  }

  // Remark. There's no reason (SLAB) constructor has to be the "literal" tool and (SLAB*) interprets RLINKs and so on. In fact, we should probably add toggles on both.
  SLOP(SLAB slab4_unit, bool raw = true)
  {
    SLOP_constructor_helper(slab4_unit, raw);
  }

  void SLOP_presented_type_helper(PRESENTED_TYPE p)
  {
    // Remark. We could turn this switch into a static "table" (C-like primitives), and also use it for the PRESENTED virtual table populator at the same time
    // Remark. Use layout()-cow_append over this->append() when constructing objects (don't want GROUPED resulting)
    switch(p)
    {
      default:
        std::cerr << "presented_type: " << (p) << "\n";
        die(Unregistered PRESENTED_TYPE in SLOP constructor)
      case NIL_UNIT:
        // unoptimized, using initialization list (iff NIL is all zeroes object): SLOP() : SLOP((SLAB*)&slab_data) { }
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_NIL_UNIT();
        break;
       case CHAR3_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_CHAR3_UNIT();
        break;
       case INT3_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_INT3_UNIT();
        break;
      case FLOAT3_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_FLOAT3_UNIT();
        break;
      case STAMP_DATETIME_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_STAMP_DATETIME_UNIT();
        break;
      case STAMP_YEAR_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_STAMP_YEAR_UNIT();
        break;
      case STAMP_MONTH_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_STAMP_MONTH_UNIT();
        break;
      case STAMP_DAY_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_STAMP_DAY_UNIT();
        break;
      case STAMP_HOUR_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_STAMP_HOUR_UNIT();
        break;
      case STAMP_MINUTE_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_STAMP_MINUTE_UNIT();
        break;
      case STAMP_SECONDS_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_STAMP_SECONDS_UNIT();
        break;
      case STAMP_MILLISECONDS_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_STAMP_MILLISECONDS_UNIT();
        break;
      case STAMP_MICROSECONDS_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_STAMP_MICROSECONDS_UNIT();
        break;
      case STAMP_NANOSECONDS_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_STAMP_NANOSECONDS_UNIT();
        break;
      case SPAN_YEAR_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_SPAN_YEAR_UNIT();
        break;
      case SPAN_MONTH_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_SPAN_MONTH_UNIT();
        break;
      case SPAN_DAY_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_SPAN_DAY_UNIT();
        break;
      case SPAN_HOUR_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_SPAN_HOUR_UNIT();
        break;
      case SPAN_MINUTE_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_SPAN_MINUTE_UNIT();
        break;
      case SPAN_SECONDS_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_SPAN_SECONDS_UNIT();
        break;
      case SPAN_MILLISECONDS_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_SPAN_MILLISECONDS_UNIT();
        break;
      case SPAN_MICROSECONDS_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_SPAN_MICROSECONDS_UNIT();
        break;
      case SPAN_NANOSECONDS_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM, .presented_type=p};
        new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
        new(presented_vtable) A_SPAN_NANOSECONDS_UNIT();
        break;
      case UNTYPED_ARRAY:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_VECTOR_PACKED, .presented_type=p,.smallcount_vector_byte_count=0, .vector_container_width_cap_type=CAPPED_WIDTH_BOUNDLESS, .attribute_known_sorted_asc=1, .attribute_known_sorted_desc=1, .i=0};
        new(layout_vtable) LAYOUT_COUNTED_VECTOR_PACKED();
        new(presented_vtable) A_UNTYPED_ARRAY();
        break;
      case CHAR0_ARRAY:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_VECTOR_PACKED, .presented_type=p,.smallcount_vector_byte_count=0, .vector_container_width_cap_type=CAPPED_WIDTH_0, .attribute_known_sorted_asc=1, .attribute_known_sorted_desc=1};
        new(layout_vtable) LAYOUT_COUNTED_VECTOR_PACKED();
        new(presented_vtable) A_CHAR0_ARRAY();
        break;
      case INT0_ARRAY:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_VECTOR_PACKED, .presented_type=p,.smallcount_vector_byte_count=0, .vector_container_width_cap_type=CAPPED_WIDTH_BOUNDLESS, .attribute_known_sorted_asc=1, .attribute_known_sorted_desc=1};
        new(layout_vtable) LAYOUT_COUNTED_VECTOR_PACKED();
        new(presented_vtable) A_INT0_ARRAY();
        break;
      case INT1_ARRAY:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_VECTOR_PACKED, .presented_type=p,.smallcount_vector_byte_count=0, .vector_container_width_cap_type=CAPPED_WIDTH_BOUNDLESS, .attribute_known_sorted_asc=1, .attribute_known_sorted_desc=1};
        new(layout_vtable) LAYOUT_COUNTED_VECTOR_PACKED();
        new(presented_vtable) A_INT1_ARRAY();
        break;
      case INT2_ARRAY:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_VECTOR_PACKED, .presented_type=p,.smallcount_vector_byte_count=0, .vector_container_width_cap_type=CAPPED_WIDTH_BOUNDLESS, .attribute_known_sorted_asc=1, .attribute_known_sorted_desc=1};
        new(layout_vtable) LAYOUT_COUNTED_VECTOR_PACKED();
        new(presented_vtable) A_INT2_ARRAY();
        break;
      case INT3_ARRAY:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_VECTOR_PACKED, .presented_type=p, .smallcount_vector_byte_count=0, .vector_container_width_cap_type=CAPPED_WIDTH_BOUNDLESS, .attribute_known_sorted_asc=1, .attribute_known_sorted_desc=1, .i=0};
        new(layout_vtable) LAYOUT_COUNTED_VECTOR_PACKED();
        new(presented_vtable) A_INT3_ARRAY();
        break;
      case FLOAT1_ARRAY:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_VECTOR_PACKED, .presented_type=p,.smallcount_vector_byte_count=0, .vector_container_width_cap_type=CAPPED_WIDTH_3, .attribute_known_sorted_asc=1, .attribute_known_sorted_desc=1};
        new(layout_vtable) LAYOUT_COUNTED_VECTOR_PACKED();
        new(presented_vtable) A_FLOAT1_ARRAY();
        break;
      case FLOAT2_ARRAY:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_VECTOR_PACKED, .presented_type=p,.smallcount_vector_byte_count=0, .vector_container_width_cap_type=CAPPED_WIDTH_3, .attribute_known_sorted_asc=1, .attribute_known_sorted_desc=1};
        new(layout_vtable) LAYOUT_COUNTED_VECTOR_PACKED();
        new(presented_vtable) A_FLOAT2_ARRAY();
        break;
      case FLOAT3_ARRAY:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_VECTOR_PACKED, .presented_type=p, .smallcount_vector_byte_count=0, .vector_container_width_cap_type=CAPPED_WIDTH_3, .attribute_known_sorted_asc=1, .attribute_known_sorted_desc=1};
        new(layout_vtable) LAYOUT_COUNTED_VECTOR_PACKED();
        new(presented_vtable) A_FLOAT3_ARRAY();
        break;
      case UNTYPED_RLINK3_ARRAY:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_VECTOR_PACKED, .presented_type=p,.smallcount_vector_byte_count=0,.vector_container_width_cap_type=CAPPED_WIDTH_3, .attribute_known_sorted_asc=1, .attribute_known_sorted_desc=1, .n=0};
        new(layout_vtable) LAYOUT_COUNTED_VECTOR_PACKED(); // Question. Don't make it PACKED because we don't want this in TRANSIENT/AUTO memory with pointers? Could put s/t in the destructor??
        new(presented_vtable) A_UNTYPED_RLINK3_ARRAY();
        break;
      case UNTYPED_SLAB4_ARRAY:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_LIST_PACKED, .presented_type=p,
                              .smallcount_list_byte_count=0,
                              .attribute_zero_if_known_that_top_level_items_all_same_stride_width=0,
                              .attribute_zero_if_known_children_at_every_depth_are_RLINK3_free=1,
                              .LIST_HOLDER_attribute_known_sorted_asc=1,
                              .LIST_HOLDER_attribute_known_sorted_desc=1,
                              .i=0};
        new(layout_vtable) LAYOUT_COUNTED_LIST_PACKED();
        new(presented_vtable) A_UNTYPED_SLAB4_ARRAY();
        break;
      case STRING_UNIT:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_VECTOR_PACKED, .presented_type=p,.smallcount_vector_byte_count=0, .vector_container_width_cap_type=CAPPED_WIDTH_0};
        new(layout_vtable) LAYOUT_COUNTED_VECTOR_PACKED();
        new(presented_vtable) A_STRING_UNIT();
        break;
      case UNTYPED_LIST:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_LIST_PACKED, .presented_type=p,
                              .smallcount_list_byte_count=0,
                              .attribute_zero_if_known_that_top_level_items_all_same_stride_width=0,
                              .attribute_zero_if_known_children_at_every_depth_are_RLINK3_free=0,
                              .LIST_HOLDER_attribute_known_sorted_asc=1,
                              .LIST_HOLDER_attribute_known_sorted_desc=1,
                              .i=0};
        new(layout_vtable) LAYOUT_COUNTED_LIST_PACKED();
        new(presented_vtable) A_UNTYPED_LIST();
        break;
      case UNTYPED_JUMP_LIST:
        *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_JUMP_LIST, .presented_type=p,  .attribute_known_sorted_asc=1, .attribute_known_sorted_desc=1, .n=0};
        new(layout_vtable) LAYOUT_COUNTED_JUMP_LIST();
        new(presented_vtable) A_UNTYPED_JUMP_LIST();
        break;
      case FOLIO_ARRAY:
        SLOP_presented_type_helper(PREFERRED_MIXED_TYPE);
        // NB. picking what LAYOUT type folio uses is not straightforward because it's basically TWO presented types that need to be passed as args, NOT 1pres 1layout.
        layout()->cow_append(INT0_ARRAY);           // sizes cache
        layout()->cow_append(PREFERRED_MIXED_TYPE); // child array array
        cowed_rewrite_presented_type(p);
        break;
      case MAP_UPG_UPG:
        SLOP_presented_type_helper(PREFERRED_MIXED_TYPE);
        DO(2, layout()->cow_append(UNTYPED_ARRAY))
        cowed_rewrite_presented_type(p);
        break;
      case MAP_NO_NO:
        // POP probably faster to build from scratch instead of leveraging MAP_UPG_UPG
        SLOP_presented_type_helper(MAP_UPG_UPG);
        cowed_rewrite_presented_type(p);
        break;
      case MAP_YES_UPG:
        // POP probably faster to build from scratch instead of upgrading
        SLOP_presented_type_helper(MAP_UPG_UPG);
        this->presented()->cow_set_attribute_map(MAP_UPG_UPG);
        break;
      case MAP_YES_YES:
        // POP probably faster to build from scratch instead of upgrading
        SLOP_presented_type_helper(MAP_YES_UPG);
        ((A_MAP_YES_UPG*)this->presented())->cow_upgrade_to_index_map();
        break;
      case SET_UNHASHED:
        SLOP_presented_type_helper(PREFERRED_MIXED_TYPE);
        layout()->cow_append(UNTYPED_ARRAY);
        cowed_rewrite_presented_type(p);
        break;
      case ENUM_INTERN:
        SLOP_presented_type_helper(PREFERRED_MIXED_TYPE);
        layout()->cow_append(SET_UNHASHED); // POP "UPG" versions of SET
        layout()->cow_append(INT0_ARRAY);
        cowed_rewrite_presented_type(p);
        break;
      case STRIDE_ARRAY:
        SLOP_presented_type_helper(PREFERRED_MIXED_TYPE);
        // POP I think you can set the layout type specifiers below (inherited from coerce_JUMP_LIST) to *_PACKED versions but I didn't verify this
        layout()->cow_append(SLOP(INT0_ARRAY, LAYOUT_TYPE_COUNTED_VECTOR));
        layout()->cow_append(SLOP(UNTYPED_LIST, LAYOUT_TYPE_COUNTED_LIST));
        cowed_rewrite_presented_type(p);
        break;
      case ERROR_UNIT:
        SLOP_error_type_helper(0, "Unspecified Error");
        break;
    }
  }

  SLOP(PRESENTED_TYPE p)
  {
    SLOP_presented_type_helper(p);
  }

  SLOP(PRESENTED_TYPE p, LAYOUT_TYPE q)
  {
    SLOP_presented_type_helper(p);

    // P_O_P second time we've reconstituted
    this->slabp->t_slab_object_layout_type = q;
    reconstitute_layout_wrapper(&this->layout_vtable, q); // or reconstitute_vtables();
  }

  SLOP(PRESENTED_TYPE p, I i) : SLOP(p)
  {
    slabp->i = i;
  }
  SLOP(PRESENTED_TYPE p, I2 i) : SLOP(p,(I)i) {}
  SLOP(PRESENTED_TYPE p, I1 i) : SLOP(p,(I)i) {}
  SLOP(PRESENTED_TYPE p, I0 i) : SLOP(p,(I)i) {}

  SLOP(PRESENTED_TYPE p, F f) : SLOP(p)
  {
    slabp->f = f;
  }
  SLOP(PRESENTED_TYPE p, F2 f) : SLOP(p,(F)f) {}
  SLOP(PRESENTED_TYPE p, F1 f) : SLOP(p,(F)f) {}

  SLOP(const I i)
  {
    *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM,.presented_type=INT3_UNIT,.i=i};
    new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
    new(presented_vtable) A_INT3_UNIT();
  }
  SLOP(const I2 i) : SLOP((I3)i) { }
  SLOP(const I2 i) __attribute__((enable_if(i != i, ""))) : SLOP((const I2)i) { }
  SLOP(const I1 i) : SLOP((I3)i) { }
  SLOP(const I0 i) : SLOP((I3)i) { }
  SLOP(const I4 i) : SLOP((I3)i) { }   // TODO: BIGINT if outside I3 range
  SLOP(const UI4 i) : SLOP((I3)i) { }  // TODO: BIGINT if outside I3 range


  SLOP(F f) // float atom constructor
  {
    *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM,.presented_type=FLOAT3_UNIT,.f=f};
    new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
    new(presented_vtable) A_FLOAT3_UNIT();
  }
  SLOP(const F2 f) : SLOP((F3)f) {}
  SLOP(const F1 f) : SLOP((F3)f) {}

  SLOP(const C c) // char atom constructor
  {
    *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_UNCOUNTED_ATOM,.presented_type=CHAR3_UNIT,.i=c};
    new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
    new(presented_vtable) A_CHAR3_UNIT();
  }

  void SLOP_string_helper(const char *string, I length)
  {
    // P_O_P we can use TAPE_HEAD_COUNTED_VECTOR and STRING_UNIT PRESENTED to do const C strings "zyx". ?? what ?? It means you can reference the c-string embedded in the binary from a TAPE_HEAD without copying it into a vector. this is sure to be a micro-op.

    *auto_slab() = (SLAB){.m_memory_expansion_size=LOG_SLAB_WIDTH, .t_slab_object_layout_type=LAYOUT_TYPE_COUNTED_VECTOR_PACKED,.presented_type=STRING_UNIT};
    new(layout_vtable) LAYOUT_COUNTED_VECTOR_PACKED();
    new(presented_vtable) A_STRING_UNIT();

    const char *t = string;
    DO(length, this->layout()->cow_append_scalar_byte_int_float_pointer((char)*t++))
  }

  SLOP(const char *string)
  {
    SLOP_string_helper(string, strlen(string));
  }

  SLOP(std::string string)
  {
    SLOP_string_helper(string.c_str(), string.length());
  }

  void SLOP_error_type_helper(I error_code, const char* string)
  {
    SLOP_presented_type_helper(PREFERRED_MIXED_TYPE);
    append(I(error_code));
    append((const char*) string);
    cowed_rewrite_presented_type(ERROR_UNIT);
  }

  SLOP(ERROR_TYPE e)
  {
    SLOP_error_type_helper((I)e, error_string(e));
  }

  SLOP(void *v)
  {
    // Question. I'm not sure what this should do, perhaps create a char0vec TAPE_HEADER of unknown length? Or just treat it as a slab*?
    die(SLOP(void*) nyi)
  }

  SLOP() : SLOP(UNTYPED_ARRAY)
  {
    // 2020.11.28 don't specify yet // : SLOP(UNTYPED_ARRAY) // empty list version - seems more useful than nil
    // SLOP_presented_type_helper(NIL_UNIT);
    // 2021.02.22
    //  SLOP x = {}; // standard 5.17.9 "The meaning of x={} is x=T()"
    //  ie SLOP x = SLOP();
    //  so specify as array for now [as if `{}` is a 0-element {1,2} or SLOP(1,2). note SLOP x = {1,2} yields the two-element array.] 
    //  we could revisit later if desired
  }


  // SLOP({1,2,3}), as of ~2014 http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_defects.html#1591
  template<typename T, size_t SIZE>
  SLOP(T (&a)[SIZE]) : SLOP(UNTYPED_ARRAY) {
    DO(SIZE, this->append(a[i]))
  }
  
  template<typename T, size_t SIZE>
  SLOP(T (&&a)[SIZE]) : SLOP(UNTYPED_ARRAY) {
    DO(SIZE, this->append(a[i]))
  }

  // template<size_t SIZE>
  // SLOP(auto (&a)[SIZE]) : SLOP(UNTYPED_ARRAY) {
  //   DO(SIZE, this->append(a[i]))
  // }
  // 
  // template<size_t SIZE>
  // SLOP(auto (&&a)[SIZE]) : SLOP(UNTYPED_ARRAY) {
  //   DO(SIZE, this->append(a[i]))
  // }

  void set_ooo_slab_flag_value()
  {
    *this->auto_slab() = {.presented_type = OUT_OF_ORDER_SLAB_FLAG};
    // POP only this should be necessary: this->auto_slab()->presented_type = OUT_OF_ORDER_SLAB_FLAG;
  }

  bool get_ooo_slab_flag_value()
  {
    return this->auto_slab()->presented_type == OUT_OF_ORDER_SLAB_FLAG;
  }

  void ooo_append(const SLOP& x)
  {
    append(x);

    SLOP& s = const_cast<SLOP&>(x);
    if(s.slabp != (SLAB*)&s.slab_data && s.layout()->header_get_memory_reference_management_arena() == REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED && s.get_ooo_slab_flag_value() )
    {
      s = NIL_UNIT;
      return;
      // equivalent to:
      s.slop_destructor_helper();
      s.neutralize();
      return;
    }
  }

  // // SLOP(1,3.4,'a',"bcd")
  SLOP(auto... arg) : SLOP(UNTYPED_ARRAY) {
    (ooo_append(arg), ...);

    if(slabp!=auto_slab() && this->layout()->header_get_memory_reference_management_arena() == REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK)
    {
      set_ooo_slab_flag_value();
    }
  }

#pragma mark - Move & Copy Constructors

  SLOP(SLOP const& x) noexcept
  {
    SLOP_copy_operator_helper(x, false, false); // cannot destruct unless you init say via `... : SLOP(NIL_UNIT)`
  }

  SLOP(SLOP const&& x) noexcept
  {
    SLOP_copy_operator_helper(x, false, true);
  }

#pragma mark - Move & Copy Assignment Operators

  SLOP& operator=(SLOP const& x) noexcept
  {
    SLOP_copy_operator_helper(x, true, false);
    return *this;
  }

  SLOP& operator=(SLOP && x) noexcept // Move Operator
  {
    assert(this != &x);

    // Question. if we COPY a SLOP, do we inherit tracking_parent to this from rhs? Answer. No
    // Question. if we MOVE a SLOP, do we inherit tracking_parent to this from rhs? Answer. No
    // NB. remember to manage parent when parent-tracking.

    SLOP_copy_operator_helper(x, true, true); // P_O_P some of the `copy` can likely be improved when you further assume move semantics
    // P_O_P: For instance, you can keep the same index_of_myself_in_cpp_workstack, provided you must rewrite the workstack entry to point to `this` instead of `x`, which is probably quicker than popping x then pushing this.

    // Question. does the [the process involving the] move operator call the destructor on the rhs...? if the answer is `no`, we may need add'l teardown here. Answer. Yes,it does call destructor, so our [reference incrementing/]cow semantics mean there's scant difference between a cpp `move` and a `copy` anyway. "Move functions should always leave both objects in a well-defined state."

    // x.slabp = x.auto_slab();
    // *x.slabp = {0};
    // x.index_of_myself_in_cpp_workstack = -1;
    // // x.SLOP_presented_type_helper(NIL_UNIT); // if this thing starts crashing b/c of layout/presented class destructors?

    return *this;
  }

  void SLOP_copy_operator_helper(SLOP const& x, const bool destruct, bool was_move = false) noexcept
  {
    // Warning. I don't think you can print objects here the default way because of recursion. `inspect` should be OK.

    if(this == &x)
    { 
      return; // check for self-assignment. would explode.
    }

    // Remark/P_O_P: the above is a good example of a certain kind of phenomenon. We should leave this as-is for now to reduce cognitive overhead elsewhere in the code, however, there's a good chance, especially if we put cycle-checking in at the top-level `amend` functionality, that such a position cannot be reached by any [non-user generated] Kerf-cpp code, or we could easily achieve that condition, and so removing the check [later] may speed up other aspects of the code, deeper-in copies and such, provided we confirm it genuinely isn't used. In this case, there may also be an impetus for keeping it, so that users of Kerf-cpp do not shoot off their foot with eg `a=b` when `&a==&b`. Also, of course, this may never be a speed bottleneck...

    SLAB* p = x.slabp;
    SLAB* d = (SLAB*)&x.slab_data;

    // Observation. There should be significant crossover here with `amend` and even `append` - refactorization potential
    if(is_tracking_parent_rlink_of_slab())
    {
      if(p != d)
      {
        assert(this->slabp != p);
        release_slab_and_replace_with(p, true, true);
      }
      else 
      {
        // P_O_P
        SLOP c = x;
        c.coerce_parented();
        *this = c;
      }

      return;
    }
    else if(is_tracking_parent_slabm_of_slab())
    {
      I claimed = this->layout()->width_aligned_claimed_in_mixed_parent();
      I incoming =    x.layout()->slab_bytes_current_used_aligned();

      assert(x.layout()->header_get_presented_type() != RLINK3_UNIT);
      assert(this->layout()->header_get_presented_type() != RLINK3_UNIT);

      // Warning. All the logic for this is in `append` and `release_slab_and_replace_with` and it's fairly elaborate
      if(claimed == incoming) // memcpy the rhs value inline
      {
        LAYOUT_BASE::write_to_slab_position(this->slabp, x, claimed);
        reconstitute_vtables();
      }
      else if (sizeof(SLAB) == claimed && !this->layout()->known_flat()) // write a RLINK
      {
        // Note same as rlink version above
        if(p != d)
        {
          assert(this->slabp != p);
          release_slab_and_replace_with(p, true, true);
        }
        else 
        {
          // P_O_P
          SLOP c = x;
          c.coerce_parented();
          *this = c;
        }
      }
      else
      {
        // Remark. In some cases, we could perhaps write a smaller thing, and then write a bigger `m` (log memory size) value, provided we ensured `claimed` was respecting the `m` value. 
        die(Disallowed case, copying slabm object inside a parent+target which cannot accept it);
      }

      return;
    }
    else if(this->is_tracking_memory_mapped())
    {
      // TODO ?
      // Should this copy the data to the drive? or not?
      // er(copy operator helper this.is_tracking_memory_mapped not yet implemented);
    }

    // TODO (moved from `unit_addition` where we would've overwritten an inline atom in a mixed list thinking it was an ok-to-write slop) here or in copy-constructor: if we copy a SLOP that's pointing to inline transient atom (can that happen?) we don't want to overwrite it here, so we either need to change that in the copy constructor, or have some flags, or something, or coerce to auto_slab at read time, or something like that. No time to fully diagnose. Same "bug" is present below in the other switch case for `r=a`. (Can we do two copy methods, one for const args and one for non?) This thing doesn't have a notion of its parent (its auto_slab should be unused; the only ones that have the notion are link-followers/pointer-followers), but it does have a pointer into transient memory in its parent, so it can write a RLINK3 there if it grows, or whatever. So what does it mean to constructor-copy that thing? Coerce? Cow? Clone? SLAB constructor SLOP(SLAB *b->slabp) <- kind of a dodge. But we can be thinking about clone semantics, and they may involve something similar

    // P_O_P: this probably [sometimes] triggers a redundant de-registering in the cpp-workstack then re-registering, easy enough to set flags
    // P_O_P: on that note, there may be a "swap" version of all this, seems unlikely to gain much
    // destruct ourselves so we can do "*this = other" cleanly elsewhere in the code
    if(destruct)
    {
      // NB. destructor handles index_of_myself_in_cpp_workstack = -1, which we should not inherit, and which we set below in slop_gain_reference
      slop_destructor_helper();
    }
    else
    {
    #if CPP_WORKSTACK_ENABLED
      assert(-1 == index_of_myself_in_cpp_workstack);
    #endif
    }

    if(x.is_tracking_memory_mapped())
    {
      // NB. As long as we're using "redundant" write/exclusive sutex lock counters on the MEMORY_MAPPED slab, we don't have to worry about transference of locks from SLOP-to-SLOP and so on: we can just let them all have a "separate" lock.
        // bool should_capture = was_move ? true : true;
        // NB. One of the issues here is the "transference" (or not) of the locks, and the ordering of such. (capture: put in `this` and "erase" in `x`, or vice verse)
        // Idea. Obsolete lock transference: We could also potentially resolve this by looking at the order of the SLOPs in the cpp_workstack (if we're fortunate enough to have it enabled)
        // Idea. Better: We can also put a counter-y thingy on the MEMORY_MAPPED slab to keep track of these, it's not a big deal (#write locks - this gives us our recursive property for writes that we're so worried about)

      SLAB* sp = x.auto_slab()->a;
      this->SLOP_constructor_helper(sp);
      return;
    }

    // TODO is there a case where we need to copy this regardless [ie when x's slab is not auto], eg do/should we inherit parent management?
    // TODO further question, how do we get modifiable slab data when x was pointing to an inline atom NOT in its auto_slab [putting it above here in this if statement], and maybe it even had parent data stored in its auto_slab. do we need a clone?

    if(p != d)
    {
      // the key thing in the copy constructor is that we [potentially] reference increment any cpp owned slabs
      slop_gain_reference(p);
    }
    else
    {
      assert(p==d);

      *auto_slab() = *d;
      // auto_slab()->memory_header_wo_layout = SLAB::good_transient_memory_header();
      slabp = auto_slab();

      // HACK, auto_slab packed arrays of pointer (RLINK3) need to increment those references
      if(x.layout()->layout_payload_chunk_type() == CHUNK_TYPE_RLINK3 && x.count() == 1)
      {
        assert(REFERENCE_MANAGEMENT_ARENA_TREE_OR_PARENTED == x[0].slabp->reference_management_arena);
        x[0].legit_reference_increment();
        #if CPP_WORKSTACK_ENABLED
          assert(-1 == index_of_myself_in_cpp_workstack);
          register_in_cpp_workstack();
        #endif
      }
    }

    // instead of memcpy
    //
    assert(8==sizeof(layout_vtable));
    assert(8==sizeof(presented_vtable));
    assert(offsetof(SLOP, presented_vtable) == offsetof(SLOP, layout_vtable) + 8);
    assert(16==sizeof(SLAB));
    //
    *(SLAB*) layout_vtable = *(SLAB*) x.layout_vtable;

    return;
  }

#pragma mark - Destructor

  void slop_destructor_helper() noexcept
  {

  #if CPP_WORKSTACK_ENABLED
    if (-1 != index_of_myself_in_cpp_workstack)
    {
      deregister_in_cpp_workstack();
    }
  #endif

    ///////////////////////////////////////////////////////////////////////////////
    // 1. Mercifully, does what we want: calls child destructor(s) then
    //    parent(s)
    // 2. When these virtual destructors are populated, even with just a
    //    logging function, having them ahead of the `slop_lose_reference` call
    //    (really, ahead of its sub call to
    //    `layout_link_parents_lose_references`) causes a crash, which I don't
    //    really understand why. they are placement-new, you're still not
    //    supposed to call them twice (which I don't think we do)
    //    The crash message is:
    //      Assertion failed: (!is_tracking_parent_rlink_of_slab() &&
    //      !is_tracking_parent_slabm_of_slab()), function coerce_to_copy_in_ram,
    //      file ./slop.cc
    //    which may mean we hosed a parent, or may just be general memory
    //    corruption or undefined behavior. At any rate typically the function
    //    we're going to want is presented()->dealloc_pre_subelt() (and
    //    further, it isn't clear whether the plain virtual destructor should
    //    be considered SLAB dealloc, or SLOP dealloc), which is only going to
    //    call itself on release of the slab, and NOT when the SLOP is
    //    destructed, although that's a bit contentious since [we have to worry
    //    about obj that want to call this, eg TRANSIENT, but won't get the
    //    chance]
    //    It may possibly be this issue: 
    //      Section 3.8 [basic.life]
    //      "A program may end the lifetime of any object by reusing the
    //      storage which the object occupies or by explicitly calling the
    //      destructor for an object of a class type with a non-trivial
    //      destructor."
    this->presented()->~PRESENTED_BASE();
    this->layout()->~LAYOUT_BASE();
    ///////////////////////////////////////////////////////////////////////////////

    bool decrement_subelements_on_free = true;
    slop_lose_reference(decrement_subelements_on_free);

    if(is_tracking_memory_mapped())
    {
      // We use this pattern and not `literal_memory_mapped_from_tracked` because we don't want to reference increment it
      SLOP s(NIL_UNIT);
      SLAB* sp = this->auto_slab()->a;
      s.slabp = sp; // assign the literal MEMORY_MAPPED slab pointer
      s.reconstitute_vtables(); // prepare it to maybe free itself
      auto& m = *(A_MEMORY_MAPPED*)s.presented();
      I r = m.decrement_redundant_write_lock_counter();
      if(0==r)SUTEX::sutex_unlock_exclusive(&sp->sutex);
    }

  }

  ~SLOP() noexcept
  {
    // POP It might be faster to: by default have the critical section disabled, but on ctrl-c, enable it and wait a millisecond or so before proceeding with the signal handling.
    if(SLOP_DESTRUCT_IS_CRITICAL_SECTION)
    {
      critical_section_wrapper([&]{
        slop_destructor_helper();
      });
    }
    else
    {
      slop_destructor_helper();
    }
  }

#pragma mark -

  SLAB* auto_slab() const;
  LAYOUT_BASE* layout() const;
  PRESENTED_BASE* presented() const;
  LAYOUT_BASE& layout_ref() const {return *layout();}

#pragma mark - Operators

  void* operator new(size_t j) = delete; // Of the things that can go wrong, I think, eg: you don't want a [normally] "stack"-duration SLOP to outlive [its/]a PARENT arena reference that might be freed. 2021.10.31 Another would be that we desire any SLOP-held locks on MEMORY_MAPPED to be of automatic duration only.
  void operator delete(void *p) = delete; // The emerging philosophy is: SLOPs should only have cpp-function-scope-level duration; store a SLAB if you want to "persist" storage beyond that scope, eg, globally or perpetually.

  friend std::ostream& operator<<(std::ostream& o, const SLOP& x){ return o << *(x.presented());}

  SLOP operator[](SLOP const& rhs) const
  {
    return index(rhs);
  }

  SLOP operator()() // this is fun. what's this for? eval()? Possibly, operator()(I/int i) for equivalent layout(index) to presented[index]
  {
    return SLOP();
  }

  SLOP operator()(...)
  {
    return SLOP();
  }

#pragma mark Truthtables

  bool truthy() const
  {
    return !this->falsy();
  }

  bool falsy() const
  {
    // POP turn into presented() class methods
    if(0 == *this) return true; // case should be covered by 0.0 case
    // if(0.0 == *this) return true; // case should be covered by 0 case
    if('\0' == *this) return true; // POP, merge with 0 or 0.0?
    if(presented()->is_nil()) return !TRUTHTABLE_VALUE_FOR_NIL;
    // TODO/Question temporal types?
    return false;
  }

  bool nullish() const
  {
    // POP turn into presented() class methods
    if('\0' == *this) return true; // POP, merge with 0 or 0.0?
    if(IN == I(*this)) return true; // case should be covered by F() case
    // if(isnan(F(*this))) return true; // case should be covered by I() case
    if(presented()->is_nil()) return true;
    // TODO/Question temporal types?
    return false;
  }

  bool stringish() const
  {
    // anything we can sensibly convert to an (unquoted) cstring/std::string
    // char_unit, charvec, string_unit
    // notably, kerf logical 1 has a useful to_string(): "1", but compare kerf logical "str" -> "\"str\"". excess quotes
    return presented()->is_stringish();
  }

  bool is_error()
  {
    return presented()->is_error();
  }

#pragma mark - Comparison and Equality Operators

  HASH_CPP_TYPE hash(HASH_METHOD_MEMBER method = HASH_METHOD_UNSPECIFIED, HASH_CPP_TYPE seed = The_Hash_Key) const;

  auto compare(const SLOP& x) const
  {
    return this->presented()->compare(x);
  }

  bool match(const SLOP& x) const
  {
    // TODO: exact floating point? hash thingies?
    return std::is_eq(compare(x));
  }

  friend auto operator<=>(const SLOP& a, const SLOP& b) {return a.compare(b); } 
  friend bool operator== (const SLOP& a, const SLOP& b) {return std::is_eq(a<=>b); } 

#pragma mark - Cast Operators

  // NB. explicit so elsewhere I upcasts to SLOP instead

  explicit operator const    I() const&  {return presented()->I_cast();}
  explicit operator const    C() const&  {return presented()->I_cast();} // for some reason, `return I();` does not work right, even casting to (C) first. cpp bug or weird standard?
  explicit operator const    F() const&  {return presented()->F_cast();}
  explicit operator const    F() const&& {return presented()->F_cast();}

  explicit operator const bool() const&  {return this->truthy();}
  explicit operator const bool() const&& {return this->truthy();}

  operator std::string() {return this->unquoted_string();}

#pragma mark -

  SLOP operator-() const&
  {
    return general_unit_atomic_uniplex_presented_combiner(unit_negate, *this);
  }

  friend SLOP operator+(const SLOP& a, const SLOP& b)
  {
    return general_unit_atomic_duplex_presented_combiner(unit_addition, a, b);
  }
  // friend SLOP operator+(const SLOP& a, const int&& b) { return operator+(a,b); }
  // friend SLOP operator+(const int&& a,  const SLOP& b) { return operator+(a,b); }

  friend SLOP operator-(const SLOP& a, const SLOP& b)
  {
    // P_O_P The way to go may be to turn the operators into a method on PRESENTED, have a generalized PRESENTED_BASE method, then in the subclasses do your specialized things but fall back to PRESENTED_BASE::method. Then we can capture int3_array x int3_array pleasurably without baking it into the general_unit_atomic_duplex_layout_combiner. In actuality, you'll probably need to do something like extending the g_u_a_d_l_c to accept an add'l argument representing the base-operator-method so it can recurse on itself, or whatever, maybe we can't get around writing custom recursive algos for each one, I dunno, you'd have to sit and think about it for a minute, whether you can do one that accepts (simpler) arguments [that don't handle their own recursion].

    // Done: combining the unit_ops is faster, we can/must do in here as a local function
    auto unit_minus = [](const SLOP &a, const SLOP &b) { return unit_addition(a,unit_negate(b)); };

    return general_unit_atomic_duplex_presented_combiner(unit_minus, a, b);
    return a+-b; // slower
  }

  friend SLOP operator*(const SLOP& a, const SLOP& b)
  {
    return general_unit_atomic_duplex_presented_combiner(unit_multiplication, a, b);
  }

  friend SLOP operator>>(const SLOP&a, const SLOP& b)
  {
    const bool compresses = true; // Feature.

    assert(b.stringish());
    FILE_OPERATIONS::write_to_drive_path(a, b.unquoted_string());
    return NIL_UNIT;
  }

  friend SLOP operator>>=(const SLOP&a, const SLOP& b)
  {
    assert(b.stringish());
    FILE_OPERATIONS::write_to_drive_path_directory_expanded(a, b.unquoted_string(), true, false);
    return NIL_UNIT;
  }

  friend SLOP& operator<<(SLOP& a, const SLOP& b)
  {
    const bool unflattens = true;
    const bool uncompresses = true; // Feature.

    assert(b.stringish());
    a = FILE_OPERATIONS::open_from_drive_path(b.unquoted_string());
    if(unflattens) a.coerce_UNFLAT();
    if(uncompresses) { }
    return a;
  }

  friend SLOP& operator<<=(SLOP& a, const SLOP& b)
  {
    assert(b.stringish());
    a = FILE_OPERATIONS::memory_mapped_from_drive_path(b.unquoted_string());
    return a;
  }

#pragma mark -

  static MEMORY_POOL& get_my_memory_pool(); 

  static SLOP AFFINE_RANGE(const SLOP &length = 0, const SLOP &base = 0, const SLOP &imult = 1)
  {
    SLOP b(PREFERRED_MIXED_TYPE);
    b.append((I)length);
    b.append(base);
    b.append(imult);
    b.cowed_rewrite_presented_type(AFFINE);
    return b;
  }

  static SLOP TAPE_HEAD_VECTOR(void* buf, I buflen, PRESENTED_TYPE p = CHAR0_ARRAY)
  {
    I extended_slab_size = sizeof(SLAB) + sizeof(void*);

    MEMORY_POOL &my_pool = SLOP::get_my_memory_pool();
    SLAB* a = (SLAB*)my_pool.pool_alloc_struct(extended_slab_size);
    // Remark. SLAB*a would leak on ctrl-c before reaching SLOP(a). This is OK.

    a->t_slab_object_layout_type=LAYOUT_TYPE_TAPE_HEAD_COUNTED_VECTOR,
    a->presented_type=p,
    a->n = buflen,
    a->va[0] = buf;

    return SLOP(a);
  }

#pragma mark -

#if CPP_WORKSTACK_ENABLED
  auto get_my_cpp_workstack();
  void register_in_cpp_workstack();
  void deregister_in_cpp_workstack() noexcept;
#endif

#pragma mark - Reference Management
  void cow(); // coerce cowed
  void coerce_to_copy_in_ram(bool reference_increment_children = true);
  SLAB* force_copy_slab_to_ram(bool reference_increment_children = true, bool preserve_tape_heads_literal = false);
  void coerce_parented(bool reference_increment_children = true);

  bool is_tracking_parent() const;
  bool is_tracking_parent_rlink_of_slab() const;
  bool is_tracking_parent_slabm_of_slab() const;
  SLAB** tracking_parent_address() const;
  bool is_tracking_memory_mapped() const;
  SLOP literal_memory_mapped_from_tracked();
  SLOP self_or_literal_memory_mapped_if_tracked() const;

  void legit_reference_increment(bool copy_on_overflow = true);
  void legit_reference_decrement(bool decrement_subelements_on_free) const noexcept;
  void layout_link_parents_gain_references();
  void layout_link_parents_lose_references() const;
  void parent_gain_reference();
  void parent_lose_reference(bool decrement_subelements_on_free = true) const;
  void slop_gain_reference(SLAB* s);
  void slop_lose_reference(bool decrement_subelements_on_free) noexcept;
  void release_slab_and_replace_with(SLAB* s, bool decrement_subelements_on_free = true, bool needs_check_for_changed_layout_or_presented = true);

  void neutralize(const bool destruct = false)
  {
    // We use this when we want to "free" a slop ahead of time without any side
    // effects, for instance, we don't want the destructor to be called on a
    // SLOP whose slabp points to known-freed or potentially-soon-to-be-freed
    // memory

    // Notably, this lets you reuse a SLOP without affecting any parent it might have had.
    // It also manages any references it may have been holding

    if(destruct) this->slop_destructor_helper();
    this->slabp = this->auto_slab();
    this->SLOP_presented_type_helper(NIL_UNIT);
  }

#pragma mark -

  SLOP index(const SLOP& rhs) const
  {
    return presented()->operator[](rhs);
  }

  SLOP& append(const SLOP& rhs)
  {
    this->presented()->cow_append(rhs);
    return *this;
  }

  SLOP& join(const SLOP &rhs)
  {
    // P_O_P special case everything Remember check for attr sorted
    this->presented()->cow_join(rhs);
    return *this;
  }

  SLOP copy_join(const SLOP &rhs)
  {
    // P_O_P special case everything
    SLOP q = *this;
    q.presented()->cow_join(rhs);
    return q;
  }

  SLOP& amend_one(const SLOP &x, const SLOP& y)
  {
    this->presented()->cow_amend_one(x, y);
    return *this;
  }

  SLOP& delete_one(const SLOP &x)
  {
    this->presented()->cow_delete_one(x);
    return *this;
  }

  SLOP& enlist()
  {
    SLOP a(UNTYPED_ARRAY);
    a.append(*this);
    *this = a;
    return *this;
  }

  SLOP last() const { return this->presented()->last(); }

  I countI() const { return this->presented()->count(); }
  SLOP count() const { return this->countI(); }
  SLOP length() const {return count();}
  SLOP size() const {return count();}

  I findI(const SLOP& needle) const;
  SLOP find(const SLOP& needle) const { return this->findI(needle); }

#pragma mark -

  void cowed_check_and_set_sorted_attributes_for_append_based_on_last_two();
    
  void coerce_FLAT_COMPLETE_JUMP_LIST_WITH_SUBLISTS();
  void coerce_STRIDE_ARRAY();
  void coerce_FLAT();
  void coerce_UNFLAT();
  void cowed_rewrite_layout_type(UC layout_type);
  void cowed_rewrite_presented_type(PRESENTED_TYPE presented_type);

  std::string unquoted_string() const;

#pragma mark - Iterators

  auto keys() const {return this->presented()->keys();}
  auto values() const {return this->presented()->values();}
  auto begin() const;
  auto end() const;

  // TODO/Question can we overload `for(auto x: Y) {}` style for-loops, probably using the PRESENTED-iterator? but it would be nice to have like a `Y.layout()` version that worked with it too. Answer. See https://stackoverflow.com/a/31457319/ Remark. Continuing that, you could do `Y.layout()`, `Y.presented()`, and then even have `Y` (SLOP) be something different. Note also `Y.keys()`, `Y.values()`, etc. 

  template<typename L> void iterator_uniplex_layout_subslop(L &lambda, bool* early_break_flag = nullptr, bool reverse = false, bool memory_mapped_descend = true) const;

  template<typename L> SLOP iterator_uniplex_layout_subslop_accumulator(L &lambda) const;

  template<typename L> void iterator_uniplex_presented_subslop(L &&lambda, bool* early_break_flag = nullptr, bool reverse = false) const;

  template<typename L> SLOP iterator_uniplex_presented_subslop_accumulator(L &lambda) const;

  template<typename L> void iterator_duplex_layout_subslop(L &lambda, const SLOP& rhs, bool* early_break_flag = nullptr) const;

  template<typename L> SLOP iterator_duplex_layout_subslop_accumulator(L &lambda, const SLOP& rhs) const;

  template<typename L> void iterator_duplex_presented_subslop(L &lambda, const SLOP& rhs, bool* early_break_flag = nullptr) const;

  template<typename L> SLOP iterator_duplex_presented_subslop_accumulator(L &lambda, const SLOP& rhs) const;

#pragma mark - Display

  std::string to_string() const
  {
    return presented()->to_string();
  }

  bool is_unit() const { return (LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM == slabp->t_slab_object_layout_type ) ||  presented()->is_unit();} // sugar  [TAPE_HEAD thing: probably for our presented HACK, which we could get rid of?]
  bool is_fixed_width_unit() const { return (LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM == slabp->t_slab_object_layout_type ) ||  presented()->is_fixed_width_unit();}

  void show() const
  {
    std::cerr << "*this->show(): " << (*this) << "\n";
  }

  void inspect_slab(SLAB* s) const
  {
    if(!s) {
    std::cerr <<  "    -----------------------------------------------------------\n";
    std::cerr <<  "        was null pointer\n";
    std::cerr <<  "    -----------------------------------------------------------\n";
      return;
    }
    std::cerr <<  "    -----------------------------------------------------------\n";
    std::cerr <<  "        s: " << ((I)s) << " or " << ((V)s) << "\n";
    std::cerr <<  "        s->t_slab_object_layout_type:    " << ((I)s->t_slab_object_layout_type) << ", " << LAYOUT_TYPE_MEMBER_STRINGS[s->t_slab_object_layout_type] <<  "\n";
    std::cerr <<  "        s->reference_management_arena:   " << ((I)s->reference_management_arena) << ", " << REFERENCE_MANAGEMENT_ARENA_MEMBER_STRINGS[s->reference_management_arena] << "\n";
    std::cerr <<  "        s->m_memory_expansion_size:      " << ((I)s->m_memory_expansion_size) << "\n";
    std::cerr <<  "        s->r_slab_reference_count:       " << ((I)s->r_slab_reference_count) << "\n";
    std::cerr <<  "        s->presented_type:               " << ((I)s->presented_type) << "\n";
    std::cerr <<  "        s->smallcount_vector_byte_count: " << ((I)s->smallcount_vector_byte_count) << "\n";
    std::cerr <<  "        s->attribute_known_sorted_asc:   " << ((I)s->attribute_known_sorted_asc) << "\n";
    std::cerr <<  "        s->attribute_known_sorted_desc:  " << ((I)s->attribute_known_sorted_desc) << "\n";
    std::cerr <<  "        s->n: " << (s->n) << "\n";
    std::cerr <<  "    -----------------------------------------------------------\n";
  }

  void inspect_helper(const char* arg="unspecified") const
  {
    // 2020.12.12 seems broken on clang:
    // __builtin_dump_struct(slabp, &printf);
    // __builtin_dump_struct(auto_slab(), &printf);

    std::cerr <<  "\n";
    std::cerr <<  "    ===========================================================\n";
    std::cerr <<  "    -----------------------------------------------------------\n";
    std::cerr <<  "    SLOP:\n";
    std::cerr <<  "    -----------------------------------------------------------\n";
    std::cerr <<  "        SLABP:\n";
    inspect_slab(this->slabp);
    std::cerr <<  "        AUTO_SLAB:\n";
    if(slabp!=(SLAB*)this->slab_data) { inspect_slab((SLAB*)this->slab_data); }
    else {std::cerr <<  "        same\n"; }
    std::cerr <<  "    -----------------------------------------------------------\n";
    std::cerr <<  "    SLOP this: " << ((I)this) << " or " << (this) << "\n";
    std::cerr <<  "    Side: " << arg << "\n";
  #if CPP_WORKSTACK_ENABLED
    std::cerr <<  "    this->index_of_myself_in_cpp_workstack:   " << (this->index_of_myself_in_cpp_workstack) << "\n";
  #endif
    std::cerr <<  "    this->slabp == auto_slab():               " << (this->slabp == (SLAB*)this->slab_data) << "\n";
    std::cerr <<  "    this->is_tracking_parent_rlink_of_slab(): " << (this->is_tracking_parent_rlink_of_slab()) << "\n";
    std::cerr <<  "    this->is_tracking_parent_slabm_of_slab(): " << (this->is_tracking_parent_slabm_of_slab()) << "\n";
    std::cerr <<  "    presented()->presented_chunk_type():      " << (presented()->presented_chunk_type()) << ", " << CHUNK_TYPE_MEMBER_STRINGS[presented()->presented_chunk_type()] << "\n";
    std::cerr <<  "    layout()->layout_payload_chunk_type():    " << (layout()->layout_payload_chunk_type()) << ", " << CHUNK_TYPE_MEMBER_STRINGS[layout()->layout_payload_chunk_type()] <<  "\n";
    std::cerr <<  "    presented()->chunk_width_log_bytes():     " << ((I)presented()->chunk_width_log_bytes()) << "\n";
    std::cerr <<  "    presented()->chunk_width_bytes():         " << ((I)presented()->chunk_width_bytes()) << "\n";
    std::cerr <<  "    layout()->payload_used_chunk_count():     " << (layout()->payload_used_chunk_count()) << "\n";
    std::cerr <<  "    ===========================================================\n";
    std::cerr <<  "\n";

    // auto g = [&](auto x) {x.inspect();};
    // iterator_uniplex_layout_subslop(g);
  }

  void inspect() const&
  {
    inspect_helper("lvalue");
  }

  void inspect() const&&
  {
    inspect_helper("rvalue");
  }


protected:
private:
};

}
