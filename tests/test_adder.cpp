#include <iostream>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include "adder.hpp"
#include "gate.h"

static int s_run = 0, s_failed = 0;

#define CHECK(expr)                                                        \
    do {                                                                   \
        ++s_run;                                                           \
        if (!(expr)) {                                                     \
            std::cerr << "FAIL [line " << __LINE__ << "]: " #expr "\n";   \
            ++s_failed;                                                    \
        }                                                                  \
    } while (0)

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

// Verify that make_nbit_adder(n) satisfies the wire-layout contract:
//   inputs  [0..n-1] = A_{n-1}..A_0
//   inputs  [n..2n-1] = B_{n-1}..B_0
//   input   [2n] = Cin
//   outputs output_wires() = [S_{n-1}, ..., S_0, Cout]
//   total wires = 4n + 1 + extra_ancilla
static void check_wire_contract(int n, int extra, Circuit& c) {
    const int expected_wires  = 4*n + 1 + extra;
    const int expected_inputs = 2*n + 1;
    CHECK(c.num_wires()  == expected_wires);
    CHECK(c.num_inputs() == expected_inputs);

    // output_wires()[i] = S_{n-1-i} for i=0..n-1; output_wires()[n] = Cout
    // S_k = wire[2n+1+(n-1-k)], so output_wires()[i] = wire[2n+1+i]
    const auto& ow = c.output_wires();
    CHECK(static_cast<int>(ow.size()) == n + 1);
    for (int i = 0; i < n; ++i)
        CHECK(ow[i] == 2*n + 1 + i);   // S_{n-1-i} = wire[2n+1+i]
    CHECK(ow[n] == 3*n + 1);           // Cout
}

// Exhaustive truth-table check for n-bit adder over all 2^(2n+1) inputs.
static bool adder_tt_correct(Circuit& c, int n) {
    int combos = 1 << (2*n + 1);
    for (int bits = 0; bits < combos; ++bits) {
        std::vector<int> input(2*n + 1);
        for (int j = 0; j < 2*n + 1; ++j)
            input[j] = (bits >> (2*n - j)) & 1;

        int A = 0, B = 0;
        for (int k = 0; k < n; ++k) {
            A = (A << 1) | input[k];
            B = (B << 1) | input[n + k];
        }
        int Cin = input[2*n];
        int sum = A + B + Cin;

        auto out = c.evaluate(input);
        for (int k = 0; k < n; ++k)
            if (out[k] != ((sum >> (n-1-k)) & 1)) return false;
        if (out[n] != ((sum >> n) & 1)) return false;
    }
    return true;
}

// Ancilla invariant: all non-output, non-input wires must return to 0.
static bool ancilla_ok(Circuit& c, int n) {
    const auto& outs = c.output_wires();
    int combos = 1 << (2*n + 1);
    for (int bits = 0; bits < combos; ++bits) {
        std::vector<int> input(2*n + 1);
        for (int j = 0; j < 2*n + 1; ++j)
            input[j] = (bits >> (2*n - j)) & 1;
        c.evaluate(input);
        const auto& ws = c.last_wires();
        for (int w = c.num_inputs(); w < c.num_wires(); ++w) {
            bool is_out = std::find(outs.begin(), outs.end(), w) != outs.end();
            if (!is_out && ws[w] != 0) return false;
        }
    }
    return true;
}

// ===========================================================================
// n = 2  tests
// ===========================================================================

static void test_nbit2_wire_contract() {
    auto c = make_nbit_adder(2, 0);
    check_wire_contract(2, 0, c);
    // make_2bit_adder is the thin wrapper
    auto c2 = make_2bit_adder();
    CHECK(c2.num_wires()  == c.num_wires());
    CHECK(c2.num_inputs() == c.num_inputs());
    CHECK(c2.output_wires() == c.output_wires());
}

static void test_nbit2_gate_count() {
    auto c = make_nbit_adder(2, 0);
    // Formula: 9n - 3 = 15 for n=2 (matches the original 15-gate 2-bit adder)
    CHECK(c.gate_count() == 9 * 2 - 3);
}

