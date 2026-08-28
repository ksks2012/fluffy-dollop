#pragma once

#include <random>
#include <vector>
#include "circuit.hpp"

// ---------------------------------------------------------------------------
// inject_junk_gates — insert no-op Toffoli gates to inflate circuit volume
//
// Junk gate invariant (ticket 0006):
//   Every injected Toffoli has at least one control wire in `junk_ancilla`.
//   All wires in `junk_ancilla` are zero-initialized and never written by any
//   existing gate, so every junk gate evaluates as identity.
//
// All three operands (ctrl1, ctrl2, target) are drawn from `junk_ancilla`.
// Because ctrl1 ∈ junk_ancilla = 0 always, the Toffoli never fires.
//
// Topological validity: since junk wires are disjoint from all existing gate
// wire sets, any insertion position respects the existing dependency ordering.
// ---------------------------------------------------------------------------
void inject_junk_gates(Circuit& circuit,
                       const std::vector<int>& junk_ancilla,
                       int count,
                       std::mt19937& rng);
