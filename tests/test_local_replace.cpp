#include <iostream>
#include <memory>
#include <random>
#include <vector>
#include <algorithm>
#include "gate.h"
#include "circuit.hpp"
#include "adder.hpp"
#include "pipeline.hpp"
#include "local_replace.hpp"

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
// Count gate types in a circuit.
// ---------------------------------------------------------------------------
static int count_toffoli(const Circuit& c) {
    const auto& gs = c.gates_ref();
    int n = 0;
    for (const auto& g : gs)
        if (dynamic_cast<const ToffoliGate*>(g.get())) ++n;
    return n;
}

static int count_cnot(const Circuit& c) {
    const auto& gs = c.gates_ref();
    int n = 0;
    for (const auto& g : gs)
        if (dynamic_cast<const CnotGate*>(g.get())) ++n;
    return n;
}

// ---------------------------------------------------------------------------
// #1a. Bare-adder hit counts: R1 must hit 2 windows on n=2 and 4 on n=4.
// ---------------------------------------------------------------------------
static void test_bare_adder_hit_counts() {
    LocalReplaceConfig cfg;
    std::mt19937 rng(42);

    {
        Circuit c = make_nbit_adder(2, 0);
        ReplaceResult r = apply_local_replace(c, cfg, rng);
        CHECK(r.r1_hits == 2);
        CHECK(r.r2_hits == 0);
        CHECK(r.total() == 2);
    }
    {
        Circuit c = make_nbit_adder(4, 0);
        ReplaceResult r = apply_local_replace(c, cfg, rng);
        CHECK(r.r1_hits == 4);
        CHECK(r.r2_hits == 0);
        CHECK(r.total() == 4);
    }
}

// ---------------------------------------------------------------------------
// #1b. Hand-built single S-triple: the window shape is replaced exactly.
// ---------------------------------------------------------------------------
static void test_hand_built_single_window() {
    // Build a tiny circuit with exactly one S-triple:
    //   CNOT(0->3); CNOT(1->3); CNOT(2->3)
    // plus surrounding gates that read s so we can observe the effect.
    // outputs = {3} so evaluate returns [s].
    Circuit c(4, 3, {3});
    c.add_gate(std::make_unique<CnotGate>(0, 3));   // a -> s
    c.add_gate(std::make_unique<CnotGate>(1, 3));   // b -> s
    c.add_gate(std::make_unique<CnotGate>(2, 3));   // c -> s

    const int before_gates  = c.gate_count();
    const int before_toffoli = count_toffoli(c);
    const int before_cnot   = count_cnot(c);

    LocalReplaceConfig cfg;
    std::mt19937 rng(42);
    ReplaceResult r = apply_local_replace(c, cfg, rng);
    CHECK(r.r1_hits == 1);
    CHECK(r.r2_hits == 0);
    CHECK(r.total() == 1);

    // +2 Toffoli, same CNOTs, 0 new wires: 3 gates -> 5 gates.
    CHECK(c.gate_count()     == before_gates + 2);
    CHECK(count_toffoli(c)   == before_toffoli + 2);
    CHECK(count_cnot(c)      == before_cnot);
    CHECK(c.num_wires()      == 4);   // 0 new wires

    // Single-pass non-overlap: within ONE call the replaced span is not
    // rescanned (hits==1 above, not 2), so R1 runs exactly once per window.
    // R1's middle is itself an S-triple, so a *fresh* invocation legitimately
    // re-matches; the pipeline runs this phase once per execution (ticket 0018).
    ReplaceResult r2 = apply_local_replace(c, cfg, rng);
    CHECK(r2.r1_hits == 1);
    CHECK(r2.r2_hits == 0);
    CHECK(r2.total() == 1);
    CHECK(c.gate_count() == before_gates + 4);
}

