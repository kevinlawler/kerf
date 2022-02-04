namespace KERF_NAMESPACE {

// SLOP general_unit_atomic_duplex_layout_combiner(decltype(&unit_addition) unit_pairwise_op, const SLOP &a, const SLOP &b) // void (*iterator)(SLOP&,SLOP&,SLOP&) 
// {
//   // NB. Factored with PRESENTED version
//   // Question. Are we ever going to use this version again now? Same q for uniplex
//   switch(TWO(a.is_unit(), b.is_unit()))
//   {
//     case TWO(true,  true):
//     {
//       return unit_pairwise_op(a, b);
//     }
//     case TWO(true,  false):
//     {
//       auto g = [&](const SLOP& x) -> SLOP {return general_unit_atomic_duplex_layout_combiner(unit_pairwise_op, a, x);};
//       return b.iterator_uniplex_layout_subslop_accumulator(g);
//     }
//     case TWO(false, true):
//     {
//       auto g = [&](const SLOP& x) -> SLOP {return general_unit_atomic_duplex_layout_combiner(unit_pairwise_op, x, b);};
//       return a.iterator_uniplex_layout_subslop_accumulator(g);
//     }
//     default:
//     case TWO(false, false):
//     {
//       auto g = [&](const SLOP& x, const SLOP& y) -> SLOP {return general_unit_atomic_duplex_layout_combiner(unit_pairwise_op, x, y);};
//       return a.iterator_duplex_layout_subslop_accumulator(g, b);
//     }
//   }
// }

SLOP general_unit_atomic_duplex_presented_combiner(decltype(&unit_addition) unit_pairwise_op, const SLOP &a, const SLOP &b) // void (*iterator)(SLOP&,SLOP&,SLOP&) 
{
  // NB. Factored with LAYOUT version
  // Observation. Our semantics are off. This has a,b semantics but the iterators it calls have this,rhs semantics.
  // NB. LAYOUT combiner not going to work on PRESENTED types correctly
  // NB. 0/n n/0 list length conformability. TODO Question. Do we want it to preserve list-type on empty lists or do we care [if we return untyped]? If we choose to do so, it might be easier to capture the returns here and pass them through a duplexer that coerces/copies untyped lists to typed.
  // Idea. Later it might make sense to do atomicity functions like this (or revise this) with the argument functions somehow specifying when they've hit bottom, or something like that. If we have a function optimized for INT3_ARRAY x INT3_ARRAY it should know somehow to stop before then. Perhaps we put such duplex switching in the operator/verb before falling back to functions like this to handle the loose cases. That seems like a superior idea.

  // P_O_P. different list combination produce different (faster/slower) combiners
  //       eg, vectors don't need to check for additional descents; nor for tape_heads and such
  // P_O_P. For fixed-width integer vectors you can just increment a pointer(accessor)/bitcount and update the same SLOP atom
  // P_O_P. Instead of appending [integer] atoms to an empty list, can also pre-allocated the integer vector and append C++-ints using the class accessor mechanism, bypassing the intermediate slop generation

  // NB. the two middle cases are equiv to mapright and mapleft, the final to mapdown
  switch(TWO(a.is_unit(), b.is_unit()))
  {
    case TWO(true,  true):
    {
      return unit_pairwise_op(a, b);
    }
    case TWO(true,  false):
    {
      auto g = [&](const SLOP& x) -> SLOP {return general_unit_atomic_duplex_presented_combiner(unit_pairwise_op, a, x);};
      return b.iterator_uniplex_presented_subslop_accumulator(g);
    }
    case TWO(false, true):
    {
      auto g = [&](const SLOP& x) -> SLOP {return general_unit_atomic_duplex_presented_combiner(unit_pairwise_op, x, b);};
      return a.iterator_uniplex_presented_subslop_accumulator(g);
    }
    default:
    case TWO(false, false):
    {
      auto g = [&](const SLOP& x, const SLOP& y) -> SLOP {return general_unit_atomic_duplex_presented_combiner(unit_pairwise_op, x, y);};
      return a.iterator_duplex_presented_subslop_accumulator(g, b);
    }
  }
}

// SLOP general_unit_atomic_uniplex_layout_combiner(decltype(&unit_negate) unit_monadic_op, const SLOP &a)
// {
//   if(a.is_unit()) return unit_monadic_op(a);
//   auto g = [&](const SLOP& x) -> SLOP {return general_unit_atomic_uniplex_layout_combiner(unit_monadic_op, x);};
//   return a.iterator_uniplex_layout_subslop_accumulator(g);
// }

SLOP general_unit_atomic_uniplex_presented_combiner(decltype(&unit_negate) unit_monadic_op, const SLOP &a)
{
  if(a.is_unit()) return unit_monadic_op(a);
  auto g = [&](const SLOP& x) -> SLOP {return general_unit_atomic_uniplex_presented_combiner(unit_monadic_op, x);};
  return a.iterator_uniplex_presented_subslop_accumulator(g);
}


SLOP range(const SLOP &x)
{
  if(I(x)<=0) return INT0_ARRAY;

  // Pass 4
  return SLOP::AFFINE_RANGE(x);

  SLOP a(UNTYPED_ARRAY);
  a.append(x);
  auto w = a.presented()->chunk_width_log_bytes();
  a.layout()->promote_or_expand_via_widths(w, -1 + (I)x);

  // Pass 3
  switch(w)
  {
    case 0: { I0 *p = (I0*)a.layout()->payload_pointer_begin(); DO(x, *p++ = i) ; break;}
    case 1: { I1 *p = (I1*)a.layout()->payload_pointer_begin(); DO(x, *p++ = i) ; break;}
    case 2: { I2 *p = (I2*)a.layout()->payload_pointer_begin(); DO(x, *p++ = i) ; break;}
    case 3: { I3 *p = (I3*)a.layout()->payload_pointer_begin(); DO(x, *p++ = i) ; break;}
  }
  a.layout()->header_add_n_chunk_width_to_byte_counter_unchecked(-1 + (I)x);
  return a;

  // Pass 2 - 10x slower than Pass 3 for TIME(range(1LL<<20));
  auto P = a.presented();
  auto L = a.layout();
  DO(x, P->integer_write_raw(L->payload_pointer_to_ith_chunk(i), i))
  L->header_add_n_chunk_width_to_byte_counter_unchecked(-1 + (I)x);
  return a;

  // Pass 1
  DO(x, a.append(i))
  return a;
}

SLOP unit_addition(const SLOP &a, const SLOP &b)
{
  // TODO In some cases, handling integer overflows and/or promotion to BIGINT

  assert(a.is_unit());
  assert(b.is_unit());

  PRESENTED_TYPE ap = a.layout()->header_get_presented_type();
  PRESENTED_TYPE bp = b.layout()->header_get_presented_type();

  switch(TWO(ap,bp))
  {
    // Remark. Code factored with unit_multiplication function
    // TODO we're missing even more valid temporal combinations, for instance {stamp,span}x{float,int} and {int,float}x{stamp,span}, and more.
    //      combos include valid conversions from nanos to micros `*1000` or `/1000` and many more.
    //      combos also include various promotions that could be triggered, eg, does adding a DAY stamp plus a SECONDS stamp or SECONDS span promote? 
    //      promotion should also occur on adding two spans of different type (-> yields SPAN_DATETIME_UNIT)
    //      SPAN_DATETIME_UNIT should be able to add with any differening type span, or itself, (-> yields SPAN_DATETIME_UNIT)
    //      Idea. a SPAN_DATETIME_UNIT which is zero except for one place could be condensed into a smaller typed span.
    //      Idea. perhaps we toss out all the [other] spans since we're not using span-arrays and just use SPAN_DATETIME_UNIT
    //      I dunno how this should all be handled, potentially on the temporal classes.
    //      provided we have a useful type ordering we can also sort a,b on types in order to reduce switch cases
    //      We need to review our remarks (on physical paper; in the readme) on limits on dates/times/etc
    //      Idea. We can also cheat by promoting SPANs to a wide SPAN_DATETIME_UNIT and then only implementing that, prior to optimizing
    case TWO(SPAN_NANOSECONDS_UNIT,  STAMP_DATETIME_UNIT    ):
    //////////////////////////////////////////////////////////
    case TWO(SPAN_YEAR_UNIT,         STAMP_YEAR_UNIT        ):
    case TWO(SPAN_MONTH_UNIT,        STAMP_MONTH_UNIT       ):
    case TWO(SPAN_DAY_UNIT,          STAMP_DAY_UNIT         ):
    case TWO(SPAN_HOUR_UNIT,         STAMP_HOUR_UNIT        ):
    case TWO(SPAN_MINUTE_UNIT,       STAMP_MINUTE_UNIT      ):
    case TWO(SPAN_SECONDS_UNIT,      STAMP_SECONDS_UNIT     ):
    case TWO(SPAN_MILLISECONDS_UNIT, STAMP_MILLISECONDS_UNIT):
    case TWO(SPAN_MICROSECONDS_UNIT, STAMP_MICROSECONDS_UNIT):
    case TWO(SPAN_NANOSECONDS_UNIT,  STAMP_NANOSECONDS_UNIT ):
    {
      // A further dodge:   [also reveals interesting left/right semantics]
      return SLOP(bp, a.presented()->I_cast() + b.presented()->I_cast());
    }

    case TWO(STAMP_DATETIME_UNIT,     SPAN_NANOSECONDS_UNIT ):
    //////////////////////////////////////////////////////////
    case TWO(STAMP_YEAR_UNIT,         SPAN_YEAR_UNIT        ):
    case TWO(STAMP_MONTH_UNIT,        SPAN_MONTH_UNIT       ):
    case TWO(STAMP_DAY_UNIT,          SPAN_DAY_UNIT         ):
    case TWO(STAMP_HOUR_UNIT,         SPAN_HOUR_UNIT        ):
    case TWO(STAMP_MINUTE_UNIT,       SPAN_MINUTE_UNIT      ):
    case TWO(STAMP_SECONDS_UNIT,      SPAN_SECONDS_UNIT     ):
    case TWO(STAMP_MILLISECONDS_UNIT, SPAN_MILLISECONDS_UNIT):
    case TWO(STAMP_MICROSECONDS_UNIT, SPAN_MICROSECONDS_UNIT):
    case TWO(STAMP_NANOSECONDS_UNIT,  SPAN_NANOSECONDS_UNIT ):
    //////////////////////////////////////////////////////////
    case TWO(SPAN_YEAR_UNIT,          SPAN_YEAR_UNIT        ):
    case TWO(SPAN_MONTH_UNIT,         SPAN_MONTH_UNIT       ):
    case TWO(SPAN_DAY_UNIT,           SPAN_DAY_UNIT         ):
    case TWO(SPAN_HOUR_UNIT,          SPAN_HOUR_UNIT        ):
    case TWO(SPAN_MINUTE_UNIT,        SPAN_MINUTE_UNIT      ):
    case TWO(SPAN_SECONDS_UNIT,       SPAN_SECONDS_UNIT     ):
    case TWO(SPAN_MILLISECONDS_UNIT,  SPAN_MILLISECONDS_UNIT):
    case TWO(SPAN_MICROSECONDS_UNIT,  SPAN_MICROSECONDS_UNIT):
    case TWO(SPAN_NANOSECONDS_UNIT,   SPAN_NANOSECONDS_UNIT ):
    // TODO: case TWO(STAMP_DATETIME_UNIT,  SPAN_DATETIME_UNIT ): // special big unit
    // TODO: case TWO( SPAN_DATETIME_UNIT, STAMP_DATETIME_UNIT ): // special big unit
    // TODO: case TWO( SPAN_DATETIME_UNIT,  SPAN_DATETIME_UNIT ): // special big unit
    {
      return SLOP(ap, a.presented()->I_cast() + b.presented()->I_cast());
    }
    case TWO(INT3_UNIT, INT3_UNIT):
      return SLOP(a.presented()->I_cast() + b.presented()->I_cast());
    case TWO(FLOAT3_UNIT, FLOAT3_UNIT):
      return SLOP(a.presented()->F_cast() + b.presented()->F_cast());
  ////////////////////////////////////////////////////
  // Question. Do we want some compiler flags here?
    case TWO(FLOAT3_UNIT, INT3_UNIT):
      return SLOP(a.presented()->F_cast() + b.presented()->I_cast());
    case TWO(INT3_UNIT, FLOAT3_UNIT):
      return SLOP(a.presented()->I_cast() + b.presented()->F_cast());
  ////////////////////////////////////////////////////
    default:
      // TODO Type Error
      return SLOP(NIL_UNIT);
  }

}

SLOP unit_multiplication(const SLOP &a, const SLOP &b)
{
  // TODO In some cases, handling integer overflows and/or promotion to BIGINT

  assert(a.is_unit());
  assert(b.is_unit());

  PRESENTED_TYPE ap = a.layout()->header_get_presented_type();
  PRESENTED_TYPE bp = b.layout()->header_get_presented_type();

  switch(TWO(ap,bp))
  {
    case TWO(INT3_UNIT, INT3_UNIT):
      return SLOP(a.presented()->I_cast() * b.presented()->I_cast());
    case TWO(FLOAT3_UNIT, FLOAT3_UNIT):
      return SLOP(a.presented()->F_cast() * b.presented()->F_cast());
  ////////////////////////////////////////////////////
  // Question. Do we want some compiler flags here?
    case TWO(FLOAT3_UNIT, INT3_UNIT):
      return SLOP(a.presented()->F_cast() * b.presented()->I_cast());
    case TWO(INT3_UNIT, FLOAT3_UNIT):
      return SLOP(a.presented()->I_cast() * b.presented()->F_cast());
  ////////////////////////////////////////////////////
    default:
      // TODO Type Error
      return SLOP(NIL_UNIT);
  }

}

SLOP unit_negate(const SLOP &a)
{
  PRESENTED_TYPE ap = a.layout()->header_get_presented_type();

  switch(ap)
  {
    case FLOAT3_UNIT:
      return SLOP(-a.presented()->F_cast());
    case INT3_UNIT:
      return SLOP(-a.presented()->I_cast());
    // case SPAN_DATETIME_UNIT: // TODO, big unit
    case SPAN_YEAR_UNIT ... SPAN_NANOSECONDS_UNIT:
    {
      return SLOP(ap, -a.presented()->I_cast());
    }
    default:
      // TODO Type Error
      return SLOP(NIL_UNIT);
  }
}

} // namespace
