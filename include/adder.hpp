#pragma once

#include "circuit.hpp"

// ---------------------------------------------------------------------------
// make_nbit_adder(n, extra_ancilla) — reversible ripple-carry adder for any n.
//
// Wire layout contract:
//   [0 .. n-1]     A_{n-1} .. A_0   (primary inputs, MSB first)
//   [n .. 2n-1]    B_{n-1} .. B_0   (primary inputs, MSB first)
//   [2n]           Cin              (primary input)
//   [2n+1 .. 3n]   S_{n-1} .. S_0  (sum outputs, MSB first)
//   [3n+1]         Cout             (carry output)
//   [3n+2 .. 4n]   carry_1..carry_{n-1}  (working ancilla, reverse-uncomputed)
//   [4n+1 ..]      extra_ancilla    (zero-initialized, unused by the adder)
//
// Total wires: 4n + 1 + extra_ancilla
// Total inputs: 2n + 1
// output_wires(): [S_{n-1}, .., S_0, Cout]
//
// Supported: any n >= 1.  Throws std::invalid_argument for n < 1.
// extra_ancilla defaults to 4.
// ---------------------------------------------------------------------------
Circuit make_nbit_adder(int n, int extra_ancilla = 4);

// Thin wrapper: make_2bit_adder() == make_nbit_adder(2, 0)
Circuit make_2bit_adder();
