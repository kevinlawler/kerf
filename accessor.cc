#pragma once
#include "kerf.h"
namespace KERF_NAMESPACE {

UC ACCESSOR_MECHANISM_BASE::minimal_int_log_width_needed_to_store_magnitude(I i)
{
  UC needed = 0;

  switch(i)
  {
    case  II:
    case -II:
    case  IN:
        #if INT_INFS_AND_NANS_SQUASH
          needed = MIN(INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS, 3); // smaller types can have their own infs and nans
        #else
          needed = 3;
        #endif
      break;
// NB. We can have collisions on say "-II0" from the larger-widthed int1, naturally occuring as -127. A true -II0 would've come in as -II3.
    case  II2:
    case -II2:
    case  IN2:
        #if 2 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
          needed = 3;
        #else
          needed = 2;
        #endif
      break;
    case  II1:
    case -II1:
    case  IN1:
        #if 1 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
          needed = 2;
        #else
          needed = 1;
        #endif
      break;
    case  II0:
    case -II0:
    case  IN0:
        #if 0 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
          needed = 1;
        #else
          needed = 0;
        #endif
      break;

    // P_O_P: case ranges, in this fashion. These don't seem to expand the binary size appreciably, sooo...
    case (-II0 + 1) ... (II0 - 1): needed = 0; break;
    case (-II1 + 1) ... (IN0 - 1): needed = 1; break;
    case ( II0 + 1) ... (II1 - 1): needed = 1; break;
    case (-II2 + 1) ... (IN1 - 1): needed = 2; break;
    case ( II1 + 1) ... (II2 - 1): needed = 2; break;
    // case (-II3 + 1) ... (IN2 - 1): needed = 3; break; // too big for switch
    // case ( II2 + 1) ... (II3 - 1): needed = 3; break; // too big for switch
    // 0 amended into two ranges based on selections below
    // -1 signed [-8,7], unsigned [0,15]: both of these are useful. unsigned math won't fit in signed parent.
    // -2 *unsigned [0,3], signed [-2,1]: unsigned math won't fit in signed parent. signed_-2 notably contains [-1,0,1] as a subsequence, which is useful.
    // -3 *unsigned [0,1], signed [-1,0]: unsigned math is representable in either choice of signed/unsigned parent (-2)
    // you could also leave out -2 if you plan to do only signed -1, then there's no promotion issues
    // Question. Is there a bitop (multiplication?) that can promote these?

    // See:  https://lists.llvm.org/pipermail/cfe-dev/2020-December/067404.html  [cfe-dev] feature request: allowed duplicated case value in switch statement attribute, including ranges

    default:

      // for case range thing, we handled everything else
      needed = 3;
      break;

      // P_O_P: not sure if this re-assignment trick is faster or the below, previous, or the case range thing
      int32_t y2 = (int64_t)i;
      int16_t y1 = (int64_t)i;
      int8_t  y0 = (int64_t)i;
      needed = (!!(y0 != i)) + (!!(y1 != i)) + (!!(y2 != i));
      break;

      // P_O_P: ceiling_log_2 can yield some early returns (bounded approximations) I think, even for signed values. This function looks pretty good already tho, so it might be tough to gain something there (even with a count-leading-zeroes instruction) if you have to add a switch statement.

      // P_O_P: you could reframe this as subtraction (3-x) to flop the comparisons, then use the trick for doing one comparison to test presence in a boundary (a<y<b). this should eliminate half the comparisons.

      needed = 0 //sic, fallthrough
        // NB. the defines are no longer needed since we handled them higher up in the switch
        // #if 2 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
        //      + (!!(i <= -II2)) + (!!(i >=  II2))
        // #else
             + (!!(i <  -IN2)) + (!!(i >   II2))
        // #endif 

        // #if 1 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
        //      + (!!(i <= -II1)) + (!!(i >=  II1))
        // #else
             + (!!(i <  -IN1)) + (!!(i >   II1))
        // #endif

        // #if 0 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
        //      + (!!(i <= -II0)) + (!!(i >=  II0))
        // #else
             + (!!(i <  -IN0)) + (!!(i >   II0))
        // #endif
      ;

      break;
  }

  assert(0 <= needed && needed <= 3);

  return needed;
}

UC ACCESSOR_MECHANISM_BASE::minimal_float_log_width_needed_to_store_magnitude(F x)
{
  UC needed = 3;

  F2 y = (F3) x;
  F1 z = (F3) x;

  // x==x is nan test
  needed = 1 + (!!(y != x && x == x)) + (!!(z != x && x == x));

  // another test is
  // float dest = numeric_cast<float>(source);
  // double residual = source - numeric_cast<double>(dest);

  assert(1 <= needed && needed <= 3);

  return needed;
}

I ACCESSOR_MECHANISM_BASE::prepare_int_for_narrowing_write(I i, UC width, UC precomputed)
{     
  // i is int3, width is new width
  switch(i)
  {

#if INT_INFS_AND_NANS_SQUASH
    case  II:
      switch(width)
      {
        #if 2 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
          case 2: return II2;
        #if 1 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
          case 1: return II1;
        #if 0 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
          case 0: return II0;
        #endif
        #endif
        #endif
      }
      [[fallthrough]];
    case -II:
      switch(width)
      {
        #if 2 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
          case 2: return -II2;
        #if 1 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
          case 1: return -II1;
        #if 0 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
          case 0: return -II0;
        #endif
        #endif
        #endif
      }
      [[fallthrough]];
    case  IN:
      switch(width)
      {
        #if 2 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
          case 2: return IN2;
        #if 1 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
          case 1: return IN1;
        #if 0 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
          case 0: return IN0;
        #endif
        #endif
        #endif
      }
      [[fallthrough]];
#endif

    default:
#if   INT_CAPPED_APPEND_STYLE == CAPPED_APPEND_C_ASSIGN
      static_assert(INT_CAPPED_APPEND_STYLE == CAPPED_APPEND_C_ASSIGN, "had a bug where we thought c enums would work in #if");
      //no-op
#elif INT_CAPPED_APPEND_STYLE == CAPPED_APPEND_CLIPS
      switch(width)
      {
        default:
        case 3:
          return i;
        case 2:
          #if INT_CAPPED_APPEND_CLIPS_TO_NEG_INF
            return MIN(MAX(-II2, i), II2);
          #endif
            assert(INT32_MIN == IN2 && IN2 < -II2);
            return MIN(MAX(INT32_MIN, i), INT32_MAX);
        case 1:
          #if INT_CAPPED_APPEND_CLIPS_TO_NEG_INF
            return MIN(MAX(-II1, i), II1);
          #endif
            assert(INT16_MIN == IN1 && IN1 < -II1);
            return MIN(MAX(INT16_MIN, i), INT16_MAX);
        case 0:
          #if INT_CAPPED_APPEND_CLIPS_TO_NEG_INF
            return MIN(MAX(-II0, i), II0);
          #endif
            assert(INT8_MIN == IN0 && IN0 < -II0);
            return MIN(MAX(INT8_MIN, i),  INT8_MAX);
      }
#elif INT_CAPPED_APPEND_STYLE == CAPPED_APPEND_REJECTS
      // POTENTIAL_OPTIMIZATION_POINT it may be faster to recompute everytime, particularly if we optimize the bitops there
      UC needed = ((precomputed != UCHAR_MAX) ? precomputed : minimal_int_log_width_needed_to_store_magnitude(i));

      if (needed > width)
      {
        ERROR(ERROR_CAPPED_APPEND);
      }
#endif
      return i;
  }
  return i;
}

I ACCESSOR_MECHANISM_BASE::prepare_int_for_widening_read(I i, UC width)
{     
  // i is int{0,1,2,3} [as assigned to I/int3], width is old width, new width is 3

  // POTENTIAL_OPTIMIZATION_POINT I think this jump table is fine, should compile with constant func args, but if you
  // instead switch on i and then width, that could be better if you have a lot of non-constant width calls

#if INT_INFS_AND_NANS_UNSQUASH
  switch(width)
  {
      default:
      case 3: 
        return i;
    #if 2 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
      case 2:
        switch(i)
        {
          case  II2: return  II3;
          case -II2: return -II3;
          case  IN2: return  IN3;
        }
        break;
    #if 1 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
      case 1:
        switch(i)
        {
          case  II1: return  II3;
          case -II1: return -II3;
          case  IN1: return  IN3;
        }
        break;
    #if 0 >= INT_MINIMAL_WIDTH_FOR_INFS_AND_NANS
      case 0:
        switch(i)
        {
          case  II0: return  II3;
          case -II0: return -II3;
          case  IN0: return  IN3;
        }
        break;
    #endif
    #endif
    #endif

  }

#endif

  return i;
}

SLAB* ACCESSOR_MECHANISM_BASE::slab_pointer_access(V pointer)
{
  return nullptr;
}

SLOP ACCESSOR_MECHANISM_BASE::slop_access(V pointer)
{
  return SLOP(slab_pointer_access(pointer));
}

}
