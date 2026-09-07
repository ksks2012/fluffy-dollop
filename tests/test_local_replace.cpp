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
        int hits = apply_local_replace(c, cfg, rng);
        CHECK(hits == 2);
    }
    {
        Circuit c = make_nbit_adder(4, 0);
        int hits = apply_local_replace(c, cfg, rng);
        CHECK(hits == 4);
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
    int hits = apply_local_replace(c, cfg, rng);
    CHECK(hits == 1);

    // +2 Toffoli, same CNOTs, 0 new wires: 3 gates -> 5 gates.
    CHECK(c.gate_count()     == before_gates + 2);
    CHECK(count_toffoli(c)   == before_toffoli + 2);
    CHECK(count_cnot(c)      == before_cnot);
    CHECK(c.num_wires()      == 4);   // 0 new wires

    // Single-pass non-overlap: within ONE call the replaced span is not
    // rescanned (hits==1 above, not 2), so R1 runs exactly once per window.
    // R1's middle is itself an S-triple, so a *fresh* invocation legitimately
    // re-matches; the pipeline runs this phase once per execution (ticket 0018).
    int hits2 = apply_local_replace(c, cfg, rng);
    CHECK(hits2 == 1);
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
        int hits = apply_local_replace(c, cfg, rng);

        // Hit count deterministic (bare adder window structure is fixed).
        CHECK(hits == (n == 2 ? 2 : 4));

        // Functional equivalence + ancilla invariant preserved.
        CHECK(check_truth_table(c));
        CHECK(check_ancilla_invariant(c));

        // Distinguishability: each window adds exactly 2 Toffoli, 0 CNOT, 0 wires.
        CHECK(c.gate_count()   == before_gates   + 2 * hits);
        CHECK(count_toffoli(c) == before_toffoli + 2 * hits);
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

    int hits = apply_local_replace(c, cfg, rng);
    CHECK(hits == 0);
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
// main
// ---------------------------------------------------------------------------
int main() {
    test_bare_adder_hit_counts();
    test_hand_built_single_window();
    test_bare_adder_equivalence();
    test_no_match_unchanged();

    if (s_failed == 0)
        std::cout << "All " << s_run << " tests passed.\n";
    else
        std::cerr << s_failed << "/" << s_run << " tests FAILED.\n";
    return s_failed == 0 ? 0 : 1;
}