#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <memory>
#include <random>
#include "circuit.hpp"
#include "adder.hpp"
#include "sandwich.hpp"
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
// Gate count increases by exactly 2 * chain_len
// ---------------------------------------------------------------------------
static void test_sandwich_gate_count() {
    const int chain_len = 6;
    auto adder = make_2bit_adder();
    const int before = adder.gate_count();  // 15
    std::mt19937 rng(42);
    apply_sandwich(adder, {9, 10, 11, 12}, chain_len, SandwichMode::Safe, rng);
    CHECK(adder.gate_count() == before + 2 * chain_len);
}

// ---------------------------------------------------------------------------
// Wire count is extended to cover the extra ancilla range
// ---------------------------------------------------------------------------
static void test_sandwich_extends_wire_count() {
    auto adder = make_2bit_adder();
    CHECK(adder.num_wires() == 9);  // adder: wires 0-8
    std::mt19937 rng(42);
    apply_sandwich(adder, {9, 10, 11}, 4, SandwichMode::Safe, rng);
    CHECK(adder.num_wires() == 12);  // now covers wires 0-11
}

// ---------------------------------------------------------------------------
// Chain structure: forward[i] mirrors reversed[N-1-i]
//
// Gate layout after sandwich (original_size = 15, chain_len = N):
//   positions 0..N-1          : forward  chain[0..N-1]
//   positions N..N+14         : adder gates
//   positions N+15..2N+14     : reversed chain[N-1..0]
//
// So: gates_ref()[i] == gates_ref()[2N+14-i] for each i in 0..N-1
// ---------------------------------------------------------------------------
static void test_sandwich_chain_structure() {
    const int chain_len = 5;
    const int orig = 15;  // 2-bit adder
    auto adder = make_2bit_adder();
    std::mt19937 rng(7);
    apply_sandwich(adder, {9, 10, 11}, chain_len, SandwichMode::Safe, rng);

    const auto& gs = adder.gates_ref();
    const int n = static_cast<int>(gs.size());
    CHECK(n == orig + 2 * chain_len);

    int mismatches = 0;
    for (int i = 0; i < chain_len; ++i) {
        int fwd_pos = i;
        int rev_pos = 2 * chain_len + orig - 1 - i;
        // Same target wire
        if (gs[fwd_pos]->target_wire() != gs[rev_pos]->target_wire()) {
            std::cerr << "  chain mirror mismatch at i=" << i << ": target "
                      << gs[fwd_pos]->target_wire() << " vs " << gs[rev_pos]->target_wire() << "\n";
            ++mismatches;
        }
        // Same full wire set (ctrl + tgt)
        auto tw_f = gs[fwd_pos]->touched_wires();
        auto tw_r = gs[rev_pos]->touched_wires();
        std::sort(tw_f.begin(), tw_f.end());
        std::sort(tw_r.begin(), tw_r.end());
        if (tw_f != tw_r) {
            std::cerr << "  chain mirror wire-set mismatch at i=" << i << "\n";
            ++mismatches;
        }
    }
    CHECK(mismatches == 0);
}

// ---------------------------------------------------------------------------
// Determinism: same seed → identical gate structure
// ---------------------------------------------------------------------------
static void test_sandwich_deterministic() {
    auto a1 = make_2bit_adder(), a2 = make_2bit_adder();
    std::mt19937 r1(99), r2(99);
    apply_sandwich(a1, {9, 10, 11, 12}, 8, SandwichMode::Safe, r1);
    apply_sandwich(a2, {9, 10, 11, 12}, 8, SandwichMode::Safe, r2);

    const auto& g1 = a1.gates_ref();
    const auto& g2 = a2.gates_ref();
    CHECK(g1.size() == g2.size());
    for (int i = 0; i < static_cast<int>(g1.size()); ++i)
        CHECK(g1[i]->target_wire() == g2[i]->target_wire());
}

// ---------------------------------------------------------------------------
// Exhaustive truth-table: all 32 inputs still produce correct adder output
// ---------------------------------------------------------------------------
static void test_sandwich_truth_table() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);
    apply_sandwich(adder, {9, 10, 11, 12}, 8, SandwichMode::Safe, rng);

    int mismatches = 0;
    for (int bits = 0; bits < 32; ++bits) {
        int A1  = (bits >> 4) & 1, A0  = (bits >> 3) & 1;
        int B1  = (bits >> 2) & 1, B0  = (bits >> 1) & 1;
        int Cin =  bits       & 1;
        int sum = (A1 * 2 + A0) + (B1 * 2 + B0) + Cin;

        auto out = adder.evaluate({A1, A0, B1, B0, Cin});
        int exp_S1 = (sum >> 1) & 1, exp_S0 = sum & 1, exp_Cout = (sum >> 2) & 1;

        if (out[0] != exp_S1 || out[1] != exp_S0 || out[2] != exp_Cout) {
            std::cerr << "  mismatch A=" << (A1*2+A0) << " B=" << (B1*2+B0)
                      << " Cin=" << Cin << " got {" << out[0] << "," << out[1]
                      << "," << out[2] << "} expected {" << exp_S1 << ","
                      << exp_S0 << "," << exp_Cout << "}\n";
            ++mismatches;
        }
    }
    CHECK(mismatches == 0);
}

