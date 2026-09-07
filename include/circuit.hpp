#pragma once

#include <vector>
#include <memory>
#include "gate.h"

class Circuit {
public:
    Circuit(int num_wires, int num_inputs, std::vector<int> output_wires);

    Circuit(const Circuit&)            = delete;
    Circuit& operator=(const Circuit&) = delete;
    Circuit(Circuit&&)                 = default;
    Circuit& operator=(Circuit&&)      = default;

    void add_gate(std::unique_ptr<Gate> gate);

    // Insert a gate at an arbitrary position in the gate list.
    // insert_gate(0, g) prepends; insert_gate(gate_count(), g) appends.
    void insert_gate(int pos, std::unique_ptr<Gate> gate);

    // Remove `count` consecutive gates starting at position `pos`.
    // remove_gates(pos, 1) removes gates[pos]; a removed gate's own object
    // is destroyed.  Positions after pos shift down by `count`.
    void remove_gates(int pos, int count);

    // Expand the wire vector to cover new_num_wires total wires.
    // New wires are zero-initialized.  No-op if already large enough.
    void extend_wires(int new_num_wires);

    std::vector<int> evaluate(const std::vector<int>& input);

    const std::vector<int>& last_wires() const { return wires_; }
    const std::vector<std::unique_ptr<Gate>>& gates_ref() const { return gates_; }

    int gate_count()  const { return static_cast<int>(gates_.size()); }
    int num_wires()   const { return num_wires_; }
    int num_inputs()  const { return num_inputs_; }
    const std::vector<int>& output_wires() const { return output_wires_; }

    // Returns gate indices in a valid topological ordering (Kahn's algorithm).
    // Gates that share no data dependency may appear in either order.
    std::vector<int> topological_sort() const;

    // Returns depth[i] for each gate i.
    // Primary input wires have depth 0; gate depth = 1 + max(predecessor depths).
    // Gates at the same depth share no data dependencies and can run in parallel.
    std::vector<int> compute_depth_levels() const;

    // Reorder the gate list according to new_order (a permutation of 0..gate_count()-1).
    // new_order[0] is the index of the gate that should appear first, etc.
    void reorder_gates(const std::vector<int>& new_order);

private:
    int num_wires_;
    int num_inputs_;
    std::vector<int> output_wires_;
    std::vector<std::unique_ptr<Gate>> gates_;
    std::vector<int> wires_;
};
