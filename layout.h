namespace KERF_NAMESPACE {
struct LAYOUT_BASE
{
  SLOP* parent();
  SLAB* auto_slab();
  auto layout();
  PRESENTED_BASE* presented();

  //SLAB or LAYOUT: contains a HEADER with some data following it
  //PAYLOAD: the ultimate value(s) intended, may not necessarily exist on this slab, as in the case of TAPE_HEAD_*

  virtual CHUNK_TYPE layout_payload_chunk_type();

     virtual I slab_bytes_current_used();
             I slab_bytes_current_used_aligned();
         SLAB* slab_pointer_begin();
             V slab_pointer_current_end(); // current_end < total_end if you overallocated (->m) for an ATOM, TAPE_HEAD, ...
             V slab_pointer_current_end_aligned(); // current_end < total_end if you overallocated (->m) for an ATOM, TAPE_HEAD, ...

               // Rule. Obj inside LIST do not use m. Have as 0 or 4 or something. Their total capacity is going to be their used capacity (rounded up to nearest SLAB_ALIGN==8) with effectively no unused. This is like a FILLED but based on byte_counter instead of ->m. See discussion at REFERENCE_MANAGEMENT_ARENA_UNMANAGED_TRANSIENT
          void memory_zero_unused_slab_space();
             I memory_slab_total_capacity(); // ? this probably should vary based on TENANCY. tenant/drive items can specify their length wo/ ->m values.  
             I memory_slab_bytes_current_unused();
             V memory_slab_pointer_total_end();

      static I header_count_memory_bytes_at_front()          {return 3;}
          auto header_get_slab_object_layout_type()          {return header_pointer_begin()->t_slab_object_layout_type;}  // may want to virtualize later for further cheating
          // bool header_get_slab_is_on_drive()                 {return !!header_pointer_begin()->a_memory_attribute_mmap_drive;}
            UC header_get_memory_reference_management_arena(){return header_pointer_begin()->reference_management_arena;} // may want to virtualize later for further cheating
            UC header_get_memory_expansion_log_width()       {return header_pointer_begin()->m_memory_expansion_size;}    // may want to virtualize later for further cheating. Warning: PROBABLY don't want to cheat at UNCOUNTED_ATOM unless you refactor some things.
            UC header_get_slab_reference_count()             {return header_pointer_begin()->r_slab_reference_count;}
          void header_decrement_slab_reference_count()       {       header_pointer_begin()->r_slab_reference_count--;}
          void header_increment_slab_reference_count()       {       header_pointer_begin()->r_slab_reference_count++;}
PRESENTED_TYPE header_get_presented_type()                   {return header_pointer_begin()->safe_presented_type(); }
          void header_set_presented_type(PRESENTED_TYPE p, bool refresh);
          bool header_get_known_sorted_asc()                 {return header_pointer_begin()->attribute_known_sorted_asc;}
          void header_set_known_sorted_asc(bool v)                  {header_pointer_begin()->attribute_known_sorted_asc = v;}
          bool header_get_known_sorted_desc()                {return header_pointer_begin()->attribute_known_sorted_desc;}
          void header_set_known_sorted_desc(bool v)                 {header_pointer_begin()->attribute_known_sorted_desc = v;}
            UC header_get_bigint_sign()                      {return header_pointer_begin()->bigint_sign;}

         SLAB* header_pointer_begin();
             V header_pointer_total_end();
     virtual I header_bytes_total_capacity();
  virtual bool header_adjacent_in_memory_to_payload();
     virtual I header_get_byte_counter_value(); // PACKED vectors use char instead of int64_t
     virtual I header_set_byte_counter_to_value_unchecked(I c);
             I header_add_to_byte_counter_unchecked(I bytes);
             I header_add_n_chunk_width_to_byte_counter_unchecked(I n);
             I header_add_chunk_width_to_byte_counter_unchecked();

     virtual V payload_pointer_begin();
     virtual I payload_bytes_total_capacity();
     virtual V payload_pointer_total_end();
     virtual I payload_bytes_current_used();
     virtual V payload_pointer_current_end();
  virtual bool payload_current_end_is_unused_space();
     virtual I payload_used_chunk_count();
     virtual I payload_countable_appendable_bytes_remaining();
     virtual V payload_pointer_to_ith_chunk(I i);

     virtual C container_width_cap_type() {return CAPPED_WIDTH_3;}

     virtual I width_aligned_claimed_in_mixed_parent();

  ////////////////////////////////////////////////////////
  // LIST / JUMP_LIST ///////////////////////////////////
  virtual bool payload_known_regular_width() {return true;}
  virtual void payload_set_known_regular_width_attribute(bool v){}
  virtual bool known_flat() {assert(header_adjacent_in_memory_to_payload()); return layout_payload_chunk_type() != CHUNK_TYPE_RLINK3;}
  virtual bool has_constant_time_algo_index() {return true;} // Technically, GROUPED elts all do if restricting #layout children <= ε
  virtual bool has_constant_time_algo_append() {return true;}
  virtual bool has_constant_time_algo_amend() {return true;}
  virtual bool has_constant_time_algo_count() {return true;}
             V COUNTED_LIST_payload_pointer_to_ith_chunk_helper(I i);
             I COUNTED_LIST_payload_count_zero_one_or_two_plus_chunks_helper();
     virtual I observed_width_aligned_or_minus_one();
     virtual I payload_count_zero_one_or_two_plus_chunks();
  ////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////
  // External exposure ///////////////////////////////////
  SLOP slop_from_unitized_layout_chunk_index(I i);
  SLOP operator[](I i); // Note: LAYOUTs are only indexed by integral i, that's a key distinguishing feature. PRESENTED are indexed by general SLOP objects. Note. The other nice thing this implies [I think it also depends on "LAYOUTs only do [numerically indexed] arrays"], is every object as a LAYOUT hierarchy that's numerically and canonically defined by an array of ints, eg [1,9,3,2] that uniquely identifies every object in the construction. We can use this for instance in writing nested objects as files on disk
  ////////////////////////////////////////////////////////

  static void write_to_slab_position(SLAB* start, const SLOP& rhs, I width_writing_aligned); // return total bytes written unaligned

  ////////////////////////////////////////////////////////
  // Expand, Append, Join ////////////////////////////////
  SLAB* more_space_on_drive(C proposed_m);
  SLAB* more_space(C proposed_m); // may return different from parent()->slabp, must manage
  void promote_or_expand_via_widths(I settled_log_width, I incoming_chunks, bool known_invariant_promotional = false, I incoming_bytes_instead = 0);
  void cow_append_scalar_byte_int_float_pointer(const SLOP &rhs);
  void cow_append(const SLOP &rhs);  // eg, grouped item builder (add a MAP's key list and value list)
  void cow_join(const SLOP &rhs);
  ////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////
  // Amend ///////////////////////////////////////////////
  void cow_amend_one(I k, const SLOP &rhs);
  void cow_delete_one(I k);
  ////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////
  // Iterator ////////////////////////////////////////////
  auto begin();
  auto end();
  ////////////////////////////////////////////////////////


  // PUTATIVE ////////////////////////////////////////////
  // virtual I payload_bytes_current_unused();
  // virtual I payload_current_unused_chunk_slots();
  // virtual I header_byte_counter_max();
  // virtual I bus_size_shallow(); -- same as slab_bytes_current_used??
  // payload_can_append_counted_chunk_to_end
  // payload_appendable_chunks_remaining
  ////////////////////////////////////////////////////////

  std::string to_string();

private:
  friend std::ostream& operator<<(std::ostream& o, LAYOUT_BASE* x) {return o << x->to_string();}

};



struct LAYOUT_UNCOUNTED_ATOM : LAYOUT_BASE
{ 
  I slab_bytes_current_used();
  I header_bytes_total_capacity();
  I header_get_byte_counter_value();
  I header_set_byte_counter_to_value_unchecked(I c);
  I payload_bytes_current_used();
  I payload_bytes_total_capacity();
};

struct LAYOUT_COUNTED_VECTOR : LAYOUT_BASE
{ 
  virtual C container_width_cap_type() {return header_pointer_begin()->vector_container_width_cap_type;}
  I slab_bytes_current_used();
  I header_bytes_total_capacity();
  I payload_bytes_current_used();
};

struct LAYOUT_COUNTED_VECTOR_FILLED : LAYOUT_COUNTED_VECTOR
{ 
  I slab_bytes_current_used();
  I header_bytes_total_capacity();
  I payload_bytes_current_used();
  bool payload_current_end_is_unused_space();
  I payload_countable_appendable_bytes_remaining();
};

struct LAYOUT_COUNTED_VECTOR_SMALLCOUNT : LAYOUT_COUNTED_VECTOR 
{
  I slab_bytes_current_used();
  I header_bytes_total_capacity();
  I header_get_byte_counter_value();
  I header_set_byte_counter_to_value_unchecked(I c);
  I payload_bytes_current_used();
  I payload_countable_appendable_bytes_remaining();
};

struct LAYOUT_COUNTED_VECTOR_PACKED : LAYOUT_COUNTED_VECTOR_SMALLCOUNT
{ 
  I header_bytes_total_capacity();
};

struct LAYOUT_TAPE_HEAD_UNCOUNTED_ATOM : LAYOUT_BASE
{ 
  bool header_adjacent_in_memory_to_payload() {return false;}
  bool known_flat() {return false;}
  I slab_bytes_current_used();
  I header_bytes_total_capacity();
  I header_get_byte_counter_value();
  I header_set_byte_counter_to_value_unchecked(I c);
  V payload_pointer_begin();
  I payload_bytes_current_used();
};

struct LAYOUT_TAPE_HEAD_COUNTED_VECTOR : LAYOUT_BASE
{ 
  bool header_adjacent_in_memory_to_payload() {return false;}
  bool known_flat() {return false;}
  I slab_bytes_current_used();
  I header_bytes_total_capacity();
  I header_get_byte_counter_value();
  I header_set_byte_counter_to_value_unchecked(I c);
  V payload_pointer_begin();
  I payload_bytes_current_used();
};

struct LAYOUT_COUNTED_LIST : LAYOUT_COUNTED_VECTOR
{
  CHUNK_TYPE layout_payload_chunk_type();
  bool has_constant_time_algo_append() {return true;}
  bool has_constant_time_algo_index() {return false;}
  bool has_constant_time_algo_amend() {return false;}
  bool has_constant_time_algo_count() {return false;}

  V payload_pointer_to_ith_chunk(I i);

  I payload_used_chunk_count(); // Remark: LISTs don't have O(1) count UNLESS regular-stride P_O_P: You could add an element_count field on top of a byte_count field(s) for LIST, but seems like a waste. Remark. All this applies to _PACKED version as well.

  I payload_count_zero_one_or_two_plus_chunks();

  C container_width_cap_type() {return CAPPED_WIDTH_BOUNDLESS;}
  bool known_flat() { return !header_pointer_begin()->attribute_zero_if_known_children_at_every_depth_are_RLINK3_free;}
  bool payload_known_regular_width() {return !header_pointer_begin()->attribute_zero_if_known_that_top_level_items_all_same_stride_width;}
  void payload_set_known_regular_width_attribute(bool v){header_pointer_begin()->attribute_zero_if_known_that_top_level_items_all_same_stride_width = !v;}
};

struct LAYOUT_COUNTED_LIST_PACKED : LAYOUT_COUNTED_VECTOR_PACKED
{
  CHUNK_TYPE layout_payload_chunk_type();
  bool has_constant_time_algo_append() {return true;}
  bool has_constant_time_algo_index() {return false;}
  bool has_constant_time_algo_amend() {return false;}
  bool has_constant_time_algo_count() {return false;}

  V payload_pointer_to_ith_chunk(I i);

  I payload_used_chunk_count();

  I payload_count_zero_one_or_two_plus_chunks();

  C container_width_cap_type() {return CAPPED_WIDTH_BOUNDLESS;}
  bool known_flat() { return !header_pointer_begin()->attribute_zero_if_known_children_at_every_depth_are_RLINK3_free;}
  bool payload_known_regular_width() {return !header_pointer_begin()->attribute_zero_if_known_that_top_level_items_all_same_stride_width;}
  void payload_set_known_regular_width_attribute(bool v){header_pointer_begin()->attribute_zero_if_known_that_top_level_items_all_same_stride_width = !v;}
};

struct LAYOUT_COUNTED_JUMP_LIST : LAYOUT_COUNTED_LIST
{
  // Question. Does an empty JUMP_LIST have an empty INTVEC at the end of the header? Or no INTVEC? Answer: Empty intvec: more regular for auxiliary functions, for ease of writing, plus empty JUMP_LIST is unusual: JUMP_LIST has specialized [rarefied] purpose (certain kind of DRIVE mmapping).
  bool has_constant_time_algo_append() {return false;}
  bool has_constant_time_algo_index() {return true;}
  bool has_constant_time_algo_amend() {return true;}
  bool has_constant_time_algo_count() {return true;}

  I slab_bytes_current_used();
  I payload_bytes_current_used();
  I header_bytes_total_capacity();
  // Remark. As currently constructed, we can leave optimized indexing functions un-built and the COUNTED_JUMP_LIST will function precisely as a [linear, un-jumped] COUNTED_LIST because the integer jump vector will be ignored inside of the header.

  I second_header_size();
  SLAB* second_list_start();
  bool known_flat();
  bool payload_known_regular_width();
  void payload_set_known_regular_width_attribute(bool v);

  // Question. Doing all these passthroughs, I wonder why we didn't just append the jumps to the end of a counted list, then ignore it as part of the header. Answer. Possibly because it causes a double count or double pass during the construction at coerce_FLAT. Not a big deal either way, really, seemingly, since there isn't much to do in terms of passthrough here.
  bool header_get_known_sorted_asc();
  void header_set_known_sorted_asc(bool v);
  bool header_get_known_sorted_desc();
  void header_set_known_sorted_desc(bool v);

  // TODO, P_O_P: update methods inside of ITERATOR to account for COUNTED_JUMP_LIST
  // I payload_used_chunk_count() {return 0;} // TODO, P_O_P O(1)
  // V payload_pointer_to_ith_chunk(I i) {return nullptr;} // TODO, P_O_P O(1)
};


LAYOUT_BASE* reconstitute_layout_wrapper(void *layout_vtable, UC layout_type)
{
  switch(layout_type)
  {
    case LAYOUT_TYPE_UNCOUNTED_ATOM:
      new(layout_vtable) LAYOUT_UNCOUNTED_ATOM();
      break;
    case LAYOUT_TYPE_COUNTED_VECTOR:
      new(layout_vtable) LAYOUT_COUNTED_VECTOR();
      break;
    case LAYOUT_TYPE_COUNTED_VECTOR_FILLED:
      new(layout_vtable) LAYOUT_COUNTED_VECTOR_FILLED();
      break;
    // case LAYOUT_TYPE_COUNTED_VECTOR_SMALLCOUNT:
    //   new(layout_vtable) LAYOUT_COUNTED_VECTOR_SMALLCOUNT();
    //   break;
    case LAYOUT_TYPE_COUNTED_VECTOR_PACKED:
      new(layout_vtable) LAYOUT_COUNTED_VECTOR_PACKED();
      break;
    case LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM:
      new(layout_vtable) LAYOUT_TAPE_HEAD_UNCOUNTED_ATOM();
      break;
    case LAYOUT_TYPE_TAPE_HEAD_COUNTED_VECTOR:
      new(layout_vtable) LAYOUT_TAPE_HEAD_COUNTED_VECTOR();
      break;
     case LAYOUT_TYPE_COUNTED_LIST:
      new(layout_vtable) LAYOUT_COUNTED_LIST();
      break;
     case LAYOUT_TYPE_COUNTED_LIST_PACKED:
      new(layout_vtable) LAYOUT_COUNTED_LIST_PACKED();
      break;
     case LAYOUT_TYPE_COUNTED_JUMP_LIST:
      new(layout_vtable) LAYOUT_COUNTED_JUMP_LIST();
      break;
    default:
      new(layout_vtable) LAYOUT_BASE();
      break;
  }

  return (LAYOUT_BASE*)layout_vtable;
}
}
