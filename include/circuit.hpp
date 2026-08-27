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

    std::vector<int> evaluate(const std::vector<int>& input);

    const std::vector<int>& last_wires() const { return wires_; }

    int gate_count()  const { return static_cast<int>(gates_.size()); }
    int num_wires()   const { return num_wires_; }
    int num_inputs()  const { return num_inputs_; }
    const std::vector<int>& output_wires() const { return output_wires_; }

private:
    int num_wires_;
    int num_inputs_;
    std::vector<int> output_wires_;
    std::vector<std::unique_ptr<Gate>> gates_;
    std::vector<int> wires_;
};
