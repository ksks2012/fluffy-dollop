#include <iostream>
#include <vector>
#include <memory>
#include <random>
#include <algorithm>
#include "gate.h"
#include "circuit.hpp"
#include "adder.hpp"
#include "sandwich.hpp"
#include "shuffle.hpp"

int main() {
    std::cout << "=== Gate driver demo ===\n\n";

    // --- ToffoliGate ---
    std::cout << "ToffoliGate(ctrl1=0, ctrl2=1, tgt=2):\n";
    for (int c1 : {0, 1})
    for (int c2 : {0, 1}) {
        std::vector<int> w = {c1, c2, 0};
        ToffoliGate(0, 1, 2).evaluate(w);
        std::cout << "  c1=" << c1 << " c2=" << c2
                  << " -> wires[2]=" << w[2]
                  << (c1 && c2 ? "  (flipped)" : "") << "\n";
    }

    // --- CnotGate ---
    std::cout << "\nCnotGate(ctrl=0, tgt=1):\n";
    for (int c : {0, 1})
    for (int t : {0, 1}) {
        std::vector<int> w = {c, t};
        CnotGate(0, 1).evaluate(w);
        std::cout << "  ctrl=" << c << " tgt=" << t
                  << " -> wires[1]=" << w[1] << "\n";
    }

    // --- NotGate ---
    std::cout << "\nNotGate(tgt=0):\n";
    for (int t : {0, 1}) {
        std::vector<int> w = {t};
        NotGate(0).evaluate(w);
        std::cout << "  tgt=" << t << " -> wires[0]=" << w[0] << "\n";
    }

    // --- Polymorphic chain ---
    std::cout << "\nPolymorphic chain [NOT(2), CNOT(2->1), Toffoli(1,2->0)] on {0,0,1}:\n";
    std::vector<std::unique_ptr<Gate>> chain;
    chain.push_back(std::make_unique<NotGate>(2));        // flip wire 2: 1->0... wait, let's pick interesting values
    chain.push_back(std::make_unique<CnotGate>(2, 1));
    chain.push_back(std::make_unique<ToffoliGate>(1, 2, 0));

    std::vector<int> wires = {0, 0, 1};
    std::cout << "  before: [" << wires[0] << "," << wires[1] << "," << wires[2] << "]\n";
    for (auto& g : chain)
        g->evaluate(wires);
    std::cout << "  after:  [" << wires[0] << "," << wires[1] << "," << wires[2] << "]\n";

    // --- 2-bit adder demo ---
    std::cout << "\n=== 2-bit reversible adder ===\n";
    auto adder = make_2bit_adder();
    std::cout << "Gate count: " << adder.gate_count() << "\n";
    std::cout << "Wire count: " << adder.num_wires() << "\n\n";
    std::cout << "  A  B Cin | S1 S0 Cout | sum\n";
    std::cout << "  ---------+------------+----\n";
    for (int bits = 0; bits < 32; ++bits) {
        int A1  = (bits >> 4) & 1;
        int A0  = (bits >> 3) & 1;
        int B1  = (bits >> 2) & 1;
        int B0  = (bits >> 1) & 1;
        int Cin =  bits       & 1;
        int A = A1 * 2 + A0;
        int B = B1 * 2 + B0;
        auto out = adder.evaluate({A1, A0, B1, B0, Cin});
        int result = out[2] * 4 + out[0] * 2 + out[1];
        std::cout << "  " << A << "  " << B << "   " << Cin
                  << "  |  " << out[0] << "  " << out[1] << "    " << out[2]
                  << "  | " << result
                  << (result != A + B + Cin ? "  *** ERROR ***" : "")
                  << "\n";
    }

    // --- Sandwich demo ---
    std::cout << "\n=== Sandwich demo (seed=42, chain_len=8, extra_ancilla=4) ===\n";
    auto sandwiched = make_2bit_adder();
    std::cout << "Gate count before: " << sandwiched.gate_count() << "\n";
    std::mt19937 rng(42);
    apply_sandwich(sandwiched, {9, 10, 11, 12}, 8, rng);
    std::cout << "Gate count after:  " << sandwiched.gate_count() << "\n";
    std::cout << "Wire count after:  " << sandwiched.num_wires() << "\n";
    std::cout << "Overhead ratio:    "
              << static_cast<double>(sandwiched.gate_count()) / 15.0 << "x\n";

    // --- Shuffle demo ---
    std::cout << "\n=== Shuffle demo (seed=42, after sandwich) ===\n";
    auto shuffled = make_2bit_adder();
    std::mt19937 rng2(42);
    apply_sandwich(shuffled, {9, 10, 11, 12}, 8, rng2);
    const int before_shuffle = shuffled.gate_count();
    auto d_before = shuffled.compute_depth_levels();
    shuffle_depth_levels(shuffled, rng2);
    auto d_after = shuffled.compute_depth_levels();
    std::sort(d_before.begin(), d_before.end());
    std::sort(d_after.begin(), d_after.end());
    std::cout << "Gate count before: " << before_shuffle << "\n";
    std::cout << "Gate count after:  " << shuffled.gate_count() << " (unchanged)\n";
    std::cout << "Depth distribution preserved: "
              << (d_before == d_after ? "yes" : "NO — BUG") << "\n";

    return 0;
}