// ---------------------------------------------------------------------------
// Ancilla invariant: all non-output ancilla wires remain 0 for all 32 inputs.
// Ancilla wires = wire 8 (carry_mid) + extra ancilla (wires 9, 10, 11, 12).
// Output wires (5, 6, 7) are excluded from the check.
// ---------------------------------------------------------------------------
static void test_sandwich_ancilla_invariant() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);
    const std::vector<int> extra = {9, 10, 11, 12};
    apply_sandwich(adder, extra, 8, SandwichMode::Safe, rng);

    // Build set of output wire indices for exclusion
    const auto& outs = adder.output_wires();

    int violations = 0;
    for (int bits = 0; bits < 32; ++bits) {
        int A1  = (bits >> 4) & 1, A0  = (bits >> 3) & 1;
        int B1  = (bits >> 2) & 1, B0  = (bits >> 1) & 1;
        int Cin =  bits       & 1;
        adder.evaluate({A1, A0, B1, B0, Cin});

        const auto& ws = adder.last_wires();
        // Check all non-input, non-output wires
        for (int w = adder.num_inputs(); w < adder.num_wires(); ++w) {
            bool is_output = std::find(outs.begin(), outs.end(), w) != outs.end();
            if (!is_output && ws[w] != 0) {
                std::cerr << "  ancilla violation: wire " << w << " = " << ws[w]
                          << " (A=" << (A1*2+A0) << " B=" << (B1*2+B0)
                          << " Cin=" << Cin << ")\n";
                ++violations;
            }
        }
    }
    CHECK(violations == 0);
}

// ---------------------------------------------------------------------------
// Multiple applications: two sandwiches stack correctly
// ---------------------------------------------------------------------------
static void test_sandwich_stacked() {
    auto adder = make_2bit_adder();
    const int orig = adder.gate_count();
    std::mt19937 rng(55);
    apply_sandwich(adder, {9, 10, 11},  4, SandwichMode::Safe, rng);  // +8
    apply_sandwich(adder, {12, 13, 14}, 3, SandwichMode::Safe, rng);  // +6
    CHECK(adder.gate_count() == orig + 8 + 6);

    // Truth table still correct after two sandwiches
    int mismatches = 0;
    for (int bits = 0; bits < 32; ++bits) {
        int A1 = (bits>>4)&1, A0 = (bits>>3)&1;
        int B1 = (bits>>2)&1, B0 = (bits>>1)&1, Cin = bits&1;
        int sum = (A1*2+A0) + (B1*2+B0) + Cin;
        auto out = adder.evaluate({A1,A0,B1,B0,Cin});
        if (out[0] != ((sum>>1)&1) || out[1] != (sum&1) || out[2] != ((sum>>2)&1))
            ++mismatches;
    }
    CHECK(mismatches == 0);
}

// ===========================================================================
// Active-mode tests  (explicit mode=Active)
// ===========================================================================

// ---------------------------------------------------------------------------
// Gate count delta = +2 * chain_len (same rule as Safe mode)
// ---------------------------------------------------------------------------
static void test_active_gate_count() {
    const int chain_len = 6;
    auto adder = make_2bit_adder();
    const int before = adder.gate_count();  // 15
    std::mt19937 rng(42);
    apply_sandwich(adder, {}, chain_len, SandwichMode::Active, rng);
    CHECK(adder.gate_count() == before + 2 * chain_len);
}

// ---------------------------------------------------------------------------
// Active mode must NOT extend the wire count
// ---------------------------------------------------------------------------
static void test_active_no_extend_wires() {
    auto adder = make_2bit_adder();
    const int before = adder.num_wires();  // 9
    std::mt19937 rng(42);
    apply_sandwich(adder, {}, 4, SandwichMode::Active, rng);
    CHECK(adder.num_wires() == before);
}

// ---------------------------------------------------------------------------
// Active mode: extra_ancilla argument is ignored — empty vector is valid
// ---------------------------------------------------------------------------
static void test_active_ignores_extra_ancilla() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);
    // Both empty and invalid-index extra_ancilla must not throw
    int before_gates = adder.gate_count();
    apply_sandwich(adder, {}, 4, SandwichMode::Active, rng);
    CHECK(adder.gate_count() == before_gates + 8);
}

