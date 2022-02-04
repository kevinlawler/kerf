namespace KERF_NAMESPACE {

ITERATOR_LAYOUT::ITERATOR_LAYOUT(const SLOP& parent, bool last, bool should_rlink_descend, bool should_memory_mapped_descend) : parent(parent), should_rlink_descend(should_rlink_descend), should_memory_mapped_descend(should_memory_mapped_descend)
{
  ITERATOR_LAYOUT_HELPER(parent, last);
}

bool ITERATOR_LAYOUT::over_rlink_reference()
{
  switch(this->chunk_type)
  {
    case CHUNK_TYPE_RLINK3:
      return true;
    case CHUNK_TYPE_VARSLAB:
      return    child_ptr->t_slab_object_layout_type == LAYOUT_TYPE_UNCOUNTED_ATOM
             && child_ptr->presented_type == RLINK3_UNIT;
    default:
      return false;
  }
}

SLAB** ITERATOR_LAYOUT::rlink_address()
{
  assert(over_rlink_reference()); // NB. We could merge the two at the expense of clarity
  switch(this->chunk_type)
  {
    case CHUNK_TYPE_RLINK3:
      return child_ptrptr;
    case CHUNK_TYPE_VARSLAB:
      return &child_ptr->a;
    default:
      die(nullptr_address requested of wrong type);
      return nullptr;
  }
}

void ITERATOR_LAYOUT::ITERATOR_LAYOUT_HELPER(const SLOP& parent, bool last)
{
  // POTENTIAL_OPTIMIZATION_POINT: split all this into virtual subclass methods instead of switch() statements

  // Remark. LIST without regular-stride attribute won't iterate `reverse` (or `last` or `previous`)
  //         LIST with regular-stride will
  //         JUMP_LIST will
  // Question. Should we just make a new iterator classes? The existing thing is just not going to work, because you can't track any of the traits of the parent, including its address. [Even if you get it to overload inside auto_slab, LIST+embedded_rlinks will cause your auto_slab to get overwritten. You need a new class.]  For the final two, just use the index methods. For the first, model it after that one's index method for traversal.

  // then the problem is our TAPE_HEAD won't be able to modify item in place IF it changes type of parent. sure it can, same as the other way.

  // INT3_ARRAY...: modify the int3 pointer via TAPE_HEAD
  // RLINK3_ARRAY/RLINK3_UNIT: NON-TAPE-HEAD
  // SLAB4_ARRAY/SLAB4_UNIT: NON-TAPE-HEAD 
  //   if nonempty return index at [0]

  LAYOUT_BASE &r = *parent.layout();

  if(r.payload_bytes_current_used() <= 0) // Hard to iterate on 0-length lists
  {
    slop = SLOP(NIL_UNIT); // or maybe *parent(); 
    known_item_count = 0;
    pos = -1;
    observed_width_aligned = -1;
    return;
  }

  pos = 0;
  first_item = r.payload_pointer_begin();
  V target = first_item;

  PRESENTED_TYPE p;

  this->chunk_type = r.layout_payload_chunk_type();

  // TODO I think there's an issue here and in next where we want the actual layout but the slop is hosing us by hiding the RLINK
  //      Remark. it's a weirder case [to achieve] than you think: you have to have a LIST which, in addition to not being regular stride, 
  //              allows RLINKs inside of it after-the-fact. currently in the code, this is an impossibility. we either start with
  //              a LIST that intends to remain flat (no rlinks), or we start with a SLAB4_ARRAY that stays regular-stride the whole time. 
  //              [ we've updated things a little...eg width_aligned_claimed_in_mixed_parent...is this thing still an issue?]
  //              that's why this "oversight" hasn't bit us yet
  //              This is s/t we might conceivably take advantage of, for instance, to STRIPE things on the drive using a LIST but
  //              without striping the inessential pieces (only turning the essential pieces into a big file with a numeric name that we
  //              link in place as a MAPPED_OBJECT in the parent list when read).
  //              potentially refactor SLOP constructor to have a method "->resolve_rlink_ptrs" 
  //        Idea. Inside `case VARSLAB` it's a switch(TWO(stride bit, rlink bit)), here and at sideways()/next()

  // Remark/TODO. when we add JUMP_LIST we'll need to branch on `layout type` as well
  switch(this->chunk_type)
  {
    case CHUNK_TYPE_RLINK3:
      known_item_count = r.payload_used_chunk_count();
      if(last)
      {
         V last_item = ((char*)r.payload_pointer_current_end()) - r.presented()->chunk_width_bytes();
         target = last_item;
         pos = known_item_count - 1;
      }
      child_ptrptr = (SLAB**)target;

/////////////////////////////////////////////////////////////////////////////////////////
      if(should_rlink_descend) slop.SLOP_constructor_helper(child_ptrptr, 0, true, should_memory_mapped_descend);
/////////////////////////////////////////////////////////////////////////////////////////

      return;
    case CHUNK_TYPE_VARSLAB:
      child_ptr = (SLAB*)target;
      known_regular_stride = parent.layout()->payload_known_regular_width();
      // Remark. If the first child is literally an RLINK_UNIT (slab of fixed known size 16), we need to make sure we're recording that as the observed_width and not whatever the first child points to (unknown width).
      // P_O_P we're doing this second one (the first one) [the constructor on slop] because we need to get width on rlink3 without getting width on what it points to - create a method that all it does is follow the rlink [leverage the work done in the existing call] instead of causing us to call SLOP_constructor_helper again. None of this may make any difference
      // P_O_P alternatively, we can do the normal slop constructor, but check is_tracking_parent_of_slab() and set observed_width_aligned=16 if so. Done
      // slop.SLOP_constructor_helper(child_ptr, 0, true, false);
      slop.SLOP_constructor_helper(child_ptr, 0, true, should_rlink_descend, should_memory_mapped_descend);
      observed_width_aligned = slop.layout()->width_aligned_claimed_in_mixed_parent();
      assert(SLAB_ALIGNED(observed_width_aligned));
      assert(known_regular_stride || !last); // we can't iterator basic LISTs in reverse, at least not naively in o(n^2)
      if(known_regular_stride) known_item_count = r.payload_bytes_current_used() / observed_width_aligned;
      // TODO? else if(is jumplist && !attribute_zero_if_jump_list_is_complete_and_no_untracked_terms ) known_item_count = count(jumps in header)
      if(last) seek_last();
      return;
    default:
    case CHUNK_TYPE_UNCHUNKED_RAW_BYTE_DATA:
    case CHUNK_TYPE_VARINT:
    case CHUNK_TYPE_VARFLOAT:
      known_item_count = r.payload_used_chunk_count();
      if(last)
      {
        V last_item = ((char*)r.payload_pointer_current_end()) - r.presented()->chunk_width_bytes();
        target = last_item;
        pos = known_item_count - 1;
      }
      // HACK
      SLOP fake = r[0];
      p = fake.layout()->header_get_presented_type();
      // lol 
      break;
  }

  SLAB s = (SLAB){.t_slab_object_layout_type=LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM,
                  .second_four=r.header_pointer_begin()->second_four,
                  .v = target};
  s.zero_sutex_values();
  s.presented_type=p;

/////////////////////////////////////////////////////////////////////////////////////////

  slop.SLOP_constructor_helper((SLAB)s);
  
  // HACK keep the old vtable for the accessors. so PRESENTED object will be "stale" in a sense 
  memcpy(slop.presented_vtable, r.parent()->presented_vtable, sizeof(r.parent()->presented_vtable));
  // Remark. I don't think this hack is necessary any more (in this manner) now that we have a parent reference and a slop around.
  //         you can write to the tape head (a UNIT) by reading from the parent (an ARRAY)
  //         Further, fixing this hack probably requires instituting new PRESENTED types of smaller units (eg INT2_UNIT), which is undesirable

/////////////////////////////////////////////////////////////////////////////////////////

  return;
}

void ITERATOR_LAYOUT::sideways(I count)
{
  switch(this->chunk_type)
  {
    case CHUNK_TYPE_VARSLAB:{
      assert(slop.slabp != slop.auto_slab());

      char *s = (char*) child_ptr; 

      if(known_regular_stride)
      {
        s += observed_width_aligned * count; 
        child_ptr = (SLAB*)s;
/////////////////////////////////////////////////////////////////////////////////////////
        slop.SLOP_constructor_helper(child_ptr, 0, true, should_rlink_descend, should_memory_mapped_descend);
/////////////////////////////////////////////////////////////////////////////////////////
      }
      else
      {
        const bool allows_linear_reverse = false; // we can't iterator basic LISTs in reverse, at least not naively in o(n^2)

        if(allows_linear_reverse)
        {
          if(count < 0)
          {
            seek_first();
            I desired_position = pos + count;
            count = desired_position;
          }
        }
        else assert(!(count < 0));

        DO(count, 
/////////////////////////////////////////////////////////////////////////////////////////
           I width = slop.layout()->width_aligned_claimed_in_mixed_parent();
/////////////////////////////////////////////////////////////////////////////////////////
           assert(SLAB_ALIGNED(width));
           s += width;
           child_ptr = (SLAB*)s;
/////////////////////////////////////////////////////////////////////////////////////////
           // Bug. I think there's likely a bug here related to should_memory_mapped_descend, because depending on whether the MEMORY_MAPPED is a child of an RLINK3 inside of a VARSLAB, or simply inline inside of a VARSLAB, we'd need to be able to compute `width` correctly and I'm not sure that the current code does that. You could do it by taking a step back and precomputing the width before allowing a descent.
           slop.SLOP_constructor_helper(child_ptr, 0, true, should_rlink_descend, should_memory_mapped_descend);
/////////////////////////////////////////////////////////////////////////////////////////
        )
        if(slop.layout()->slab_pointer_current_end_aligned() >= parent.layout()->slab_pointer_current_end_aligned())
        {
          is_end_hack = true;
        }
      }
      break;
    }
    case CHUNK_TYPE_RLINK3:{  // we read a (SLAB**) inside an RLINK3_ARRAY
      if(DEBUG)
      {
        if(should_rlink_descend) assert(slop.auto_slab()->t_slab_object_layout_type == LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM);
        if(should_rlink_descend) assert(slop.slabp != slop.auto_slab());
      }

      child_ptrptr += count;

/////////////////////////////////////////////////////////////////////////////////////////
      if(should_rlink_descend) slop.SLOP_constructor_helper(child_ptrptr, 0, true, should_memory_mapped_descend);
/////////////////////////////////////////////////////////////////////////////////////////
      break;
    }
    default:
    case CHUNK_TYPE_UNCHUNKED_RAW_BYTE_DATA:
    case CHUNK_TYPE_VARINT:
    case CHUNK_TYPE_VARFLOAT:
      // POP If we're using range-based for `operator*` semantics, we can depend on that instead of TAPE_HEAD dereferencing, ie, we can just populate a normal SLOP via indexing and dispense with TAPE_HEAD altogether.
      assert(slop.layout()->header_get_slab_object_layout_type() == LAYOUT_TYPE_TAPE_HEAD_UNCOUNTED_ATOM);
      assert(slop.slabp == slop.auto_slab());
      slop.slabp->s += (count * slop.presented()->chunk_width_bytes());
      break;
  }

  pos += count;
}

void ITERATOR_LAYOUT::next(I count)
{

  /////////////////////////////////////////////////////////////////////////////////////////
  // HACK
  bool reverse = count <= 0;
  if(known_item_count != -1 && pos + 1 >= known_item_count && !reverse)
  {
    is_end_hack = true;
    return;
  }
  /////////////////////////////////////////////////////////////////////////////////////////

  sideways(count);
}

void ITERATOR_LAYOUT::previous(I count)
{
  sideways(-count);
}

bool ITERATOR_LAYOUT::has_next(bool reverse)
{
  if(reverse) return has_previous();
  if(known_item_count != -1) return (pos + 1) < known_item_count;

/////////////////////////////////////////////////////////////////////////////////////////
  bool more_left = slop.layout()->slab_pointer_current_end_aligned() < parent.layout()->slab_pointer_current_end_aligned();
  // ???  child_ptr and child_ptrptr < parent.layout()->slab_pointer_current_end_aligned();
  // ???  tape_head handled by known count 
/////////////////////////////////////////////////////////////////////////////////////////
 
  if(!more_left)
  {
    known_item_count = pos + 1; // cache discovered item_count
  }

  return more_left;
}

bool ITERATOR_LAYOUT::has_previous()
{
  if(known_item_count != -1) return pos > 0;
  return first_item < child_ptr;
}


void ITERATOR_LAYOUT::seek_first()
{
  if(known_item_count == 0) return;
  ITERATOR_LAYOUT_HELPER(parent, false);
}

void ITERATOR_LAYOUT::seek_last()
{
  if(known_item_count == 0) return;

  if(known_item_count != -1)
  {
    next(known_item_count - 1 - pos);
    return;
  }

  while(has_next()) next();

  known_item_count = pos + 1;
}

#pragma mark - Presented

ITERATOR_PRESENTED::ITERATOR_PRESENTED(const SLOP& parent, bool last) : ITERATOR_LAYOUT(parent, last)
{

  if(parent.presented()->is_simply_layout()) return;

  // POP the redundant constructor (`:ITERATOR_LAYOUT`) slows things down, esp. as long as ITERATOR_LAYOUT's p.o.p. is unfinished, regarding not initializing the SLOP before operator* is called.
  slop.neutralize(true);
  pos = 0;
  known_item_count = parent.countI();
}

void ITERATOR_PRESENTED::sideways(I count)
{
  if(parent.presented()->is_simply_layout())
  {
    return ITERATOR_LAYOUT::sideways(count);
  }

  pos += count;
}

SLOP& ITERATOR_PRESENTED::operator*()
{
  if(parent.presented()->is_simply_layout())
  {
    return ITERATOR_LAYOUT::operator*();
  }

  slop = parent[pos];
  return slop;
}

 

}
