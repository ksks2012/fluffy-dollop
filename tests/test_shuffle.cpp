#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include "circuit.hpp"
#include "adder.hpp"
#include "sandwich.hpp"
#include "shuffle.hpp"

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
// Helpers
// ---------------------------------------------------------------------------

// Run the adder truth-table check; returns number of mismatches.
static int adder_truth_table_errors(Circuit& c) {
    int bad = 0;
    for (int bits = 0; bits < 32; ++bits) {
        int A1 = (bits>>4)&1, A0 = (bits>>3)&1;
        int B1 = (bits>>2)&1, B0 = (bits>>1)&1, Cin = bits&1;
        int sum = (A1*2+A0) + (B1*2+B0) + Cin;
        auto out = c.evaluate({A1, A0, B1, B0, Cin});
        if (out[0] != ((sum>>1)&1) || out[1] != (sum&1) || out[2] != ((sum>>2)&1))
            ++bad;
    }
    return bad;
}

// Check all non-input, non-output wires are 0 after every evaluate.
// Returns number of violations across all 32 inputs.
static int ancilla_violations(Circuit& c) {
    const auto& outs = c.output_wires();
    int violations = 0;
    for (int bits = 0; bits < 32; ++bits) {
        int A1=(bits>>4)&1, A0=(bits>>3)&1, B1=(bits>>2)&1, B0=(bits>>1)&1, Cin=bits&1;
        c.evaluate({A1,A0,B1,B0,Cin});
        const auto& ws = c.last_wires();
        for (int w = c.num_inputs(); w < c.num_wires(); ++w) {
            bool is_out = std::find(outs.begin(), outs.end(), w) != outs.end();
            if (!is_out && ws[w] != 0) ++violations;
        }
    }
    return violations;
}

// ---------------------------------------------------------------------------
// Gate count must be unchanged by shuffle
// ---------------------------------------------------------------------------
static void test_shuffle_gate_count_unchanged() {
    auto adder = make_2bit_adder();
    const int before = adder.gate_count();
    std::mt19937 rng(42);
    shuffle_depth_levels(adder, rng);
    CHECK(adder.gate_count() == before);
}

// ---------------------------------------------------------------------------
// Depth-level multiset must be unchanged after shuffle
// (same depths, just potentially at different positions)
// ---------------------------------------------------------------------------
static void test_shuffle_depth_distribution_unchanged() {
    auto adder = make_2bit_adder();
    auto d_before = adder.compute_depth_levels();
    std::sort(d_before.begin(), d_before.end());

    std::mt19937 rng(42);
    shuffle_depth_levels(adder, rng);

    auto d_after = adder.compute_depth_levels();
    std::sort(d_after.begin(), d_after.end());

    CHECK(d_before == d_after);
}

// ---------------------------------------------------------------------------
// After shuffle, recomputed depth levels must be internally consistent:
// same-depth gates must still be mutually independent.
// ---------------------------------------------------------------------------
static void test_shuffle_same_depth_independent_after_shuffle() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(17);
    shuffle_depth_levels(adder, rng);

    auto depths = adder.compute_depth_levels();
    const auto& gs = adder.gates_ref();
    int n = adder.gate_count();

    int violations = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = i+1; j < n; ++j) {
            if (depths[i] != depths[j]) continue;
            // Same depth: must be independent
            int ta = gs[i]->target_wire(), tb = gs[j]->target_wire();
            bool dep = false;
            for (int w : gs[j]->touched_wires()) if (w == ta) { dep = true; break; }
            if (!dep) for (int w : gs[i]->touched_wires()) if (w == tb) { dep = true; break; }
            if (dep) {
                std::cerr << "  same-depth dependency after shuffle: gates at positions "
                          << i << " and " << j << " (depth " << depths[i] << ")\n";
                ++violations;
            }
        }
    }
    CHECK(violations == 0);
}

// ---------------------------------------------------------------------------
// Determinism: same seed → same gate ordering
// ---------------------------------------------------------------------------
static void test_shuffle_deterministic() {
    auto a1 = make_2bit_adder(), a2 = make_2bit_adder();
    std::mt19937 r1(99), r2(99);
    shuffle_depth_levels(a1, r1);
    shuffle_depth_levels(a2, r2);

    const auto& g1 = a1.gates_ref();
    const auto& g2 = a2.gates_ref();
    CHECK(g1.size() == g2.size());
    for (int i = 0; i < static_cast<int>(g1.size()); ++i)
        CHECK(g1[i]->target_wire() == g2[i]->target_wire());
}

