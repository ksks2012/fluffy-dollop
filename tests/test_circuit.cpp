#include <iostream>
#include <vector>
#include "circuit.hpp"
#include "adder.hpp"

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
// Circuit class unit tests
// ---------------------------------------------------------------------------
static void test_circuit_empty_evaluate_returns_zeros() {
    Circuit c(2, 1, {1});
    auto out = c.evaluate({0});
    CHECK(out.size() == 1);
    CHECK(out[0] == 0);
}

static void test_circuit_single_cnot() {
    Circuit c(2, 1, {1});
    c.add_gate(std::make_unique<CnotGate>(0, 1));
    CHECK(c.evaluate({1})[0] == 1);
    CHECK(c.evaluate({0})[0] == 0);
}

static void test_circuit_gate_count() {
    Circuit c(3, 2, {2});
    CHECK(c.gate_count() == 0);
    c.add_gate(std::make_unique<CnotGate>(0, 2));
    CHECK(c.gate_count() == 1);
    c.add_gate(std::make_unique<CnotGate>(1, 2));
    CHECK(c.gate_count() == 2);
}

static void test_circuit_wires_reset_between_calls() {
    // Wire state must be zero-initialised at the start of every evaluate()
    Circuit c(2, 1, {1});
    c.add_gate(std::make_unique<CnotGate>(0, 1));
    c.evaluate({1});          // wire 1 → 1
    auto out = c.evaluate({0});  // wire 0=0, so wire 1 must stay 0
    CHECK(out[0] == 0);
}

static void test_circuit_multiple_outputs() {
    // XOR both bits: output = {w0^w2, w1^w2}
    Circuit c(3, 3, {0, 1});
    c.add_gate(std::make_unique<CnotGate>(2, 0));
    c.add_gate(std::make_unique<CnotGate>(2, 1));
    auto out = c.evaluate({0, 1, 1});
    CHECK(out[0] == 1);  // 0^1
    CHECK(out[1] == 0);  // 1^1
}

// ---------------------------------------------------------------------------
// 2-bit adder — gate count
// ---------------------------------------------------------------------------
static void test_adder_gate_count() {
    auto adder = make_2bit_adder();
    CHECK(adder.gate_count() == 15);
}

// ---------------------------------------------------------------------------
// 2-bit adder — spot-checks (from ticket)
// ---------------------------------------------------------------------------
static void test_adder_1_plus_1_cin0() {
    // A=1, B=1, Cin=0 → sum=2 ⇒ S1=1,S0=0,Cout=0
    auto adder = make_2bit_adder();
    auto out = adder.evaluate({0, 1, 0, 1, 0});
    CHECK(out[0] == 1);
    CHECK(out[1] == 0);
    CHECK(out[2] == 0);
}

static void test_adder_3_plus_3_cin1() {
    // A=3, B=3, Cin=1 → sum=7 ⇒ S1=1,S0=1,Cout=1
    auto adder = make_2bit_adder();
    auto out = adder.evaluate({1, 1, 1, 1, 1});
    CHECK(out[0] == 1);
    CHECK(out[1] == 1);
    CHECK(out[2] == 1);
}

static void test_adder_2_plus_1_cin1() {
    // A=2, B=1, Cin=1 → sum=4 ⇒ S1=0,S0=0,Cout=1
    auto adder = make_2bit_adder();
    auto out = adder.evaluate({1, 0, 0, 1, 1});
    CHECK(out[0] == 0);
    CHECK(out[1] == 0);
    CHECK(out[2] == 1);
}

static void test_adder_zero() {
    // A=0, B=0, Cin=0 → sum=0
    auto adder = make_2bit_adder();
    auto out = adder.evaluate({0, 0, 0, 0, 0});
    CHECK(out[0] == 0);
    CHECK(out[1] == 0);
    CHECK(out[2] == 0);
}

// ---------------------------------------------------------------------------
// 2-bit adder — exhaustive truth-table (all 32 inputs)
// ---------------------------------------------------------------------------
static void test_adder_exhaustive_truth_table() {
    auto adder = make_2bit_adder();
    int mismatches = 0;

    for (int bits = 0; bits < 32; ++bits) {
        // Input encoding: bits[4..0] = A1 A0 B1 B0 Cin
        int A1  = (bits >> 4) & 1;
        int A0  = (bits >> 3) & 1;
        int B1  = (bits >> 2) & 1;
        int B0  = (bits >> 1) & 1;
        int Cin =  bits       & 1;

        int sum = (A1 * 2 + A0) + (B1 * 2 + B0) + Cin;  // 0..7

        int exp_S1   = (sum >> 1) & 1;
        int exp_S0   =  sum       & 1;
        int exp_Cout = (sum >> 2) & 1;

        auto out = adder.evaluate({A1, A0, B1, B0, Cin});

        if (out[0] != exp_S1 || out[1] != exp_S0 || out[2] != exp_Cout) {
            std::cerr << "  mismatch A=" << (A1*2+A0)
                      << " B=" << (B1*2+B0) << " Cin=" << Cin
                      << " got S1=" << out[0] << " S0=" << out[1]
                      << " Cout=" << out[2]
                      << " expected S1=" << exp_S1 << " S0=" << exp_S0
                      << " Cout=" << exp_Cout << "\n";
            ++mismatches;
        }
    }
    CHECK(mismatches == 0);
}

// ---------------------------------------------------------------------------
// 2-bit adder — ancilla invariant (wire 8 = carry_mid must be 0 after every run)
// ---------------------------------------------------------------------------
static void test_adder_ancilla_invariant() {
    auto adder = make_2bit_adder();
    int violations = 0;

    for (int bits = 0; bits < 32; ++bits) {
        int A1  = (bits >> 4) & 1;
        int A0  = (bits >> 3) & 1;
        int B1  = (bits >> 2) & 1;
        int B0  = (bits >> 1) & 1;
        int Cin =  bits       & 1;

        adder.evaluate({A1, A0, B1, B0, Cin});

        if (adder.last_wires()[8] != 0) {
            std::cerr << "  ancilla violation A=" << (A1*2+A0)
                      << " B=" << (B1*2+B0) << " Cin=" << Cin
                      << " carry_mid=" << adder.last_wires()[8] << "\n";
            ++violations;
        }
    }
    CHECK(violations == 0);
}

// ---------------------------------------------------------------------------
int main() {
    // Circuit class
    test_circuit_empty_evaluate_returns_zeros();
    test_circuit_single_cnot();
    test_circuit_gate_count();
    test_circuit_wires_reset_between_calls();
    test_circuit_multiple_outputs();
    // Adder
    test_adder_gate_count();
    test_adder_1_plus_1_cin0();
    test_adder_3_plus_3_cin1();
    test_adder_2_plus_1_cin1();
    test_adder_zero();
    test_adder_exhaustive_truth_table();
    test_adder_ancilla_invariant();

    if (s_failed == 0)
        std::cout << "All " << s_run << " tests passed.\n";
    else
        std::cerr << s_failed << "/" << s_run << " tests FAILED.\n";
    return s_failed == 0 ? 0 : 1;
}
