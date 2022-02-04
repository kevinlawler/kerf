namespace KERF_NAMESPACE {
SLOP unit_negate(const SLOP &a);
SLOP unit_addition(const SLOP &a, const SLOP &b);
SLOP unit_multiplication(const SLOP &a, const SLOP &b);

// SLOP general_unit_atomic_uniplex_layout_combiner(decltype(&unit_negate) unit_monadic_op, const SLOP &a);
SLOP general_unit_atomic_uniplex_presented_combiner(decltype(&unit_negate) unit_monadic_op, const SLOP &a);
// SLOP general_unit_atomic_duplex_layout_combiner(decltype(&unit_addition) unit_pairwise_op, const SLOP &a, const SLOP &b);
SLOP general_unit_atomic_duplex_presented_combiner(decltype(&unit_addition) unit_pairwise_op, const SLOP &a, const SLOP &b);

SLOP range(const SLOP &x);
}