// ---------------------------------------------------------------------------
// #2. Equivalence + distinguishability on bare adders (ticket 0017 Done gate).
// ---------------------------------------------------------------------------
static void test_bare_adder_equivalence() {
    LocalReplaceConfig cfg;
    for (int n : {2, 4}) {
        Circuit c = make_nbit_adder(n, 0);
        const int before_gates    = c.gate_count();
        const int before_toffoli  = count_toffoli(c);
        const int before_wires    = c.num_wires();

        std::mt19937 rng(7);
        ReplaceResult r = apply_local_replace(c, cfg, rng);

        // Hit count deterministic (bare adder window structure is fixed).
        CHECK(r.r1_hits == (n == 2 ? 2 : 4));
        CHECK(r.r2_hits == 0);
        CHECK(r.total() == (n == 2 ? 2 : 4));

        // Functional equivalence + ancilla invariant preserved.
        CHECK(check_truth_table(c));
        CHECK(check_ancilla_invariant(c));

        // Distinguishability: each window adds exactly 2 Toffoli, 0 CNOT, 0 wires.
        CHECK(c.gate_count()   == before_gates   + 2 * r.total());
        CHECK(count_toffoli(c) == before_toffoli + 2 * r.total());
        CHECK(count_cnot(c)    == c.gate_count() - count_toffoli(c));
        CHECK(c.num_wires()    == before_wires);
    }
}

// ---------------------------------------------------------------------------
// #3. No-match circuit stays unchanged (ticket 0017).
// ---------------------------------------------------------------------------
static void test_no_match_unchanged() {
    LocalReplaceConfig cfg;
    std::mt19937 rng(42);

    // Toffoli(a,b,s); CNOT(a,s); CNOT(b,s): only 2 consecutive CNOTs, no triple.
    Circuit c(4, 2, {1, 2});
    c.add_gate(std::make_unique<ToffoliGate>(0, 1, 2));
    c.add_gate(std::make_unique<CnotGate>(0, 1));
    c.add_gate(std::make_unique<CnotGate>(0, 1));
    c.add_gate(std::make_unique<CnotGate>(0, 1));

    const int before_gates = c.gate_count();
    const int before_wires = c.num_wires();
    auto before_types = [&]() { std::vector<int> t; for (auto& g : c.gates_ref())
        t.push_back(dynamic_cast<const CnotGate*>(g.get()) ? 1 : 0); return t; }();
    auto before_targets = [&]() { std::vector<int> t; for (auto& g : c.gates_ref())
        t.push_back(g->target_wire()); return t; }();

    ReplaceResult r = apply_local_replace(c, cfg, rng);
    CHECK(r.r1_hits == 0);
    CHECK(r.r2_hits == 0);
    CHECK(r.total() == 0);
    CHECK(c.gate_count() == before_gates);
    CHECK(c.num_wires()  == before_wires);

    // Gate sequence bit-for-bit unchanged.
    std::vector<int> after_types, after_targets;
    for (auto& g : c.gates_ref()) {
        after_types.push_back(dynamic_cast<const CnotGate*>(g.get()) ? 1 : 0);
        after_targets.push_back(g->target_wire());
    }
    CHECK(after_types == before_types);
    CHECK(after_targets == before_targets);
}

// ---------------------------------------------------------------------------
// #4. R2 hand-built single 3-Toffoli window.
// ---------------------------------------------------------------------------
static void test_r2_hand_built_window() {
    // Build: Toffoli(0,1,3); Toffoli(0,2,3); Toffoli(1,2,3)
    // 3 distinct controls {0,1,2}, same target 3.
    // z selection is index-based (gelý 4n+1 per ADR 0005): for n=1,
    // 4n+1 = 5, so wire 5 is the extra-ancilla z. The circuit has 6 wires.
    Circuit c(6, 3, {3});
    c.add_gate(std::make_unique<ToffoliGate>(0, 1, 3));
    c.add_gate(std::make_unique<ToffoliGate>(0, 2, 3));
    c.add_gate(std::make_unique<ToffoliGate>(1, 2, 3));

    const int before_gates  = c.gate_count();
    const int before_toffoli = count_toffoli(c);
    const int before_cnot   = count_cnot(c);

    LocalReplaceConfig cfg;
    cfg.enable_r2 = true;
    std::mt19937 rng(42);
    ReplaceResult r = apply_local_replace(c, cfg, rng);
    CHECK(r.r1_hits == 0);   // no S-triple
    CHECK(r.r2_hits == 1);   // one 3-Toffoli window
    CHECK(r.total() == 1);

    // +2 CNOT, same Toffoli, z borrowed and restored: 3 gates -> 5 gates.
    CHECK(c.gate_count()     == before_gates + 2);
    CHECK(count_toffoli(c)   == before_toffoli);   // unchanged
    CHECK(count_cnot(c)      == before_cnot + 2);
    CHECK(c.num_wires()      == 6);   // no new wires

    // Functional equivalence: T ⊕= maj(0,1,2) in all cases.
    // maj(a,b,c) = majority of 3 bits.
    for (int a = 0; a <= 1; ++a)
        for (int b = 0; b <= 1; ++b)
            for (int ci = 0; ci <= 1; ++ci) {
                int maj = (a&b) | (a&ci) | (b&ci);
                // Original: T = a&b ^ a&ci ^ b&ci = maj
                // After R2: same
                std::vector<int> input = {a, b, ci};
                auto out = c.evaluate(input);
                CHECK(out[0] == maj);
            }

    // z (wire 5) must be 0 after every evaluation (restored).
    // (Already checked by check_ancilla_invariant below.)
    CHECK(check_ancilla_invariant(c));
}

