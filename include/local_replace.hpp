#pragma once

#include <random>
#include "circuit.hpp"

// ---------------------------------------------------------------------------
// ReplaceResult — returned by apply_local_replace, holds per-pattern hits.
// ---------------------------------------------------------------------------
struct ReplaceResult {
    int sound_hits = 0;  // Sound (T-c) replacements
    int r1_hits = 0;   // R1 (S-triple) replacements
    int r2_hits = 0;   // R2 (carry-majority 3-Toffoli) replacements
    int total() const { return sound_hits + r1_hits + r2_hits; }
};

// ---------------------------------------------------------------------------
// LocalReplaceConfig — phase configuration for local replacement.
// ---------------------------------------------------------------------------
struct LocalReplaceConfig {
    bool enable_sound = false;   // enable Sound T-c pass (Pass 0, default off)
    bool enable_r2 = false;   // enable R2 carry-majority pass (default off)
};

// ---------------------------------------------------------------------------
// apply_local_replace — multi-pass, in-place local replacement (Sound + R1 + R2).
//
// Pass 0 (Sound, if enable_sound): S-triple (3x CNOT, same target s, distinct
//   controls a,b,c) → CNOT(a,z); CNOT(z,s); CNOT(a,z); CNOT(b,s); CNOT(c,s)
//   Net: s ⊕= a⊕b⊕c; borrows z (extra-ancilla wire), restored by window end.
//   z = first extra-ancilla wire (index >= 4n+1), selected once at pass start.
//   If no extra-ancilla wire exists, Sound reports zero hits.
//
// Pass 1 (R1): S-triple → Toffoli(a,b,s); CNOT(a,s); CNOT(b,s); CNOT(c,s);
//   Toffoli(a,b,s).  Net: s ⊕= a⊕b⊕c; adds 2 Toffoli per hit; 0 ancilla.
//
// Pass 2 (R2, if enable_r2): carry-majority 3-Toffoli (same target T, 3 distinct
//   controls) → CNOT(a,z); Toffoli(b,z,T); Toffoli(ci,z,T); Toffoli(b,ci,T);
//   CNOT(a,z).  Net: T ⊕= maj(a,b,ci); adds 2 CNOT per hit; borrows z (restored).
//   z = first extra-ancilla wire (index >= 4n+1), selected once at pass start.
//   If no extra-ancilla wire exists, R2 reports zero hits.
//
// Passes run Sound→R1→R2, each single-pass/non-overlapping. rng is unused.
// ---------------------------------------------------------------------------
ReplaceResult apply_local_replace(Circuit& circuit, const LocalReplaceConfig& cfg, std::mt19937& rng);