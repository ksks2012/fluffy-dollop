#pragma once

#include <random>
#include "circuit.hpp"

// ---------------------------------------------------------------------------
// ReplaceResult — returned by apply_local_replace, holds per-pattern hits.
// ---------------------------------------------------------------------------
struct ReplaceResult {
    int r1_hits = 0;   // R1 (S-triple) replacements
    int r2_hits = 0;   // R2 (carry-majority 3-Toffoli) replacements
    int total() const { return r1_hits + r2_hits; }
};

// ---------------------------------------------------------------------------
// LocalReplaceConfig — phase configuration for local replacement.
// ---------------------------------------------------------------------------
struct LocalReplaceConfig {
    bool enable_r2 = false;   // enable R2 carry-majority pass (default off)
};

// ---------------------------------------------------------------------------
// apply_local_replace — two-pass, in-place local replacement (R1 + R2).
//
// Pass 1 (R1): S-triple (3x CNOT, same target s, distinct controls a,b,c)
//   → Toffoli(a,b,s); CNOT(a,s); CNOT(b,s); CNOT(c,s); Toffoli(a,b,s)
//   Net: s ⊕= a⊕b⊕c; adds 2 Toffoli per hit; 0 ancilla wires.
//
// Pass 2 (R2): carry-majority 3-Toffoli (same target T, 3 distinct controls)
//   → CNOT(a,z); Toffoli(b,z,T); Toffoli(ci,z,T); Toffoli(b,ci,T); CNOT(a,z)
//   Net: T ⊕= maj(a,b,ci); adds 2 CNOT per hit; borrows z (restored).
//   z = first extra-ancilla wire (index >= 4n+1), selected once at pass start.
//   If no extra-ancilla wire exists, R2 reports zero hits.
//
// Both passes are single-pass, non-overlapping. R1 runs fully first, then R2.
// ---------------------------------------------------------------------------
ReplaceResult apply_local_replace(Circuit& circuit, const LocalReplaceConfig& cfg, std::mt19937& rng);