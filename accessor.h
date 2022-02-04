#pragma once

namespace KERF_NAMESPACE {

struct ACCESSOR_MECHANISM_BASE
{
  // Thought. The chunk width determines the accessor method, needs its own vtable in SLOP? Subclasses on COUNTED_VECTOR, et al?
  // We could do it with a mask...sounds janky.
  // Possibly only for integers (4) and floats (2), totaling 6, which doesn't sound so bad.
  // There's a way to do it variably, with memcpy (mind endianness), which is quick but sounds way lamer
  // We can also do this with a function pointer table, retrieving from inside the accessor function: also sounds janky but may work (this is basically a vtable anyway?)
  // We could do it all four ways and then return (what about aligned access? beyond-end-of-area-access?) Use a mask to round the pointer down to the max access we need (8 byte), then do the "four" accesses, then return out of the table. Or do one access and return using a [second] mask out of the table (this is also dependent on index. single byte ints are in different places based on index.). This should work? What if repeated 8-byte access is a lot slower (for instance in the case of single byte vectors)?
  // Idea. This may be better defined by type-of-pointer returned, eg, int8_t*, int16_t*, int32_t*, int64_t*, float*, double*
  // Idea. We could also punt this to PRESENTED/RESOLVED types and make them do sized int/float vectors instead.
  // Note. But then all this is going to hose us on variable-widthed times and dates? I think that means we need separate vtable in SLOP. (Unless you want to fork LAYOUT: seems too big; and forking PRESENTED is definitely too big)
  // Question. We can fix in size nearly all the time objects and then that objection won't matter?
  static UC minimal_int_log_width_needed_to_store_magnitude(I i);
  static UC minimal_float_log_width_needed_to_store_magnitude(F x);
  static I prepare_int_for_narrowing_write(I i, UC width, UC precomputed = UCHAR_MAX);
  static I prepare_int_for_widening_read(I i, UC width);

  virtual UC chunk_width_log_bytes() = 0;
  virtual UC chunk_width_bytes() {return POW2(chunk_width_log_bytes());} 

  virtual I integer_access_raw(V pointer) {return IN;}
  virtual void integer_write_raw(V pointer, I x) {return;}

  virtual F float_access_raw(V pointer) {return FN;}
  virtual void float_write_raw(V pointer, F x) {return;}

  virtual V pointer_access_raw(V pointer) { return (V)integer_access_raw(pointer);} 

  virtual void slab4_write_raw(V pointer, SLAB x) {  *(SLAB*)pointer = x;}

  I integer_access_wrapped(V pointer)
  { 
    I x = integer_access_raw(pointer);
    x = ACCESSOR_MECHANISM_BASE::prepare_int_for_widening_read(x, chunk_width_log_bytes());
    return x;
  } 

  void integer_write_wrapped(V pointer, I x, UC precomputed = UCHAR_MAX)
  { 
    x = ACCESSOR_MECHANISM_BASE::prepare_int_for_narrowing_write(x, chunk_width_log_bytes(), precomputed);
    integer_write_raw(pointer, x);
  } 


  // TENTATIVE block ///////////////////////////////
  virtual SLAB* slab_pointer_access(V pointer);
  // TODO: there's 3 sorts of SLOP accesses: the rlinkvector_unit deref, the SLABwithRLINK deref, and the tenant inline slab
  //       rmanaged we can do with the writer-slop, the other we can do in place with a regular slop and it will detect tenancy
  //       two of them will deref for the return (assert() no doubled links / depth > 1)
  virtual SLOP slop_access(V pointer);
  //////////////////////////////////////////////////

};

struct ACCESSOR_MECHANISM_3 : virtual ACCESSOR_MECHANISM_BASE
{ 
  UC chunk_width_log_bytes() { return 3; }
  I integer_access_raw(V pointer) {return *(int64_t*)pointer;}
  void integer_write_raw(V pointer, I x) {*(int64_t*)pointer = x;}
  F float_access_raw(V pointer) {return *(double*)pointer;}
  void float_write_raw(V pointer, F x) {*(double*)pointer = x;}
};

// NB. 2020.11.29 We've marked these final because we need to ensure single [virtual] inheritance in PRESENTED
//
// struct ACCESSOR_MECHANISM_4 final : virtual ACCESSOR_MECHANISM_BASE
// { 
//   UC chunk_width_log_bytes() { return 4; }
//   I integer_access_raw(V pointer) {return ((SLAB*)pointer)->i;}
//   void integer_write_raw(V pointer, I x) {((SLAB*)pointer)->i = x;}
//   F float_access_raw(V pointer) {return ((SLAB*)pointer)->f;}
//   void float_write_raw(V pointer, F x) {((SLAB*)pointer)->f = x;}
// };
// 
// struct ACCESSOR_MECHANISM_2 final : virtual ACCESSOR_MECHANISM_BASE
// { 
//   UC chunk_width_log_bytes() { return 2; }
//   I integer_access_raw(V pointer) {return *(int32_t*)pointer;} 
//   void integer_write_raw(V pointer, I x) {*(int32_t*)pointer = x;}
//   F float_access_raw(V pointer) {return *(float*)pointer;}
//   void float_write_raw(V pointer, F x) {*(float*)pointer = x;}
// };
// 
// struct ACCESSOR_MECHANISM_1 final : virtual ACCESSOR_MECHANISM_BASE
// { 
//   UC chunk_width_log_bytes() { return 1; }
//   I integer_access_raw(V pointer) {return *(int16_t*)pointer;} 
//   void integer_write_raw(V pointer, I x) {*(int16_t*)pointer = x;}
//   F float_access_raw(V pointer) {return *(F1*)pointer;}
//   void float_write_raw(V pointer, F x) {*(F1*)pointer = x;}
// };
// 
// struct ACCESSOR_MECHANISM_0 final : virtual ACCESSOR_MECHANISM_BASE
// { 
//   UC chunk_width_log_bytes() { return 0; }
//   I integer_access_raw(V pointer) {return *(int8_t*)pointer;} 
//   void integer_write_raw(V pointer, I x) {*(int8_t*)pointer = x;}
//   F float_access_raw(V pointer) {return FN;}
//   void float_write_raw(V pointer, F x) {return;}
// };

}
