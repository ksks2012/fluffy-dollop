#include "inject_junk.hpp"
#include "gate.h"
#include <algorithm>
#include <stdexcept>

void inject_junk_gates(Circuit& circuit,
                       const std::vector<int>& junk_ancilla,
                       int count,
                       std::mt19937& rng) {
    if (junk_ancilla.size() < 2)
        throw std::invalid_argument("inject_junk_gates: need >= 2 junk ancilla wires");
    if (count < 1)
        throw std::invalid_argument("inject_junk_gates: count must be >= 1");

    // Extend circuit to cover all junk wire indices
    int max_wire = *std::max_element(junk_ancilla.begin(), junk_ancilla.end());
    circuit.extend_wires(max_wire + 1);

    int k = static_cast<int>(junk_ancilla.size());
    std::uniform_int_distribution<int> wire_dist(0, k - 1);

    for (int g = 0; g < count; ++g) {
        // Pick ctrl1 from junk_ancilla (always 0 → gate is a no-op)
        int ctrl1 = junk_ancilla[wire_dist(rng)];

        // Pick ctrl2 from junk_ancilla \ {ctrl1}
        std::vector<int> pool;
        pool.reserve(k - 1);
        for (int w : junk_ancilla)
            if (w != ctrl1) pool.push_back(w);
        int ctrl2 = pool[std::uniform_int_distribution<int>(0, static_cast<int>(pool.size()) - 1)(rng)];

        // Target: any junk ancilla wire (can equal ctrl1 or ctrl2 — still a no-op)
        int target = junk_ancilla[wire_dist(rng)];

        // Insert at a random valid position.
        // Because junk wires are never touched by existing gates, any position
        // maintains the existing gates' topological ordering.
        int pos = std::uniform_int_distribution<int>(0, circuit.gate_count())(rng);
        circuit.insert_gate(pos, std::make_unique<ToffoliGate>(ctrl1, ctrl2, target));
    }
}
