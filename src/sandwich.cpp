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
    // Active mode — pool = all current circuit wires [0..num_wires()-1].
    //               ctrl and tgt are drawn from this single pool with ctrl != tgt.
    //               Does NOT call extend_wires.
    //               See docs/adr/0002-active-sandwiching-mode.md for the correctness
    //               argument: F⁻¹ ∘ Adder ∘ F cancels the chain's contribution at
    //               every phase boundary for the supported circuit family.
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
        // Active mode: pool is ALL current circuit wires [0..num_wires()-1].
        // Does NOT call extend_wires.
        // Defensive guard for future small circuits (never triggered for the 2-bit adder).
        int n = circuit.num_wires();
        if (n < 2)
            throw std::invalid_argument(
                "apply_sandwich: Active mode needs >= 2 circuit wires");

        ctrl_pool.resize(n);
        std::iota(ctrl_pool.begin(), ctrl_pool.end(), 0);
        tgt_pool_global = ctrl_pool;   // same pool; ctrl excluded per-iteration below
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