// ---------------------------------------------------------------------------
// #5. R2 bare-adder hit counts with extra_ancilla >= 1.
//     R1 runs first, then R2. On the bare adder (no sandwich), R1 consumes
//     some carry windows, so r2 == n (2 for n=2, 4 for n=4) — deterministic
//     (no RNG). This differs from the ADR's R2-standalone count (2n-1) because
//     R1 runs in the same pass. Gold standard: r2 == n, TT + ancilla PASS.
// ---------------------------------------------------------------------------
static void test_r2_bare_adder_hit_counts() {
    LocalReplaceConfig cfg;
    cfg.enable_r2 = true;
    std::mt19937 rng(42);

    {
        Circuit c = make_nbit_adder(2, 1);
        ReplaceResult r = apply_local_replace(c, cfg, rng);
        CHECK(r.r1_hits == 2);
        CHECK(r.r2_hits == 2);   // deterministic: r2 == n on the bare adder
        CHECK(r.total() == 4);
        CHECK(check_truth_table(c));
        CHECK(check_ancilla_invariant(c));
    }
    {
        Circuit c = make_nbit_adder(4, 1);
        ReplaceResult r = apply_local_replace(c, cfg, rng);
        CHECK(r.r1_hits == 4);
        CHECK(r.r2_hits == 4);   // deterministic: r2 == n on the bare adder
        CHECK(r.total() == 8);
        CHECK(check_truth_table(c));
        CHECK(check_ancilla_invariant(c));
    }
}

// ---------------------------------------------------------------------------
// #6. R2 + R1 combined: gate-type distinguishability on n=4, extra_ancilla=1.
//     R1 adds 2 Toffoli per hit; R2 adds 2 CNOT per hit.
//     Key invariant: delta Toffoli == 2*r1_hits, delta CNOT == 2*r2_hits.
// ---------------------------------------------------------------------------
static void test_r2_combined_gate_distinguishability() {
    Circuit c = make_nbit_adder(4, 1);
    const int before_toffoli = count_toffoli(c);
    const int before_cnot    = count_cnot(c);
    const int before_wires   = c.num_wires();

    LocalReplaceConfig cfg;
    cfg.enable_r2 = true;
    std::mt19937 rng(7);
    ReplaceResult r = apply_local_replace(c, cfg, rng);

    CHECK(r.r1_hits == 4);
    CHECK(r.r2_hits > 0);

    // R1: +2 Toffoli per hit; R2: +2 CNOT per hit
    CHECK(count_toffoli(c) == before_toffoli + 2 * r.r1_hits);
    CHECK(count_cnot(c)    == before_cnot    + 2 * r.r2_hits);
    CHECK(c.num_wires()    == before_wires);

    CHECK(check_truth_table(c));
    CHECK(check_ancilla_invariant(c));
}

