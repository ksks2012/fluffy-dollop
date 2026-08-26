#include <iostream>
#include <vector>
#include <memory>
#include "gate.h"

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

    return 0;
}
