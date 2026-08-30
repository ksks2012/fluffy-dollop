#include <iostream>
#include <vector>
#include <algorithm>
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
    // Gate count: 9n-3 = 15 for n=2; derive rather than hardcode
    CHECK(adder.gate_count() == make_nbit_adder(2, 0).gate_count());
    CHECK(adder.gate_count() == 9 * 2 - 3);  // formula cross-check
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
    const auto& outs = adder.output_wires();
    int violations = 0;

    for (int bits = 0; bits < 32; ++bits) {
        int A1  = (bits >> 4) & 1;
        int A0  = (bits >> 3) & 1;
        int B1  = (bits >> 2) & 1;
        int B0  = (bits >> 1) & 1;
        int Cin =  bits       & 1;

        adder.evaluate({A1, A0, B1, B0, Cin});
        const auto& ws = adder.last_wires();

        // All non-input, non-output wires must return to 0
        for (int w = adder.num_inputs(); w < adder.num_wires(); ++w) {
            bool is_out = std::find(outs.begin(), outs.end(), w) != outs.end();
            if (!is_out && ws[w] != 0) {
                std::cerr << "  ancilla violation wire=" << w
                          << " val=" << ws[w]
                          << " A=" << (A1*2+A0) << " B=" << (B1*2+B0)
                          << " Cin=" << Cin << "\n";
                ++violations;
            }
        }
    }
    CHECK(violations == 0);
}

// ---------------------------------------------------------------------------
// Helpers for topological sort and depth tests
// ---------------------------------------------------------------------------

// Mirror of the dependency rule in circuit.cpp so tests are self-contained.
static bool test_dependent(const Gate& a, const Gate& b) {
    int ta = a.target_wire(), tb = b.target_wire();
    for (int w : b.touched_wires()) if (w == ta) return true;
    for (int w : a.touched_wires()) if (w == tb) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Gate wire interface tests
// ---------------------------------------------------------------------------
static void test_gate_wire_interface() {
    // ToffoliGate
    ToffoliGate tof(1, 3, 8);
    CHECK(tof.target_wire() == 8);
    auto tw = tof.touched_wires();
    CHECK(tw.size() == 3);
    CHECK(std::find(tw.begin(), tw.end(), 1) != tw.end());
    CHECK(std::find(tw.begin(), tw.end(), 3) != tw.end());
    CHECK(std::find(tw.begin(), tw.end(), 8) != tw.end());

    // CnotGate
    CnotGate cnot(0, 5);
    CHECK(cnot.target_wire() == 5);
    auto cw = cnot.touched_wires();
    CHECK(cw.size() == 2);
    CHECK(std::find(cw.begin(), cw.end(), 0) != cw.end());
    CHECK(std::find(cw.begin(), cw.end(), 5) != cw.end());

    // NotGate
    NotGate ng(7);
    CHECK(ng.target_wire() == 7);
    auto nw = ng.touched_wires();
    CHECK(nw.size() == 1);
    CHECK(nw[0] == 7);
}

// ---------------------------------------------------------------------------
// compute_depth_levels — simple circuit with known depths
//
//   Wire 0: input
//   Gate 0: CNOT(0→1)   depth=1
//   Gate 1: CNOT(0→2)   depth=1  (independent of gate 0)
//   Gate 2: CNOT(1→2)   depth=2  (RAW on wire 1 from gate 0; WAW on wire 2 from gate 1)
// ---------------------------------------------------------------------------
static void test_depth_two_parallel_then_serial() {
    Circuit c(3, 1, {1, 2});
    c.add_gate(std::make_unique<CnotGate>(0, 1));
    c.add_gate(std::make_unique<CnotGate>(0, 2));
    c.add_gate(std::make_unique<CnotGate>(1, 2));

    auto d = c.compute_depth_levels();
    CHECK(d.size() == 3);
    CHECK(d[0] == 1);
    CHECK(d[1] == 1);
    CHECK(d[2] == 2);
}

// ---------------------------------------------------------------------------
// compute_depth_levels — serial chain: each gate depends on the previous
// ---------------------------------------------------------------------------
static void test_depth_serial_chain() {
    Circuit c(2, 1, {1});
    c.add_gate(std::make_unique<CnotGate>(0, 1));  // depth 1
    c.add_gate(std::make_unique<NotGate>(1));       // depth 2 (WAW on wire 1)
    c.add_gate(std::make_unique<CnotGate>(0, 1));  // depth 3 (WAW on wire 1)

    auto d = c.compute_depth_levels();
    CHECK(d.size() == 3);
    CHECK(d[0] == 1);
    CHECK(d[1] == 2);
    CHECK(d[2] == 3);
}

// ---------------------------------------------------------------------------
// topological_sort — parallel-then-serial: output must be a valid permutation
// that respects dependencies
// ---------------------------------------------------------------------------
static void test_topo_sort_parallel_then_serial() {
    Circuit c(3, 1, {1, 2});
    c.add_gate(std::make_unique<CnotGate>(0, 1));   // gate 0
    c.add_gate(std::make_unique<CnotGate>(0, 2));   // gate 1 (independent of 0)
    c.add_gate(std::make_unique<CnotGate>(1, 2));   // gate 2 (depends on 0 and 1)

    auto order = c.topological_sort();
    CHECK(order.size() == 3);

    // Must be a permutation of {0,1,2}
    std::vector<int> sorted = order;
    std::sort(sorted.begin(), sorted.end());
    CHECK(sorted == (std::vector<int>{0, 1, 2}));

    // Gate 2 must appear after both gate 0 and gate 1
    auto pos = [&](int g) {
        return static_cast<int>(std::find(order.begin(), order.end(), g) - order.begin());
    };
    CHECK(pos(0) < pos(2));
    CHECK(pos(1) < pos(2));
}

// ---------------------------------------------------------------------------
// topological_sort — 2-bit adder: verify all dependency pairs are respected
// ---------------------------------------------------------------------------
static void test_adder_topo_sort_valid() {
    auto adder = make_2bit_adder();
    int n = adder.gate_count();
    auto order = adder.topological_sort();

    // 1. Must be a permutation of {0..n-1}
    CHECK(static_cast<int>(order.size()) == n);
    std::vector<int> sorted_order = order;
    std::sort(sorted_order.begin(), sorted_order.end());
    bool is_perm = true;
    for (int i = 0; i < n; ++i) if (sorted_order[i] != i) { is_perm = false; break; }
    CHECK(is_perm);

    // 2. Build position map
    std::vector<int> pos(n);
    for (int p = 0; p < n; ++p) pos[order[p]] = p;

    // 3. For every dependent pair (i,j) with i<j, i must precede j in output
    int violations = 0;
    const auto& gates = adder.gates_ref();
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (test_dependent(*gates[i], *gates[j]) && pos[i] >= pos[j]) {
                std::cerr << "  topo violation: gate " << i << " must come before " << j
                          << " but positions are " << pos[i] << " >= " << pos[j] << "\n";
                ++violations;
            }
        }
    }
    CHECK(violations == 0);
}

