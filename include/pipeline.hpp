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
    SandwichMode sandwich_mode       = SandwichMode::Active;
    int  sandwich_chain_length       = 8;

    // Phase 2 — shuffle
    bool enable_shuffle       = true;

    // Phase 3 — junk injection
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
    bool ancilla_ok;       // all non-output ancilla wires = 0 for all 32 inputs
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
// Phase order: adder (0) → sandwich (1) → shuffle (2) → inject (3)
// Disabled phases are skipped; their stats entry is omitted.
// ---------------------------------------------------------------------------
PipelineResult run_full_pipeline(const PipelineConfig& cfg = {});

// ---------------------------------------------------------------------------
// check_truth_table / check_ancilla_invariant — shared verification helpers
// ---------------------------------------------------------------------------
bool check_truth_table(Circuit& c);
bool check_ancilla_invariant(Circuit& c);
