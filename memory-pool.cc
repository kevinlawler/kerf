#pragma once
namespace KERF_NAMESPACE {

void MEMORY_POOL::pool_dealloc(SLAB* z)
{
  // HACK TODO, technically, this is determined by the layout, "where" the lane value lives (if anywhere! it may be implicit)
  char lane = z->m_memory_expansion_size;

#if TEST_TRACK_ALLOCATIONS
  if(!test_alloc_tracker.contains(z)) {
    die(Test alloc tracker attempted to repool a memory address element we did not have tracked in the set.)
  }

  critical_section_wrapper([&]{
#endif

#if TEST_TRY_POOL_ZEROES_DEALLOC
  bzero(z, POW2(lane));
#endif

#if !TEST_TRY_POOL_NEVER_REUSES_STRUCTS
  pool_repool(z, lane);
#endif

#if TEST_TRACK_ALLOCATIONS
  test_alloc_tracker.erase(z);
  });
#endif
}



MEMORY_POOL::~MEMORY_POOL()
{
#if TEST_TRACK_ALLOCATIONS
  if(!test_alloc_tracker.empty())
  {
    std::cerr << "Error: Pool memory still resident. Active memory structures: " << test_alloc_tracker.size() << "\n";

    if(!The_Did_Interrupt_Flag)
    {
      for(auto v : test_alloc_tracker)
      {
        SLOP u((SLAB*)v);
        std::cerr << "unfreed " << v << " [+" << (I)u.layout()->header_get_slab_reference_count() << "]" << ": " << (u) << "\n";
      }
    }

  }
#endif
}


} // namespace
