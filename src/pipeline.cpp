#include "pipeline.hpp"
#include "adder.hpp"
#include "sandwich.hpp"
#include "shuffle.hpp"
#include "inject_junk.hpp"
#include "local_replace.hpp"
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
    phases.push_back({
        "adder",
        circuit.gate_count(),
        circuit.num_wires(),
        1.0,
        check_ancilla_invariant(circuit)
    });

    // Phase 1 — sandwich
    if (cfg.enable_sandwich) {
        apply_sandwich(circuit, safe_ancilla, cfg.sandwich_chain_length,
                       cfg.sandwich_mode, rng);
        phases.push_back({
            "sandwich",
            circuit.gate_count(),
            circuit.num_wires(),
            static_cast<double>(circuit.gate_count()) / ORIGINAL_GATES,
            check_ancilla_invariant(circuit)
        });
    }

    // Phase 2 — local replacement (R1): single-pass, fixed rewrite.
    // Runs BEFORE shuffle: shuffle reorders gates within each depth level, and
    // the three S-CNOTs of an R1 window are serially dependent (all write s,
    // so they sit at different depths), so shuffle would interleave unrelated
    // same-depth gates between them and destroy the strict adjacency the match
    // relies on. Replacing while the structure is still tight keeps hits == n.
    if (cfg.enable_replace) {
        LocalReplaceConfig replace_cfg;
        ReplaceResult rr = apply_local_replace(circuit, replace_cfg, rng);
        phases.push_back({
            "replace",
            circuit.gate_count(),
            circuit.num_wires(),
            static_cast<double>(circuit.gate_count()) / ORIGINAL_GATES,
            check_ancilla_invariant(circuit),
            rr.total()
        });
    }

    // Phase 3 — shuffle
    if (cfg.enable_shuffle) {
        shuffle_depth_levels(circuit, rng);
        phases.push_back({
            "shuffle",
            circuit.gate_count(),
            circuit.num_wires(),
            static_cast<double>(circuit.gate_count()) / ORIGINAL_GATES,
            check_ancilla_invariant(circuit)
        });
    }

    // Phase 4 — inject junk
    if (cfg.enable_inject) {
        inject_junk_gates(circuit, inject_pool, cfg.inject_count, rng);
        phases.push_back({
            "inject",
            circuit.gate_count(),
            circuit.num_wires(),
            static_cast<double>(circuit.gate_count()) / ORIGINAL_GATES,
            check_ancilla_invariant(circuit)
        });
    }

    return PipelineResult(std::move(circuit), std::move(phases));
}
