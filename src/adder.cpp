#include "adder.hpp"
#include "gate.h"
#include <stdexcept>

// ---------------------------------------------------------------------------
// Canonical gate sequence for make_nbit_adder (VBE-style, O(n) carry wires)
// ===========================================================================
//
// Wire layout contract (see adder.hpp):
//   inputs  [0..n-1]    A_{n-1}..A_0    (MSB first)
//           [n..2n-1]   B_{n-1}..B_0    (MSB first)
//           [2n]        Cin
//   outputs output_wires() = [S_{n-1}..S_0, Cout]
//   working [3n+2..4n]  carry_1..carry_{n-1}  (n-1 wires, reverse-uncomputed)
//   [4n+1 ..]           extra_ancilla          (zero, unused by the adder)
//
// Total wires: (2n+1) + (n+1) + (n-1) + extra = 4n + 1 + extra
// Total inputs: 2n + 1
//
// Wire aliases (for bit k, 0 = LSB):
//   ak  = n-1-k            A_k
//   bk  = 2n-1-k           B_k
//   sk  = 2n+1 + (n-1-k)   S_k output
//   cin = 2n               Cin
//   cout= 3n+1             Cout output
//   ck  = 3n+2 + (k-1)     carry_k working wire, for k = 1 .. n-1
//   C_k = (k==0) ? cin : carry_k
//
// 1. Forward carry chain (3 Toffoli per bit):
//      carry_{k+1} = maj(A_k, B_k, C_k)   for k = 0 .. n-2  -> working ck
//      Cout        = maj(A_{n-1},B_{n-1},carry_{n-1})       -> output cout
//    where each maj(x,y,z) = x·y ^ x·z ^ y·z = Toff(x,y,T);Toff(x,z,T);Toff(y,z,T)
//
// 2. Sums (3 CNOT per bit):
//      S_k = A_k ^ B_k ^ C_k               for k = 0 .. n-1
//
// 3. Reverse uncompute of the carry chain (k = n-2 down to 0), restoring
//    each carry_{k+1} to 0 with the 3 Toffoli in reverse order.
//
// Step 3 proves the ancilla invariant: reverse-emitting the majority that
// wrote carry_{k+1} (and cancelling against identical operands, since
// Toffoli is an involution) restores it to 0, provided the controls are
// still intact — guaranteed because uncompute runs in reverse creation order.
//
// For n = 2 this reproduces the original hardcoded 2-bit adder gate-for-gate.
// Total gates: 3n (carry) + 3n (sums) + 3(n-1) (uncompute) = 9n - 3
// ---------------------------------------------------------------------------

Circuit make_nbit_adder(int n, int extra_ancilla) {
    if (n < 1)
        throw std::invalid_argument("make_nbit_adder: n must be >= 1");

    const int num_wires  = 4*n + 1 + extra_ancilla;
    const int num_inputs = 2*n + 1;

    // Output wires: S_{n-1}, S_{n-2}, ..., S_0, Cout
    std::vector<int> output_wires;
    output_wires.reserve(n + 1);
    for (int k = n-1; k >= 0; --k)
        output_wires.push_back(2*n + 1 + (n-1-k));   // S_k
    output_wires.push_back(3*n + 1);                 // Cout

    Circuit c(num_wires, num_inputs, std::move(output_wires));

    auto ak = [&](int k) { return n-1-k; };               // A_k
    auto bk = [&](int k) { return 2*n-1-k; };             // B_k
    auto sk = [&](int k) { return 2*n+1+(n-1-k); };       // S_k
    auto ck = [&](int k) { return 3*n+1+k; };             // carry_k, k=1..n-1
    const int cin  = 2*n;
    const int cout_w = 3*n + 1;

    auto Ck = [&](int k) { return (k == 0) ? cin : ck(k); };

    // 1. Forward carry chain: carry_{k+1} / Cout = maj(A_k, B_k, C_k)
    for (int k = 0; k < n; ++k) {
        int a = ak(k), b = bk(k), ci = Ck(k);
        int target = (k == n-1) ? cout_w : ck(k+1);
        c.add_gate(std::make_unique<ToffoliGate>(a, b, target));
        c.add_gate(std::make_unique<ToffoliGate>(a, ci, target));
        c.add_gate(std::make_unique<ToffoliGate>(b, ci, target));
    }

    // 2. Sums: S_k = A_k ^ B_k ^ C_k
    for (int k = 0; k < n; ++k) {
        int a = ak(k), b = bk(k), ci = Ck(k);
        c.add_gate(std::make_unique<CnotGate>(a, sk(k)));
        c.add_gate(std::make_unique<CnotGate>(b, sk(k)));
        c.add_gate(std::make_unique<CnotGate>(ci, sk(k)));
    }

    // 3. Reverse uncompute: clear carry_{k+1} for k = n-2 down to 0
    for (int k = n-2; k >= 0; --k) {
        int a = ak(k), b = bk(k), ci = Ck(k);
        int target = ck(k+1);
        c.add_gate(std::make_unique<ToffoliGate>(b, ci, target));
        c.add_gate(std::make_unique<ToffoliGate>(a, ci, target));
        c.add_gate(std::make_unique<ToffoliGate>(a, b, target));
    }

    return c;
}

Circuit make_2bit_adder() {
    return make_nbit_adder(2, 0);
}
