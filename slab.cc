#pragma once

namespace KERF_NAMESPACE {

void SLAB::callback_from_mapped_queue_to_store_index(I index)
{
  // Idea. instead of doing this here as a callback, why not return the `index` from the push_back executed by MEMORY_MAPPED, then you get something easier to reason about. Is it some consideration with the lock? easily avoided I think

  // R̶e̶m̶a̶r̶k̶.̶ ̶T̶h̶i̶s̶ ̶i̶s̶ ̶t̶h̶r̶e̶a̶d̶ ̶s̶a̶f̶e̶ ̶b̶e̶c̶a̶u̶s̶e̶ ̶i̶t̶'̶s̶ ̶c̶a̶l̶l̶e̶d̶ ̶f̶r̶o̶m̶ ̶s̶e̶l̶f̶ ̶t̶h̶r̶e̶a̶d̶ Warning. Apparently it wasn't. So we use the neutralizing strategy of the other callback. Idea. I think the issue may possibly be that we really don't want to put some other thread's cpp arena on our own workstack.
  // SLOP p(this);
  // p.presented()->callback_from_mapped_queue_to_store_index(index);

  SLOP p(NIL_UNIT);
  p.slabp = this;
  assert(-1 == p.index_of_myself_in_cpp_workstack); // don't put someone else's cpp-arena-object on our own workstack
  p.reconstitute_vtables();
  p.presented()->callback_from_mapped_queue_to_store_index(index);
  p.neutralize();
}

void SLAB::callback_from_mapped_queue_to_inform_removed()
{
  SLOP p(NIL_UNIT);
  p.slabp = this;
  assert(-1 == p.index_of_myself_in_cpp_workstack); // don't put someone else's cpp-arena-object on our own workstack
  p.reconstitute_vtables();
  p.presented()->callback_from_mapped_queue_to_inform_removed();
  p.neutralize();
}

} // namespace