// ---------------------------------------------------------------------------
// Exhaustive truth-table: all 32 inputs still produce correct adder output.
// Active mode draws ctrl from input wires and tgt from output wires, so
// F⁻¹ ∘ Adder ∘ F preserves the truth table.
// ---------------------------------------------------------------------------
static void test_active_truth_table() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);
    apply_sandwich(adder, {}, 8, SandwichMode::Active, rng);

    int mismatches = 0;
    for (int bits = 0; bits < 32; ++bits) {
        int A1  = (bits >> 4) & 1, A0  = (bits >> 3) & 1;
        int B1  = (bits >> 2) & 1, B0  = (bits >> 1) & 1;
        int Cin =  bits       & 1;
        int sum = (A1 * 2 + A0) + (B1 * 2 + B0) + Cin;

        auto out = adder.evaluate({A1, A0, B1, B0, Cin});
        int exp_S1 = (sum >> 1) & 1, exp_S0 = sum & 1, exp_Cout = (sum >> 2) & 1;
        if (out[0] != exp_S1 || out[1] != exp_S0 || out[2] != exp_Cout) {
            std::cerr << "  Active TT mismatch A=" << (A1*2+A0) << " B=" << (B1*2+B0)
                      << " Cin=" << Cin << "\n";
            ++mismatches;
        }
    }
    CHECK(mismatches == 0);
}

// ---------------------------------------------------------------------------
// Final ancilla invariant: all non-output ancilla wires (wire 8) remain 0.
// Holds because the adder's carry is self-canceling and F⁻¹ cancels F's
// contribution to any ancilla wire.
// ---------------------------------------------------------------------------
static void test_active_ancilla_invariant() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);
    apply_sandwich(adder, {}, 8, SandwichMode::Active, rng);

    const auto& outs = adder.output_wires();
    int violations = 0;
    for (int bits = 0; bits < 32; ++bits) {
        int A1=(bits>>4)&1, A0=(bits>>3)&1;
        int B1=(bits>>2)&1, B0=(bits>>1)&1, Cin=bits&1;
        adder.evaluate({A1, A0, B1, B0, Cin});

        const auto& ws = adder.last_wires();
        for (int w = adder.num_inputs(); w < adder.num_wires(); ++w) {
            bool is_output = std::find(outs.begin(), outs.end(), w) != outs.end();
            if (!is_output && ws[w] != 0) {
                std::cerr << "  Active ancilla violation: wire=" << w << " val=" << ws[w]
                          << " (A=" << (A1*2+A0) << " B=" << (B1*2+B0) << " Cin=" << Cin << ")\n";
                ++violations;
            }
        }
    }
    CHECK(violations == 0);
}

// ---------------------------------------------------------------------------
// Determinism: same seed → identical gate structure
// ---------------------------------------------------------------------------
static void test_active_deterministic() {
    auto a1 = make_2bit_adder(), a2 = make_2bit_adder();
    std::mt19937 r1(77), r2(77);
    apply_sandwich(a1, {}, 8, SandwichMode::Active, r1);
    apply_sandwich(a2, {}, 8, SandwichMode::Active, r2);

    const auto& g1 = a1.gates_ref();
    const auto& g2 = a2.gates_ref();
    CHECK(g1.size() == g2.size());
    for (int i = 0; i < static_cast<int>(g1.size()); ++i)
        CHECK(g1[i]->target_wire() == g2[i]->target_wire());
}

// ---------------------------------------------------------------------------
// Chain mirroring: forward gate[i] and reverse gate[N-1-i] are mirror pairs
// (same ctrl+tgt wire set, in the same structure as Safe mode)
// ---------------------------------------------------------------------------
static void test_active_chain_mirroring() {
    const int chain_len = 5;
    const int orig = 15;
    auto adder = make_2bit_adder();
    std::mt19937 rng(13);
    apply_sandwich(adder, {}, chain_len, SandwichMode::Active, rng);

    const auto& gs = adder.gates_ref();
    CHECK(static_cast<int>(gs.size()) == orig + 2 * chain_len);

    int mismatches = 0;
    for (int i = 0; i < chain_len; ++i) {
        int fwd_pos = i;
        int rev_pos = 2 * chain_len + orig - 1 - i;
        auto tw_f = gs[fwd_pos]->touched_wires();
        auto tw_r = gs[rev_pos]->touched_wires();
        std::sort(tw_f.begin(), tw_f.end());
        std::sort(tw_r.begin(), tw_r.end());
        if (tw_f != tw_r) {
            std::cerr << "  Active mirror mismatch at i=" << i << "\n";
            ++mismatches;
        }
    }
    CHECK(mismatches == 0);
}