// ---------------------------------------------------------------------------
// Different seeds produce different orderings (probabilistic — uses two seeds
// that are known to differ for the 2-bit adder's 8-level depth structure)
// ---------------------------------------------------------------------------
static void test_shuffle_different_seeds_differ() {
    auto a1 = make_2bit_adder(), a2 = make_2bit_adder();
    std::mt19937 r1(1), r2(9999);
    shuffle_depth_levels(a1, r1);
    shuffle_depth_levels(a2, r2);

    const auto& g1 = a1.gates_ref();
    const auto& g2 = a2.gates_ref();
    bool any_differ = false;
    for (int i = 0; i < static_cast<int>(g1.size()); ++i)
        if (g1[i]->target_wire() != g2[i]->target_wire()) { any_differ = true; break; }
    CHECK(any_differ);
}

// ---------------------------------------------------------------------------
// Truth-table: plain adder, all 32 inputs correct after shuffle
// ---------------------------------------------------------------------------
static void test_shuffle_truth_table_plain() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);
    shuffle_depth_levels(adder, rng);
    CHECK(adder_truth_table_errors(adder) == 0);
}

// ---------------------------------------------------------------------------
// Ancilla invariant: all non-output ancilla wires = 0 after shuffle
// ---------------------------------------------------------------------------
static void test_shuffle_ancilla_invariant_plain() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);
    shuffle_depth_levels(adder, rng);
    CHECK(ancilla_violations(adder) == 0);
}

// ---------------------------------------------------------------------------
// Shuffle after sandwich: truth table and ancilla invariant still hold
// ---------------------------------------------------------------------------
static void test_shuffle_after_sandwich_truth_table() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);
    apply_sandwich(adder, {9, 10, 11, 12}, 8, rng);
    shuffle_depth_levels(adder, rng);
    CHECK(adder_truth_table_errors(adder) == 0);
}

static void test_shuffle_after_sandwich_ancilla_invariant() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);
    apply_sandwich(adder, {9, 10, 11, 12}, 8, rng);
    shuffle_depth_levels(adder, rng);
    CHECK(ancilla_violations(adder) == 0);
}

// ---------------------------------------------------------------------------
// Shuffle after sandwich: depth distribution unchanged
// ---------------------------------------------------------------------------
static void test_shuffle_after_sandwich_depth_distribution() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(7);
    apply_sandwich(adder, {9, 10, 11, 12}, 6, rng);

    auto d_before = adder.compute_depth_levels();
    std::sort(d_before.begin(), d_before.end());

    shuffle_depth_levels(adder, rng);

    auto d_after = adder.compute_depth_levels();
    std::sort(d_after.begin(), d_after.end());

    CHECK(d_before == d_after);
}

// ---------------------------------------------------------------------------
// reorder_gates: simple 3-gate circuit, explicit permutation check
// ---------------------------------------------------------------------------
static void test_reorder_gates_basic() {
    // Gates: CNOT(0→1), CNOT(1→2), NOT(2)
    // Reorder to [2, 0, 1] → NOT(2), CNOT(0→1), CNOT(1→2)
    // (note: this is NOT a valid topological order — just testing reorder mechanics)
    Circuit c(3, 1, {1, 2});
    c.add_gate(std::make_unique<CnotGate>(0, 1));  // gate 0: target=1
    c.add_gate(std::make_unique<CnotGate>(1, 2));  // gate 1: target=2
    c.add_gate(std::make_unique<NotGate>(2));       // gate 2: target=2

    c.reorder_gates({2, 0, 1});  // [NOT(2), CNOT(0→1), CNOT(1→2)]

    const auto& gs = c.gates_ref();
    CHECK(gs[0]->target_wire() == 2);  // NOT(2)
    CHECK(gs[1]->target_wire() == 1);  // CNOT(0→1)
    CHECK(gs[2]->target_wire() == 2);  // CNOT(1→2)
}

// ---------------------------------------------------------------------------
int main() {
    test_shuffle_gate_count_unchanged();
    test_shuffle_depth_distribution_unchanged();
    test_shuffle_same_depth_independent_after_shuffle();
    test_shuffle_deterministic();
    test_shuffle_different_seeds_differ();
    test_shuffle_truth_table_plain();
    test_shuffle_ancilla_invariant_plain();
    test_shuffle_after_sandwich_truth_table();
    test_shuffle_after_sandwich_ancilla_invariant();
    test_shuffle_after_sandwich_depth_distribution();
    test_reorder_gates_basic();

    if (s_failed == 0)
        std::cout << "All " << s_run << " tests passed.\n";
    else
        std::cerr << s_failed << "/" << s_run << " tests FAILED.\n";
    return s_failed == 0 ? 0 : 1;
}
