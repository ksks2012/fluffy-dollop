#include "adder.hpp"
#include "gate.h"

Circuit make_2bit_adder() {
    //                num_wires  num_inputs  output_wires
    Circuit c(        9,         5,          {5, 6, 7});

    // Step 1: S0 = A0 ^ B0 ^ Cin
    c.add_gate(std::make_unique<CnotGate>(1, 6));
    c.add_gate(std::make_unique<CnotGate>(3, 6));
    c.add_gate(std::make_unique<CnotGate>(4, 6));

    // Step 2: carry_mid = majority(A0, B0, Cin)
    c.add_gate(std::make_unique<ToffoliGate>(1, 3, 8));
    c.add_gate(std::make_unique<ToffoliGate>(1, 4, 8));
    c.add_gate(std::make_unique<ToffoliGate>(3, 4, 8));

    // Step 3: S1 = A1 ^ B1 ^ carry_mid
    c.add_gate(std::make_unique<CnotGate>(0, 5));
    c.add_gate(std::make_unique<CnotGate>(2, 5));
    c.add_gate(std::make_unique<CnotGate>(8, 5));

    // Step 4: Cout = majority(A1, B1, carry_mid)
    c.add_gate(std::make_unique<ToffoliGate>(0, 2, 7));
    c.add_gate(std::make_unique<ToffoliGate>(0, 8, 7));
    c.add_gate(std::make_unique<ToffoliGate>(2, 8, 7));

    // Step 5: uncompute carry_mid
    c.add_gate(std::make_unique<ToffoliGate>(3, 4, 8));
    c.add_gate(std::make_unique<ToffoliGate>(1, 4, 8));
    c.add_gate(std::make_unique<ToffoliGate>(1, 3, 8));

    return c;
}