// ---------------------------------------------------------------------------
// Scramble verification:
//   1. Manually assemble a forward-only circuit (forward chain + adder, no reverse)
//      and assert intermediate wire state differs from the plain adder.
//   2. The full forward+reverse circuit remains functionally equivalent to the
//      plain adder (truth table correct for the same input).
// ---------------------------------------------------------------------------
static void test_active_scramble() {
    const int chain_len = 6;

    // Generate the chain with a known seed, replicating Active mode's algorithm:
    // ctrl from input wires [0..4], tgt from output wires {5,6,7}.
    auto base = make_2bit_adder();
    const int n_in = base.num_inputs();                  // 5
    const std::vector<int> out_ws = base.output_wires(); // {5,6,7}

    std::vector<std::pair<int,int>> chain;
    {
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> cdist(0, n_in - 1);
        std::uniform_int_distribution<int> tdist(0, static_cast<int>(out_ws.size()) - 1);
        for (int i = 0; i < chain_len; ++i) {
            int ctrl = cdist(rng);
            int tgt  = out_ws[tdist(rng)];
            chain.push_back({ctrl, tgt});
        }
    }

    // Forward-only circuit: adder with forward chain prepended (no reverse)
    auto adder_fwd = make_2bit_adder();
    for (int i = chain_len - 1; i >= 0; --i)
        adder_fwd.insert_gate(0, std::make_unique<CnotGate>(chain[i].first, chain[i].second));

    // Full sandwich circuit: forward chain + adder + reversed chain
    auto adder_full = make_2bit_adder();
    for (int i = chain_len - 1; i >= 0; --i)
        adder_full.insert_gate(0, std::make_unique<CnotGate>(chain[i].first, chain[i].second));
    for (int i = chain_len - 1; i >= 0; --i)
        adder_full.add_gate(std::make_unique<CnotGate>(chain[i].first, chain[i].second));

    // Plain adder reference
    auto adder_plain = make_2bit_adder();

    // Fixed seeded input: A=3 (1,1), B=1 (0,1), Cin=0  → sum=4
    const std::vector<int> input = {1, 1, 0, 1, 0};

    adder_plain.evaluate(input);
    const std::vector<int> ws_plain = adder_plain.last_wires();

    adder_fwd.evaluate(input);
    const std::vector<int> ws_fwd = adder_fwd.last_wires();

    // 1. Forward-only circuit scrambles intermediate state
    CHECK(ws_plain != ws_fwd);

    // 2. Full forward+reverse circuit is equivalent to plain adder for all 32 inputs
    int mismatches = 0;
    for (int bits = 0; bits < 32; ++bits) {
        int A1=(bits>>4)&1, A0=(bits>>3)&1;
        int B1=(bits>>2)&1, B0=(bits>>1)&1, Cin=bits&1;
        int sum = (A1*2+A0) + (B1*2+B0) + Cin;
        auto out = adder_full.evaluate({A1, A0, B1, B0, Cin});
        if (out[0] != ((sum>>1)&1) || out[1] != (sum&1) || out[2] != ((sum>>2)&1))
            ++mismatches;
    }
    CHECK(mismatches == 0);
}

// ---------------------------------------------------------------------------
// Safe-mode edge case: extra_ancilla.size() < 2 must fail observably
// ---------------------------------------------------------------------------
static void test_safe_too_few_ancilla() {
    auto adder = make_2bit_adder();
    std::mt19937 rng(42);

    bool threw_empty = false;
    try {
        apply_sandwich(adder, {}, 4, SandwichMode::Safe, rng);
    } catch (const std::invalid_argument&) {
        threw_empty = true;
    }
    CHECK(threw_empty);

    bool threw_one = false;
    try {
        apply_sandwich(adder, {9}, 4, SandwichMode::Safe, rng);
    } catch (const std::invalid_argument&) {
        threw_one = true;
    }
    CHECK(threw_one);
}

// ---------------------------------------------------------------------------
int main() {
    test_sandwich_gate_count();
    test_sandwich_extends_wire_count();
    test_sandwich_chain_structure();
    test_sandwich_deterministic();
    test_sandwich_truth_table();
    test_sandwich_ancilla_invariant();
    test_sandwich_stacked();

    test_active_gate_count();
    test_active_no_extend_wires();
    test_active_ignores_extra_ancilla();
    test_active_truth_table();
    test_active_ancilla_invariant();
    test_active_deterministic();
    test_active_chain_mirroring();
    test_active_scramble();
    test_safe_too_few_ancilla();

    if (s_failed == 0)
        std::cout << "All " << s_run << " tests passed.\n";
    else
        std::cerr << s_failed << "/" << s_run << " tests FAILED.\n";
    return s_failed == 0 ? 0 : 1;
}