// ---------------------------------------------------------------------------
// Spot-check: verify the canonical per-bit gate sequence at known positions
// for n=2 (see make_nbit_adder source comment for the full canonical sequence).
//
// Wires for n=2, extra=0: a1=0,a0=1,b1=2,b0=3,cin=4, s1=5,s0=6,cout=7,
//   carry_1=8.  (total 9 wires, 15 gates)
//
//   G0:  Toffoli(a0=1, b0=3, carry_1=8)  forward carry_1 = maj(A0,B0,Cin) step1
//   G1:  Toffoli(a0=1, cin=4, 8)         step2
//   G2:  Toffoli(b0=3, cin=4, 8)         step3 -> carry_1
//   G3:  Toffoli(a1=0, b1=2, cout=7)     forward Cout = maj(A1,B1,carry_1) step1
//   G4:  Toffoli(a1=0, carry_1=8, 7)     step2
//   G5:  Toffoli(b1=2, carry_1=8, 7)     step3 -> Cout
//   G6:  CNOT(a0=1, s0=6)                S0 steps
//   G7:  CNOT(b0=3, 6)
//   G8:  CNOT(cin=4, 6)                  -> S0
//   G9:  CNOT(a1=0, s1=5)                S1 steps
//   G10: CNOT(b1=2, 5)
//   G11: CNOT(carry_1=8, 5)              -> S1
//   G12: Toffoli(b0=3, cin=4, 8)         reverse-uncompute carry_1 step1
//   G13: Toffoli(a0=1, cin=4, 8)         step2
//   G14: Toffoli(a0=1, b0=3, 8)          step3 -> carry_1 = 0
// ---------------------------------------------------------------------------
static void test_nbit2_gate_sequence_spotcheck() {
    auto c = make_nbit_adder(2, 0);
    const auto& gs = c.gates_ref();
    CHECK(static_cast<int>(gs.size()) == 15);

    auto has = [](const std::vector<int>& tw, int w) {
        return std::find(tw.begin(), tw.end(), w) != tw.end();
    };

    // G0: Toffoli(a0=1, b0=3, carry_1=8)
    CHECK(gs[0]->target_wire() == 8);
    { auto tw = gs[0]->touched_wires(); CHECK(has(tw,1) && has(tw,3)); }

    // G3: Toffoli(.., cout=7)
    CHECK(gs[3]->target_wire() == 7);

    // G6: CNOT(a0=1, s0=6)
    CHECK(gs[6]->target_wire() == 6);

    // G9: CNOT(a1=0, s1=5)
    CHECK(gs[9]->target_wire() == 5);

    // G14: Toffoli(a0=1, b0=3, carry_1=8) — last uncompute step
    CHECK(gs[14]->target_wire() == 8);
    { auto tw = gs[14]->touched_wires(); CHECK(has(tw,1) && has(tw,3)); }
}

static void test_nbit2_truth_table() {
    auto c = make_nbit_adder(2, 0);
    CHECK(adder_tt_correct(c, 2));
}

static void test_nbit2_ancilla_invariant() {
    auto c = make_nbit_adder(2, 0);
    CHECK(ancilla_ok(c, 2));
}

// n=2 with extra_ancilla=4
static void test_nbit2_extra_ancilla() {
    auto c = make_nbit_adder(2, 4);
    CHECK(c.num_wires() == 4*2 + 1 + 4);  // 13 wires
    CHECK(c.num_inputs() == 5);
    CHECK(adder_tt_correct(c, 2));
    CHECK(ancilla_ok(c, 2));
}

// ===========================================================================
// n = 4  tests
// ===========================================================================

static void test_nbit4_wire_contract() {
    auto c = make_nbit_adder(4, 0);
    check_wire_contract(4, 0, c);
    // Spot-check specific wire assignments for n=4 (base wire count 17)
    // A3..A0 = w[0..3], B3..B0 = w[4..7], Cin = w[8]
    // S3..S0 = w[9..12], Cout = w[13], carry_1..carry_3 = w[14..16]
    CHECK(c.num_wires()  == 17);
    CHECK(c.num_inputs() == 9);
    const auto& ow = c.output_wires();
    CHECK(ow[0] == 9);   // S3 = wire[2n+1+0] = wire[9]
    CHECK(ow[3] == 12);  // S0 = wire[2n+1+3] = wire[12]
    CHECK(ow[4] == 13);  // Cout
}

static void test_nbit4_gate_count() {
    auto c = make_nbit_adder(4, 0);
    // Formula: 9n - 3 = 33 for n=4
    CHECK(c.gate_count() == 9 * 4 - 3);
}

static void test_nbit4_truth_table() {
    auto c = make_nbit_adder(4, 0);
    CHECK(adder_tt_correct(c, 4));
}

static void test_nbit4_ancilla_invariant() {
    auto c = make_nbit_adder(4, 0);
    CHECK(ancilla_ok(c, 4));
}

// ===========================================================================
// Error cases
// ===========================================================================

static void test_nbit_invalid_n() {
    bool threw = false;
    try { make_nbit_adder(0); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);

    threw = false;
    try { make_nbit_adder(-1); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}

// ===========================================================================
int main() {
    test_nbit2_wire_contract();
    test_nbit2_gate_count();
    test_nbit2_gate_sequence_spotcheck();
    test_nbit2_truth_table();
    test_nbit2_ancilla_invariant();
    test_nbit2_extra_ancilla();

    test_nbit4_wire_contract();
    test_nbit4_gate_count();
    test_nbit4_truth_table();
    test_nbit4_ancilla_invariant();

    test_nbit_invalid_n();

    if (s_failed == 0)
        std::cout << "All " << s_run << " tests passed.\n";
    else
        std::cerr << s_failed << "/" << s_run << " tests FAILED.\n";
    return s_failed == 0 ? 0 : 1;
}