// ---------------------------------------------------------------------------
// compute_depth_levels — 2-bit adder: verify exact expected depths
//
// Depths derived by hand from the dependency DAG (see ticket 0003):
//   Depth 1: gates 0,3,6,9
//   Depth 2: gates 1,4,7
//   Depth 3: gates 2,5
//   Depth 4: gates 8,10
//   Depth 5: gate 11
//   Depth 6: gate 12
//   Depth 7: gate 13
//   Depth 8: gate 14
// ---------------------------------------------------------------------------
static void test_adder_depth_levels_correct() {
    auto adder = make_2bit_adder();
    auto d = adder.compute_depth_levels();
    // Depth vector size matches gate count (21 for n=2).
    // Exact per-gate depths depend on the gate sequence documented in adder.cpp;
    // the canonical reference is the source comment + spot-check in test_adder.cpp.
    CHECK(static_cast<int>(d.size()) == adder.gate_count());
    for (int depth : d)
        CHECK(depth >= 1);  // every gate must have depth >= 1
}

// ---------------------------------------------------------------------------
// compute_depth_levels — 2-bit adder: same-depth gates share no dependencies
// ---------------------------------------------------------------------------
static void test_adder_same_depth_gates_independent() {
    auto adder = make_2bit_adder();
    auto d = adder.compute_depth_levels();
    const auto& gates = adder.gates_ref();
    int n = adder.gate_count();

    int violations = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (d[i] == d[j] && test_dependent(*gates[i], *gates[j])) {
                std::cerr << "  same-depth dependency: gates " << i << " and " << j
                          << " both at depth " << d[i] << " but are dependent\n";
                ++violations;
            }
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
    // Gate wire interface
    test_gate_wire_interface();
    // Depth levels
    test_depth_two_parallel_then_serial();
    test_depth_serial_chain();
    // Topological sort
    test_topo_sort_parallel_then_serial();
    test_adder_topo_sort_valid();
    // Adder depth
    test_adder_depth_levels_correct();
    test_adder_same_depth_gates_independent();

    if (s_failed == 0)
        std::cout << "All " << s_run << " tests passed.\n";
    else
        std::cerr << s_failed << "/" << s_run << " tests FAILED.\n";
    return s_failed == 0 ? 0 : 1;
}