// ---------------------------------------------------------------------------
// #8. Sound — T3 window-level exhaustive enumeration of the T-c rewrite.
//     Hand-built single S-triple + a borrow-ancilla wire z.
//     Wires: 0=a,1=b,2=c,3=s,4=unused,5=z. num_inputs = 4(s as input) → n=1
//     → 4n+1 = 5 → z = wire 5. Original: CNOT(a,s); CNOT(b,s); CNOT(c,s)
//     T-c:      CNOT(a,z); CNOT(z,s); CNOT(a,z); CNOT(b,s); CNOT(c,s)
// ---------------------------------------------------------------------------
static void test_sound_t3_window_enumeration() {
    // Build the window with z as the borrow ancilla wire (index 5, >= 4*1+1).
    Circuit c(6, 4, {3});   // wires 0..5; s is an input so we can enumerate it
    c.add_gate(std::make_unique<CnotGate>(0, 3));   // a -> s
    c.add_gate(std::make_unique<CnotGate>(1, 3));   // b -> s
    c.add_gate(std::make_unique<CnotGate>(2, 3));   // c -> s

    const int before_gates = c.gate_count();
    const int before_cnot  = count_cnot(c);

    LocalReplaceConfig cfg;
    cfg.enable_sound = true;
    std::mt19937 rng(42);
    ReplaceResult r = apply_local_replace(c, cfg, rng);
    CHECK(r.sound_hits == 1);
    CHECK(r.r1_hits == 0);
    CHECK(r.r2_hits == 0);
    CHECK(r.total() == 1);

    // 3 CNOT -> 5 CNOT, no new wires, no Toffoli added.
    CHECK(c.gate_count() == before_gates + 2);
    CHECK(count_cnot(c)  == before_cnot + 2);
    CHECK(count_toffoli(c) == 0);
    CHECK(c.num_wires()  == 6);

    // Exhaustive enumeration over all 16 assignments (a,b,c,s); z starts at 0.
    // Expected: s' = s ⊕ a ⊕ b ⊕ c, z' = 0 for every assignment.
    for (int a = 0; a <= 1; ++a)
        for (int b = 0; b <= 1; ++b)
            for (int cc = 0; cc <= 1; ++cc)
                for (int ss = 0; ss <= 1; ++ss) {
                    std::vector<int> input = {a, b, cc, ss};
                    auto out = c.evaluate(input);
                    CHECK(out[0] == (ss ^ a ^ b ^ cc));
                    const auto& ws = c.last_wires();
                    CHECK(ws[5] == 0);           // z restored to 0 at window end
                }
    // z lights up mid-window when a==1: first gate is CNOT(a,z), so with a=1,
    // after gate 1 z must be 1; the full rewrite still ends z=0 and keeps TT.
    // (The existence proof at gate granularity is ticket 0028 T5; here we pin
    //  the structural fact that the rewrite STARTS with CNOT whose control is
    //  a and target is z.)
    CHECK(c.gates_ref()[0]->target_wire() == 5);          // first gate targets z
    CHECK(dynamic_cast<const CnotGate*>(c.gates_ref()[0].get()) != nullptr);
    CHECK(check_ancilla_invariant(c));
}

// ---------------------------------------------------------------------------
// #9. Sound extra_ancilla=0 degenerate case: sound_hits == 0, no crash.
// ---------------------------------------------------------------------------
static void test_sound_no_extra_ancilla() {
    LocalReplaceConfig cfg;
    cfg.enable_sound = true;
    std::mt19937 rng(42);

    Circuit c = make_nbit_adder(4, 0);
    const int before_wires = c.num_wires();
    ReplaceResult r = apply_local_replace(c, cfg, rng);

    CHECK(r.sound_hits == 0);   // no z available → zero Sound hits, no crash
    CHECK(c.num_wires() == before_wires);
    CHECK(check_truth_table(c));
    CHECK(check_ancilla_invariant(c));
}

// ---------------------------------------------------------------------------
// #10. Sound bare-adder hit counts n=2/4, extra_ancilla>=1: TT + ancilla PASS,
//      sound_hits recorded deterministically (no RNG). Sound runs as Pass 0
//      before R1; window shape is same as R1 (S-triple), so the exact hit
//      counts get locked in this ticket on first green run.
// ---------------------------------------------------------------------------
static void test_sound_bare_adder_hit_counts() {
    LocalReplaceConfig cfg;
    cfg.enable_sound = true;
    std::mt19937 rng(42);

    {
        Circuit c = make_nbit_adder(2, 1);
        ReplaceResult r = apply_local_replace(c, cfg, rng);
        CHECK(r.sound_hits == 2);
        CHECK(r.r1_hits == 0);   // Sound consumes every S-triple first
        CHECK(r.r2_hits == 0);
        CHECK(r.total() == 2);
        CHECK(check_truth_table(c));
        CHECK(check_ancilla_invariant(c));
    }
    {
        Circuit c = make_nbit_adder(4, 1);
        ReplaceResult r = apply_local_replace(c, cfg, rng);
        CHECK(r.sound_hits == 4);
        CHECK(r.r1_hits == 0);
        CHECK(r.r2_hits == 0);
        CHECK(r.total() == 4);
        CHECK(check_truth_table(c));
        CHECK(check_ancilla_invariant(c));
    }
}

