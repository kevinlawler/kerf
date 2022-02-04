namespace KERF_NAMESPACE {
#pragma mark - "" Operators

SLOP operator "" _y(unsigned long long i)
{
  return SLOP(SPAN_YEAR_UNIT, (I)i);
}

SLOP operator "" _Y(unsigned long long i)
{
  return SLOP(STAMP_YEAR_UNIT, (I)i);
}

} // namespace

