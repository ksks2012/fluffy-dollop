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
    for (int bits = 0; bits < 32; ++bits) {
        int A1=(bits>>4)&1, A0=(bits>>3)&1;
        int B1=(bits>>2)&1, B0=(bits>>1)&1, Cin=bits&1;
        int sum = (A1*2+A0) + (B1*2+B0) + Cin;
        auto out = c.evaluate({A1, A0, B1, B0, Cin});
        if (out[0] != ((sum>>1)&1) || out[1] != (sum&1) || out[2] != ((sum>>2)&1))
            return false;
    }
    return true;
}

bool check_ancilla_invariant(Circuit& c) {
    const auto& outs = c.output_wires();
    for (int bits = 0; bits < 32; ++bits) {
        int A1=(bits>>4)&1, A0=(bits>>3)&1;
        int B1=(bits>>2)&1, B0=(bits>>1)&1, Cin=bits&1;
        c.evaluate({A1, A0, B1, B0, Cin});
        const auto& ws = c.last_wires();
        for (int w = c.num_inputs(); w < c.num_wires(); ++w) {
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
    static constexpr int ORIGINAL_GATES = 15;  // canonical 2-bit adder

    Circuit circuit = make_2bit_adder();
    std::vector<PhaseStats> phases;
    std::mt19937 rng(cfg.seed);

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
