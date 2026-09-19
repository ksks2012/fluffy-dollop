#pragma once

#include <string>
#include <vector>
#include <random>
#include "circuit.hpp"
#include "sandwich.hpp"

// ---------------------------------------------------------------------------
// PipelineConfig — controls which phases run and their parameters.
// All booleans default to true; set to false for isolated debugging.
// ---------------------------------------------------------------------------
struct PipelineConfig {
    int seed = 42;
    // ADR 0007: keyed PRNG seeding. Empty key → seed; non-empty key →
    // seed_seq(key bytes + "local-mixing-v1") → mt19937. Pass enablement and
    // parameters are NOT driven by the key (key only seeds the rng stream).
    std::string key;

    // Adder size
    int n              = 2;   // bits per operand (default: 2-bit adder)
    int extra_ancilla  = 4;   // extra zero-initialized wires appended after the
                               // adder's working carries; drive Safe sandwich and
                               // junk injection pool derivation

    // ADR 0008 §Config surface: multi-round mixing. rounds == number of full
    // (sandwich → replace → shuffle) mixing shakes. rounds == 1 → identical to
    // today's single-pass output. Orthogonal to key/seed (ADR 0007/0008 §Q6):
    // key only seeds the rng; rounds only repeats the chain.
    int  rounds = 1;

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
    // ADR 0008 §PhaseStats: per-round attribution. adder→0; per-round phases→
    // their round's index; inject→rounds-1. Names carry NO round suffix.
    int  round_index = 0;
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
// make_rng — ADR 0007 keyed PRNG construction. Single seam for the rng stream.
// Precedence: !cfg.key.empty() → seed_seq(key bytes + "local-mixing-v1") →
// mt19937; otherwise mt19937(cfg.seed) (exactly today's behavior). Returns a
// fresh engine; the caller passes it by reference through every rng consumer.
// ---------------------------------------------------------------------------
std::mt19937 make_rng(const PipelineConfig& cfg);

// ---------------------------------------------------------------------------
// check_truth_table / check_ancilla_invariant — shared verification helpers
// ---------------------------------------------------------------------------
bool check_truth_table(Circuit& c);
bool check_ancilla_invariant(Circuit& c);
