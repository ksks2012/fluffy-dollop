#include "shuffle.hpp"
#include <algorithm>
#include <map>

void shuffle_depth_levels(Circuit& circuit, std::mt19937& rng) {
    auto depths = circuit.compute_depth_levels();
    int n = circuit.gate_count();

    // Group gate indices by depth level (std::map keeps keys sorted ascending)
    std::map<int, std::vector<int>> by_depth;
    for (int i = 0; i < n; ++i)
        by_depth[depths[i]].push_back(i);

    // Shuffle within each depth level
    for (auto& [d, indices] : by_depth)
        std::shuffle(indices.begin(), indices.end(), rng);

    // Build new ordering: depth levels ascending, shuffled within each level.
    // This is a valid topological order because every depth-d gate comes after
    // all depth-(d-1) predecessors and before all depth-(d+1) successors.
    std::vector<int> new_order;
    new_order.reserve(n);
    for (auto& [d, indices] : by_depth)
        for (int idx : indices)
            new_order.push_back(idx);

    circuit.reorder_gates(new_order);
}