// ---------------------------------------------------------------------------
// #11. Sound — T5 intermediate-state existence proof (half-sequence
//      recombination, per ADR 0006 Testing Decisions). No production
//      partial-evaluate API: the "half" circuit is assembled test-locally from
//      the PRODUCED rewrite's first gate only, proving z == 1 exists mid-window
//      for a reproducible input (a==1), while the completed window restores
//      z == 0 and stays TT-equivalent (equivalence pinned in T3, #8/#10).
// ---------------------------------------------------------------------------
static void test_sound_t5_intermediate_z() {
    // Build a single S-triple window, run Sound, then inspect the produced
    // rewrite.
    Circuit c(6, 4, {3});   // a=0, b=1, c=2, s=3; z = wire 5 (>= 4*1+1)
    c.add_gate(std::make_unique<CnotGate>(0, 3));
    c.add_gate(std::make_unique<CnotGate>(1, 3));
    c.add_gate(std::make_unique<CnotGate>(2, 3));

    LocalReplaceConfig cfg;
    cfg.enable_sound = true;
    std::mt19937 rng(42);
    ReplaceResult r = apply_local_replace(c, cfg, rng);
    CHECK(r.sound_hits == 1);

    // The produced rewrite must START with CNOT(control=a=0, target=z=5):
    // this is the first gate that writes z, so z carries the a-charge mid-window.
    const CnotGate* cn = dynamic_cast<const CnotGate*>(c.gates_ref()[0].get());
    CHECK(cn != nullptr);
    CHECK(cn->control() == 0);   // a
    CHECK(cn->target()  == 5);   // z

    // Half-sequence recombination: a fresh circuit consisting of ONLY that
    // preamble gate. Evaluation is full-circuit (no partial-evaluate helper),
    // so this IS the window's exact intermediate state truncated at gate 1.
    Circuit half(6, 1, {5});   // input = {a} (wire 0), output = z (wire 5)
    half.add_gate(std::make_unique<CnotGate>(0, 5));
    CHECK(half.evaluate({1})[0] == 1);   // a==1 → z==1 mid-window (existence)
    CHECK(half.evaluate({0})[0] == 0);   // a==0 → z==0 (no spurious charge)

    // Completed window: z restored to 0, net s ⊕= a⊕b⊕c.
    c.evaluate({1, 1, 1, 0});
    CHECK(c.last_wires()[5] == 0);       // z restored at window end
    CHECK(c.last_wires()[3] == 1);       // s = 0 ⊕ 1⊕1⊕1 = 1
}

// ---------------------------------------------------------------------------
// #7. R2 extra_ancilla=0 degenerate case: r2_hits == 0, no crash.
// ---------------------------------------------------------------------------
static void test_r2_no_extra_ancilla() {
    LocalReplaceConfig cfg;
    cfg.enable_r2 = true;
    std::mt19937 rng(42);

    Circuit c = make_nbit_adder(4, 0);
    const int before_wires = c.num_wires();
    ReplaceResult r = apply_local_replace(c, cfg, rng);

    CHECK(r.r1_hits == 4);   // R1 still matches (no extra wire needed)
    CHECK(r.r2_hits == 0);   // no z available → zero hits
    CHECK(r.total() == 4);
    CHECK(c.num_wires() == before_wires);
    CHECK(check_truth_table(c));
    CHECK(check_ancilla_invariant(c));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    test_bare_adder_hit_counts();
    test_hand_built_single_window();
    test_bare_adder_equivalence();
    test_no_match_unchanged();
    test_r2_hand_built_window();
    test_r2_bare_adder_hit_counts();
    test_r2_combined_gate_distinguishability();
    test_r2_no_extra_ancilla();
    test_sound_t3_window_enumeration();
    test_sound_no_extra_ancilla();
    test_sound_bare_adder_hit_counts();
    test_sound_t5_intermediate_z();

    if (s_failed == 0)
        std::cout << "All " << s_run << " tests passed.\n";
    else
        std::cerr << s_failed << "/" << s_run << " tests FAILED.\n";
    return s_failed == 0 ? 0 : 1;
}