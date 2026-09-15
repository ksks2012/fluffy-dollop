#pragma once

#include <string>
#include <vector>
#include "circuit.hpp"
#include "sandwich.hpp"

// ---------------------------------------------------------------------------
// PipelineConfig — controls which phases run and their parameters.
// All booleans default to true; set to false for isolated debugging.
// ---------------------------------------------------------------------------
struct PipelineConfig {
    int seed = 42;

    // Adder size
    int n              = 2;   // bits per operand (default: 2-bit adder)
    int extra_ancilla  = 4;   // extra zero-initialized wires appended after the
                               // adder's working carries; drive Safe sandwich and
                               // junk injection pool derivation

    // Phase 1 — sandwich
    bool enable_sandwich             = true;
    // Sandwich mode defaults to Safe: Active is known to NOT guarantee
    // truth-table / ancilla equivalence (ADR 0002 correction; ticket 0024).
    SandwichMode sandwich_mode       = SandwichMode::Safe;
    int  sandwich_chain_length       = 8;

    // Phase 2 — local replacement (Sound + R1 + optional R2)
    bool enable_replace       = true;
    bool enable_sound         = false;  // Sound T-c pass (Pass 0, default off)
    bool enable_replace_r2    = false;  // R2 carry-majority pass (default off)

    // Phase 3 — shuffle
    bool enable_shuffle       = true;

    // Phase 4 — junk injection
    bool enable_inject        = true;
    int  inject_count         = 12;
};

// ---------------------------------------------------------------------------
// PhaseStats — recorded after each phase boundary
// ---------------------------------------------------------------------------
struct PhaseStats {
    std::string name;
    int  gate_count;
    int  wire_count;
    double overhead;       // gate_count / original_gate_count (adder's gate count)
    bool ancilla_ok;       // all non-output ancilla wires = 0 for all inputs
    int  replacements = 0; // total windows replaced (Sound + R1 + R2)
    int  replacements_sound = 0; // Sound (T-c) replacements
    int  replacements_r1 = 0; // R1 (S-triple) replacements
    int  replacements_r2 = 0; // R2 (carry-majority) replacements
    int  depth_levels = 0; // max topological depth (ticket 0022)
    int  count_cnot = 0;    // number of CnotGate in the circuit
    int  count_toffoli = 0; // number of ToffoliGate
    int  count_not = 0;     // number of NotGate (reserved; always 0 today)
};

// ---------------------------------------------------------------------------
// PipelineResult — returned by run_full_pipeline()
// Circuit is move-only (non-copyable).
// ---------------------------------------------------------------------------
struct PipelineResult {
    Circuit circuit;
    std::vector<PhaseStats> phases;

    PipelineResult(Circuit&& c, std::vector<PhaseStats>&& p)
        : circuit(std::move(c)), phases(std::move(p)) {}

    PipelineResult(const PipelineResult&)            = delete;
    PipelineResult& operator=(const PipelineResult&) = delete;
    PipelineResult(PipelineResult&&)                 = default;
    PipelineResult& operator=(PipelineResult&&)      = default;
};

// ---------------------------------------------------------------------------
// run_full_pipeline — executes phases in order and records stats.
//
// Phase order: adder (0) → sandwich (1) → replace (2) → shuffle (3) → inject (4)
// replace runs BEFORE shuffle: shuffle interleaves unrelated same-depth gates
// between the S-triple windows (which sit at different depths), destroying the
// strict adjacency the R1 match requires. See ADR 0004 order amendment.
// Disabled phases are skipped; their stats entry is omitted.
// ---------------------------------------------------------------------------
PipelineResult run_full_pipeline(const PipelineConfig& cfg = {});

// ---------------------------------------------------------------------------
// check_truth_table / check_ancilla_invariant — shared verification helpers
// ---------------------------------------------------------------------------
bool check_truth_table(Circuit& c);
bool check_ancilla_invariant(Circuit& c);
