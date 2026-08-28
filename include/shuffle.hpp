#pragma once

#include <random>
#include "circuit.hpp"

// ---------------------------------------------------------------------------
// shuffle_depth_levels — randomly permute gates within each depth level
//
// Algorithm:
//   1. Compute depth levels for every gate.
//   2. Group gate indices by depth (ascending).
//   3. Shuffle each group independently using `rng`.
//   4. Concatenate groups (depth ascending) and reorder the gate list.
//
// Correctness invariants after shuffle:
//   - Same gate count.
//   - Same multiset of depth values (sanity-checked by recomputing depths).
//   - Same-depth gates remain independent → any permutation is safe.
//   - Functional equivalence: truth table unchanged for all inputs.
// ---------------------------------------------------------------------------
void shuffle_depth_levels(Circuit& circuit, std::mt19937& rng);
