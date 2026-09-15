#include "pipeline.hpp"
#include "adder.hpp"
#include "sandwich.hpp"
#include "shuffle.hpp"
#include "inject_junk.hpp"
#include "local_replace.hpp"
#include "gate.h"
#include <algorithm>
#include <random>

// ---------------------------------------------------------------------------
// Verification helpers
// ---------------------------------------------------------------------------

bool check_truth_table(Circuit& c) {
    int n_in = c.num_inputs();
    int n    = (n_in - 1) / 2;          // bits per operand: num_inputs = 2n+1
    int combos = 1 << n_in;             // 2^(2n+1)
    const auto& out_idx = c.output_wires();  // [S_{n-1},..,S_0, Cout]

    for (int bits = 0; bits < combos; ++bits) {
        // Build input vector: wire[j] = bit (n_in-1-j) of `bits`
        std::vector<int> input(n_in);
        for (int j = 0; j < n_in; ++j)
            input[j] = (bits >> (n_in - 1 - j)) & 1;

        // Compute A, B, Cin from input
        int A = 0, B = 0;
        for (int k = 0; k < n; ++k) {
            A = (A << 1) | input[k];
            B = (B << 1) | input[n + k];
        }
        int Cin = input[2*n];
        int sum = A + B + Cin;

        auto out = c.evaluate(input);

        // out[k] = S_{n-1-k} for k=0..n-1, out[n] = Cout
        for (int k = 0; k < n; ++k)
            if (out[k] != ((sum >> (n-1-k)) & 1)) return false;
        if (out[n] != ((sum >> n) & 1)) return false;
    }
    return true;
}

