#include "circuit.hpp"
#include <algorithm>
#include <queue>

// ---------------------------------------------------------------------------
// Dependency rule (symmetric):
//   gates A and B are dependent (require ordering) iff
//     A.target_wire() ∈ B.touched_wires()   (RAW or WAW)  OR
//     B.target_wire() ∈ A.touched_wires()   (WAR or WAW)
// ---------------------------------------------------------------------------
static bool gates_dependent(const Gate& a, const Gate& b) {
    int tgt_a = a.target_wire();
    int tgt_b = b.target_wire();
    for (int w : b.touched_wires()) if (w == tgt_a) return true;
    for (int w : a.touched_wires()) if (w == tgt_b) return true;
    return false;
}

Circuit::Circuit(int num_wires, int num_inputs, std::vector<int> output_wires)
    : num_wires_(num_wires)
    , num_inputs_(num_inputs)
    , output_wires_(std::move(output_wires))
    , wires_(num_wires, 0)
{}

void Circuit::add_gate(std::unique_ptr<Gate> gate) {
    gates_.push_back(std::move(gate));
}

void Circuit::insert_gate(int pos, std::unique_ptr<Gate> gate) {
    gates_.insert(gates_.begin() + pos, std::move(gate));
}

void Circuit::remove_gates(int pos, int count) {
    gates_.erase(gates_.begin() + pos, gates_.begin() + pos + count);
}

void Circuit::extend_wires(int new_num_wires) {
    if (new_num_wires > num_wires_) {
        num_wires_ = new_num_wires;
        wires_.resize(new_num_wires, 0);
    }
}

std::vector<int> Circuit::evaluate(const std::vector<int>& input) {
    std::fill(wires_.begin(), wires_.end(), 0);
    for (int i = 0; i < num_inputs_; ++i)
        wires_[i] = input[i];
    for (const auto& g : gates_)
        g->evaluate(wires_);
    std::vector<int> out;
    out.reserve(output_wires_.size());
    for (int idx : output_wires_)
        out.push_back(wires_[idx]);
    return out;
}

std::vector<int> Circuit::topological_sort() const {
    int n = static_cast<int>(gates_.size());

    // Build directed dependency graph: adj[i] contains all j > i such that
    // gate i must come before gate j.
    std::vector<std::vector<int>> adj(n);
    std::vector<int> indegree(n, 0);

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (gates_dependent(*gates_[i], *gates_[j])) {
                adj[i].push_back(j);
                indegree[j]++;
            }
        }
    }

    // Kahn's algorithm with FIFO queue for deterministic (stable) output.
    std::queue<int> q;
    for (int i = 0; i < n; ++i)
        if (indegree[i] == 0) q.push(i);

    std::vector<int> order;
    order.reserve(n);
    while (!q.empty()) {
        int v = q.front(); q.pop();
        order.push_back(v);
        for (int u : adj[v])
            if (--indegree[u] == 0) q.push(u);
    }
    return order;  // size == n guaranteed (circuit DAG is acyclic by construction)
}

std::vector<int> Circuit::compute_depth_levels() const {
    int n = static_cast<int>(gates_.size());
    std::vector<int> depth(n, 1);

    // Gates are stored in a valid topological order, so every predecessor of
    // gate j has index < j.  A single O(n²) pass suffices.
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < j; ++i) {
            if (gates_dependent(*gates_[i], *gates_[j])) {
                depth[j] = std::max(depth[j], depth[i] + 1);
            }
        }
    }
    return depth;
}

void Circuit::reorder_gates(const std::vector<int>& new_order) {
    std::vector<std::unique_ptr<Gate>> reordered;
    reordered.reserve(gates_.size());
    for (int idx : new_order)
        reordered.push_back(std::move(gates_[idx]));
    gates_ = std::move(reordered);
}
