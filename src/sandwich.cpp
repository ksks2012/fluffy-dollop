#include "sandwich.hpp"
#include "gate.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>

void apply_sandwich(Circuit& circuit,
                    const std::vector<int>& extra_ancilla,
                    int chain_len,
                    SandwichMode mode,
                    std::mt19937& rng) {
    if (chain_len < 1)
        throw std::invalid_argument("apply_sandwich: chain_len must be >= 1");

    // ---------------------------------------------------------------------------
    // Build ctrl_pool and tgt_pool for CNOT pair selection.
    //
    // Safe mode  — both pools = extra_ancilla (all zero-initialized, adder never
    //              touches them).  Every chain gate evaluates as a no-op, so
    //              functional equivalence is trivially preserved.
    //
    // Active mode — ctrl drawn from input wires [0..num_inputs-1] (the adder only
    //               READS these; they are never written by any gate).
    //               tgt  drawn from output wires (the circuit's output_wires set;
    //               they start at 0 on every evaluate() call and are write-only
    //               from the adder's perspective).
    //
    //   Why F⁻¹ ∘ Adder ∘ F = Adder on the 32 valid inputs:
    //     • At F time every output wire is 0, so F XORs ctrl_value into tgt.
    //     • The adder XORs its own contribution into tgt.
    //     • ctrl is an input wire — the adder never modifies it.
    //     • At F⁻¹ time ctrl still holds the original input value, so the XOR
    //       applied by F is exactly cancelled → net tgt = adder contribution. ✓
    //     • Output wires are disjoint from input wires, so ctrl != tgt always.
    //   Genuine scrambling: during execution tgt wires briefly carry
    //   input_value XOR adder_partial_result — real intermediate mixing.
    // ---------------------------------------------------------------------------

    std::vector<int> ctrl_pool, tgt_pool_global;

    if (mode == SandwichMode::Safe) {
        if (extra_ancilla.size() < 2)
            throw std::invalid_argument("apply_sandwich: need >= 2 extra ancilla wires");

        // Extend circuit to cover all extra_ancilla wire indices
        int max_wire = *std::max_element(extra_ancilla.begin(), extra_ancilla.end());
        circuit.extend_wires(max_wire + 1);

        ctrl_pool      = extra_ancilla;
        tgt_pool_global = extra_ancilla;   // picked separately after excluding ctrl

    } else {
        // Active mode: ctrl = input wires, tgt = output wires.
        // Defensive guard for future small circuits (never triggered for 2-bit adder).
        int n_in = circuit.num_inputs();
        const auto& out_wires = circuit.output_wires();
        if (n_in < 1 || out_wires.empty())
            throw std::invalid_argument(
                "apply_sandwich: Active mode needs >= 1 input wire and >= 1 output wire");

        ctrl_pool.resize(n_in);
        std::iota(ctrl_pool.begin(), ctrl_pool.end(), 0);
        tgt_pool_global.assign(out_wires.begin(), out_wires.end());
    }

    // ---------------------------------------------------------------------------
    // Generate the forward chain: chain_len random (ctrl, tgt) pairs.
    //
    // Safe mode:  ctrl and tgt both drawn from extra_ancilla, excluding ctrl.
    // Active mode: ctrl drawn from ctrl_pool (inputs), tgt from tgt_pool_global
    //              (outputs). The pools are disjoint, so ctrl != tgt always.
    // ---------------------------------------------------------------------------
    std::uniform_int_distribution<int> ctrl_dist(0, static_cast<int>(ctrl_pool.size()) - 1);

    std::vector<std::pair<int, int>> chain;
    chain.reserve(chain_len);

    for (int i = 0; i < chain_len; ++i) {
        int ctrl = ctrl_pool[ctrl_dist(rng)];

        // Build tgt pool: tgt_pool_global minus ctrl (handles Safe mode's shared pool;
        // for Active mode ctrl_pool and tgt_pool_global are disjoint so ctrl is never
        // in tgt_pool_global and no filtering is needed — the erase_if is a no-op).
        std::vector<int> tgt_candidates;
        tgt_candidates.reserve(tgt_pool_global.size());
        for (int w : tgt_pool_global)
            if (w != ctrl) tgt_candidates.push_back(w);

        int tgt = tgt_candidates[
            std::uniform_int_distribution<int>(0, static_cast<int>(tgt_candidates.size()) - 1)(rng)];
        chain.push_back({ctrl, tgt});
    }

    // ---------------------------------------------------------------------------
    // Prepend forward chain [chain[0], …, chain[N-1]] before gate index 0.
    // Inserting backwards at position 0 achieves forward order.
    // ---------------------------------------------------------------------------
    for (int i = chain_len - 1; i >= 0; --i)
        circuit.insert_gate(0, std::make_unique<CnotGate>(chain[i].first, chain[i].second));

    // Append reversed chain [chain[N-1], …, chain[0]] after the last gate.
    for (int i = chain_len - 1; i >= 0; --i)
        circuit.add_gate(std::make_unique<CnotGate>(chain[i].first, chain[i].second));
}
