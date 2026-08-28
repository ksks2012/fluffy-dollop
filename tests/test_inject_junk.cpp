#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include "circuit.hpp"
#include "adder.hpp"
#include "sandwich.hpp"
#include "shuffle.hpp"
#include "inject_junk.hpp"

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
static int adder_truth_table_errors(Circuit& c) {
    int bad = 0;
    for (int bits = 0; bits < 32; ++bits) {
        int A1=(bits>>4)&1, A0=(bits>>3)&1, B1=(bits>>2)&1, B0=(bits>>1)&1, Cin=bits&1;
        int sum = (A1*2+A0) + (B1*2+B0) + Cin;
        auto out = c.evaluate({A1,A0,B1,B0,Cin});
        if (out[0]!=((sum>>1)&1) || out[1]!=(sum&1) || out[2]!=((sum>>2)&1)) ++bad;
    }
    return bad;
}

static int ancilla_violations(Circuit& c) {
    const auto& outs = c.output_wires();
    int v = 0;
    for (int bits = 0; bits < 32; ++bits) {
        int A1=(bits>>4)&1, A0=(bits>>3)&1, B1=(bits>>2)&1, B0=(bits>>1)&1, Cin=bits&1;
        c.evaluate({A1,A0,B1,B0,Cin});
        const auto& ws = c.last_wires();
        for (int w = c.num_inputs(); w < c.num_wires(); ++w) {
            bool is_out = std::find(outs.begin(), outs.end(), w) != outs.end();
            if (!is_out && ws[w] != 0) ++v;
        }
    }
    return v;
}

// ---------------------------------------------------------------------------
// Gate count increases by exactly `count`
// ---------------------------------------------------------------------------
static void test_inject_gate_count() {
    auto adder = make_2bit_adder();
    const int before = adder.gate_count();
    std::mt19937 rng(42);
    inject_junk_gates(adder, {9, 10, 11}, 7, rng);
    CHECK(adder.gate_count() == before + 7);
}

// ---------------------------------------------------------------------------
// Wire count is extended to cover junk wire indices
// ---------------------------------------------------------------------------
static void test_inject_extends_wire_count() {
    auto adder = make_2bit_adder();
    CHECK(adder.num_wires() == 9);
    std::mt19937 rng(42);
    inject_junk_gates(adder, {9, 10, 11}, 3, rng);
    CHECK(adder.num_wires() == 12);
}

// ---------------------------------------------------------------------------
// Junk gate invariant: every Toffoli that touches junk wires has at least
// one control in junk_ancilla.
// Controls = touched_wires() minus target_wire().
// ---------------------------------------------------------------------------
static void test_inject_junk_gate_invariant() {
    auto adder = make_2bit_adder();
    const std::vector<int> junk = {9, 10, 11};
    std::mt19937 rng(42);
    inject_junk_gates(adder, junk, 12, rng);

    const auto& gs = adder.gates_ref();
    for (int i = 0; i < static_cast<int>(gs.size()); ++i) {
        auto tw = gs[i]->touched_wires();
        int tgt = gs[i]->target_wire();

        // Does this gate touch any junk wire?
        bool touches_junk = false;
        for (int w : tw)
            if (std::find(junk.begin(), junk.end(), w) != junk.end())
                { touches_junk = true; break; }
        if (!touches_junk) continue;  // existing adder gate, skip

        // Verify at least one control (tw \ {target}) is in junk_ancilla
        bool has_junk_ctrl = false;
        for (int w : tw) {
            if (w == tgt) continue;
            if (std::find(junk.begin(), junk.end(), w) != junk.end())
                { has_junk_ctrl = true; break; }
        }
        if (!has_junk_ctrl) {
            std::cerr << "  gate " << i << " has no junk control: target=" << tgt << "\n";
        }
        CHECK(has_junk_ctrl);
    }
}