bool check_ancilla_invariant(Circuit& c) {
    int n_in  = c.num_inputs();
    int combos = 1 << n_in;
    const auto& outs = c.output_wires();

    for (int bits = 0; bits < combos; ++bits) {
        std::vector<int> input(n_in);
        for (int j = 0; j < n_in; ++j)
            input[j] = (bits >> (n_in - 1 - j)) & 1;

        c.evaluate(input);
        const auto& ws = c.last_wires();
        for (int w = n_in; w < c.num_wires(); ++w) {
            bool is_out = std::find(outs.begin(), outs.end(), w) != outs.end();
            if (!is_out && ws[w] != 0) return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// measure_structural — per-phase structural metrics (ticket 0022), computed at
// the same timing as check_ancilla_invariant (immediately after each phase).
// ---------------------------------------------------------------------------
struct PhaseMetrics {
    int depth_levels;
    int count_cnot;
    int count_toffoli;
    int count_not;
};

static PhaseMetrics measure_structural(const Circuit& c) {
    const auto& gs = c.gates_ref();
    int cn = 0, ct = 0, nc = 0;
    for (const auto& g : gs) {
        if (dynamic_cast<const CnotGate*>(g.get()))        ++cn;
        else if (dynamic_cast<const ToffoliGate*>(g.get())) ++ct;
        else if (dynamic_cast<const NotGate*>(g.get()))    ++nc;
    }
    const auto depths = c.compute_depth_levels();
    int maxd = depths.empty() ? 0 : *std::max_element(depths.begin(), depths.end());
    return {maxd, cn, ct, nc};
}

// Build a PhaseStats snapshot for the circuit's current state.
static PhaseStats make_phase_stats(const std::string& name, const Circuit& c,
                                   int original_gates, bool ancilla_ok,
                                   int replacements, int replacements_sound,
                                   int replacements_r1,
                                   int replacements_r2) {
    PhaseMetrics m = measure_structural(c);
    PhaseStats s;
    s.name            = name;
    s.gate_count      = c.gate_count();
    s.wire_count      = c.num_wires();
    s.overhead        = static_cast<double>(c.gate_count()) / original_gates;
    s.ancilla_ok      = ancilla_ok;
    s.replacements    = replacements;
    s.replacements_sound = replacements_sound;
    s.replacements_r1 = replacements_r1;
    s.replacements_r2 = replacements_r2;
    s.depth_levels    = m.depth_levels;
    s.count_cnot      = m.count_cnot;
    s.count_toffoli   = m.count_toffoli;
    s.count_not       = m.count_not;
    return s;
}

// ---------------------------------------------------------------------------
// derive_free_ancilla — wires that are: not a primary input, not a primary
// output, and never the target of any existing gate.  These are safe to use
// as the Safe-mode sandwich ancilla pool (CNOT chains on them are no-ops
// because they start at 0 and the adder never writes them).
// ---------------------------------------------------------------------------
static std::vector<int> derive_free_ancilla(const Circuit& c) {
    const auto& outs  = c.output_wires();
    const auto& gates = c.gates_ref();
    std::vector<int> free_wires;
    for (int w = c.num_inputs(); w < c.num_wires(); ++w) {
        bool is_out = std::find(outs.begin(), outs.end(), w) != outs.end();
        if (is_out) continue;
        bool is_target = false;
        for (const auto& g : gates)
            if (g->target_wire() == w) { is_target = true; break; }
        if (!is_target) free_wires.push_back(w);
    }
    return free_wires;
}

// ---------------------------------------------------------------------------
// run_full_pipeline
// ---------------------------------------------------------------------------

PipelineResult run_full_pipeline(const PipelineConfig& cfg) {
    // Build the adder for the configured bit-width.
    Circuit circuit = make_nbit_adder(cfg.n, cfg.extra_ancilla);
    std::vector<PhaseStats> phases;
    std::mt19937 rng(cfg.seed);
    const int ORIGINAL_GATES = circuit.gate_count();  // derived from factory

    // Derive Safe-mode sandwich ancilla: extra zero wires never written by the
    // adder's gates (= the extra_ancilla range [4n+1 .. 4n+extra_ancilla]).
    const std::vector<int> safe_ancilla = derive_free_ancilla(circuit);

    // Derive inject-junk pool: 3 wires immediately beyond the current circuit
    // (inject_junk calls extend_wires internally to cover them).
    const int inject_base = circuit.num_wires();  // 4n+1+extra_ancilla
    const std::vector<int> inject_pool = {inject_base, inject_base+1, inject_base+2};

    // Phase 0 — adder (always runs)
    phases.push_back(make_phase_stats(
        "adder", circuit, ORIGINAL_GATES, check_ancilla_invariant(circuit),
        0, 0, 0, 0));

    // Phase 1 — sandwich
    if (cfg.enable_sandwich) {
        apply_sandwich(circuit, safe_ancilla, cfg.sandwich_chain_length,
                       cfg.sandwich_mode, rng);
        phases.push_back(make_phase_stats(
            "sandwich", circuit, ORIGINAL_GATES, check_ancilla_invariant(circuit),
            0, 0, 0, 0));
    }

    // Phase 2 — local replacement (R1 + optional R2): single-pass, fixed rewrite.
    // Runs BEFORE shuffle: shuffle reorders gates within each depth level, and
    // the three S-CNOTs of an R1 window are serially dependent (all write s,
    // so they sit at different depths), so shuffle would interleave unrelated
    // same-depth gates between them and destroy the strict adjacency the match
    // relies on. Replacing while the structure is still tight keeps hits == n.
    if (cfg.enable_replace) {
        LocalReplaceConfig replace_cfg;
        replace_cfg.enable_sound = cfg.enable_sound;
        replace_cfg.enable_r2    = cfg.enable_replace_r2;
        ReplaceResult rr = apply_local_replace(circuit, replace_cfg, rng);
        phases.push_back(make_phase_stats(
            "replace", circuit, ORIGINAL_GATES, check_ancilla_invariant(circuit),
            rr.total(), rr.sound_hits, rr.r1_hits, rr.r2_hits));
    }

    // Phase 3 — shuffle
    if (cfg.enable_shuffle) {
        shuffle_depth_levels(circuit, rng);
        phases.push_back(make_phase_stats(
            "shuffle", circuit, ORIGINAL_GATES, check_ancilla_invariant(circuit),
            0, 0, 0, 0));
    }

    // Phase 4 — inject junk
    if (cfg.enable_inject) {
        inject_junk_gates(circuit, inject_pool, cfg.inject_count, rng);
        phases.push_back(make_phase_stats(
            "inject", circuit, ORIGINAL_GATES, check_ancilla_invariant(circuit),
                0, 0, 0, 0));
    }

    return PipelineResult(std::move(circuit), std::move(phases));
}
