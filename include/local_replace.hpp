#pragma once

#include <random>
#include "circuit.hpp"

// ---------------------------------------------------------------------------
// LocalReplaceConfig — phase configuration for local replacement.
// Reserved for second-version knobs (replace_max, multi-round, identity
// drawing).  v1 keeps this minimal: the single R1 pattern always replaces
// every matched window exactly once, so the config currently carries nothing.
// ---------------------------------------------------------------------------
struct LocalReplaceConfig {
};

// ---------------------------------------------------------------------------
// apply_local_replace — single-pass, in-place local replacement (R1, ticket 0017)
//
// Scans the circuit's topological gate sequence for the R1 window pattern and
// replaces each match in place with the R1 equivalent sub-circuit:
//
//   Window  : CNOT(a,s); CNOT(b,s); CNOT(c,s)       (3 strictly-adjacent CNOTs)
//             - same target wire s, pairwise-distinct controls a,b,c
//   Replace : Toffoli(a,b,s); CNOT(a,s); CNOT(b,s); CNOT(c,s); Toffoli(a,b,s)
//
// Net effect is `s ⊕= a ⊕ b ⊕ c` for both writings (the two Toffoli contribute
// `a∧b` and cancel), so the window's external interface is unchanged.
//
// Scanning is single-pass and non-overlapping: after a replacement, scanning
// resumes *after* the new sub-circuit so no matched span is rescanned.
//
// v1 uses the fixed R1 rewrite only — `rng` is retained solely to keep the
// signature uniform with the other pipeline phases and is not used.
//
// Returns the number of replacements made (hit count).
// ---------------------------------------------------------------------------
int apply_local_replace(Circuit& circuit, const LocalReplaceConfig& cfg, std::mt19937& rng);