// ---------------------------------------------------------------------------
// Injected Toffoli target wires must be in junk_ancilla
// (all operands stay on junk wires — no data wires modified)
// ---------------------------------------------------------------------------
static void test_inject_target_wires_in_junk() {
    auto adder = make_2bit_adder();
    const std::vector<int> junk = {9, 10, 11};
    const int orig = adder.gate_count();
    std::mt19937 rng(7);
    inject_junk_gates(adder, junk, 8, rng);

    const auto& gs = adder.gates_ref();
    // Identify newly added gates: those touching junk wires
    for (int i = 0; i < static_cast<int>(gs.size()); ++i) {
        auto tw = gs[i]->touched_wires();
        bool touches_junk = false;
        for (int w : tw)
            if (std::find(junk.begin(), junk.end(), w) != junk.end())
                { touches_junk = true; break; }
        if (!touches_junk) continue;

        // All touched wires must be in junk_ancilla
        for (int w : tw) {
            bool in_junk = std::find(junk.begin(), junk.end(), w) != junk.end();
            if (!in_junk) {
                std::cerr << "  junk gate " << i << " touches non-junk wire " << w << "\n";
            }
            CHECK(in_junk);
        }
    }
}

// ---------------------------------------------------------------------------
// Determinism: same seed → same result
// ---------------------------------------------------------------------------
static void test_inject_deterministic() {
    auto a1 = make_2bit_adder(), a2 = make_2bit_adder();
    std::mt19937 r1(99), r2(99);
    inject_junk_gates(a1, {9, 10, 11}, 6, r1);
    inject_junk_gates(a2, {9, 10, 11}, 6, r2);
    const auto& g1 = a1.gates_ref();
    const auto& g2 = a2.gates_ref();
    CHECK(g1.size() == g2.size());
    for (int i = 0; i < static_cast<int>(g1.size()); ++i)
        CHECK(g1[i]->target_wire() == g2[i]->target_wire());
}

// ---------------------------------------------------------------------------
// Truth-table: 32 inputs still correct after injection
// ---------------------------------------------------------------------------
static void test_inject_truth_table() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);
    inject_junk_gates(adder, {9, 10, 11}, 10, rng);
    CHECK(adder_truth_table_errors(adder) == 0);
}

// ---------------------------------------------------------------------------
// Ancilla invariant: all non-output ancilla wires = 0 for all 32 inputs
// ---------------------------------------------------------------------------
static void test_inject_ancilla_invariant() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);
    inject_junk_gates(adder, {9, 10, 11}, 10, rng);
    CHECK(ancilla_violations(adder) == 0);
}

// ---------------------------------------------------------------------------
// Full pipeline: sandwich → shuffle → inject; truth table and ancilla hold
// ---------------------------------------------------------------------------
static void test_inject_full_pipeline_truth_table() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);
    apply_sandwich(adder, {9, 10, 11, 12}, 8, rng);
    shuffle_depth_levels(adder, rng);
    inject_junk_gates(adder, {13, 14, 15}, 12, rng);
    CHECK(adder_truth_table_errors(adder) == 0);
}

static void test_inject_full_pipeline_ancilla_invariant() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);
    apply_sandwich(adder, {9, 10, 11, 12}, 8, rng);
    shuffle_depth_levels(adder, rng);
    inject_junk_gates(adder, {13, 14, 15}, 12, rng);
    CHECK(ancilla_violations(adder) == 0);
}

// ---------------------------------------------------------------------------
// Multiple inject calls stack correctly (gate count additive)
// ---------------------------------------------------------------------------
static void test_inject_stacked_count() {
    auto adder = make_2bit_adder();
    const int orig = adder.gate_count();
    std::mt19937 rng(1);
    inject_junk_gates(adder, {9, 10, 11}, 5, rng);
    inject_junk_gates(adder, {9, 10, 11}, 3, rng);
    CHECK(adder.gate_count() == orig + 5 + 3);
    CHECK(adder_truth_table_errors(adder) == 0);
}

// ---------------------------------------------------------------------------
int main() {
    test_inject_gate_count();
    test_inject_extends_wire_count();
    test_inject_junk_gate_invariant();
    test_inject_target_wires_in_junk();
    test_inject_deterministic();
    test_inject_truth_table();
    test_inject_ancilla_invariant();
    test_inject_full_pipeline_truth_table();
    test_inject_full_pipeline_ancilla_invariant();
    test_inject_stacked_count();

    if (s_failed == 0)
        std::cout << "All " << s_run << " tests passed.\n";
    else
        std::cerr << s_failed << "/" << s_run << " tests FAILED.\n";
    return s_failed == 0 ? 0 : 1;
}
