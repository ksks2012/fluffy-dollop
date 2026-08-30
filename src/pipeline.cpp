#include "pipeline.hpp"
#include "adder.hpp"
#include "sandwich.hpp"
#include "shuffle.hpp"
#include "inject_junk.hpp"
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
// run_full_pipeline
// ---------------------------------------------------------------------------

PipelineResult run_full_pipeline(const PipelineConfig& cfg) {
    Circuit circuit = make_2bit_adder();
    std::vector<PhaseStats> phases;
    std::mt19937 rng(cfg.seed);
    const int ORIGINAL_GATES = circuit.gate_count();  // derived from factory

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
        apply_sandwich(circuit, cfg.sandwich_ancilla, cfg.sandwich_chain_length,
                       cfg.sandwich_mode, rng);
        phases.push_back({
            "sandwich",
            circuit.gate_count(),
            circuit.num_wires(),
            static_cast<double>(circuit.gate_count()) / ORIGINAL_GATES,
            check_ancilla_invariant(circuit)
        });
    }

    // Phase 2 — shuffle
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

    // Phase 3 — inject junk
    if (cfg.enable_inject) {
        inject_junk_gates(circuit, cfg.inject_ancilla, cfg.inject_count, rng);
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
