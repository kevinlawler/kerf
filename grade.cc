namespace KERF_NAMESPACE {
auto INT_COMPARE(I a, I b)
{
  return a <=> b;
}

auto FLOAT_COMPARE(F a, F b)
{
  F E=0.00000000000000000001; //also suggested: small multiple of DBL_EPSILON

  auto EQUAL = std::weak_ordering::equivalent;
  auto ASCENDING = std::weak_ordering::less;
  auto DESCENDING = std::weak_ordering::greater;

  if(FLOAT_NANS_COMPARE_AS_SMALLEST)
  {
    if(isnan(a))
    {
      if(isnan(b)) return EQUAL;
      return ASCENDING;
    }
    else if(isnan(b))
    {
      return DESCENDING;
    }
  }
  
  if(isinf(a))
  {
    if(isinf(b))
    {
      return ((a<0 && b<0) || (a>0 && b>0))?EQUAL:(a<0 && b>0)?ASCENDING:DESCENDING;
    }
    return a<0?ASCENDING:DESCENDING;
  }
  else if(isinf(b))
  {
    return b>0?ASCENDING:DESCENDING;
  }

  if(ABS(a-b) <= E*MAX(ABS(a),ABS(b)))return EQUAL;
  return a<b?ASCENDING:DESCENDING;
}

std::weak_ordering PRESENTED_BASE::compare(const SLOP& x)
{
  // TODO skip deleted keys on maps

  bool ua = parent()->is_fixed_width_unit();
  bool ub = x.is_fixed_width_unit();

  if(ua && ub)
  {
    PRESENTED_TYPE pa = layout()->header_get_presented_type();
    PRESENTED_TYPE pb = x.layout()->header_get_presented_type();

    switch(TWO(pa,pb))
    {
      case TWO(NIL_UNIT, NIL_UNIT):
        return std::weak_ordering::equivalent;
      case TWO(CHAR3_UNIT, CHAR3_UNIT): [[fallthrough]];
      case TWO(INT3_UNIT, INT3_UNIT):
        return INT_COMPARE(I_cast(), x.presented()->I_cast());
    #if NUMERIC_COMPARE_INT_FLOAT
      case TWO(  INT3_UNIT, FLOAT3_UNIT): [[fallthrough]];
      case TWO(FLOAT3_UNIT,   INT3_UNIT): [[fallthrough]];
    #endif
      case TWO(FLOAT3_UNIT, FLOAT3_UNIT):
        return FLOAT_COMPARE(F_cast(), x.presented()->F_cast());
    }

    return pa <=> pb;
  }
  else if(!ua && !ub)
  {
    // TODO Bigint - see remarks at A_BIGINT_UNIT::hash_code
    // TODO ?? SORT_ORDER_STRINGS_LEXICOGRAPHIC

    // char SC(S a, S b)//strcmp unfortunately does not draw from {-1,0,1}
    // {
    //   int x=strcmp(a,b);
    //   R x<0?ASCENDING:x>0?DESCENDING:EQUAL;
    // }
    // 
    // char lexicographic(K x, K y)
    // {
    //   I n = MIN(xn, yn); 
    // 
    //   //POTENTIAL_OPTIMIZATION_POINT
    //   /[>maybe compressing this into one pass is faster?
    //   /[>maybe find some workaround to avoid toupper function calls
    //   DO(n, C a = toupper(xC[i]), b = toupper(yC[i]); if(a!=b) return IC((UC)a,(UC)b);)
    // 
    //   I on_length = IC(xn, yn);
    //   if(on_length) return on_length;
    // 
    //   DO(n, C a = xC[i], b = yC[i]; if(a!=b) return IC((UC)a,(UC)b);)
    // 
    //   return EQUAL;
    // }

    auto cc = parent()->countI() <=> x.countI();
    if(std::is_neq(cc)) return cc;

    // Question. So all empty lists are equal then, regardless of type? Answer. Yes, because representational type

    auto c = std::weak_ordering::equivalent;
    bool early_break_flag = false;

    auto g = [&](const SLOP& a, const SLOP& b) { c = a.compare(b); early_break_flag = std::is_neq(c);};
    parent()->iterator_duplex_presented_subslop(g, x, &early_break_flag);

    return c;
  }

  return (!ua) <=> (!ub); 
  return std::weak_ordering::equivalent;
}

#pragma mark -

} // namspace
