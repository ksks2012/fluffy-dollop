#include "sandwich.hpp"
#include "gate.h"
#include <algorithm>
#include <stdexcept>

void apply_sandwich(Circuit& circuit,
                    const std::vector<int>& extra_ancilla,
                    int chain_len,
                    SandwichMode mode,
                    std::mt19937& rng) {
    (void)mode;  // Safe behaviour is the only implemented mode; Active reserved for ticket 0009
    if (extra_ancilla.size() < 2)
        throw std::invalid_argument("apply_sandwich: need >= 2 extra ancilla wires");
    if (chain_len < 1)
        throw std::invalid_argument("apply_sandwich: chain_len must be >= 1");

    // Extend circuit to cover all extra_ancilla wire indices
    int max_wire = *std::max_element(extra_ancilla.begin(), extra_ancilla.end());
    circuit.extend_wires(max_wire + 1);

    int k = static_cast<int>(extra_ancilla.size());

    // Build tgt pool for each draw: extra_ancilla minus the chosen ctrl
    // Reuse a single distribution for ctrl selection; compute tgt inline.
    std::uniform_int_distribution<int> ctrl_dist(0, k - 1);

    // Generate the forward chain: N random (ctrl, tgt) pairs
    std::vector<std::pair<int, int>> chain;
    chain.reserve(chain_len);
    for (int i = 0; i < chain_len; ++i) {
        int ctrl = extra_ancilla[ctrl_dist(rng)];

        // Pick tgt from extra_ancilla \ {ctrl}
        std::vector<int> tgt_pool;
        tgt_pool.reserve(k - 1);
        for (int w : extra_ancilla)
            if (w != ctrl) tgt_pool.push_back(w);

        int tgt = tgt_pool[std::uniform_int_distribution<int>(0, k - 2)(rng)];
        chain.push_back({ctrl, tgt});
    }

    // Prepend forward chain [chain[0], …, chain[N-1]] before gate index 0.
    // Inserting backwards at position 0 achieves forward order:
    //   insert chain[N-1] → [chain[N-1], …]
    //   insert chain[N-2] → [chain[N-2], chain[N-1], …]
    //   …
    //   insert chain[0]   → [chain[0], chain[1], …, chain[N-1], <original>]
    for (int i = chain_len - 1; i >= 0; --i)
        circuit.insert_gate(0, std::make_unique<CnotGate>(chain[i].first, chain[i].second));

    // Append reversed chain [chain[N-1], …, chain[0]] after the last gate.
    for (int i = chain_len - 1; i >= 0; --i)
        circuit.add_gate(std::make_unique<CnotGate>(chain[i].first, chain[i].second));
}
