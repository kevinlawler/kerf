namespace KERF_NAMESPACE {
struct ITERATOR_LAYOUT
{
  // Question. Can we HACK this to support the END thing?

  // Question/TODO: Is it better to put this on SLOP (or perhaps LAYOUT) as SLOP::ITERATOR_LAYOUT or LAYOUT:ITERATOR_LAYOUT as a dependent name? Are there any advantages to this? (We did run in to some trouble originally writing this, in a non-dependent fashion, in getting the includes right, see attached remarks.) Such a nested class did not increase the class size of the parent in my tests. 

  const SLOP& parent;

  SLOP slop; 

  CHUNK_TYPE chunk_type;

  V first_item;

  I pos = -1;
  I& i = pos;
  I known_item_count = -1;
  
  SLAB** child_ptrptr;

  SLAB* child_ptr;
  I observed_width_aligned = -1;
  bool known_regular_stride = true;
  
  bool should_rlink_descend = true; // we're using this for reading RLINK from the drive without dereferencing null pointers POP if this slows things down we should split it out
  bool should_memory_mapped_descend = true;

  // TODO this [the regular formulation without the hack, currently] doesn't work [off-by-one] because of end() semantics ... we need to mod to fit. go ahead and do lazy operator* at the same time. TIP we might want to do a "full" class rewrite, using the original as a reference. then just rename (swap the class names) to make it work
  // HACK
  bool is_end_hack = false;

  void ITERATOR_LAYOUT_HELPER(const SLOP& parent, bool last = false);
  ITERATOR_LAYOUT() : parent(slop) {}; // sic, garbage
  ITERATOR_LAYOUT(const SLOP& parent, bool last = false, bool should_rlink_descend = true, bool should_memory_mapped_descend = true);

  ~ITERATOR_LAYOUT()
  {
    // NB. 2021.09.18 This method seemed necessary to solve some bug where the iterator slop was still looking at a freed SLAB and messing with it. I dunno if this is the only or the right way to do it but it worked.
    // slop.era se_for_reuse(); // Warning: I don't think you can do this, because it's the unadulterated destructor that causes problems
    slop.neutralize();
  }

  virtual void sideways(I count = 1);
  void next(I count = 1);
  void previous(I count = 1);
  bool has_next(bool reverse = false);
  bool has_previous();
  void seek_last();
  void seek_first();

  bool over_rlink_reference();
  SLAB** rlink_address();

  bool operator!=(void* rhs) // bool operator!=(const ITERATOR_LAYOUT& rhs)
  {
    return !is_end_hack;
    return this->has_next();
  }
 
  virtual SLOP& operator*()
  {
    // P_O_P/TODO actually not "instantiating"/loading/pre-loading the iterator values until this operator* method is called is likely to be more performant and useful
    return slop;
  }
 
  auto operator++()
  {
    next();
    return *this;
  }
 

};

struct ITERATOR_PRESENTED : ITERATOR_LAYOUT
{
  // POP: I just slapped this together, there are probably lots of missed opportunities
  // in construction as well as speed optimization.
  // one is subclassing on presented objects, possibly this is done at `virtual PRESENTED_BASE::begin()` or possibly you wedge it into ITERATOR_PRESENTED
  // POP for instance, if AFFINE boxes/unboxes too much, we can have it iterate over INT3s in many cases, but beware this is not possible in every case (for instance, when BASE or ADD or IMULT are vectors). simply check the types/vals first (the most common case will be length n with base 0 and mult 1).

  ITERATOR_PRESENTED() : ITERATOR_LAYOUT() { };
  ITERATOR_PRESENTED(const SLOP& parent, bool last = false);

  void sideways(I count = 1);
  SLOP& operator*();
};

auto LAYOUT_BASE::begin() { return ITERATOR_LAYOUT(*parent()); }
auto LAYOUT_BASE::end() { return nullptr; } // return ITERATOR_LAYOUT();

auto PRESENTED_BASE::begin() { return ITERATOR_PRESENTED(*parent()); }
auto PRESENTED_BASE::end() {return nullptr;}

auto SLOP::begin() const { return presented()->begin(); }
auto SLOP::end() const { return presented()->end(); }

// NB. This pair of methods is necessary so we can do `for(auto x : slop.layout())` vs. `layout_ref()`
auto begin(LAYOUT_BASE* L) { return L->begin(); }
auto end(LAYOUT_BASE* L) { return L->end(); }
// NB. This pair of methods is necessary so we can do `for(auto x : slop.presented())` vs. `presented_ref()`
auto begin(PRESENTED_BASE* P) { return P->begin(); }
auto end(PRESENTED_BASE* P) { return P->end(); }

} // namespace
