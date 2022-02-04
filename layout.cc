#pragma once

namespace KERF_NAMESPACE {

//////////////////////////////////////////

SLOP* LAYOUT_BASE::parent()
{
  return (SLOP*)(((char*)this) - offsetof(SLOP, layout_vtable));
}

SLAB* LAYOUT_BASE::auto_slab()
{
  return (SLAB*)(((char*)this) - offsetof(SLOP, layout_vtable) + offsetof(SLOP, slab_data));
}

PRESENTED_BASE* LAYOUT_BASE::presented()
{
  return (PRESENTED_BASE*)(((char*)this) - offsetof(SLOP, layout_vtable) + offsetof(SLOP, presented_vtable));
  return parent()->presented();
}

std::string LAYOUT_BASE::to_string()
{
  std::string s = "";
  s += "LAYOUT[";
  I i = 0;
  for(auto& x : this)
  {
     if(i>0) s+=", ";
     s += x.to_string(); 
     i++;
  }
  s += "]";
  return s;
}

auto LAYOUT_BASE::layout()
{
  return this;
}

CHUNK_TYPE LAYOUT_BASE::layout_payload_chunk_type()
{
  // we'll overload this later for some layout types to be able to override presented()
  // The reason this happened like this is that, we basically decided to overload "presented_type" with an [implied/munged] "chunk_type" inside of it [to this point, chunk type was derived from presented type. but that breaks down when GROUPED can be either PREFERRED_MIXED_ARRAY *OR* COUNTED_LIST, since they can't have a fixed chunk_type. Similar explanation applies to chunk_WIDTH ].
  // That overload...sort of made some things simpler, at least then.
  // We still don't want to split chunk type out though [into its own separate bits in the SLAB header], so since it's determined uniquely by (layout_type, presented_type), we can do this and move forward [this: create this virtualizable method on LAYOUT and refactor the existing methods to use this. what we're doing here will break down later if chunk_type somehow becomes a more complicated derivation of the two, or looks elsewhere besides the two. in the first case double-dispatch methods work. in the second, ... perhaps a method on SLAB:: Idea. where we get cross chatter between LAYOUT and PRESENTED, maybe it should've been a SLAB method, or SLOP method.]
  return presented()->presented_chunk_type();
}

SLOP LAYOUT_BASE::operator[](I i)
{
  return slop_from_unitized_layout_chunk_index(i);
}

//////////////////////////////////////////

SLAB* LAYOUT_BASE::slab_pointer_begin()
{
  return parent()->slabp;
}

V LAYOUT_BASE::memory_slab_pointer_total_end()
{
  return ((char*)slab_pointer_begin()) + memory_slab_total_capacity();
}

V LAYOUT_BASE::slab_pointer_current_end()
{
  return ((char*)slab_pointer_begin()) + slab_bytes_current_used();
}

V LAYOUT_BASE::slab_pointer_current_end_aligned()
{
  return ((char*)slab_pointer_begin()) + (slab_bytes_current_used_aligned());
}

I LAYOUT_BASE::memory_slab_total_capacity()
{
  return POW2(header_get_memory_expansion_log_width());
}

I LAYOUT_BASE::slab_bytes_current_used()
{
  // in general, but will override for TAPE_HEAD
  I header = header_bytes_total_capacity();
  assert(0 == (header % SLAB_ALIGN));
  I payload = payload_bytes_current_used();
  I sum = header + payload;
  return sum;
}

I LAYOUT_BASE::slab_bytes_current_used_aligned()
{
  return round_up_nearest_multiple_slab_align(slab_bytes_current_used());
}

I LAYOUT_BASE::memory_slab_bytes_current_unused()
{
  return memory_slab_total_capacity() - slab_bytes_current_used();
}

void LAYOUT_BASE::memory_zero_unused_slab_space()
{
  bzero(slab_pointer_current_end(), memory_slab_bytes_current_unused());
}

//////////////////////////////////////////

SLAB* LAYOUT_BASE::header_pointer_begin()
{
  return slab_pointer_begin();
  return parent()->slabp;
}

I LAYOUT_BASE::header_bytes_total_capacity()
{
  return offsetof(SLAB, i);
}

V LAYOUT_BASE::header_pointer_total_end()
{
  return ((char*)header_pointer_begin()) + header_bytes_total_capacity();
}

bool LAYOUT_BASE::header_adjacent_in_memory_to_payload()
{
  return true;
}

I LAYOUT_BASE::header_get_byte_counter_value()
{
  return header_pointer_begin()->n;
}

I LAYOUT_BASE::header_set_byte_counter_to_value_unchecked(I c)
{
  return (header_pointer_begin()->n = c);
}

I LAYOUT_BASE::header_add_to_byte_counter_unchecked(I c)
{
  return header_set_byte_counter_to_value_unchecked(c + header_get_byte_counter_value());
}

I LAYOUT_BASE::header_add_n_chunk_width_to_byte_counter_unchecked(I n)
{
  return header_add_to_byte_counter_unchecked(n * presented()->chunk_width_bytes());
}

I LAYOUT_BASE::header_add_chunk_width_to_byte_counter_unchecked()
{
  return header_add_n_chunk_width_to_byte_counter_unchecked(1);
}

void LAYOUT_BASE::header_set_presented_type(PRESENTED_TYPE p, bool refresh)
{
  header_pointer_begin()->presented_type = p;
  if(refresh) reconstitute_presented_wrapper(&parent()->presented_vtable, p);
}

//////////////////////////////////////////

V LAYOUT_BASE::payload_pointer_begin()
{
  if (header_adjacent_in_memory_to_payload()) header_pointer_total_end();
  return header_pointer_total_end();
}

I LAYOUT_BASE::payload_bytes_total_capacity()
{
  if (!header_adjacent_in_memory_to_payload()) return presented()->chunk_width_bytes();
  return memory_slab_total_capacity() - header_bytes_total_capacity();
}

V LAYOUT_BASE::payload_pointer_total_end()
{
  return ((char*)payload_pointer_begin()) + payload_bytes_total_capacity();
}

I LAYOUT_BASE::payload_bytes_current_used()
{
  return header_bytes_total_capacity();
}

V LAYOUT_BASE::payload_pointer_current_end()
{
  return ((char*)payload_pointer_begin()) + payload_bytes_current_used();
}

bool LAYOUT_BASE::payload_current_end_is_unused_space()
{
  return 0 < (payload_bytes_total_capacity() - payload_bytes_current_used());
}

I LAYOUT_BASE::payload_used_chunk_count()
{ 
  return payload_bytes_current_used() >> presented()->chunk_width_log_bytes();
  // return presented()->layout_payload_used_chunk_count();
}

I LAYOUT_BASE::payload_countable_appendable_bytes_remaining()
{
  return payload_bytes_total_capacity() - payload_bytes_current_used();
  // In practice, INT64_MAX is always enough
  return MIN(INT64_MAX - header_get_byte_counter_value(), payload_bytes_total_capacity() - payload_bytes_current_used());
}

V LAYOUT_BASE::payload_pointer_to_ith_chunk(I i)
{
  return ((char*)payload_pointer_begin()) + (presented()->chunk_width_bytes() * i); 
}

//////////////////////////////////////////

I LAYOUT_UNCOUNTED_ATOM::slab_bytes_current_used()
{
  return header_bytes_total_capacity() + payload_bytes_current_used();
  return (16);
  return (sizeof(SLAB));
}

I LAYOUT_COUNTED_VECTOR::slab_bytes_current_used()
{
  return header_bytes_total_capacity() + payload_bytes_current_used();
}

I LAYOUT_COUNTED_VECTOR_FILLED::slab_bytes_current_used()
{
  return memory_slab_total_capacity();
  return header_bytes_total_capacity() + payload_bytes_current_used();
}

I LAYOUT_COUNTED_VECTOR_SMALLCOUNT::slab_bytes_current_used()
{
  return header_bytes_total_capacity() + payload_bytes_current_used();
}

I LAYOUT_TAPE_HEAD_UNCOUNTED_ATOM::slab_bytes_current_used()
{
  assert(16==sizeof(SLAB));
  return (sizeof(SLAB));
}

I LAYOUT_TAPE_HEAD_COUNTED_VECTOR::slab_bytes_current_used()
{
  assert(24 == sizeof(SLAB) + sizeof(V));
  return sizeof(SLAB) + sizeof(V);
}

//////////////////////////////////////////

I LAYOUT_UNCOUNTED_ATOM::header_bytes_total_capacity()
{
  return offsetof(SLAB, i);
}

I LAYOUT_COUNTED_VECTOR::header_bytes_total_capacity()
{
  return offsetof(SLAB, g);
}

I LAYOUT_COUNTED_VECTOR_FILLED::header_bytes_total_capacity()
{
  return offsetof(SLAB, g);
}

I LAYOUT_COUNTED_VECTOR_SMALLCOUNT::header_bytes_total_capacity()
{
  return offsetof(SLAB, g);
}

I LAYOUT_COUNTED_VECTOR_PACKED::header_bytes_total_capacity()
{
  return offsetof(SLAB, i);
}

#pragma mark - JUMP Methods /////////////////////////////////////

I LAYOUT_COUNTED_JUMP_LIST::slab_bytes_current_used()
{
  // in general, but will override for TAPE_HEAD
  I header = LAYOUT_COUNTED_LIST::header_bytes_total_capacity();
  assert(SLAB_ALIGNED(header));
  I fake_payload = LAYOUT_COUNTED_LIST::payload_bytes_current_used();
  I sum = header + fake_payload;
  return sum;
}

I LAYOUT_COUNTED_JUMP_LIST::payload_bytes_current_used()
{
  // Remark. This was actually fine when we were cheating and reported payload totals as containing the header jump integers. That however might come back to bite us later, so we haven't done it here. Calculating LAYOUT_COUNTED_JUMP_LIST::header_bytes_total_capacity() requires a SLOP initialization...which could actually be optimized away. We don't want to lose integer 0-3 promotion, but we can start with a COUNTED_VECTOR instead of a VECTOR_PACKED [inside SLOP::coerce_JUMP_LIST], and that gives us a fixed int3 byte counter we can directly pull instead of going through the SLOP rigmarole. Penalty Θ(8 bytes), but only in small instances, aka, not relevant. OK, I'm going to fix this then. OK, so now that's done. Overall, we took a penalty of O(24 bytes) to expand intvec (when small) and to add a header to the second subobject (always).

  return header_get_byte_counter_value() - header_bytes_total_capacity() + LAYOUT_COUNTED_LIST::header_bytes_total_capacity();
}

I LAYOUT_COUNTED_JUMP_LIST::second_header_size()
{
  return 16;
}

SLAB* LAYOUT_COUNTED_JUMP_LIST::second_list_start()
{
  auto h = LAYOUT_COUNTED_JUMP_LIST::header_bytes_total_capacity();
  auto s = (SLAB*) (((char *)slab_pointer_begin()) + h - second_header_size());
  return s;
}

bool LAYOUT_COUNTED_JUMP_LIST::known_flat()
{
  return SLOP(second_list_start()).layout()->known_flat();
}

bool LAYOUT_COUNTED_JUMP_LIST::payload_known_regular_width()
{
  return SLOP(second_list_start()).layout()->payload_known_regular_width();
}

void LAYOUT_COUNTED_JUMP_LIST::payload_set_known_regular_width_attribute(bool v)
{
  return SLOP(second_list_start()).layout()->payload_set_known_regular_width_attribute(v);
}

bool LAYOUT_COUNTED_JUMP_LIST::header_get_known_sorted_asc()
{
  return SLOP(second_list_start()).layout()->header_get_known_sorted_asc();
}

bool LAYOUT_COUNTED_JUMP_LIST::header_get_known_sorted_desc()
{
  return SLOP(second_list_start()).layout()->header_get_known_sorted_desc();
}

void LAYOUT_COUNTED_JUMP_LIST::header_set_known_sorted_asc(bool v)
{
  return SLOP(second_list_start()).layout()->header_set_known_sorted_asc(v);
}

void LAYOUT_COUNTED_JUMP_LIST::header_set_known_sorted_desc(bool v)
{
  return SLOP(second_list_start()).layout()->header_set_known_sorted_desc(v);
}

I LAYOUT_COUNTED_JUMP_LIST::header_bytes_total_capacity()
{
  I super_header_size = LAYOUT_COUNTED_LIST::header_bytes_total_capacity();
  assert(super_header_size == offsetof(SLAB, g));
  assert(SLAB_ALIGNED(super_header_size));

  SLAB *jump_start = (SLAB*) (((char *)slab_pointer_begin()) + super_header_size);
  SLOP jumps(jump_start);
  I jumps_size = jumps.layout()->slab_bytes_current_used_aligned();
  assert(SLAB_ALIGNED(jumps_size));

  // P_O_P since we forced this to be unpacked, you can derive the bytecount without traversing SLOP above
  assert(jumps.layout()->header_get_slab_object_layout_type() == LAYOUT_TYPE_COUNTED_VECTOR);

  I second_list_header = second_header_size();

  if(DEBUG)
  {
    SLAB *second_list_start = (SLAB*) (((char *)slab_pointer_begin()) + super_header_size + jumps_size);
    SLOP data(second_list_start);
    assert(data.layout()->header_get_slab_object_layout_type() == LAYOUT_TYPE_COUNTED_LIST);
    assert(second_list_header == data.layout()->header_bytes_total_capacity());
    assert(SLAB_ALIGNED(second_list_header));
  }

  I sum = super_header_size + jumps_size + second_list_header;
  return sum;
}
#pragma mark - JUMP Methods /////////////////////////////////////

I LAYOUT_TAPE_HEAD_UNCOUNTED_ATOM::header_bytes_total_capacity()
{
  return offsetof(SLAB, g);
}

I LAYOUT_TAPE_HEAD_COUNTED_VECTOR::header_bytes_total_capacity()
{
  // Remark. TAPE_HEAD_COUNTED_VECTOR needs minimum size 24 bytes, not 16
  return offsetof(SLAB, g) + sizeof(V);
}

//////////////////////////////////////////
I LAYOUT_UNCOUNTED_ATOM::header_get_byte_counter_value()
{
  return 0; //no-op
}


I LAYOUT_UNCOUNTED_ATOM::header_set_byte_counter_to_value_unchecked(I c)
{
  return 0; //no-op
}

I LAYOUT_COUNTED_VECTOR_SMALLCOUNT::header_get_byte_counter_value()
{
  return header_pointer_begin()->smallcount_vector_byte_count;
}

I LAYOUT_COUNTED_VECTOR_SMALLCOUNT::header_set_byte_counter_to_value_unchecked(I c)
{
  return (header_pointer_begin()->smallcount_vector_byte_count = c);
}

I LAYOUT_TAPE_HEAD_UNCOUNTED_ATOM::header_get_byte_counter_value()
{
  return header_pointer_begin()->n;
  return 0; //no-op, instead of increasing pointer value
}

I LAYOUT_TAPE_HEAD_UNCOUNTED_ATOM::header_set_byte_counter_to_value_unchecked(I c)
{
  return (header_pointer_begin()->n = c);
  return 0; //no-op, instead of increasing pointer value
}

I LAYOUT_TAPE_HEAD_COUNTED_VECTOR::header_get_byte_counter_value()
{
  die(hi)
  return *(I*)(((char*)header_pointer_total_end()) - sizeof(V) - sizeof(I));
  return *(I*)header_pointer_begin()->g;
  return 0; //no-op, instead of increasing pointer value
}

I LAYOUT_TAPE_HEAD_COUNTED_VECTOR::header_set_byte_counter_to_value_unchecked(I c)
{
  return (header_pointer_begin()->n = c);
  return 0; //no-op, instead of increasing pointer value
}

//////////////////////////////////////////

V LAYOUT_TAPE_HEAD_UNCOUNTED_ATOM::payload_pointer_begin()
{
  return header_pointer_begin()->v;
}

V LAYOUT_TAPE_HEAD_COUNTED_VECTOR::payload_pointer_begin()
{
  return *(V*)&(header_pointer_begin()->g);
}

//////////////////////////////////////////

I LAYOUT_UNCOUNTED_ATOM::payload_bytes_total_capacity()
{
  assert(8==header_bytes_total_capacity());
  return header_bytes_total_capacity(); // implies same size as header
  return presented()->chunk_width_bytes();
  return offsetof(SLAB, g) - offsetof(SLAB, i);
  return sizeof(SLAB::i);
}

bool LAYOUT_COUNTED_VECTOR_FILLED::payload_current_end_is_unused_space() // not needed, but we can make it explicit
{
  return false;
}

I LAYOUT_COUNTED_VECTOR_FILLED::payload_countable_appendable_bytes_remaining() // not needed, but we can make it explicit
{
  return 0;
}

I LAYOUT_COUNTED_VECTOR_SMALLCOUNT::payload_countable_appendable_bytes_remaining()
{
  assert(1==sizeof(SLAB::smallcount_vector_byte_count));
  // assert: count is unsigned. dunno how to do this! (without doing arithmetic tests)
  // Idea/Remark. You could sidestep some of these tests by ensuring that smallcount/PACKED only are used on slabs of ->m below a certain size (in this case <=8)
  return MIN(POW2(LAYOUT_PACKED_CROSSOVER_LOG_WIDTH) - header_get_byte_counter_value(), LAYOUT_BASE::payload_countable_appendable_bytes_remaining());
  return MIN(UCHAR_MAX - header_get_byte_counter_value(), LAYOUT_BASE::payload_countable_appendable_bytes_remaining());
  // This assumes that we go partway into m==9, but we've decided that's a bad idea
  return MIN(UCHAR_MAX, LAYOUT_BASE::payload_countable_appendable_bytes_remaining());
}

//////////////////////////////////////////

I LAYOUT_UNCOUNTED_ATOM::payload_bytes_current_used()
{
  return payload_bytes_total_capacity();
}

I LAYOUT_COUNTED_VECTOR::payload_bytes_current_used()
{
  return header_get_byte_counter_value();
}

I LAYOUT_COUNTED_VECTOR_FILLED::payload_bytes_current_used()
{
  return payload_bytes_total_capacity();
}

I LAYOUT_COUNTED_VECTOR_SMALLCOUNT::payload_bytes_current_used()
{
  return header_get_byte_counter_value();
}

I LAYOUT_TAPE_HEAD_UNCOUNTED_ATOM::payload_bytes_current_used()
{
  return presented()->chunk_width_bytes();
}

I LAYOUT_TAPE_HEAD_COUNTED_VECTOR::payload_bytes_current_used()
{
  return header_pointer_begin()->i;
}

//////////////////////////////////////////

CHUNK_TYPE LAYOUT_COUNTED_LIST::layout_payload_chunk_type()
{
  return CHUNK_TYPE_VARSLAB;
}

CHUNK_TYPE LAYOUT_COUNTED_LIST_PACKED::layout_payload_chunk_type()
{
  return CHUNK_TYPE_VARSLAB;
}

I LAYOUT_BASE::observed_width_aligned_or_minus_one()
{
  ITERATOR_LAYOUT t(*parent());

  if(0 == t.known_item_count) return -1;

  return t.observed_width_aligned;
}

I LAYOUT_BASE::payload_count_zero_one_or_two_plus_chunks()
{
  I c = payload_used_chunk_count();

  switch(c)
  {
    case 0: return 0;
    case 1: return 1;
    default: return 2;
  }
}

I LAYOUT_BASE::COUNTED_LIST_payload_count_zero_one_or_two_plus_chunks_helper()
{
  ITERATOR_LAYOUT t(*parent());

  assert(!(t.known_item_count == 0 && t.pos != -1));

  C c = !!(t.pos == 0);
  C d = !!(t.has_next());
  C sum = 0 + c + d;

  return sum;
}

I LAYOUT_COUNTED_LIST::payload_count_zero_one_or_two_plus_chunks()
{
  return COUNTED_LIST_payload_count_zero_one_or_two_plus_chunks_helper();
}

I LAYOUT_COUNTED_LIST_PACKED::payload_count_zero_one_or_two_plus_chunks()
{
  return COUNTED_LIST_payload_count_zero_one_or_two_plus_chunks_helper();
}

I LAYOUT_COUNTED_LIST::payload_used_chunk_count()
{
  ITERATOR_LAYOUT t(*parent());
  t.seek_last();
  I n = t.known_item_count;
  assert(n != -1);
  return n;
}

I LAYOUT_COUNTED_LIST_PACKED::payload_used_chunk_count()
{
  ITERATOR_LAYOUT t(*parent());
  t.seek_last();
  I n = t.known_item_count;
  assert(n != -1);
  return n;
}

V LAYOUT_BASE::COUNTED_LIST_payload_pointer_to_ith_chunk_helper(I i)
{
  ITERATOR_LAYOUT t(*parent());
  if(i>0) t.next(i);
  return t.child_ptr; 
}

V LAYOUT_COUNTED_LIST::payload_pointer_to_ith_chunk(I i)
{
  return COUNTED_LIST_payload_pointer_to_ith_chunk_helper(i);
}

V LAYOUT_COUNTED_LIST_PACKED::payload_pointer_to_ith_chunk(I i)
{
  return COUNTED_LIST_payload_pointer_to_ith_chunk_helper(i);
}

#pragma mark - Layout Indexing 

SLOP LAYOUT_BASE::slop_from_unitized_layout_chunk_index(I i)
{
  // NB. "unitized" - don't index into a STRING's bytes

  switch(this->layout_payload_chunk_type())
  {
    case CHUNK_TYPE_UNCHUNKED_RAW_BYTE_DATA:
      return SLOP((C)presented()->integer_access_raw(payload_pointer_to_ith_chunk(i)));
    case CHUNK_TYPE_VARINT:
      return SLOP((I)presented()->integer_access_wrapped(payload_pointer_to_ith_chunk(i)));
    case CHUNK_TYPE_VARFLOAT:
      return SLOP((F)presented()->float_access_raw(payload_pointer_to_ith_chunk(i)));
    case CHUNK_TYPE_VARSLAB:
    { 
      // P_O_P I think we can move this into class methods (out from switch), since only LIST/JUMP_LIST can have VARSLAB chunks?

      SLAB *s = (SLAB*) payload_pointer_to_ith_chunk(i);
      SLOP p(s);
      // HACK. this should probably be refactored into the SLOP initialization method somehow?
      //       we're handling non-rlinked slab4 elements
      // NB. Also, wo/ tracking the RLINK3 children via SLABM_UNIT, you can't change your mind and deconvert a RLINK3_UNIT inside of a SLABM_UNIT into say an inline int3_unit atom that is NOT rlinked.
      if(!this->known_flat() && !p.is_tracking_parent_rlink_of_slab() && !p.is_tracking_memory_mapped())
      {
        *p.auto_slab() = (SLAB){
                          .m_memory_expansion_size=LOG_SLAB_WIDTH,
                          .t_slab_object_layout_type=LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM,
                          .presented_type=SLABM_UNIT,
                          .a=s
                         };
        assert(p.is_tracking_parent_slabm_of_slab());
      }

      return p;
    }
    case CHUNK_TYPE_RLINK3:
      return SLOP((SLAB**)payload_pointer_to_ith_chunk(i));
      return SLOP(*(SLAB**)payload_pointer_to_ith_chunk(i));
    default:
      return SLOP(NIL_UNIT);
      return SLOP(slab_pointer_begin());
  }
}

#pragma mark -


I LAYOUT_BASE::width_aligned_claimed_in_mixed_parent()
{
  constexpr I rlink_slab_width = sizeof(SLAB);
  assert(SLAB_ALIGNED(rlink_slab_width));

  // Warning. I think this will break should we make all LIST/SLAB4 children "parented tracking" and not merely the RLINKing children
  if(parent()->is_tracking_parent_rlink_of_slab()) return rlink_slab_width; 

  I used = slab_bytes_current_used_aligned();

  return used;
}

#pragma mark - Space in Memory

SLAB* LAYOUT_BASE::more_space_on_drive(C proposed_m)
{
  // TODO: DRIVE we can expand differently
  die(Did not implement drive expansion)
  return slab_pointer_begin();
}

SLAB* LAYOUT_BASE::more_space(C proposed_m)
{
  // in_place: try to expand in place via mmap, disk or in-memory
  // out_of_place: allocate a new run of memory and return that
  C current_m = this->header_get_memory_expansion_log_width();
  if(proposed_m < current_m) return slab_pointer_begin();

  // TODO
  if(/* this->header_get_slab_is_on_drive() */ 0 & 0)
  {
    // disk is always "in_place"
    return more_space_on_drive(proposed_m);
  }

  bool in_memory_tries_in_place = true;
  if(in_memory_tries_in_place)
  {
    // POTENTIAL_OPTIMIZATION_POINT
    // We could also try to mremap (linux) larger (for in-memory), and/or mach_vm_remap (macOS)
    // possibly we should only try this for mmap'd contiguous memory that the pool considered too large to break into chunks
    //if(it worked) return this->slabp; otherwise fall through

    // TODO: if this is a TENANT, we can't expand it in place. What do you do in that instance? [throw a kerf error] If you can put a LINK, you could leave that there instead. For certain kinds of TENANTS, you could also attempt to expand the parent. but that's advanced TODO. this also applies to disk TODO It may help to look at what's in the PARENTED part of the slop==parent(): if you are parented, and TRANSIENT, you prob shouldn't be expanding.
    // TODO auto memory won't expand in place either
    // TODO SHARED memory may, but you need to allocate that SHARED as well, or disallow
  }

  MEMORY_POOL &my_pool = parent()->get_my_memory_pool();
  SLAB* s = (SLAB*)my_pool.pool_alloc_struct(POW2(proposed_m));

#if DEBUG
  SLOP d; // don't initialize (would set reference counting and such)
  d.slabp = s;
  reconstitute_layout_wrapper(d.layout_vtable, d.slabp->t_slab_object_layout_type); // init LAYOUT only
  LAYOUT_BASE *e = d.layout();

  assert(proposed_m <= e->header_get_memory_expansion_log_width()); // NB. pool may decide differently from requested [larger]
  assert(0 == e->header_get_slab_reference_count());
  assert(REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK == e->header_get_memory_reference_management_arena());
  d.neutralize();
#endif

  // Remark. I think this is bad if we do it on newly-allocated mmap'd ANON memory [as is happens when the pool returns a large m slab] because it's already guaranteed to be zero. I wouldn't bet on the OS detecting the invariant write; other wasted motion could be slow as well.
  // NB. malloc's realloc new memory is undefined | but we're not using it because it's not in_place
  // Remark. Since this is in expand, it's always going to be new mmapped space, so this shouldn't be here at all. Wait, unless it's an older struct from the pool. We can put a thingy here for that when we know, with a toggle. POOL_MAX_LANE_RETAINED_IN_POOL.
  // Question. Anyway, why do we even need this in the first place?
  // if(zeroes_new) layout()->zero_unused_slab_space();

  return s;
}



#pragma mark - Append

void LAYOUT_BASE::promote_or_expand_via_widths(I settled_log_width, I incoming_chunks, bool known_invariant_promotional, I incoming_bytes_instead)
{
  // Idea. for in-reverse promote, we can hack it by winding the byte-counter down instead of up and by cow-appending using an updated SLOP from indices into the old SLOP, both referencing the same slab Idea. a cow_layout_append() is really just a cow_layout_amend_one_at_i() in the final [count-th] place. presumably layout_amend is simpler than a presented_amend. Idea. If promoting from say int3 to slab-atoms, you can still promote the ints into [the second half of] a width4 place, and then go back and fill all the atom headers.

  // Question. How wide of memory m will a packed vector fill? And do we need to store the count as chunk_count instead of byte_count?
  // Answer. Packed counts: w0,w1,w2,w3. w0: max count 255, max byte use 263. slightly over 8==m into 9==m, half empty. At that point 8 bytes is ~3% of size. So I think we don't need chunk-measured/pow2 packed counts, single byte counts is fine. Packed inside 16==sizeof(SLAB): Inside of a regular slab you can store: 8 sized int0_array, 4 sized int1_array, 2 sized int2_array, 1 sized int3_array. Similarly for FLOAT, 2 sized float2_array, 1 sized float3_array. 

  I existing_payload_bytes = this->payload_bytes_current_used();
  I existing_bytes_promoted = existing_payload_bytes;
  I eventual_payload_bytes = existing_bytes_promoted + incoming_bytes_instead;

  bool promoting_wider_chunks = false;

  I existing_chunks = -1;

  if(!known_invariant_promotional) // possibly promoting \ this is the branch for varint/varfloat versus rlink/varslab/data
  {
    C current_log_width = presented()->chunk_width_log_bytes();

    assert(current_log_width <= settled_log_width);
    existing_chunks = existing_payload_bytes >> current_log_width; // derives layout()->payload_used_chunk_count()
    assert(existing_chunks == this->payload_used_chunk_count());

    promoting_wider_chunks = (current_log_width < settled_log_width);

    existing_bytes_promoted = POW2(settled_log_width) * existing_chunks;
    I eventual_total_chunks = existing_chunks + incoming_chunks;
    eventual_payload_bytes  = POW2(settled_log_width) * eventual_total_chunks; 
  }

  UC old_layout = header_get_slab_object_layout_type(); // is it PACKED?

  bool header_width_changing = false;
  bool smallcount_byte_freed = false;
  UC revised_layout = old_layout;  // most should be normal COUNTED_VECTOR, except filled

  // For SMALLCOUNT/PACKED types, this may "overflow" the counter
  I eventual_total_bytes = eventual_payload_bytes + this->header_bytes_total_capacity(); // cf. PACKED vs. regular

  switch(old_layout)
  {
    case LAYOUT_TYPE_COUNTED_LIST:
    case LAYOUT_TYPE_COUNTED_VECTOR:
      break;
    case LAYOUT_TYPE_COUNTED_VECTOR_FILLED:
      // TODO preserve FILLED - calculate_layout_based_on_stats()
      // TODO can we skip this entirely by rejecting appends to FILLED? or do we need that for a hack...? we still need to be able to expand them
      die(Implement promote/expand for FILLED)
      break;
    // case LAYOUT_TYPE_COUNTED_VECTOR_SMALLCOUNT:
    //   if(eventual_payload_bytes > (I)UCHAR_MAX)
    //   {
    //     // TODO SMALLCOUNT should reject such appends as overflow the counter, here or elsewhere
    //   }
    //   break;
    case LAYOUT_TYPE_COUNTED_LIST_PACKED: [[fallthrough]];
    case LAYOUT_TYPE_COUNTED_VECTOR_PACKED:
      if(eventual_total_bytes > POW2(LAYOUT_PACKED_CROSSOVER_LOG_WIDTH)) 
      {
        I delta_header = 8;
        assert(delta_header == (LAYOUT_COUNTED_LIST().header_bytes_total_capacity() - LAYOUT_COUNTED_LIST_PACKED().header_bytes_total_capacity()));
        assert(delta_header == (LAYOUT_COUNTED_VECTOR().header_bytes_total_capacity() - LAYOUT_COUNTED_VECTOR_PACKED().header_bytes_total_capacity()));
        eventual_total_bytes += delta_header;
        switch(old_layout)
        {
          case LAYOUT_TYPE_COUNTED_LIST_PACKED:  revised_layout = LAYOUT_TYPE_COUNTED_LIST; break;
          case LAYOUT_TYPE_COUNTED_VECTOR_PACKED: revised_layout = LAYOUT_TYPE_COUNTED_VECTOR; break;
        }
        header_width_changing = true;
        smallcount_byte_freed = true;
      }
      break;
    default:
      die(Bad LAYOUT_TYPE passed to promote_expand. No TAPE_HEADS or _ATOMS);
      break;
  }

  I implied_log_capacity = ceiling_log_2(eventual_total_bytes);
  SLAB* dest = this->more_space(implied_log_capacity);
  // NB. dest may not equal this->slabp anymore. Only happens if we got lucky and did it in place

  ///////////////////////////////////////////////////
  // HACK: if is_in_place then we're using different vtables in `this` and dest_slop for the same slabp (which we wrote on before initing into dest_slop)
  dest->t_slab_object_layout_type = revised_layout;
  ///////////////////////////////////////////////////

  // second_four should happen before setting presented_type if presented_type is inside it
  assert(offsetof(SLAB,presented_type) == offsetof(SLAB, second_four) + sizeof(dest->second_four) - 1); 
  dest->second_four = this->header_pointer_begin()->second_four;
  // dest->vector_container_width_cap_type = this->slabp->vector_container_width_cap_type;

  // TODO? to genericize this...we need to do type promotion stuff or pass it or something (eg float2->float3, *->slab4, ...)
  // TODO we can probably make this branchless by creating/virtualizing a method that returns promotion types based on old_width, new_width, ...
  auto old_presented = this->header_get_presented_type(); // if in-place, `this`'s value will also change, as dest's does
  // Question. Are we guaranteed to be able to access presented_type like this if we later change it[s width or position] based on layout_type?
  dest->presented_type = old_presented;

  switch(this->layout_payload_chunk_type())
  {
    default:
    case CHUNK_TYPE_VARSLAB:
    case CHUNK_TYPE_RLINK3:
    case CHUNK_TYPE_UNCHUNKED_RAW_BYTE_DATA:
      break;
    case CHUNK_TYPE_VARINT:
      dest->presented_type = PRESENTED_BASE::presented_int_array_type_for_width(settled_log_width);
      break;
    case CHUNK_TYPE_VARFLOAT:
      dest->presented_type = PRESENTED_BASE::presented_float_array_type_for_width(settled_log_width);
      break;
  }

  bool is_in_place = (dest == this->slab_pointer_begin());

  if(!is_in_place) dest->zero_sutex_values(); // POP I don't think this is necessary if sutex is in the front by the memory
  SLOP dest_slop(dest);

  header_width_changing =  header_width_changing;
  bool empty_thing = !is_in_place;

  bool forced_reverse = is_in_place && (promoting_wider_chunks || header_width_changing);
  bool payload_writing_necessary = !is_in_place || promoting_wider_chunks || header_width_changing; 

  bool TRY_FORWARD_IF_ALLOWED = false;
  bool forward = TRY_FORWARD_IF_ALLOWED && !forced_reverse;
  bool reverse = !forward;

  if(payload_writing_necessary)
  {
    I cwb0 =     this->presented()->chunk_width_bytes();
    I cwb1 = dest_slop.presented()->chunk_width_bytes();
    char* lpb0 = (char*)    this->payload_pointer_begin();
    char* lpb1 = (char*)dest_slop.layout()->payload_pointer_begin();

    switch(this->layout_payload_chunk_type())
    {
      case CHUNK_TYPE_VARSLAB: // this thing won't promote so this is just memcpying
      case CHUNK_TYPE_RLINK3: // this thing could widen someday? into slab4?
      case CHUNK_TYPE_UNCHUNKED_RAW_BYTE_DATA:
      {
        // if this width isn't changing, this forward or back, has the same result as memcpy(lpb1, lpb0, n*cwb0)
        // aka, expand without promote
        assert(!is_in_place);
        assert(!promoting_wider_chunks);
        assert((cwb0) == (cwb1));
        // memcpy(lpb1, lpb0, n*cwb0);
        memcpy(lpb1, lpb0, existing_payload_bytes);
        // I x;
        // x = this->presented()->integer_access_raw(lpb0 + (k * cwb0));
        // dest_slop.presented()->integer_write_raw(lpb1 + (k * cwb1), x);
        break;
      }
      // Question. We can use the same strategy for a `demote`? (We get a quick-and-dirty `demote` with append()s)
      case CHUNK_TYPE_VARINT:
      {
        I n = existing_chunks;
        DO(n, I k = reverse ? n - i - 1 : i;
              I x;
              x = this->presented()->integer_access_wrapped(lpb0 + (k * cwb0));
              // x = ACCESSOR_MECHANISM_BASE::prepare_int_for_widening_read(x, current_log_width);
              // x = ACCESSOR_MECHANISM_BASE::prepare_int_for_narrowing_write(x, settled_log_width);
              dest_slop.presented()->integer_write_wrapped(lpb1 + (k * cwb1), x);
        )
        break;
      }
      case CHUNK_TYPE_VARFLOAT:
      {
        // NB. width may change from `this` to `dest`
        // so it's not memcpy unless !promoting_wider_chunks
        I n = existing_chunks;
        DO(n, I k = reverse ? n - i - 1 : i;
              F y;
              y = this->presented()->float_access_raw(lpb0 + (k * cwb0));
              dest_slop.presented()->float_write_raw(lpb1 + (k * cwb1), y);
        )
        break;
      }
      default:
        break;
    }
  }

  dest_slop.layout()->header_set_byte_counter_to_value_unchecked(existing_bytes_promoted); // only do after done writing, b/c of PACKED->normal upgrade (changing header size)

  if(smallcount_byte_freed)
  {
    // Optional - (as of writing we weren't using for anything)
    dest->smallcount_vector_byte_count = 0;
  }

  bool presented_changed = dest->presented_type != old_presented;
  bool layout_changed = revised_layout != old_layout;

  if(parent()->slabp != dest)
  {
    bool free_subelements = false;
    parent()->release_slab_and_replace_with(dest, free_subelements, true); // generally, don't free subelements, because this is merely a LAYOUT memcopy
  }
  else if(presented_changed || layout_changed)
  {
    // POP I think currently, a layout change in place on the same slab cannot occur here, because it only happens currently as a result of a size increase, and not a pending smallcount overflow, but this is crossover definition dependent.
    // POP the release_slab_and_replace* function above also calls reconstitute currently, so this could be an "else if" changed
    // POP You could split is so presented_changed only updates the presented vtable, and so on. Probably none of this matters.

    // Remark. I had to add this because there was a bug writing a 130 to a INT0_ARRAY where it wasn't "promoting" right to capture the correct byte width. I think it needs to go to the bottom (or alternatively, outside this promote method wherever it is called) because we do operations on the "old" this->presented() type above. We were relying on the `release_slab_and_replace...` method to update the vtables, when the slabs swap, but we neglected to identify the vtables could also need updating if the presented_type changes in-place (same slab).
    parent()->reconstitute_vtables();
  }

}

void LAYOUT_BASE::cow_append_scalar_byte_int_float_pointer(const SLOP &rhs)
{
    C current_width = presented()->chunk_width_log_bytes();
    C insisted_width = current_width;
    I incoming_chunks = 1;

    I x; 
    F y;

    switch(layout_payload_chunk_type())
    {
      default:
      case CHUNK_TYPE_VARSLAB:
        die(Unreachable);
        break;
      case CHUNK_TYPE_RLINK3:
      case CHUNK_TYPE_UNCHUNKED_RAW_BYTE_DATA:
        x = rhs.presented()->integer_access_raw(rhs.layout()->payload_pointer_begin());
        break;
      case CHUNK_TYPE_VARINT:
        x = rhs.presented()->integer_access_raw(rhs.layout()->payload_pointer_begin());
        insisted_width = ACCESSOR_MECHANISM_BASE::minimal_int_log_width_needed_to_store_magnitude(x);
        break;
      case CHUNK_TYPE_VARFLOAT:
        y = rhs.presented()->float_access_raw(rhs.layout()->payload_pointer_begin());
        insisted_width = ACCESSOR_MECHANISM_BASE::minimal_float_log_width_needed_to_store_magnitude(y);
        break;
    }

    C hard_cap = container_width_cap_type();
    assert(hard_cap >= current_width);

    C bargained_width = MIN(insisted_width, hard_cap);

    C settled_width = MAX(current_width, bargained_width);

    bool magnitude_exceeded_cap = insisted_width > hard_cap;

    bool promoted_enough = (current_width >= settled_width);

    // Question. should we get rid of these checks and handle them deeper in the promote/expand function? 
    // Answer. no, it's much slower. we could however move these *same* checks deeper? that might be an improvement
    bool has_the_bytes = this->payload_countable_appendable_bytes_remaining() >= POW2(settled_width);

    bool ok = has_the_bytes && promoted_enough;
    if(!ok)
    {
      this->promote_or_expand_via_widths(settled_width, incoming_chunks);
    }

    // TODO if we got this far and STILL didn't cow things, we need to split it and copy it before appending

    switch(layout_payload_chunk_type())
    {
      default:
      case CHUNK_TYPE_VARSLAB:
        die(Unreachable);
        break;
      case CHUNK_TYPE_RLINK3:
      case CHUNK_TYPE_UNCHUNKED_RAW_BYTE_DATA:
        presented()->integer_write_raw(this->payload_pointer_current_end(), x);
        break;
      case CHUNK_TYPE_VARINT:
        // x = ACCESSOR_MECHANISM_BASE::prepare_int_for_narrowing_write(x, settled_width, insisted_width);
        presented()->integer_write_wrapped(this->payload_pointer_current_end(), x, insisted_width);
        break;
      case CHUNK_TYPE_VARFLOAT:
        presented()->float_write_raw(this->payload_pointer_current_end(), y);
        break;
    }

    this->header_add_chunk_width_to_byte_counter_unchecked();

    parent()->cowed_check_and_set_sorted_attributes_for_append_based_on_last_two();

    return;
}

void LAYOUT_BASE::write_to_slab_position(SLAB* start, const SLOP& rhs, I width_writing_aligned)
{
  I header_size = rhs.layout()->header_bytes_total_capacity();
  I payload_size = rhs.layout()->payload_bytes_current_used();
  assert(SLAB_ALIGNED(header_size));
  I total_unaligned = header_size + payload_size;
  assert(width_writing_aligned == round_up_nearest_multiple_slab_align(total_unaligned));

  memcpy(start,                         rhs.layout()->header_pointer_begin(),  header_size);  // copy header
  memcpy(((char *)start) + header_size, rhs.layout()->payload_pointer_begin(), payload_size); // copy payload

  start->reference_management_arena = REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT;
  // start->m_memory_expansion_size = MEMORY_UNPERMISSIONED_TENANT_NOEXPAND;
  // NB. can be strictly less than LOG_SLAB_WIDTH, b/c we're allowing width 8
  // NB. because of floor, the used space will always expand past [or end precisely at] the indicated m_memory_expansion_size window. You can't do ceil here because that might indicate you could write past the used boundary, and in a LIST of any sort we're packing along used rounded up to alignment mulitple, not rounded up to powers of two. Now, for the final entry in the list, recursively, it's a different story. Basically, it can expand provided it updates every parent list recursively.
  // Remark. At this point, though, why not use MEMORY_UNPERMISSIONED_TENANT_NOEXPAND since you'll always require an expand or special check? I guess it doesn't matter. However, if we increase width_appending_aligned before this point in the code we do have expansion room.
  I wf = floor_log_2(width_writing_aligned);
  assert(wf >= 3);
  start->m_memory_expansion_size = wf;

  // pad up to width_appending_aligned - this is actually accomplished simply by increasing the byte counter. we can zero it if we like
  bool optional_zero_padding = false;
  if(optional_zero_padding)
  {
    I pad_width_to_align = width_writing_aligned - total_unaligned;
    bzero(((char*)start) + total_unaligned, pad_width_to_align);
  }

  return;
}

void LAYOUT_BASE::cow_append(const SLOP &rhs)
{
  // Rule. This function does NOT check for cycle-creation. Not its purview
  // Rule. This function MUST NOT turn the layout into a GROUPED, e.g., you can't append a STRING_UNIT to an UNTYPED_ARRAY and have it convert the [ungrouped single-buffer layout] UNTYPED_ARRAY to a GROUPED STRIDE_ARRAY or GROUPED STRING_CORD. Use PRESENTED'S version of `append` for that.

  // Remark. for a fast/lame object copy/clone, we should be able to integral iterate i and do LAYOUT appends, with appropriate reference increments (SHALLOW). (key here is PRESENTED never needs to be [logically] roundtripped for this, it's all dependent on LAYOUT.) you can also do DEEP by not reference incrementing and doing sub-copies, I suppose. | Have to be careful with FILLED vectors. Maybe we can do memcpy or toggle `FILLED` attr off before we start and back on at the end..

  // Remark. All this really makes us reconsider doing LAYOUT type of slab_vector, and perhaps even RLINKVECTOR, in order to compartmentalize all the layout types that can have reference counting (LAYOUT_TYPE_SLAB_REDIRECT_MANAGED included). 

  // TODO attribute_known_sorted_asc, attribute_known_sorted_desc. how did we do in kerf1? is this already done?

  // TODO certain appends, of items which maintain headers (incl. varslabs, rlinks, ...?) should inherit/derive from the memory attribute flags of parent, if that's what we decided to do (eg ON_DRIVE attr goes to children?), possibly TENANT arena.

  //slab_copy(no_ref_inc) <-- this WILL ref increment if r>1 here, as we clone. Can itself use layout_append
  //subelement reference counting is going to be needed for any SLAB/RLINK containing types, except on certain very simple copies
  //this func prob. goes in PRESENTED classes, not SLOP

  // P_O_P we actually want to postpone cows till further in (at promotion/expansion time), because it can save us a copy, in the case of multiply-reference items, if we have to copy for cow and then copy again for promotion/expansion, as that's two copies with the first redundant. Anyway this is an optimization not a requirement

  assert(parent() != &rhs); // could we handle this case? more importantly, should we? create a copy? (this does not necessarily imply cycle creation. but see above `NB`)

  parent()->cow();

  switch(TWO(header_get_presented_type(), rhs.layout()->header_get_presented_type()))
  {
    case TWO( CHAR0_ARRAY,   CHAR3_UNIT):
    case TWO(  INT0_ARRAY,    INT3_UNIT):
    case TWO(  INT1_ARRAY,    INT3_UNIT):
    case TWO(  INT2_ARRAY,    INT3_UNIT):
    case TWO(  INT3_ARRAY,    INT3_UNIT):
    case TWO(FLOAT1_ARRAY,  FLOAT3_UNIT):
    case TWO(FLOAT2_ARRAY,  FLOAT3_UNIT):
    case TWO(FLOAT3_ARRAY,  FLOAT3_UNIT):
    case TWO(STAMP_DATETIME_ARRAY,     STAMP_DATETIME_UNIT    ):
    case TWO(STAMP_YEAR_ARRAY,         STAMP_YEAR_UNIT        ):
    case TWO(STAMP_MONTH_ARRAY,        STAMP_MONTH_UNIT       ):
    case TWO(STAMP_DAY_ARRAY,          STAMP_DAY_UNIT         ):
    case TWO(STAMP_HOUR_ARRAY,         STAMP_HOUR_UNIT        ):
    case TWO(STAMP_MINUTE_ARRAY,       STAMP_MINUTE_UNIT      ):
    case TWO(STAMP_SECONDS_ARRAY,      STAMP_SECONDS_UNIT     ):
    case TWO(STAMP_MILLISECONDS_ARRAY, STAMP_MILLISECONDS_UNIT):
    case TWO(STAMP_MICROSECONDS_ARRAY, STAMP_MICROSECONDS_UNIT):
    case TWO(STAMP_NANOSECONDS_ARRAY,  STAMP_NANOSECONDS_UNIT ):
      cow_append_scalar_byte_int_float_pointer(rhs);
      return;
    case TWO(  INT0_ARRAY,  FLOAT3_UNIT):
    case TWO(  INT1_ARRAY,  FLOAT3_UNIT):
    case TWO(  INT2_ARRAY,  FLOAT3_UNIT):
    case TWO(  INT3_ARRAY,  FLOAT3_UNIT):
      // TODO another case we might like to catch, is the FLOAT3 is purely integral without a decimal (it has a strict conversion to an integer (because the exponent places the decimal after the mantissa) and we may opt to ignore its floating point type for purpose of promotion, though it might promote our INT width, IOW, we just retry the append from here with a temporary rhs of INT3(FLOAT3->F_cast()) ).
      // TODO note, we could easily handle Floating inf,-inf,nan when the integer type supports such
      // TODO what did we do in kerf1 that was special about mixed int/float appends? did we promote to float or something? adding a float to an intvec promotes it to floatvec (not mixed) (possibly, disk or caps will reject this. we may need a bit flag on slab), adding an int to a floatvec casts it to float with no change to vector.
      // TODO this raises an issue. the question of how promotes work is possibly more complicated, because integer caps [+ floating caps?] may want to reject promotion to mixed lists, promotion to floatvec, etc. eg any change to vector type (beyond width). TEMPORAL types will want to reject floatvec-promote.  Possible flags: CAN_PROMOTE_INT_TO_FLOAT_ARRAY (TEMPORAL won't have set, maybe DISK/tables won't have set, specifying a sub-unbounded with should probably OFF this by default unless specified otherwise.), CAN_PROMOTE_TO_LIST (note: consider UNBOUNDED/BOUNDLESS width / bigints. I would say are same.). Could also be shoehorned into cap width indicator [int0, int1, int2, float2, int3, float3, unbounded ] - this won't work because promoted int1 getting caught in float2 will prevent promotion to int3 as desired. So then there's another flag/enum to consider: APPEND_FLOATS_TO_CAPPED_INT_ASSIGN ;  APPEND_FLOATS_TO_CAPPED_INT_CLIP ; APPEND_FLOATS_TO_CAPPED_INT_REJECT
      // TODO float promotion has weird wrinkles, because int2->float2 loses integer precision.  you can do max int1->float2 and int2->float3 wo/ loss of precision. Lossy int3->float3 is probably OK.
      // TODO for us, we could also elect to mix int and float in mixed-slab...this is not ideal i bet. Possibly insisted on in some types of promotion combinations
      // TODO note that, some floats are .000 integral, so we could have a flag to keep those as INT appends...?
      // Remark. We can handle this [easily here], after whatever compiler options take effect, by creating a I3 i3 = munge_float(F3), then cow_append_scalar_byte_int_float_pointer(SLOP(i3)); and return

      break;
    case TWO(FLOAT1_ARRAY,    INT3_UNIT):
    case TWO(FLOAT2_ARRAY,    INT3_UNIT):
    case TWO(FLOAT3_ARRAY,    INT3_UNIT):
      // TODO also for floats: FLOAT_ARRAY_APPEND_INT_*.  What if INT is too big/small (float inf? clip? c-assign? error?)? What if INT fits (c-assign, error)? Compiler flags? Vector bit flag?
      // TODO note, we could easily handle Int inf,-inf,nan when the int type supports such
      // Remark. We can handle this [easily here], after whatever compiler options take effect, by creating a F3 f3 = munge_float(I3), then cow_append_scalar_byte_int_float_pointer(SLOP(f3)); and return
      break;
    case TWO(  INT0_ARRAY,  BIGINT_UNIT):
    case TWO(  INT1_ARRAY,  BIGINT_UNIT):
    case TWO(  INT2_ARRAY,  BIGINT_UNIT):
    case TWO(  INT3_ARRAY,  BIGINT_UNIT):
      // TODO
      break;
    case TWO(UNTYPED_ARRAY, 0) ... TWO(UNTYPED_ARRAY, UCHAR_MAX): 
      {
        // the appropriate [promotion] cap is to leave it unchanged when possible
        // the appropriate LAYOUT is left unchanged, but we need to debug error on certain invalid ones
        assert(header_get_slab_object_layout_type() != LAYOUT_TYPE_COUNTED_VECTOR_FILLED);
        
        // Remark. is this always ZERO length? maybe call it UNTYPED_EMPTY_LIST or something

        PRESENTED_TYPE revised_type;
        CAPPED_WIDTH_TYPE revised_cap = header_pointer_begin()->vector_container_width_cap_type;

        switch(rhs.layout()->header_get_presented_type())
        {
          case CHAR3_UNIT:
            revised_type = CHAR0_ARRAY;
            revised_cap = CAPPED_WIDTH_0;
            break;
          case FLOAT3_UNIT:
            revised_type = FLOAT1_ARRAY;
            revised_cap = CAPPED_WIDTH_3;
            break;
          case INT3_UNIT:
            revised_type = INT0_ARRAY;
            revised_cap = CAPPED_WIDTH_BOUNDLESS;
            break;
          case STAMP_DATETIME_UNIT:
            revised_type = STAMP_DATETIME_ARRAY;
            revised_cap = CAPPED_WIDTH_3;
            break;
          case STAMP_YEAR_UNIT:
            revised_type = STAMP_YEAR_ARRAY;
            revised_cap = CAPPED_WIDTH_1;
            break;
          case STAMP_MONTH_UNIT:
            revised_type = STAMP_MONTH_ARRAY;
            revised_cap = CAPPED_WIDTH_2;
            break;
          case STAMP_DAY_UNIT:
            revised_type = STAMP_DAY_ARRAY;
            revised_cap = CAPPED_WIDTH_2;
            break;
          case STAMP_HOUR_UNIT:
            revised_type = STAMP_HOUR_ARRAY;
            revised_cap = CAPPED_WIDTH_0;
            break;
          case STAMP_MINUTE_UNIT:
            revised_type = STAMP_MINUTE_ARRAY;
            revised_cap = CAPPED_WIDTH_1;
            break;
          case STAMP_SECONDS_UNIT:
            revised_type = STAMP_SECONDS_ARRAY;
            revised_cap = CAPPED_WIDTH_2;
            break;
          case STAMP_MILLISECONDS_UNIT:
            revised_type = STAMP_MILLISECONDS_ARRAY;
            revised_cap = CAPPED_WIDTH_2;
            break;
          case STAMP_MICROSECONDS_UNIT:
            revised_type = STAMP_MICROSECONDS_ARRAY;
            revised_cap = CAPPED_WIDTH_3;
            break;
          case STAMP_NANOSECONDS_UNIT:
            revised_type = STAMP_NANOSECONDS_ARRAY;
            revised_cap = CAPPED_WIDTH_3;
            break;
          default:
            revised_type = UNTYPED_RLINK3_ARRAY; // you can't do PREFERRED_MIXED_ARRAY_TYPE until you handle potential LAYOUT changes for SLAB4/LIST 
            break;
        }

        header_pointer_begin()->presented_type = revised_type;
        header_pointer_begin()->vector_container_width_cap_type = revised_cap; // TODO this is vector specific, should wrap as LAYOUT methods, w/ no-ops
        parent()->reconstitute_vtables();
        this->cow_append(rhs);

        assert(!presented()->is_grouped());
        return;
      }
    default:
      break;
  }

  switch(this->layout_payload_chunk_type())
  {
    case CHUNK_TYPE_RLINK3:
    {
      assert(!this->presented()->is_unit()); // only because we didn't account for this [whether this happens or not, i think it might not] yet. I think we can just break to fall out of the switch anyway

      // case TWO(RLINK3_ARRAY, RLINK3_UNIT): // these exist, but when you append SLOP, they'll have already morphed into grabbable ->slabp

      // Remark. you know, maybe this would've been smarter to coerce as C++ (cpp_heap, with registration), then we can hack it at the end to be PARENT and rc==1. | maybe we should've waited until it was on there to change the arena and increment it?? | maybe it would be safer on an RCSP3Unit, while we do all this stuff [possibly, but auto RCP3UNIT doesn't rc pointers]. also, machinery is there in scalar | Same thing goes for the SLAB4_ARRAY version below.

      assert(rhs.slabp != parent()->slabp); // partial cycle check
      // TODO Here at RLINK3_ARRAY and at SLAB4_ARRAY: Isn't there a check we need to make somewhere about not creating a cycle? reference counting cycle. Yes, there's a trick we used in Kona/Kerf1, you just need to check if the top-level RHS is anything on the chain to root (kerf tree root, or function tree root, what have you), b/c you can assume RHS is cycle free (induction), hence it can only form a cycle if a subelement is in LHS ancestry, hence top RHS must be LHS or LHS ancestor [graph theory!]. Anyway: append() is probably the wrong place to check for this b/c of speed/accessibility reasons, we can add a toggleable debug-time cycle check assert [thorough], but for speed reasons the real check should pr. take place in a top-level amend function and occur as you descend. See kona/kerf1 source.

      SLOP c = rhs; // P_O_P can we do this without a copy? keep rhs wo/ dropping the const thing?

      // if(rhs.is_tracking_memory_mapped()) { c = rhs.self_or_literal_memory_mapped_if_tracked(); }

      c.coerce_parented();
      c.legit_reference_increment();

      // NB. until this pointer makes it onto the RLINK3_ARRAY farther down, it's untracked (risk of memory leak)
      SLOP temp((I)c.layout()->slab_pointer_begin()); // pack the SLAB* in an INT3_UNIT
      cow_append_scalar_byte_int_float_pointer(temp);
      return;
    }
    case CHUNK_TYPE_VARSLAB:
    {
      if(rhs.layout()->header_get_slab_object_layout_type() == LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM)
      {
        SLOP hack = rhs.operator[](0)[0]; // P_O_P this is probably slow.
        cow_append(hack);
        return;
      }

      bool regular_stride = this->payload_known_regular_width();

      bool left_flat = this->known_flat();
      bool right_already_flat = rhs.layout()->known_flat();

      // NB. keyword BROKEN_CHAIN_RLINK. see FILE_OPERATIONS::read_from_drive_path_directory_expanded or maybe `open_from_...` for a discussion
      bool must_append_coerced_flat_to_flat = left_flat; 
      bool allowing_append_flat_slab4_and_slab4_containing_rlink3 = !must_append_coerced_flat_to_flat;

      if(must_append_coerced_flat_to_flat && !right_already_flat)
      {
        // TODO we'd rather use coerce_FLAT here but we want UNTYPED_LIST version not JUMP_LIST, likely these methods can be factored/equipped with toggles in signature
        // make rhs flat
        SLOP u(UNTYPED_LIST);
        // flat iterate rhs into u
        auto g = [&](SLOP x) {
          u.layout()->cow_append(x);
        };
        rhs.iterator_uniplex_layout_subslop(g);
        if(rhs.presented()->is_grouped())
        {
          u.cowed_rewrite_presented_type(this->layout()->header_get_presented_type());
          u.slabp->presented_reserved = parent()->slabp->presented_reserved;
        }
        cow_append(u);

        return;
      }

      I observed_width = -1;

      const SLOP *flat_rhs = &rhs;

      SLAB b = {0};
      SLOP temp;

      I remain = this->payload_countable_appendable_bytes_remaining();
      I width_appending_aligned = rhs.layout()->slab_bytes_current_used_aligned();
      I rup2_width = ceiling_log_2(width_appending_aligned);
      bool bigger_than_slab4 = rup2_width  > LOG_SLAB_WIDTH;
      bool   less_than_slab4 = rup2_width  < LOG_SLAB_WIDTH;

      static constexpr I rlink_fixed_log_width = LOG_SLAB_WIDTH;
      static constexpr I rlink_fixed_width = POW2(rlink_fixed_log_width);

      // P_O_P we actually want to skew towards not breaking regular-stride as well? ### not sure this case will arise, allows rlinks but uses none and all children are bigger than slab4.
      // Remark: there's actually a four-quadrant matrix here and SLAB4_ARRAY corresponds to 1. regular stride 2. [may] contains links/[may]not flat. In addition to always wanting to maintain width-16 entries.
      if(allowing_append_flat_slab4_and_slab4_containing_rlink3 && (bigger_than_slab4 || (regular_stride && less_than_slab4)))
      {
        // re: less_than_slab4
        // Explanation. In the case of allowing_rlinks, make sure we don't inadvertently append any length-8 items, say empty arrays, into the LIST before we get a chance to standardize on length-16 items for regularity with rlink [regular_stride/payload_known_regular_width()]. Motivation. This is for SLAB4_ARRAY so when we're doing GROUPED item building we can append empty lists and so on, and then later we can append to those inline sublists and have them convert to a RLINK. Note. if(bigger_than_slab4) is always going to set this value anyway
        // re: bigger_than_slab4
        // same story. we don't want to break stride regularity (among other things), so we squash into rlink.
        // Observation. Given these constraints...maybe SLAB4 should be a separate layout type from LIST? Perhaps we can overcome this by adding a knew function stride_skip or something in layout instead of used_width? The advantage in the separate layout type is you always get a known width in the iterator, expanders, etc. without resolving it everywhere and munging it over LIST-class's returned results.

        // Remark. Important.
        // Only size16 blocks can be rewritten as rlinks [in O(1) time]! Size8 blocks are too small and trigger O(n) full rewrite. Size>16 blocks will leave "gaps" and mess up iteration, because RLINK has no way to report that irregular width. Two possible workarounds: 1. If A. the replaced object was a power of 2 and B. we changed width reporting to look at m and not just used, that would help. 2. If we made a special layout which was just a "spacer" (consumes used-data specified by int3 I 64-bit int 8 bytes wide) then we could put an RLINK unit in it somehow or another type of unit.


        SLOP c = rhs; // P_O_P can we do this without a copy? keep rhs wo/ dropping the const thing?
        c.coerce_parented();
        // NB. until this pointer makes it onto the SLAB4_ARRAY farther down, it's untracked (risk of memory leak)
        c.legit_reference_increment();
        c.legit_reference_increment(); // This second one is necessary if slop_lose_reference frees auto_slab RLINK3 children (we could rewrite it to enclose in a gain, anyway)

        b = SLAB::good_rlink_slab(c.layout()->slab_pointer_begin());
        // b.t_slab_object_layout_type = LAYOUT_TYPE_UNCOUNTED_ATOM;
        // b.presented_type = RLINK3_UNIT;
        // b.a = c.layout()->slab_pointer_begin();

        assert(SLAB_ALIGNED(rlink_fixed_width));
        width_appending_aligned = rlink_fixed_width;

        temp.SLOP_constructor_helper((SLAB)b); // mostly fixes up b's memory header
        // temp.slabp->m_memory_expansion_size = MEMORY_UNPERMISSIONED_TENANT_NOEXPAND;
        assert(temp.slabp->m_memory_expansion_size == rlink_fixed_log_width);
        flat_rhs = &temp;
      }

      // Expand if needed
      if(remain < width_appending_aligned)
      {
        I invariant = this->presented()->chunk_width_log_bytes();
        I incoming_chunks = round_up_nearest_multiple(width_appending_aligned, POW2(invariant)) >> (invariant);
        assert(incoming_chunks * POW2(invariant) >= width_appending_aligned);
        this->promote_or_expand_via_widths(invariant, incoming_chunks, true, width_appending_aligned);
      }

      const SLOP& ready = *flat_rhs;

      char *start = (char*)this->payload_pointer_current_end();

      if(DEBUG)
      {
        I used = this->payload_bytes_current_used();
        assert(SLAB_ALIGNED(used));
      }

      // write the darn thing
      LAYOUT_BASE::write_to_slab_position((SLAB*)start, ready, width_appending_aligned);

      I before = 0;
      if(DEBUG)
      {
        before = this->header_get_byte_counter_value();
      }

      // increment the byte counter
      this->header_add_to_byte_counter_unchecked(width_appending_aligned);

      if(DEBUG)
      {
        I after = this->header_get_byte_counter_value();
        assert(before + width_appending_aligned == after);
      }

      // Set the stride bit
      if(regular_stride && -1 != (observed_width = this->observed_width_aligned_or_minus_one()) && width_appending_aligned != observed_width)
      {
        this->payload_set_known_regular_width_attribute(false);
        assert(!this->payload_known_regular_width());
      }

      parent()->cowed_check_and_set_sorted_attributes_for_append_based_on_last_two();

      return;
    }
    default:
      break;
  }

  // going to UNTYPED here instead of letting UNITs fall into the generic MIXED_LAYOUT promote below lets us create [smaller, specialized] vectors when possible. The generic promote implies we already lost [passed, missed, forfeited] the opportunity to do this.
  if(presented()->is_unit())
  {
    SLOP u(UNTYPED_ARRAY);
    u.layout()->cow_append(*parent());
    u.layout()->cow_append(rhs);

    *parent() = u;

    assert(!presented()->is_grouped());
    return;
  }

  // generic promote 
  {
    // POTENTIAL_OPTIMIZATION_POINT for certain lists (int,float,char vectors, etc.), you can promote them in place without going through this rigmarole, but I guess only to slab4_array not rc_slabpointer3_array 
    // P_O_P. rc_slabpointer3_array does have a nice one though... you use the same slab4_array you would've produced for slab4_array and then you produce pointers to it. I guess that doesn't work because the slab4 is dangling. You can fix it by setting the values to NOT be TRANSIENT but instead managed arena when writing the slabs, provided the pool doesn't complain about you chunking up its items. This does raise another idea though, which is that we can allocate transient atoms that are managed by something external (maybe this is a bad path). You know, you can do this, you just make the m large for the first atom, then all the others can be transient and you can point to them. That still doesn't work, cause then if the header atom gets overwritten its game over [for the rest].)

    SLOP r(PREFERRED_MIXED_TYPE);

    // P_O_P: don't even need to cow() the first time
    auto g = [&](const SLOP& x) {r.layout()->cow_append(x);};
    parent()->iterator_uniplex_layout_subslop(g);

    r.layout()->cow_append(rhs);

    // *this = r;
    *parent() = r;
        
    assert(!presented()->is_grouped());
    return;
  }

}

void LAYOUT_BASE::cow_join(const SLOP &rhs)
{
  // P_O_P special case everything Reminder: check attr sorted asc/desc
  auto g = [&](const SLOP& x){this->cow_append(x);};
  rhs.iterator_uniplex_layout_subslop(g);
}

#pragma mark -

void LAYOUT_BASE::cow_amend_one(I k, const SLOP &rhs)
{
    // P_O_P: Θ(n) Reminder: check attr sorted asc/desc
    // Reminder Don't forget actual cow() when switching to O(1)
    SLOP u(UNTYPED_ARRAY);

    // inherit layout/presented attributes (Question. Does this work in all cases (UNTYPED_ARRAY won't genuinely recognize some eg INT*_ARRAY?, but must port them)?)
    u.layout()->header_pointer_begin()->second_four = layout()->header_pointer_begin()->second_four;
    // cleanup
    u.layout()->header_pointer_begin()->presented_type = UNTYPED_ARRAY;
    u.layout()->header_set_byte_counter_to_value_unchecked(0);

    I i = 0;

    auto g = [&](const SLOP& x) { u.layout()->cow_append(k==i++ ? rhs : x); };
    parent()->iterator_uniplex_layout_subslop(g);

    // TODO Bug this is an incomplete capture, the whole function is not right: we need presented attributes, layout attributes, ...
    // (Idea. we can do this with a SLAB struct buffer and two SLAB:: functions accepting two arguments: one to populate the buffer, one to read from it)
    // possibly we should wait and fix this at the same time as doing the O(1) version instead of O(n) (note for some layouts, you'll
    // only get O(n))
    // u.layout()->header_set_presented_type(layout()->header_get_presented_type(), true); // NB. trigger vtable refresh
    // See SLOP::coerce_FLAT_JUMP_LIST_WITH_SUBLISTS, and see also SLOP::coerce_UNFLAT

    bool grouped = presented()->is_grouped();
    auto old_presented = layout()->header_get_presented_type();
    auto old_presented_reserved = parent()->slabp->presented_reserved;

    *parent() = u;

    if(grouped)
    {
      parent()->cowed_rewrite_presented_type(old_presented);
      parent()->slabp->presented_reserved = old_presented_reserved;
    }

    
    return;
}

void LAYOUT_BASE::cow_delete_one(I k)
{
    // P_O_P: Θ(n)  Reminder: check attr sorted asc/desc
    SLOP u(UNTYPED_ARRAY);
  
    I i = 0;

    auto g = [&](const SLOP& x) { if(i++==k)return; u.layout()->cow_append(x);};
    parent()->iterator_uniplex_layout_subslop(g);

    *parent() = u;
    
    return;
}


} // namespace
