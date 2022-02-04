#pragma once
namespace KERF_NAMESPACE {

I PAGE_SIZE_BYTES = sysconf(_SC_PAGE_SIZE);

struct MEMORY_POOL
{
  void* pool_alloc_struct(I bytes_requested)
  {
    assert(POOL_LANE_STRUCT_MIN >= LOG_SLAB_WIDTH);

    void* v = pool_alloc_with_min_lane(bytes_requested, POOL_LANE_STRUCT_MIN);

#if TEST_TRACK_ALLOCATIONS
  if(test_alloc_tracker.contains(v)) {
    die(Test alloc tracker somehow already had this memory pointer element.)
  }
  test_alloc_tracker.insert(v);
#endif

    return v;
  }

  void pool_dealloc(SLAB* z);

#if TEST_TRACK_ALLOCATIONS
  std::set<void*> test_alloc_tracker;
#endif

protected:
  virtual void* pool_depool(char lane) = 0;
  virtual void* pool_anonymous_system_memory(int64_t requested_bytes, bool shared = 0) = 0; //share with forked children?
  virtual void pool_repool(void* v, char lane) = 0;

  char min_lane_for_bytes(I bytes_requested, char min_lane)
  {
    char lane = ceiling_log_2(bytes_requested);
    lane = MAX(lane, min_lane);
    return lane;
  }

  void* pool_alloc_with_min_lane(I bytes_requested, char min_lane)
  {
    void *z;
    char lane = min_lane_for_bytes(bytes_requested, min_lane);
    z = pool_depool(lane);

    assert(min_lane >= LOG_SLAB_WIDTH);
    char refs = 0;
    *(SLAB*)z = (SLAB){.m_memory_expansion_size=(UC)lane, .reference_management_arena=REFERENCE_MANAGEMENT_ARENA_CPP_WORKSTACK, .r_slab_reference_count=(UC)refs};

    return z;
  }

public:
  virtual ~MEMORY_POOL();

};

struct THREAD_SAFE_MALLOC_POOL : MEMORY_POOL
{
protected:
  virtual void* pool_depool(char lane)
  {
    return malloc(POW2(lane));
  }

  virtual void pool_repool(void* v, char lane)
  {
    free(v);
  }

  virtual void* pool_anonymous_system_memory(int64_t requested_bytes, bool shared = 0)
  {
    if(shared) {std::cerr << "cannot share malloc memory\n"; kerf_exit(-1);}
    return malloc(requested_bytes);
  }
};

struct THREAD_UNSAFE_MMAP_LANE_SLAB_POOL : MEMORY_POOL
{
  // POTENTIAL_OPTIMIZATION_POINT: a similar class which has lanes by BOTH {slab_width} X {object-type}
  // TODO see break.c for a long list of useful assert()'s we should implement

  // TODO have a #define for the minmum number of bytes we'll allocate via mmap ANON for slicing up for pool structs or for returning normally. This needs to be "high" relative to sysctl vm.max_map_count ~ 655300. On kerf1 we subdivided 1 page, the mmap minimum, and never actually ran into this problem.
};



}
