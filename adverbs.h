namespace KERF_NAMESPACE {

SLOP accum( )
{
  // m: n or 1v  (arity/adicity 1 verb)
  // d: 2v or 2n? (can f[x,y] func of two args be a 2n and work? what did kerf1 do?)

  // m'n  -> ok
  // d'n  -> err
  // d/:n -> ok
  // nd'n -> ok

  // lambda-monadic: void, each/mapright, fold-repeater, unfold-repeater
  // lambda-dyadic: mapdown, mapright, mapleft, mapback, fold-dyadic, unfold-dyadic
  // NB: lambda-dyadic-{mapleft,mapright} are lambda-monadic-each with 1 bound arg (left or right respectively)
  // triplex etc iterators... we can generalize with an index function or something. may not occur in c++ land but in kerfvm land.
  // the ones we need right now are: void, each/mapright, mapdown, maybe map{left,right}
  // null arg: don't accumulate, non-null SLOP ref arg: accumulate in this slop; alternatively, pass flag and return value

  // monadic: builds: void each, dyadic arg: over scan mapback
  // dyadic:  each eachleft eachright [over scan mapback]

  // the lambda is the capture into the list??

  // Idea: cpp_each_mapright, cpp_fold, ... 

  return SLOP();
}


#pragma mark - 

template<typename L>
SLOP fold(L &lambda, const SLOP& a)
{
  // lambda ex: auto g = [&](const SLOP& x, const SLOP& y) -> SLOP {return x+y;};
  int i = false;
  SLOP sink = a;

  for(auto& x : a)
  {
    switch(i)
    {
      case false:
        sink = x;
        i = true;
        break;
      default:
        sink = lambda(sink, x);
        break;
    }
  }

  return sink;
}

template<typename L>
SLOP fold(L &lambda, const SLOP& a, const SLOP& b)
{
  // return lambda(b, fold(lambda, a)); // Question: I'd like to do this but it does yield ooo execution. Does it matter? Answer. For lambdas modifying global/local state [ie, general kerf lambdas {[x]x}], yes, for fast lambdas like `+`, no. So you could use it there and benefit from reusing optimized versions of `sum` and not rewriting weird ones to also sum the b arg.

  // lambda ex: auto g = [&](const SLOP& x, const SLOP& y) -> SLOP {return x+y;};
  SLOP sink = b;

  for(auto& x : a)
  {
    sink = lambda(sink, x);
  }

  return sink;
}

#pragma mark - 

template<typename L>
SLOP unfold(L &lambda, const SLOP& a)
{
  // lambda ex: auto g = [&](const SLOP& x, const SLOP& y) -> SLOP {return x+y;};
  int i = false;
  SLOP accum = UNTYPED_ARRAY;
  SLOP sink = a;
  accum.append(a);

  for(auto& x : a)
  {
    switch(i)
    {
      case false:
        sink = x;
        accum = UNTYPED_ARRAY;
        accum.append(sink);
        i = true;
        break;
      default:
        sink = lambda(sink, x);
        accum.append(sink);
        break;
    }
  }

  return accum;
}

template<typename L>
SLOP unfold(L &lambda, const SLOP& a, const SLOP& b)
{
  // lambda ex: auto g = [&](const SLOP& x, const SLOP& y) -> SLOP {return x+y;};
  SLOP accum = UNTYPED_ARRAY;
  SLOP sink = b;

  accum.append(sink);

  for(auto& x : a)
  {
    sink = lambda(sink, x);
    accum.append(sink);
  }

  return accum;
}

#pragma mark - 

template<typename L>
SLOP mapdown(L &lambda, const SLOP& a)
{
  return a.iterator_uniplex_presented_subslop_accumulator(lambda);
}

template<typename L>
SLOP mapdown(L &lambda, const SLOP& a, const SLOP& b)
{
  return a.iterator_duplex_presented_subslop_accumulator(lambda, b);
}

#pragma mark - 

template<typename L>
SLOP mapleft(L &lambda, const SLOP& a, const SLOP& b)
{
  // iterate left fix right
  auto g = [&](const SLOP& x) -> SLOP {return lambda(x, b);};
  return a.iterator_uniplex_presented_subslop_accumulator(g);
}

template<typename L>
SLOP mapright(L &lambda, const SLOP& a, const SLOP& b)
{
  // fix left iterate right
  auto g = [&](const SLOP& x) -> SLOP {return lambda(a, x);};
  return b.iterator_uniplex_presented_subslop_accumulator(g);
}

#pragma mark - 

template<typename L>
SLOP mapback(L &lambda, const SLOP& a)
{
  int i = false;
  SLOP accum = a;
  SLOP before = NIL_UNIT;

  for(auto& x : a)
  {
    switch(i)
    {
      case false:
        accum = UNTYPED_ARRAY;
        before = x;
        accum.append(before);
        i = true;
        break;
      default:
        accum.append(lambda(x, before));
        before = x;
        break;
    }
  }

  return accum;
}

template<typename L>
SLOP mapback(L &lambda, const SLOP& a, const SLOP& b)
{
  int i = false;
  SLOP accum = a;
  SLOP before = b;

  for(auto& x : a)
  {
    switch(i)
    {
      case false:
        accum = UNTYPED_ARRAY;
        i = true;
        [[fallthrough]];
      default:
        accum.append(lambda(x, before));
        before = x;
        break;
    }
  }

  return accum;
}

#pragma mark - 

template<typename L>
SLOP mapcores(L &&lambda, const SLOP& a)
{
  // POP: thread pool with condition signalling instead of spawning new threads every time
  // POP: use non-kerf threads (plain threads un-kerf-initialized) when sub-threads do not need to make use of kerfvm, etc.
  // POP: you can try "interleaved" version. in kerf1 "segmented" was always faster  (thread1: indexes 0 1, thread2: 2 3; versus [0 2, 1 3] with interleaved) 

  // Observation. If we had a "SUBSEQUENCE" object (a refcounted reference to another object then perhaps a list of interval-coordinate-pairs or single-integer-indexes, maybe with repeats) it would be trivial to specify these operations

  ////////////////////////////////////////////////////////////////////////////////////////////
  LAYOUT_BASE &r = *a.layout();
  bool cc = r.has_constant_time_algo_count();
  bool ci = r.has_constant_time_algo_index();
  if(!cc || !ci)
  {
    // POP: LIST-like case. you can parallelize this by first doing an O(n) pass and saving segment jump-off points (separate iterators) for the threads. (because you can't trivially split the LIST into segments without iterating through it.) May/may not need to do special iterator copies to retain parent tracking. Idea/POP. Another approach is to have thread0 begin processessing from item 0, while thread1 begins *iterating* but not processing from item 0 until it hits item k (segments of length k) and then begins processing *after* handing off the iterator to thread2, which begins iterating to item 2*k, then begins processing after handing off the iterator to thread3... This is easy to implement with a thread pool.
    // POP In some cases it may be cheaper to convert the [small] LIST into a regular array (!) and work on that
    // return a.it3r4t0r_unipl3x_l4y0ut_subslop(lambda);

    // Remark. This is actually an even more complicated rabbit hole than you think. Let's leave it for now because we can handle it gracefully with the normal method. However, other considerations:
    // • constant time algo is not a sufficient test. it's OK to have linear time IF the lists are small enough. you want this for simple parallelizing on LISTS and on FOLIOS with small sub-LISTs
    // • PRESENTED needs its own constant_time? methods [to do a correct PRESENTED-style here instead of LAYOUT-style] [so the above code is wrong/incomplete], but as above, this is not sufficient. On FOLIO you'd return early if any sublist was false and return false. On MAP, you'd farm out to key/value list returns. etc.
    // • possibly we'd want to disallow LISTs on FOLIO, though I don't think so
    // Idea. What you need a category of attribute methods, such as "has_efficient_indexing" which is a *heuristic* based attribute. O(1) indexers are considered efficient [there's a layout member call for this piece], but so are O(n) indexers such as unjumped LISTs which are of small length, for instance less than a define EFFICIENT_LINEAR_INDEX_UPPER_BOUND defined as say 1000. In some instance you can calculate this quickly, for instance [i'm making this up], no need to recurse if you know a parent list is only 8000 bytes long [and flat] and has 1000 members. Note that `known_flat` includes LIST [O(n)] so is not the same.
  }
  ////////////////////////////////////////////////////////////////////////////////////////////

  I m = a.countI();

  I k = MAX(1, !!m + (m - !!m)/The_Cores_Count); // segment width; use m/Cores integer ceiling rounds up
  // TODO we don't want [3,3,3,3,3,2,0,0] for 17/8. you can spread it out without much work. use regular int division (it rounds down), find+store the mod remainder r, then every segment less-than the mod is incremented by 1. you get a simple analytic method for converting it.  (i < r)*(k+1) + (i >= r)*k

  I threads_needed = MIN(m, The_Cores_Count);

  // POP. Idea. (Instead of allocing a pool each time, we can have a global pool, either per-thread or singleton, which has two different outcomes from the outcome we have now.) We can use a pool like this [not only for our socket polling executions, but] to more-cleanly implement mapcores AND to have the option of executing mapcores "sequentially" across threads (choose: each thread gets its own mapcores pool, or they all share one pool. This is a good programmatic toggle to have. The difference is whether all mapcores subthreads fight, across invocations, or whether each invocation subthread must wait on a core to be "mapcores free" to process its segment. Obviously this ignores that other threads not described here may be contending for the cores.) 
  // Remark/Warning note if you actually get threads to bind (tightly) to cores [which you can't currently accomplish on macos], then you MAY want an option on the thread pool that avoids the main thread/main core [technically, any thread/core owning an automatic-duration pool] and/or disables tight binding and/or etc., eg down below, if the main thread is thread0 on core0, then whenever 0 is equiv thread_i mod cores, that thread will never execute! in this model, assuming tight binding.

  // Concern. There's an issue I think with mapcores where if you're asking other threads to return a value to the main thread, especially if you're doing this in a loop, you might start "starving/depleting" their mempools and "fattening" the main thread's mempool (assume small allocations which aren't immediately de-alloced). This is kind of like a leak. There must be many ways to solve this. One would be to track thread_id origins inside of FOLIO metadata and to make an attempt to return objects to their providing thread mempool. Another would be to have a global pool with locks, you can even toggle into [using it] or toggle out of it with threads (this likely also needs a bit in every header in order to redirect these things back to the multithread pool). You could also partially prevent this problem by using PAGE sized "accum" values below and then munmapping them when freed (or like, using a memory attribute, Idea. maybe a flag on the thread to use the minimum of a PAGE for the duration) .
  // Idea. Per-thread flag: "switch to special locked multithreading pool for the duration". Then you can free... how? If you set a bit on the attributes section. You need to redirect it back to the multithreading pool. This would solve the mapcores problem.

  // Question. Do we need to recurse through the returned object to make sure it's OK in terms of arenas/references? Answer. I don't think so. 


  THREAD_POOL pool(threads_needed);

  std::future<SLOP> async[threads_needed];

  for (I i = 0; i < threads_needed; i++) 
  {
    async[i] = pool.add([i,k,m,&a,&lambda]()->SLOP {
      SLOP accum = UNTYPED_ARRAY;
      I segment_start = i*k;
      I segment_end = MIN(segment_start + k, m);
      for(I j = segment_start; j < segment_end; j++)
      {
        // POP a[j] is linear on some LISTs, see above p.o.p. discussion
        accum.append(lambda(a[j]));
      }
      // Bug TODO I think we want to do accum.coerce_parented(); here, but we're getting workstack errors, we need a better function that will remove the thread from its mempool tracker, maybe something like coerce_crossthread, but then the receiving thread needs to track it as well, so maybe it gets a capture, or we have a receiver func that's like "origin_thread_id dest_thread_id" and it handles the removes.
      // ./memory-pool.cc:10: Test alloc tracker attempted to repool a memory address element we did not have tracked in the set.
      return accum; 
    });
  }

  SLOP folio(FOLIO_ARRAY);

  for (I i = 0; i < threads_needed; i++) 
  {
    auto g = async[i].get();

    // TODO error checking
    // if(is error g) goto fail)
    // TODO I don't think you should fail early? cause it's going to leave w/e is running in the background at the time as a leak. error != ctrl-c. You can save something like that as a POP

    // ( TODO. change mapcores adverb to use KERF_POOL, b̶u̶t̶ ̶f̶i̶r̶s̶t̶ ̶d̶o̶ ̶s̶e̶t̶j̶m̶p̶ ̶w̶r̶a̶p̶p̶e̶r̶ ̶t̶o̶ ̶K̶E̶R̶F̶_̶T̶H̶R̶E̶A̶D̶ ̶i̶n̶i̶t̶ ̶t̶h̶i̶n̶g̶ ̶(̶s̶l̶i̶g̶h̶t̶l̶y̶ ̶t̶r̶i̶c̶k̶y̶)̶ Reminder. Adverb should fail gracefully if one thread throws an error. H̶a̶v̶e̶ ̶i̶t̶s̶ ̶s̶e̶t̶j̶m̶p̶ ̶c̶o̶d̶e̶ ̶o̶c̶c̶u̶r̶ ̶B̶E̶T̶W̶E̶E̶N̶ ̶w̶h̶e̶n̶ ̶i̶t̶ ̶s̶t̶a̶r̶t̶s̶ ̶a̶n̶d̶ ̶a̶f̶t̶e̶r̶ ̶w̶h̶e̶n̶ ̶i̶t̶ ̶b̶u̶b̶b̶l̶e̶s̶ ̶t̶h̶e̶ ̶v̶a̶l̶u̶e̶/̶e̶r̶r̶o̶r̶/̶w̶h̶a̶t̶e̶v̶e̶r̶ ̶r̶e̶m̶a̶i̶n̶s̶ ̶o̶n̶ ̶t̶h̶e̶ ̶s̶t̶a̶c̶k̶ ̶u̶p̶ ̶t̶o̶ ̶t̶h̶e̶ ̶c̶a̶l̶l̶i̶n̶g̶ ̶t̶h̶r̶e̶a̶d̶.̶ in the calling thread detect the presence of an error and return that and do cleanup. TODO. Idea. Fix cross-thread threadpool returns to manage the slab so that the thing goes from parented/unamanaged to cppstack[/parented] in the receiver, so that all memory is automatically managed like this. [we need a special function/method that ports the objects over] (`void *retval; threads[i].join(&retval); SLAB *s = (SLAB*)retval; SLOP t(s); f.join(t);`) Idea. Can we just trivially return a SLOP as a future/promise (ie is it already managed correctly by the shared future)? ) Remark. Bug. S̶e̶e̶m̶s̶ ̶l̶i̶k̶e̶ ̶w̶e̶'̶r̶e̶ ̶g̶e̶t̶t̶i̶n̶g̶ ̶a̶w̶a̶y̶ ̶w̶i̶t̶h̶ ̶i̶t̶. We don't get away with it. try mapcores on list of size 300. consider the previous remark about automating this in the thread return. Remember also that if we return a struct to a separate memory pool it's going to complain until we turn that off/globalize the tracker.
    // Idea. FOLIO flag where all vecs are big enough to sidestep pool. This solves mapcores sink issue. You don't even need big enough to sidestep pool. Just big enough to be unmapped/released without having say fragmented atoms inside it (ie no pool subdivisions) - and then I suppose memory pool methods to support such. This may need recursive implementation for making slabs conform, like a slop coerce method. (MEMORY_MAPPED slabs would be exempt I guess.) Alternative. You also get this if you never subdivide below a page, then you can always free the page (ie, atoms are PAGE or greater). 
  
    folio.join(g);
  }

succeed:
  return folio;
fail:
  // TODO: return error
  return folio;
}

}
