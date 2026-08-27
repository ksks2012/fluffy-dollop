#include "circuit.hpp"

Circuit::Circuit(int num_wires, int num_inputs, std::vector<int> output_wires)
    : num_wires_(num_wires)
    , num_inputs_(num_inputs)
    , output_wires_(std::move(output_wires))
    , wires_(num_wires, 0)
{}

void Circuit::add_gate(std::unique_ptr<Gate> gate) {
    gates_.push_back(std::move(gate));
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
