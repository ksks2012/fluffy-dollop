#pragma once

#include <random>
#include <vector>
#include "circuit.hpp"

// ---------------------------------------------------------------------------
// SandwichMode — selects the wire-selection strategy used when building the
// CNOT chain:
//
//   Safe   — chain wires are drawn exclusively from extra_ancilla (all
//             zero-initialized), so every CNOT in the chain is a no-op and
//             functional equivalence is guaranteed by construction.
//
//   Active — (reserved for ticket 0009) chain wires are drawn from a mix of
//             ancilla and live circuit wires; cancellation of the forward /
//             reversed chain preserves the overall truth-table.
// ---------------------------------------------------------------------------
enum class SandwichMode { Safe, Active };

// ---------------------------------------------------------------------------
// apply_sandwich — CNOT chain sandwich around the entire circuit
//
// Wire budget contract (from ticket 0004):
//   - Wires 0–8 already belong to the adder; this function must not touch them.
//   - extra_ancilla: list of wire indices (≥9) used exclusively for the chain.
//     All must be zero-initialized and never written by the existing circuit.
//
// Operation:
//   1. Extends circuit's wire count to cover max(extra_ancilla)+1.
//   2. Generates a random CNOT chain of length `chain_len` on extra_ancilla wires.
//   3. Prepends the forward chain  [chain[0], chain[1], …, chain[N-1]] before gate 0.
//   4. Appends  the reversed chain [chain[N-1], …, chain[0]] after the last gate.
//
// Why functional equivalence is preserved:
//   All extra_ancilla wires start at 0, so every CNOT(ctrl=0, tgt) is a no-op.
//   The adder (wires 0–8) is sandwiched in-place and never overlaps the chain wires.
//   After the adder, extra_ancilla wires are still 0, so the reversed chain is also a no-op.
// ---------------------------------------------------------------------------
void apply_sandwich(Circuit& circuit,
                    const std::vector<int>& extra_ancilla,
                    int chain_len,
                    SandwichMode mode,
                    std::mt19937& rng);
