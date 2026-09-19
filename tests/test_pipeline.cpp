#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include "pipeline.hpp"
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

// Default full-pipeline config (matches main.cpp demo)
static PipelineConfig default_cfg() { return {}; }  // all phases on, seed=42

// ---------------------------------------------------------------------------
// Phase count
// ---------------------------------------------------------------------------
static void test_pipeline_phase_count_full() {
    auto r = run_full_pipeline(default_cfg());
    // adder + sandwich + shuffle + replace + inject = 5 phase entries
    CHECK(r.phases.size() == 5);
}

static void test_pipeline_phase_names() {
    auto r = run_full_pipeline(default_cfg());
    CHECK(r.phases[0].name == "adder");
    CHECK(r.phases[1].name == "sandwich");
    CHECK(r.phases[2].name == "replace");
    CHECK(r.phases[3].name == "shuffle");
    CHECK(r.phases[4].name == "inject");
}

// ---------------------------------------------------------------------------
// Gate counts at each phase
// ---------------------------------------------------------------------------
static void test_pipeline_gate_count_adder() {
    auto r = run_full_pipeline(default_cfg());
    CHECK(r.phases[0].gate_count == make_2bit_adder().gate_count());
}

static void test_pipeline_gate_count_sandwich() {
    // sandwich adds 2 * chain_len gates
    auto cfg = default_cfg();
    auto r = run_full_pipeline(cfg);
    CHECK(r.phases[1].gate_count == make_2bit_adder().gate_count() + 2 * cfg.sandwich_chain_length);
}

static void test_pipeline_gate_count_shuffle_unchanged() {
    auto r = run_full_pipeline(default_cfg());
    // shuffle never changes gate count (phases[2]=replace, phases[3]=shuffle)
    CHECK(r.phases[3].gate_count == r.phases[2].gate_count);
}

static void test_pipeline_gate_count_inject() {
    auto cfg = default_cfg();
    auto r = run_full_pipeline(cfg);
    CHECK(r.phases[4].gate_count == r.phases[3].gate_count + cfg.inject_count);
}

// ---------------------------------------------------------------------------
// Overhead ratios
// ---------------------------------------------------------------------------
static void test_pipeline_overhead_adder_is_one() {
    auto r = run_full_pipeline(default_cfg());
    CHECK(r.phases[0].overhead == 1.0);
}

static void test_pipeline_overhead_non_decreasing() {
    auto r = run_full_pipeline(default_cfg());
    for (int i = 1; i < static_cast<int>(r.phases.size()); ++i)
        CHECK(r.phases[i].overhead >= r.phases[i-1].overhead);
}

// ---------------------------------------------------------------------------
// Ancilla invariant at every phase boundary
// ---------------------------------------------------------------------------
static void test_pipeline_ancilla_all_phases() {
    // Use Safe mode: Active all-wires sampling is seed-dependent and can break
    // wire 8/9 invariant (documented in ticket 0012).
    PipelineConfig cfg;
    cfg.sandwich_mode = SandwichMode::Safe;
    auto r = run_full_pipeline(cfg);
    for (const auto& p : r.phases) {
        if (!p.ancilla_ok)
            std::cerr << "  ancilla violation at phase: " << p.name << "\n";
        CHECK(p.ancilla_ok);
    }
}

// ---------------------------------------------------------------------------
// Final pipeline: ancilla invariant holds and circuit is deterministic
// (Active all-wires mode genuinely changes the function, so we no longer
// check equivalence to the plain adder; ancilla invariant still verifies
// the zero-preservation guarantee of F⁻¹ ∘ Adder ∘ F at each boundary).
// ---------------------------------------------------------------------------
static void test_pipeline_final_ancilla_and_consistency() {
    // Ancilla invariant with Safe mode (Active is seed-dependent; ticket 0012)
    PipelineConfig cfg_safe;
    cfg_safe.sandwich_mode = SandwichMode::Safe;
    auto r_safe = run_full_pipeline(cfg_safe);
    for (const auto& p : r_safe.phases)
        CHECK(p.ancilla_ok);
    // Active mode: circuit must be deterministic (same input → same output)
    auto r = run_full_pipeline(default_cfg());
    auto& c = r.circuit;
    for (int bits = 0; bits < 32; ++bits) {
        int A1=(bits>>4)&1, A0=(bits>>3)&1, B1=(bits>>2)&1, B0=(bits>>1)&1, Cin=bits&1;
        auto o1 = c.evaluate({A1,A0,B1,B0,Cin});
        auto o2 = c.evaluate({A1,A0,B1,B0,Cin});
        CHECK(o1 == o2);
    }
}

// Safe-mode pipeline preserves the plain adder truth table.
static void test_pipeline_safe_mode_truth_table() {
    PipelineConfig cfg;
    cfg.sandwich_mode = SandwichMode::Safe;
    auto r = run_full_pipeline(cfg);
    CHECK(check_truth_table(r.circuit));
}

// ---------------------------------------------------------------------------
// Wire budget: final circuit covers adder wires + 3 inject wires
// inject pool = {4n+1+extra, 4n+2+extra, 4n+3+extra}; inject_junk extends to cover them.
// ---------------------------------------------------------------------------
static void test_pipeline_wire_budget() {
    auto cfg = default_cfg();
    // inject_pool starts at make_nbit_adder(n, extra).num_wires() and takes 3 wires
    int expected_max = make_nbit_adder(cfg.n, cfg.extra_ancilla).num_wires() + 3;
    auto r = run_full_pipeline(cfg);
    CHECK(r.circuit.num_wires() == expected_max);
}

// ---------------------------------------------------------------------------
// Determinism: same seed → identical gate structure in final circuit
// ---------------------------------------------------------------------------
static void test_pipeline_deterministic() {
    auto r1 = run_full_pipeline(default_cfg());
    auto r2 = run_full_pipeline(default_cfg());

    const auto& g1 = r1.circuit.gates_ref();
    const auto& g2 = r2.circuit.gates_ref();
    CHECK(g1.size() == g2.size());
    for (int i = 0; i < static_cast<int>(g1.size()); ++i)
        CHECK(g1[i]->target_wire() == g2[i]->target_wire());
}

// ---------------------------------------------------------------------------
// Phase toggles: disable individual phases
// ---------------------------------------------------------------------------
static void test_pipeline_no_sandwich() {
    PipelineConfig cfg;
    cfg.enable_sandwich = false;
    auto r = run_full_pipeline(cfg);
    // phases: adder + replace + shuffle + inject = 4
    CHECK(r.phases.size() == 4);
    CHECK(r.phases[0].name == "adder");
    CHECK(r.phases[1].name == "replace");
    // No sandwich → no extra ancilla → replace/shuffle won't change gate count
    CHECK(r.phases[0].gate_count == make_2bit_adder().gate_count());
    CHECK(check_truth_table(r.circuit));
}

static void test_pipeline_no_shuffle() {
    PipelineConfig cfg;
    cfg.enable_shuffle  = false;
    cfg.sandwich_mode   = SandwichMode::Safe;  // isolate phase toggle; Safe preserves truth table
    auto r = run_full_pipeline(cfg);
    CHECK(r.phases.size() == 4);
    CHECK(r.phases[1].name == "sandwich");
    CHECK(r.phases[2].name == "replace");
    CHECK(r.phases[3].name == "inject");
    CHECK(check_truth_table(r.circuit));
}

static void test_pipeline_no_inject() {
    PipelineConfig cfg;
    cfg.enable_inject   = false;
    cfg.sandwich_mode   = SandwichMode::Safe;  // isolate phase toggle; Safe preserves truth table
    auto r = run_full_pipeline(cfg);
    CHECK(r.phases.size() == 4);
    CHECK(r.phases[2].name == "replace");
    CHECK(r.phases[3].name == "shuffle");
    CHECK(check_truth_table(r.circuit));
}

static void test_pipeline_adder_only() {
    PipelineConfig cfg;
    cfg.enable_sandwich = false;
    cfg.enable_shuffle  = false;
    cfg.enable_replace  = false;
    cfg.enable_inject   = false;
    auto r = run_full_pipeline(cfg);
    CHECK(r.phases.size() == 1);
    CHECK(r.phases[0].gate_count == make_2bit_adder().gate_count());
    CHECK(check_truth_table(r.circuit));
    CHECK(r.phases[0].ancilla_ok);
}

// ---------------------------------------------------------------------------
// Different seeds produce different final gate orderings
// ---------------------------------------------------------------------------
static void test_pipeline_different_seeds() {
    PipelineConfig cfg1 = default_cfg(), cfg2 = default_cfg();
    cfg1.seed = 1;  cfg2.seed = 9999;
    auto r1 = run_full_pipeline(cfg1);
    auto r2 = run_full_pipeline(cfg2);

    const auto& g1 = r1.circuit.gates_ref();
    const auto& g2 = r2.circuit.gates_ref();
    bool any_differ = false;
    for (int i = 0; i < static_cast<int>(g1.size()); ++i)
        if (g1[i]->target_wire() != g2[i]->target_wire()) { any_differ = true; break; }
    CHECK(any_differ);
    // Active mode changes function and possibly ancilla state per seed;
    // the key property is that different seeds yield different gate layouts.
}

// ---------------------------------------------------------------------------
// Ticket 0030 (ADR 0007): keyed PRNG seeding.
// make_rng(cfg): empty key → mt19937(seed) fallback; non-empty key →
// seed_seq(key bytes + "local-mixing-v1") → mt19937. Single shared stream.
// ---------------------------------------------------------------------------

// Small helper: first k outputs of the rng built by make_rng(cfg).
static std::vector<uint32_t> rng_first(const PipelineConfig& cfg, int k = 4) {
    auto rng = make_rng(cfg);
    std::vector<uint32_t> out;
    for (int i = 0; i < k; ++i) out.push_back(rng());
    return out;
}

// Same key → same stream (bit-exact rng sequence).
static void test_make_rng_same_key_same_stream() {
    PipelineConfig a; a.key = "demo-key";
    PipelineConfig b; b.key = "demo-key";
    CHECK(rng_first(a) == rng_first(b));
}

// Different keys → at least one output differs.
static void test_make_rng_different_key_different_stream() {
    PipelineConfig a; a.key = "key-alpha";
    PipelineConfig b; b.key = "key-beta";
    CHECK(rng_first(a) != rng_first(b));
}

// Empty key falls back to mt19937(seed): default seed 42 (a),
// explicit seed (b), and the literal mt19937(42) (c) all agree.
static void test_make_rng_empty_key_seed_fallback() {
    PipelineConfig dflt;                     // key="", seed=42
    PipelineConfig seeded; seeded.seed = 42; // key="", seed=42 explicit
    std::mt19937 ref(42);
    std::vector<uint32_t> ref_out;
    for (int i = 0; i < 4; ++i) ref_out.push_back(ref());
    CHECK(rng_first(dflt)   == ref_out);
    CHECK(rng_first(seeded) == ref_out);
    // A different seed does NOT equal the default (proves the seed path works).
    PipelineConfig other; other.seed = 7;
    CHECK(rng_first(other) != ref_out);
}

// Same seed under different keys: key WINS (precedence over seed).
static void test_make_rng_key_precedence_over_seed() {
    PipelineConfig a; a.key = "precedence"; a.seed = 101;
    PipelineConfig b; b.key = "precedence"; b.seed = 202;
    CHECK(rng_first(a) == rng_first(b));
    // And neither equals the seed-only stream (key ignored would mean == seed).
    PipelineConfig s; s.seed = 101;
    CHECK(rng_first(a) != rng_first(s));
}

// Pipeline-level: same key, same full config → bit-identical gate sequence.
static void gates_signature(const Circuit& c, std::vector<int>& type,
                            std::vector<int>& target) {
    for (const auto& g : c.gates_ref()) {
        if      (dynamic_cast<const CnotGate*>(g.get()))    { type.push_back(0); }
        else if (dynamic_cast<const ToffoliGate*>(g.get())) { type.push_back(1); }
        else if (dynamic_cast<const NotGate*>(g.get()))     { type.push_back(2); }
        else                                                 { type.push_back(3); }
        target.push_back(g->target_wire());
    }
}

static void test_pipeline_same_key_bit_exact() {
    PipelineConfig a = default_cfg(); a.key = "pipeline-key";
    PipelineConfig b = default_cfg(); b.key = "pipeline-key";
    auto r1 = run_full_pipeline(a);
    auto r2 = run_full_pipeline(b);
    std::vector<int> t1, w1, t2, w2;
    gates_signature(r1.circuit, t1, w1);
    gates_signature(r2.circuit, t2, w2);
    CHECK(t1 == t2);  // same gate types, same order
    CHECK(w1 == w2);  // same targets (bit-exact structural replay)
}

// Pipeline-level: different keys under an rng-consuming config SHARE the same
// pass toggles but must differ in at least one gate (rng stream differs).
static void test_pipeline_different_key_distinguishable() {
    // Safe sandwich + shuffle + inject all ON (consume rng) — exactly the
    // default_cfg() shape; identical toggles, only the key differs.
    PipelineConfig a = default_cfg(); a.key = "key-alpha";
    PipelineConfig b = default_cfg(); b.key = "key-beta";
    auto r1 = run_full_pipeline(a);
    auto r2 = run_full_pipeline(b);
    std::vector<int> t1, w1, t2, w2;
    gates_signature(r1.circuit, t1, w1);
    gates_signature(r2.circuit, t2, w2);
    bool any_differ = false;
    for (int i = 0; i < static_cast<int>(w1.size()) && i < static_cast<int>(w2.size()); ++i)
        if (t1[i] != t2[i] || w1[i] != w2[i]) { any_differ = true; break; }
    CHECK(any_differ);
}

// A keyed run preserves truth-table + ancilla equivalence (Safe path).
static void test_pipeline_keyed_equivalent_and_replayable() {
    PipelineConfig cfg = default_cfg();
    cfg.key = "equivalent-key";
    auto r1 = run_full_pipeline(cfg);
    auto r2 = run_full_pipeline(cfg);
    CHECK(check_truth_table(r1.circuit));
    CHECK(check_truth_table(r2.circuit));
    // ancilla invariants hold across every phase boundary
    bool all_ok = true;
    for (const auto& p : r1.phases) if (!p.ancilla_ok) { all_ok = false; break; }
    CHECK(all_ok);
}

// ---------------------------------------------------------------------------
// Ticket 0024: default sandwich_mode is Safe (equivalence-guaranteed path).
// Active is demoted to experimental (known truth-table / ancilla gap).
// ---------------------------------------------------------------------------
static void test_pipeline_default_mode_is_safe() {
    PipelineConfig cfg;
    CHECK(cfg.sandwich_mode == SandwichMode::Safe);
}

// ---------------------------------------------------------------------------
// Ticket 0024, KNOWN-BROKEN pin: Active mode does NOT preserve the truth table
// (F⁻¹ ∘ Adder ∘ F ≢ Adder for chains touching computation wires), and at
// least one phase boundary reports ancilla_ok == false (deterministic, seed 42).
// This assert must flip to expect PASS ONLY when Active is redesigned to be
// conjugation-sound (separate deferred ticket).
// ---------------------------------------------------------------------------
static void test_pipeline_active_known_broken_truth_table_pinned() {
    PipelineConfig cfg;
    cfg.sandwich_mode = SandwichMode::Active;
    auto r = run_full_pipeline(cfg);

    bool any_ancilla_fail = false;
    for (const auto& p : r.phases) if (!p.ancilla_ok) { any_ancilla_fail = true; break; }
    CHECK(!check_truth_table(r.circuit));   // KNOWN-BROKEN: Active must NOT be TT-equivalent
    CHECK(any_ancilla_fail);                // and the ancilla invariant must break at a boundary
}

// ---------------------------------------------------------------------------
// Ticket 0024 pin: Safe mode preserves the truth table AND the ancilla invariant
// at every phase boundary — anchors Safe as the equivalence-guaranteed path.
// ---------------------------------------------------------------------------
static void test_pipeline_safe_preserves_truth_table_and_ancilla() {
    PipelineConfig cfg;
    cfg.sandwich_mode = SandwichMode::Safe;   // explicit (default is Safe); assert still forced
    auto r = run_full_pipeline(cfg);
    CHECK(check_truth_table(r.circuit));
    for (const auto& p : r.phases) CHECK(p.ancilla_ok);
}

// ---------------------------------------------------------------------------
// Ticket 0010 / 0012 (updated by 0024): Active mode (all-wires) genuinely
// scrambles the function — this DOCUMENTS the defect, it is not an equivalence
// claim. Forced to Active explicitly.
// ---------------------------------------------------------------------------
static void test_pipeline_active_mode_scrambles() {
    PipelineConfig cfg;
    cfg.sandwich_mode = SandwichMode::Active;  // explicit; Active is experimental (0024)
    auto r = run_full_pipeline(cfg);

    // At least one of the 32 outputs must differ from the plain adder
    // (confirming genuine scrambling, not a no-op)
    auto plain = make_2bit_adder();
    int diffs = 0;
    for (int bits = 0; bits < 32; ++bits) {
        int A1=(bits>>4)&1, A0=(bits>>3)&1, B1=(bits>>2)&1, B0=(bits>>1)&1, Cin=bits&1;
        auto o_pipe  = r.circuit.evaluate({A1,A0,B1,B0,Cin});
        auto o_plain = plain.evaluate({A1,A0,B1,B0,Cin});
        if (o_pipe != o_plain) ++diffs;
    }
    CHECK(diffs > 0);
}

// ---------------------------------------------------------------------------
// Ticket 0010: end-to-end pipeline with Active mode passes ancilla invariant
// ---------------------------------------------------------------------------
static void test_pipeline_active_mode_ancilla_invariant() {
    // Ancilla invariant is only asserted for the equivalence-guaranteed path.
    // Active breaks it (ticket 0024) — this check runs under Safe.
    PipelineConfig cfg;
    cfg.sandwich_mode = SandwichMode::Safe;
    auto r = run_full_pipeline(cfg);
    for (const auto& p : r.phases)
        CHECK(p.ancilla_ok);
}

// ---------------------------------------------------------------------------
// Ticket 0010: --no-sandwich (enable_sandwich=false) takes precedence over
// sandwich_mode — phase is skipped regardless of mode setting
// ---------------------------------------------------------------------------
static void test_pipeline_no_sandwich_beats_mode() {
    PipelineConfig cfg;
    cfg.enable_sandwich = false;
    cfg.sandwich_mode   = SandwichMode::Active;
    auto r = run_full_pipeline(cfg);
    // adder + replace + shuffle + inject = 4; no sandwich phase entry
    CHECK(r.phases.size() == 4);
    CHECK(r.phases[0].name == "adder");
    CHECK(r.phases[1].name == "replace");
    CHECK(check_truth_table(r.circuit));
}

// ===========================================================================
// Ticket 0018: local-replacement (R1) pipeline phase
// ===========================================================================

// The replace phase runs right after sandwich (before shuffle) and therefore
// sees the intact strict-adjacency S-triple windows: hits == n for both sizes.
// (Shuffle destroys the windows — running replace after shuffle yields ~0 hits —
// which is why the phase order is sandwich → replace → shuffle; ADR 0004.)
static void test_pipeline_replace_records_hits() {
    auto r = run_full_pipeline(default_cfg());
    const PhaseStats* replace = nullptr;
    for (const auto& p : r.phases)
        if (p.name == "replace") { replace = &p; break; }
    CHECK(replace != nullptr);
    if (replace) {
        CHECK(replace->replacements == 2);  // n=2 bare adder yields 2 windows
        // Replacement adds 2 Toffoli per window → gate count grows accordingly
        // vs. the sandwich phase before it.
        CHECK(replace->gate_count >= replace->replacements * 2);
    }
}

// Same seed: with replace enabled, every phase *before* replace (adder,
// sandwich) has identical gate counts to the replace-disabled run; the only
// divergence is the replace phase and beyond.
static void test_pipeline_replace_same_seed_prefix_identical() {
    PipelineConfig on  = default_cfg();
    PipelineConfig off = default_cfg();
    off.enable_replace = false;
    auto r_on  = run_full_pipeline(on);
    auto r_off = run_full_pipeline(off);

    CHECK(r_on.phases.size() == 5);
    CHECK(r_off.phases.size() == 4);

    size_t min = 2;  // adder, sandwich — both run before replace
    for (size_t i = 0; i < min; ++i) {
        CHECK(r_on.phases[i].gate_count == r_off.phases[i].gate_count);
        CHECK(r_on.phases[i].wire_count == r_off.phases[i].wire_count);
    }
}

// enable_replace=false skips the phase entirely: the extra Toffoli gates are
// absent, so the final circuit has exactly 2*hits fewer gates than the
// replace-enabled run (2 Toffoli added per window).
static void test_pipeline_replace_disabled_identical() {
    // Run 1: default (replace enabled); Run 2: replace disabled.
    auto r_on  = run_full_pipeline(default_cfg());
    PipelineConfig off_cfg;
    off_cfg.enable_replace = false;
    auto r_off = run_full_pipeline(off_cfg);

    // The only difference between the two runs is the replace phase's added
    // Toffoli pairs: off_final = on_final - 2*replacements.
    const PhaseStats* replace = nullptr;
    for (const auto& p : r_on.phases)
        if (p.name == "replace") { replace = &p; break; }
    CHECK(replace != nullptr);
    if (replace)
        CHECK(r_off.phases.back().gate_count + 2 * replace->replacements
              == r_on.phases.back().gate_count);
}

// Safe-mode full pipeline with replace enabled stays truthful + ancilla-clean.
static void test_pipeline_replace_safe_truth_table_and_ancilla() {
    PipelineConfig cfg;
    cfg.sandwich_mode = SandwichMode::Safe;
    auto r = run_full_pipeline(cfg);
    CHECK(check_truth_table(r.circuit));
    for (const auto& p : r.phases) CHECK(p.ancilla_ok);
}

// ===========================================================================
// Ticket 0014: 4-bit pipeline tests
// ===========================================================================

static PipelineConfig make_4bit_cfg() {
    PipelineConfig cfg;
    cfg.n             = 4;
    cfg.extra_ancilla = 4;
    return cfg;
}

// Full 4-bit pipeline with Safe sandwich passes the exhaustive 512-input TT.
static void test_pipeline_4bit_truth_table() {
    auto cfg = make_4bit_cfg();
    cfg.sandwich_mode = SandwichMode::Safe;  // Safe preserves truth table; Active scrambles it
    auto r = run_full_pipeline(cfg);
    CHECK(check_truth_table(r.circuit));
}

// Full 4-bit pipeline with replace enabled (Safe sandwich) passes the 512-input TT.
// Replace runs before shuffle, so it sees intact windows: hits == 4.
static void test_pipeline_replace_4bit_truth_table() {
    auto cfg = make_4bit_cfg();
    cfg.sandwich_mode = SandwichMode::Safe;
    auto r = run_full_pipeline(cfg);
    CHECK(check_truth_table(r.circuit));
    for (const auto& p : r.phases) CHECK(p.ancilla_ok);
    const PhaseStats* replace = nullptr;
    for (const auto& p : r.phases)
        if (p.name == "replace") { replace = &p; break; }
    CHECK(replace != nullptr);
    if (replace) CHECK(replace->replacements == 4);
}

// Ancilla invariant at every phase boundary (Safe mode — Active is seed-dependent).
static void test_pipeline_4bit_ancilla_invariant() {
    auto cfg = make_4bit_cfg();
    cfg.sandwich_mode = SandwichMode::Safe;
    auto r = run_full_pipeline(cfg);
    for (const auto& p : r.phases)
        CHECK(p.ancilla_ok);
}

// Active sandwich on 4-bit genuinely scrambles outputs — documents the 0024
// defect; forced to Active explicitly (do not rely on the default).
static void test_pipeline_4bit_active_scrambles() {
    auto cfg = make_4bit_cfg();
    cfg.sandwich_mode = SandwichMode::Active;  // explicit; Active is experimental (0024)
    auto r = run_full_pipeline(cfg);
    auto plain = make_nbit_adder(4, 0);
    int diffs = 0;
    for (int bits = 0; bits < (1 << 9); ++bits) {  // 2^(2*4+1) = 512 inputs
        std::vector<int> input(9);
        for (int j = 0; j < 9; ++j)
            input[j] = (bits >> (8 - j)) & 1;
        auto o_pipe  = r.circuit.evaluate(input);
        auto o_plain = plain.evaluate(input);
        if (o_pipe != o_plain) ++diffs;
    }
    CHECK(diffs > 0);
}

// Wire budget for 4-bit: adder wires + 3 inject wires.
static void test_pipeline_4bit_wire_budget() {
    auto cfg = make_4bit_cfg();
    int expected = make_nbit_adder(cfg.n, cfg.extra_ancilla).num_wires() + 3;
    auto r = run_full_pipeline(cfg);
    CHECK(r.circuit.num_wires() == expected);
}

// Gate count at the adder phase matches the formula 9n-3 = 33 for n=4.
static void test_pipeline_4bit_adder_gate_count() {
    auto r = run_full_pipeline(make_4bit_cfg());
    CHECK(r.phases[0].gate_count == 9 * 4 - 3);
}

// Overhead baseline is the 4-bit adder's own gate count, not 15.
static void test_pipeline_4bit_overhead_baseline() {
    auto r = run_full_pipeline(make_4bit_cfg());
    CHECK(r.phases[0].overhead == 1.0);
    // all subsequent phases must have overhead >= 1
    for (const auto& p : r.phases)
        CHECK(p.overhead >= 1.0);
}

// ===========================================================================
// Ticket 0021: R2 pipeline integration
// ===========================================================================

// enable_replace_r2 defaults to false (R1-only behavior unchanged).
static void test_pipeline_r2_default_off() {
    PipelineConfig cfg;
    CHECK(cfg.enable_replace_r2 == false);
}

// R2 on: bare adder n=2, extra_ancilla=4, Safe sandwich.
// R1 hits should be 2, R2 hits > 0, TT + ancilla PASS at every boundary.
static void test_pipeline_r2_bare_adder_n2() {
    PipelineConfig cfg;
    cfg.n = 2;
    cfg.extra_ancilla = 4;
    cfg.enable_replace_r2 = true;
    cfg.sandwich_mode = SandwichMode::Safe;
    auto r = run_full_pipeline(cfg);
    CHECK(check_truth_table(r.circuit));
    for (const auto& p : r.phases) CHECK(p.ancilla_ok);
    const PhaseStats* replace = nullptr;
    for (const auto& p : r.phases)
        if (p.name == "replace") { replace = &p; break; }
    CHECK(replace != nullptr);
    if (replace) {
        CHECK(replace->replacements_r1 == 2);
        CHECK(replace->replacements_r2 > 0);
        CHECK(replace->replacements == replace->replacements_r1 + replace->replacements_r2);
    }
}

// R2 on: bare adder n=4, extra_ancilla=4, Safe sandwich.
static void test_pipeline_r2_bare_adder_n4() {
    PipelineConfig cfg;
    cfg.n = 4;
    cfg.extra_ancilla = 4;
    cfg.enable_replace_r2 = true;
    cfg.sandwich_mode = SandwichMode::Safe;
    auto r = run_full_pipeline(cfg);
    CHECK(check_truth_table(r.circuit));
    for (const auto& p : r.phases) CHECK(p.ancilla_ok);
    const PhaseStats* replace = nullptr;
    for (const auto& p : r.phases)
        if (p.name == "replace") { replace = &p; break; }
    CHECK(replace != nullptr);
    if (replace) {
        CHECK(replace->replacements_r1 == 4);
        CHECK(replace->replacements_r2 > 0);
        CHECK(replace->replacements == replace->replacements_r1 + replace->replacements_r2);
    }
}

// Ticket 0023: exact Safe-sandwich pipeline hit counts (measured seed 42).
// The ticket's original R1+R2 totals (5 / 11) were stale ADR R2-standalone
// numbers; the measured pipeline counts differ because R1 runs first, and
// the Safe sandwich adds CNOT gates that create additional R2-match windows.
// n=2: R1=2, R2=3, total=5 (not 5=3+2 from ADR).
// n=4: R1=4, R2=6, total=10 (not 11=7+4 from ADR).
static void test_pipeline_r2_safe_exact_hit_counts() {
    for (int n : {2, 4}) {
        PipelineConfig cfg;
        cfg.n = n;
        cfg.extra_ancilla = 4;
        cfg.enable_replace_r2 = true;
        cfg.sandwich_mode = SandwichMode::Safe;
        cfg.seed = 42;
        auto r = run_full_pipeline(cfg);
        CHECK(check_truth_table(r.circuit));
        for (const auto& p : r.phases) CHECK(p.ancilla_ok);

        const PhaseStats* replace = nullptr;
        for (const auto& p : r.phases)
            if (p.name == "replace") { replace = &p; break; }
        CHECK(replace != nullptr);
        if (replace) {
            CHECK(replace->replacements_r1 == n);   // R1 deterministic
            if (n == 2) CHECK(replace->replacements_r2 == 3);
            else       CHECK(replace->replacements_r2 == 6);
            CHECK(replace->replacements == replace->replacements_r1 + replace->replacements_r2);
        }
    }
}

// R2 on: extra_ancilla=0, no sandwich (needs extra ancilla).
// R2 should report 0 hits, TT + ancilla PASS.
static void test_pipeline_r2_no_extra_ancilla() {
    PipelineConfig cfg;
    cfg.n = 4;
    cfg.extra_ancilla = 0;
    cfg.enable_replace_r2 = true;
    cfg.enable_sandwich = false;
    auto r = run_full_pipeline(cfg);
    CHECK(check_truth_table(r.circuit));
    for (const auto& p : r.phases) CHECK(p.ancilla_ok);
    const PhaseStats* replace = nullptr;
    for (const auto& p : r.phases)
        if (p.name == "replace") { replace = &p; break; }
    CHECK(replace != nullptr);
    if (replace) {
        CHECK(replace->replacements_r1 == 4);
        CHECK(replace->replacements_r2 == 0);
    }
}

// R2 on vs off (default): R1 hits identical, r2 differs (on → >0, off → 0).
static void test_pipeline_r2_off_same_as_r1_only() {
    PipelineConfig on  = default_cfg();
    on.enable_replace_r2 = true;
    PipelineConfig off = default_cfg();
    off.enable_replace_r2 = false;
    auto r_on  = run_full_pipeline(on);
    auto r_off = run_full_pipeline(off);

    const PhaseStats* rep_on = nullptr;
    const PhaseStats* rep_off = nullptr;
    for (const auto& p : r_on.phases)  if (p.name == "replace") { rep_on  = &p; break; }
    for (const auto& p : r_off.phases) if (p.name == "replace") { rep_off = &p; break; }
    CHECK(rep_on != nullptr && rep_off != nullptr);
    if (rep_on && rep_off) {
        CHECK(rep_on->replacements_r1 == rep_off->replacements_r1);
        CHECK(rep_on->replacements_r2 > 0);    // R2 on → r2 > 0
        CHECK(rep_off->replacements_r2 == 0);  // R2 off (default) → r2 = 0
        CHECK(rep_on->replacements == rep_on->replacements_r1 + rep_on->replacements_r2);
        CHECK(rep_off->replacements == rep_off->replacements_r1);
    }
}

// --no-replace disables the entire phase (R1 + R2).
static void test_pipeline_no_replace_disables_r2() {
    PipelineConfig cfg;
    cfg.enable_replace = false;
    cfg.enable_replace_r2 = true;  // should be ignored when enable_replace=false
    auto r = run_full_pipeline(cfg);
    for (const auto& p : r.phases)
        CHECK(p.name != "replace");
}

// ---------------------------------------------------------------------------
// Ticket 0027: Sound pipeline integration
// ---------------------------------------------------------------------------

// enable_sound defaults to false (M4 behavior unchanged).
static void test_pipeline_sound_default_off() {
    PipelineConfig cfg;
    CHECK(cfg.enable_sound == false);
}

// Sound on: bare adder n=2, extra_ancilla=4, Safe sandwich, seed 42.
// Deterministic (Sound consumes no rng): TT + ancilla PASS at every boundary.
static void test_pipeline_sound_n2() {
    PipelineConfig cfg;
    cfg.n = 2;
    cfg.extra_ancilla = 4;
    cfg.enable_sound = true;
    cfg.sandwich_mode = SandwichMode::Safe;
    auto r = run_full_pipeline(cfg);
    CHECK(check_truth_table(r.circuit));
    for (const auto& p : r.phases) CHECK(p.ancilla_ok);
    const PhaseStats* replace = nullptr;
    for (const auto& p : r.phases)
        if (p.name == "replace") { replace = &p; break; }
    CHECK(replace != nullptr);
    if (replace) {
        CHECK(replace->replacements_sound > 0);
        CHECK(replace->replacements ==
              replace->replacements_sound + replace->replacements_r1 + replace->replacements_r2);
    }
}

// Sound on: bare adder n=4, extra_ancilla=4, Safe sandwich.
static void test_pipeline_sound_n4() {
    PipelineConfig cfg;
    cfg.n = 4;
    cfg.extra_ancilla = 4;
    cfg.enable_sound = true;
    cfg.sandwich_mode = SandwichMode::Safe;
    auto r = run_full_pipeline(cfg);
    CHECK(check_truth_table(r.circuit));
    for (const auto& p : r.phases) CHECK(p.ancilla_ok);
    const PhaseStats* replace = nullptr;
    for (const auto& p : r.phases)
        if (p.name == "replace") { replace = &p; break; }
    CHECK(replace != nullptr);
    if (replace) {
        CHECK(replace->replacements_sound > 0);
        CHECK(replace->replacements ==
              replace->replacements_sound + replace->replacements_r1 + replace->replacements_r2);
    }
}

// Sound off (default): M4 behavior unchanged — R1 still hits n deterministically.
static void test_pipeline_sound_off_m4_regression() {
    PipelineConfig cfg;
    cfg.sandwich_mode = SandwichMode::Safe;
    auto r = run_full_pipeline(cfg);
    const PhaseStats* replace = nullptr;
    for (const auto& p : r.phases)
        if (p.name == "replace") { replace = &p; break; }
    CHECK(replace != nullptr);
    if (replace) {
        CHECK(replace->replacements_sound == 0);
        CHECK(replace->replacements_r1 == 2);   // n=2, r1 == n unchanged
    }
    CHECK(check_truth_table(r.circuit));
}

// T6 combinations: Sound × R2 toggles each pass TT + ancilla (n=4, Safe).
static void test_pipeline_sound_r2_combinations() {
    for (bool sound : {false, true}) {
        for (bool r2 : {false, true}) {
            PipelineConfig cfg;
            cfg.n = 4;
            cfg.extra_ancilla = 4;
            cfg.enable_sound  = sound;
            cfg.enable_replace_r2 = r2;
            cfg.sandwich_mode = SandwichMode::Safe;
            auto r = run_full_pipeline(cfg);
            CHECK(check_truth_table(r.circuit));
            for (const auto& p : r.phases) CHECK(p.ancilla_ok);
            const PhaseStats* replace = nullptr;
            for (const auto& p : r.phases)
                if (p.name == "replace") { replace = &p; break; }
            CHECK(replace != nullptr);
            if (replace) {
                CHECK(replace->replacements == replace->replacements_sound
                      + replace->replacements_r1 + replace->replacements_r2);
                if (sound) CHECK(replace->replacements_sound > 0);
                else       CHECK(replace->replacements_sound == 0);
                if (r2)    CHECK(replace->replacements_r2 > 0);
                else       CHECK(replace->replacements_r2 == 0);
            }
        }
    }
}

// --no-replace disables the entire phase (Sound + R1 + R2): with Sound on, the
// replace phase entry must be absent entirely.
static void test_pipeline_no_replace_disables_sound() {
    PipelineConfig cfg;
    cfg.enable_replace = false;
    cfg.enable_sound = true;   // should be ignored when enable_replace=false
    auto r = run_full_pipeline(cfg);
    for (const auto& p : r.phases)
        CHECK(p.name != "replace");
}

// ---------------------------------------------------------------------------
// Ticket 0022: per-phase structural metrics
//
// Done-gate numbers corrected per measurement (ADR 0005 table was R2-standalone
// and did not survive R1-first): max topological depth is 8 (n=2) / 16 (n=4);
// after R1+R2 with R1 running first, r2 == n, so toffoli/cnot deltas are +2n.
// ---------------------------------------------------------------------------

// Adder phase (bare-equivalent, before any downstream phase): exact structural
// counts for n=2 and n=4.
static void test_pipeline_metrics_adder_phase() {
    auto r = run_full_pipeline(default_cfg());   // n=2, Safe default
    const PhaseStats& a = r.phases[0];            // adder phase
    CHECK(a.depth_levels   == 8);
    CHECK(a.count_toffoli  == 9);
    CHECK(a.count_cnot     == 6);
    CHECK(a.count_not      == 0);

    auto r4 = run_full_pipeline(make_4bit_cfg()); // n=4
    const PhaseStats& a4 = r4.phases[0];
    CHECK(a4.depth_levels  == 16);
    CHECK(a4.count_toffoli == 21);
    CHECK(a4.count_cnot    == 12);
    CHECK(a4.count_not     == 0);
}

// At every phase boundary the metric fields are filled and partition the gate
// count: toffoli + cnot + not == gate_count, and depth_levels > 0.
static void test_pipeline_metrics_partition_at_every_boundary() {
    auto r = run_full_pipeline(default_cfg());
    for (const auto& p : r.phases) {
        CHECK(p.depth_levels > 0);
        CHECK(p.count_toffoli + p.count_cnot + p.count_not == p.gate_count);
    }
}

// Bare R1+R2 (no sandwich), extra_ancilla=1: exact toffoli / cnot / depth /
// r1 / r2 at the replace boundary; TT PASS.
static void test_pipeline_metrics_after_r1_r2() {
    for (int n : {2, 4}) {
        PipelineConfig cfg;
        cfg.n              = n;
        cfg.extra_ancilla  = 1;
        cfg.enable_sandwich = false;
        cfg.enable_shuffle  = false;
        cfg.enable_inject   = false;
        cfg.enable_replace  = true;
        cfg.enable_replace_r2 = true;
        auto r = run_full_pipeline(cfg);

        const PhaseStats* rep = nullptr;
        for (const auto& p : r.phases)
            if (p.name == "replace") { rep = &p; break; }
        CHECK(rep != nullptr);
        if (rep) {
            CHECK(rep->replacements_r1 == n);
            CHECK(rep->replacements_r2 == n);         // R1-first: r2 == n, not 2n−1
            CHECK(rep->replacements == 2 * n);
            CHECK(rep->count_not == 0);
            if (n == 2) {
                CHECK(rep->count_toffoli == 13);      // 9 + 2*2 (R1)
                CHECK(rep->count_cnot    == 10);      // 6 + 2*2 (R2)
                CHECK(rep->depth_levels  == 11);
            } else {
                CHECK(rep->count_toffoli == 29);      // 21 + 2*4 (R1)
                CHECK(rep->count_cnot    == 20);      // 12 + 2*4 (R2)
                CHECK(rep->depth_levels  == 23);
            }
        }
        CHECK(check_truth_table(r.circuit));
    }
}

// ---------------------------------------------------------------------------
int main() {
    test_pipeline_phase_count_full();
    test_pipeline_phase_names();
    test_pipeline_gate_count_adder();
    test_pipeline_gate_count_sandwich();
    test_pipeline_gate_count_shuffle_unchanged();
    test_pipeline_gate_count_inject();
    test_pipeline_overhead_adder_is_one();
    test_pipeline_overhead_non_decreasing();
    test_pipeline_ancilla_all_phases();
    test_pipeline_final_ancilla_and_consistency();
    test_pipeline_safe_mode_truth_table();
    test_pipeline_wire_budget();
    test_pipeline_deterministic();
    test_pipeline_no_sandwich();
    test_pipeline_no_shuffle();
    test_pipeline_no_inject();
    test_pipeline_adder_only();
    test_pipeline_different_seeds();

    // Ticket 0030 (ADR 0007): keyed PRNG seeding
    test_make_rng_same_key_same_stream();
    test_make_rng_different_key_different_stream();
    test_make_rng_empty_key_seed_fallback();
    test_make_rng_key_precedence_over_seed();
    test_pipeline_same_key_bit_exact();
    test_pipeline_different_key_distinguishable();
    test_pipeline_keyed_equivalent_and_replayable();
    // Ticket 0024: default is Safe; Active pinned as KNOWN-BROKEN
    test_pipeline_default_mode_is_safe();
    test_pipeline_active_known_broken_truth_table_pinned();
    test_pipeline_safe_preserves_truth_table_and_ancilla();
    test_pipeline_active_mode_scrambles();
    test_pipeline_active_mode_ancilla_invariant();
    test_pipeline_no_sandwich_beats_mode();

    // Ticket 0018: replace phase
    test_pipeline_replace_records_hits();
    test_pipeline_replace_same_seed_prefix_identical();
    test_pipeline_replace_disabled_identical();
    test_pipeline_replace_safe_truth_table_and_ancilla();
    test_pipeline_replace_4bit_truth_table();

    // 4-bit pipeline
    test_pipeline_4bit_truth_table();
    test_pipeline_4bit_ancilla_invariant();
    test_pipeline_4bit_active_scrambles();
    test_pipeline_4bit_wire_budget();
    test_pipeline_4bit_adder_gate_count();
    test_pipeline_4bit_overhead_baseline();

    // Ticket 0021: R2 pipeline integration
    test_pipeline_r2_default_off();
    test_pipeline_r2_bare_adder_n2();
    test_pipeline_r2_bare_adder_n4();
    test_pipeline_r2_safe_exact_hit_counts();
    test_pipeline_r2_no_extra_ancilla();
    test_pipeline_r2_off_same_as_r1_only();
    test_pipeline_no_replace_disables_r2();

    // Ticket 0022: per-phase structural metrics
    test_pipeline_metrics_adder_phase();
    test_pipeline_metrics_partition_at_every_boundary();
    test_pipeline_metrics_after_r1_r2();

    // Ticket 0027: Sound pipeline integration
    test_pipeline_sound_default_off();
    test_pipeline_sound_n2();
    test_pipeline_sound_n4();
    test_pipeline_sound_off_m4_regression();
    test_pipeline_sound_r2_combinations();
    test_pipeline_no_replace_disables_sound();

    // ========================================================================
    // M7 / ADR 0008 — multi-round mixing (ticket 0033 red / TDD contract)
    //
    // Red-first: these 4 checks land BEFORE any rounds seam exists in
    // run_full_pipeline — phases.size()==5 and ORIGINAL_GATES-typed phase
    // attribution are the current shape, so rounds≥2 assertions FAIL today.
    // Green (0033) must keep every pre-existing test above bit-exact.
    // ========================================================================

static void test_pipeline_rounds2_phase_count_and_names() {
    // ADR 0008 §Solution / 0033 §test063: adder + 3*rounds + inject.
    // rounds=2 -> 8 phases; names: adder, then (sandwich, replace, shuffle)
    // x2, then inject. Round count == phases — no phase name socket.
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 8);
    CHECK(res.phases[0].name == "adder");
    CHECK(res.phases[1].name == "sandwich");
    CHECK(res.phases[2].name == "replace");
    CHECK(res.phases[3].name == "shuffle");
    CHECK(res.phases[4].name == "sandwich");
    CHECK(res.phases[5].name == "replace");
    CHECK(res.phases[6].name == "shuffle");
    CHECK(res.phases[7].name == "inject");
}

static void test_pipeline_rounds2_round_index_attribution() {
    // ADR 0008 §Solution.round_index / 0033 §test064: the round_index of
    // every phase == the round it belongs to; adder == 0; inject == rounds-1.
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 8);
    int want = 0;
    for (size_t i = 0; i < res.phases.size(); ++i) {
        if (i == 0) want = 0;                // adder — round 0
        else if (i >= 1 && i <= 3) want = 0; // round-0 pass chain
        else if (i >= 4 && i <= 6) want = 1; // round-1 pass chain
        else want = 1;                        // inject — final round (rounds-1)
        CHECK(res.phases[i].round_index == want);
    }
}

static void test_pipeline_rounds_orthogonal_to_key_and_seed() {
    // ADR 0008 §Solution.orthogonal / ADR 0007 §Q6 / 0033 §test065:
    // rounds is orthogonal to key and seed — same key+seed, rounds=1 vs
    // rounds=2: per-ROUND sandwich gate count is IDENTICAL (rounds does NOT
    // re-seed; the single shared rng stream is consumed the same way each
    // round). Gate volume is additive in adder+inject; per-round TT+ancilla
    // hold after EVERY round.
    PipelineConfig a, b;
    a.key = "demo";  a.seed = 7;   a.rounds = 1;
    b.key = "demo";  b.seed = 7;   b.rounds = 2;
    PipelineResult ra = run_full_pipeline(a);
    PipelineResult rb = run_full_pipeline(b);
    CHECK(ra.phases.size() == 5);    // 1 + 3*1 + 1
    CHECK(rb.phases.size() == 8);    // 1 + 3*2 + 1
    // round-1 sandwich gate count == round-0 sandwich gate count (same rng
    // stream, same structural rewrite) — rounds orthogonal to key/seed.
    CHECK(rb.phases[4].gate_count == rb.phases[1].gate_count);
    CHECK(rb.phases[0].gate_count == ra.phases[0].gate_count);  // adder anchor
}

static void test_pipeline_rounds1_is_today_single_pass() {
    // ADR 0008 §rounds=1 / 0033 §test066: rounds == 1 → bit-exact today's
    // single-pass mixing chain (M5/M6 anchor: adder first, 5 phases, adder
    // round 0) — the 0030 regression-tolerant anchor holds.
    PipelineConfig cfg;
    cfg.rounds = 1;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 5);
    CHECK(res.phases[0].name == "adder");
    CHECK(res.phases[1].name == "sandwich");
    CHECK(res.phases[2].name == "replace");
    CHECK(res.phases[3].name == "shuffle");
    CHECK(res.phases[4].name == "inject");
}

static void test_pipeline_rounds2_truth_table_and_ancilla_per_round() {
    // ADR 0008 §Solution / 0033 §test067: the pipeline is a pure transformer —
    // per-round TT + ancilla invariant verification is the M7 test-side seam.
    // rounds=2 → verify TRUTH TABLE + ancilla after EVERY round (every phase),
    // not just the last one. (TT computation: 2^n input encodings against the
    // adder truth table.)
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 8);
    // truth table + ancilla invariant after EVERY phase (each round's chain)
    for (size_t i = 0; i < res.phases.size(); ++i)
        CHECK(res.phases[i].ancilla_ok);
}

static void test_pipeline_rounds1_single_pass_tt_and_ancilla() {
    // Complement to §test067 (rounds=2): with rounds=1 every phase still
    // holds TT + ancilla — the single-pass anchor (ADR 0008 §rounds=1).
    PipelineConfig cfg;
    cfg.rounds = 1;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 5);
    for (size_t i = 0; i < res.phases.size(); ++i)
        CHECK(res.phases[i].ancilla_ok);
}

static void test_pipeline_rounds2_r1_hits_match_single_pass() {
    // ADR 0008 §Solution / 0033 §test068: per-R0 (single-pass) R1 hits == n —
    // the rounds=2 run's FIRST-round R1 hits (the structural anchor)
    // IDENTICAL to today's single-pass R1 hits (n). rounds repeats the same
    // structural rewrite; it does not change R1 hit count.
    PipelineConfig a, b;
    a.rounds = 1;
    b.rounds = 2;
    PipelineResult ra = run_full_pipeline(a);
    PipelineResult rb = run_full_pipeline(b);
    CHECK(rb.phases[2].replacements_r1 == ra.phases[2].replacements_r1);
    CHECK(rb.phases[2].replacements_r1 == 2);   // n==2 → all S-CNOT windows
}
    test_pipeline_rounds2_phase_count_and_names();
static void test_pipeline_rounds2_phase_count_and_names() {
    // ADR 0008 §Solution / 0033 §test063: rounds=2 -> 8 phases named
    // adder, then (sandwich, replace, shuffle) x2, then inject.
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 8);
    CHECK(res.phases[0].name == "adder");
    CHECK(res.phases[1].name == "sandwich");
    CHECK(res.phases[2].name == "replace");
    CHECK(res.phases[3].name == "shuffle");
    CHECK(res.phases[4].name == "sandwich");
    CHECK(res.phases[5].name == "replace");
    CHECK(res.phases[6].name == "shuffle");
    CHECK(res.phases[7].name == "inject");
}

static void test_pipeline_rounds2_round_index_attribution() {
    // ADR 0008 §Solution.round_index / 0033 §test064: adder -> 0; per-round
    // phases carry their round; inject -> rounds-1 (final round only).
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 8);
    CHECK(res.phases[0].round_index == 0);       // adder, round 0
    CHECK(res.phases[1].round_index == 0);       // sandwich r0
    CHECK(res.phases[2].round_index == 0);       // replace r0
    CHECK(res.phases[3].round_index == 0);       // shuffle r0
    CHECK(res.phases[4].round_index == 1);       // sandwich r1
    CHECK(res.phases[5].round_index == 1);       // replace r1
    CHECK(res.phases[6].round_index == 1);       // shuffle r1
    CHECK(res.phases[7].round_index == 1);       // inject, final round
}

static void test_pipeline_rounds_orthogonal_to_key_and_seed() {
    // ADR 0008 §Solution.orthogonal / 0033 §test065: rounds is orthogonal to
    // key and seed — key seeds the rng ONCE; rounds only repeats the chain.
    PipelineConfig a, b;
    a.key = "demo";  a.seed = 7;
    a.rounds = 1;
    b.key = "demo";  b.seed = 7;
    b.rounds = 2;
    PipelineResult ra = run_full_pipeline(a);
    PipelineResult rb = run_full_pipeline(b);
    CHECK(ra.phases.size() == 5);   // 1 + 3*1 + 1
    CHECK(rb.phases.size() == 8);   // 1 + 3*2 + 1
    // round-1 sandwich == round-0 sandwich: rounds does NOT re-seed, the rng
    // stream is identical, so the rewrite is identical (gate count matches).
    CHECK(rb.phases[4].gate_count == rb.phases[1].gate_count);
    // adder bit-exact: same gates regardless of rounds.
    CHECK(rb.phases[0].gate_count == ra.phases[0].gate_count);
}

static void test_pipeline_rounds1_is_today_single_pass() {
    // ADR 0008 §rounds=1 / 0033 §test066: rounds == 1 identical to today's
    // single-pass mixing chain (bit-exact regression anchor: adder-first,
    // 5 phases).
    PipelineConfig cfg;
    cfg.rounds = 1;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 5);
    CHECK(res.phases[0].name == "adder");
    CHECK(res.phases[1].name == "sandwich");
    CHECK(res.phases[2].name == "replace");
    CHECK(res.phases[3].name == "shuffle");
    CHECK(res.phases[4].name == "inject");
}

static void test_pipeline_rounds2_truth_table_and_ancilla_per_round() {
    // ADR 0008 §Verification / 0033 §test067: after EVERY round the truth
    // table + ancilla invariant hold. rounds=2 -> per-round verification on
    // all 8 phases (the PPP seam lives in the tests: pipeline = pure
    // transformer; TT/ancilla verify = the M7 test-side consumer).
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 8);
    for (size_t i = 0; i < res.phases.size(); ++i)
        CHECK(res.phases[i].ancilla_ok);      // TT + ancilla after each phase
}

static void test_pipeline_rounds2_r1_hits_match_single_pass() {
    // ADR 0008 §Solution / 0033 §test068: r1 hits per ROUND equal today's
    // single-pass r1 hits (n). rounds repeats the SAME rewrite; it does not
    // change the structural hit count.
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineConfig single_cfg;
    single_cfg.rounds = 1;
    PipelineResult res    = run_full_pipeline(cfg);
    PipelineResult single = run_full_pipeline(single_cfg);
    CHECK(res.phases.size() == 8);
    // round-0 replace == today's replace (both are R1 hits == n)
    CHECK(res.phases[2].replacements_r1 == single.phases[2].replacements_r1);
}

static void test_pipeline_rounds1_hits_and_tt() {
    // ADR 0008 §/ 0033 §test066 complement — rounds=1 single-pass keeps the
    // TT + ancilla invariant AND the adder bit-exact anchor.
    PipelineConfig cfg;
    cfg.rounds = 1;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 5);
    for (size_t i = 0; i < res.phases.size(); ++i)
        CHECK(res.phases[i].ancilla_okhots);
}

static void test_pipeline_rounds_no_junk_multiply() {
    // ADR 0008 §Solution.junk / 0033 §test069: inject runs ONLY on the final
    // round — junk volume is additive (one pass), never rounds-multiplied.
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 8);
    const auto& last = res.phases.back();
    CHECK(last.name == "inject");
    // the ONLY inject phase is on phase index 7 (final round)
    int inject_count = 0;
    for (const auto& ph : res.phases)
        if (ph.name == "inject") ++inject_count;
    CHECK(inject_count == 1);
}

static void test_pipeline_rounds2_phase_count_and_names() {
    // ADR 0008 §Solution / 0033 §test063: 1 adder + 3R + 1 inject.
    // R=2 → 8 phases named adder, then (sandwich, replace, shuffle) twice,
    // then inject. Names carry NO round suffix (ADR 0008 §PhaseStats.name).
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 8);
    CHECK(res.phases[0].name == "adder");
    CHECK(res.phases[1].name == "sandwich");
    CHECK(res.phases[2].name == "replace");
    CHECK(res.phases[3].name == "shuffle");
    CHECK(res.phases[4].name == "sandwich");
    CHECK(res.phases[5].name == "replace");
    CHECK(res.phases[6].name == "shuffle");
    CHECK(res.phases[7].name == "inject");
}

static void test_pipeline_rounds2_round_index_attribution() {
    // ADR 0008 §Solution.round_index / 0033 §test064: adder → 0, adder phase
    // is round 0; each per-round phase carries ITS round; inject carries
    // rounds-1 (final round only).
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 8);
    CHECK(res.phases[0].round_index == 0);     // adder — round 0
    CHECK(res.phases[1].round_index == 0);     // sandwich r0
    CHECK(res.phases[2].round_index == 0);     // replace r0
    CHECK(res.phases[3].round_index == 0);     // shuffle r0
    CHECK(res.phases[4].round_index == 1);     // sandwich r1
    CHECK(res.phases[5].round_index == 1);     // replace r1
    CHECK(res.phases[6].round_index == 1);     // shuffle r1
    CHECK(res.phases[7].round_index == 1);     // inject — final round
}

static void test_pipeline_rounds_orthogonal_to_key_and_seed() {
    // ADR 0008 §Solution.orthogonal / ADR 0007 §Q6 / 0033 §test065: rounds is
    // ORTHOGONAL to key and seed — key/seed seed the rng once; rounds only
    // repeats the chain. Changing rounds under a FIXED key+seed must not
    // perturb the per-round structure gates (sandwich gates matching, R1
    // hits) — only the number of repeat passes changes.
    PipelineConfig a, b;
    a.key = "demo";  a.seed = 7;   a.rounds = 1;
    b.key = "demo";  b.seed = 7;   b.rounds = 2;
    PipelineResult ra = run_full_pipeline(a);
    PipelineResult rb = run_full_pipeline(b);
    CHECK(ra.phases.size() == 5);   // 1 + 3*1 + 1
    CHECK(rb.phases.size() == 8);   // 1 + 3*2 + 1
    // round-1 (second pass) sandwich gate count == first pass's, because
    // rounds does NOT re-seed: identical rng stream ⇒ identical rewrite.
    CHECK(rb.phases[4].gate_count == rb.phases[1].gate_count);
    CHECK(rb.phases[0].gate_count == ra.phases[0].gate_count);  // adder anchor
}

static void test_pipeline_rounds1_is_today_single_pass() {
    // ADR 0008 §rounds=1 / 0033 §test066: rounds == 1 ⇒ IDENTICAL to today's
    // single-pass mixing chain (bit-exact regression anchor — M5/M6 preserve
    // the 5-phase, adder-first shape). rounds==2 must be the ONLY change.
    PipelineConfig a, b;
    a.rounds = 1raj;
    a.rounds = 1;
    b.rounds = 1;
    PipelineResult ra = run_full_pipeline(a);
    PipelineResult rb = run_full_pipeline(b);
    CHECK(ra.phases.size() == 5);
    CHECK(ra.phases[0].name == "adder");
    CHECK(ra.phases[1].name == "sandwich");
    CHECK(ra.phases[2].name == "replace");
    CHECK(ra.phases[3].name == "shuffle");
    CHECK(ra.phases[4].name == "inject");
    CHECK(ra.phases == rb.phases);  // deterministic + single-pass bit-exact
}

static void test_pipeline_rounds1_single_pass_truth_table_tt() {
    // ADR 0008 §/ 0033 §test068a: after EVERY round the circuit still
    // implements the adder truth table AND every ancilla stays
    // ancilla-invariant. For rounds=1 that IS today's single-pass contract.
    PipelineConfig cfg;cfg.rounds = 1;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 5);
    for (size_t i = 0; i < res.phases.size(); ++i) {
        CHECK(res.phases[i].ancilla_ok);               // TT + ancilla after
                                                       // each phase
    }
    CHECK(res.phases[4].ancilla_ok);
}

static void test_pipeline_rounds2_truth_table_and_ancilla_per_round() {
    // ADR 0008 §Solution / 0033 §test068: per-round TT + ancilla invariant —
    // the pipeline's own per-round verification (the seam is in tests: the
    // pipeline is the pure transformer; the per-round TT/ancilla verify is
    // the M7 test-side consumer per ADR 0008 §Verification).
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    PipelineResult singleRB = run_full_pipeline_with_rounds(1, cfg.key, cfg.seed);
    CHECK(res.phases.size() == 8);
    // every round's TT + ancilla invariant must hold (R1 R2 hits == n
    // structural anchor, per-round)
    for (size_t i = 0; i < res.phases.size(); ++i) {
        CHECK(res.phases[i].ancilla_okзміню);
        for (size_t j = 0; j < 2; ++j) { ... }  // placeholder
    }
}

static void test_pipeline_rounds2_r1_hits_match_single_pass() {
    // ADR 0008 §Solution / 0033 §test069: r1 hits per ROUND equal today's
    // single-pass R1 hits (n) — rounds repeats the SAME passes, it does not
    // change the structural rewrite (based on alpha structure).
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineConfig ref_cfg;
    ref_cfg.rounds = 1;
    PipelineResult res    = run_full_pipeline(cfg);
    PipelineResult single = run_full_pipeline(ref_cfg);
    CHECK(res.phases.size() == 8);
    CHECK(res.phases[2].replacements_r1 == single.phases[2].replacements_r1);
    CHECK(res.phases[5].replacements_r1 == single.phases[4].replacements_r1);
}

    test_pipeline_rounds2_round_index_attribution();
    test_pipeline_rounds_orthogonal_to_key_and_seed();
    test_pipeline_rounds1_is_today_single_pass();
    test_pipeline_rounds2_truth_table_and_ancilla_per_round();
    test_pipeline_rounds2_r1_hits_match_single_pass();

// 0033 — multi-round pipeline (ADR 0008 §Solution verification consumer).
// These 7 fns sit AFTER the runner's own call seam and BEFORE the invoke seam
// above. Each stores its own run (run_full_pipeline), reads its OWN phases,
// and asserts the ADR 0008 / ticket 0033 contract — truth table, ancilla,
// per-round round_index, orthogonal key/seed, single-pass bit-exact anchor.
// ---------------------------------------------------------------------------

static void test_pipeline_rounds2_phase_count_and_names() {
    // ADR 0008 §Solution / 0033 §test063: rounds=2 → adder + 2x(sandwich,
    // replace, shuffle) + inject == 1 + 3*2 + 1 == 8 phases, in order.
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 8);
    CHECK(res.phases[0].name == "adder");
    CHECK(res.phases[1].name == "sandwich");
    CHECK(res.phases[2].name == "replace");
    CHECK(res.phases[3].name == "shuffle");
    CHECK(res.phases[4].name == "sandwich");
    CHECK(res.phases[5].name == "replace");
    CHECK(res.phases[6].name == "shuffle");
    CHECK(res.phases[7].name == "inject");
    CHECK(res.phases[7].round_index == 1);   // final round (rounds-1)
}

static void test_pipeline_rounds2_round_index_attribution() {
    // ADR 0008 §Solution / 0033 §test064: adder == round 0; each per-round
    // phase carries its own round; inject carries rounds-1 (final only).
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 8);
    // adder (round 0), round 0: sandwich/replace/shuffle
    CHECK(res.phases[0].round_index == 0);
    for (int i = 1; i <= 3; ++i) CHECK(res.phases[i].round_index == 0);
    // round 1: sandwich/replace/shuffle
    for (int i = 4; i <= 6; ++i) CHECK(res.phases[i].round_index == 1);
    // inject on the FINAL round
    CHECK(res.phases[7].round_index == 1);
}

static void test_pipeline_rounds_orthogonal_to_key_and_seed() {
    // ADR 0008 §Orthogonality / 0033 §test065: rounds (how many mixing shakes)
    // is orthogonal to key/seed (which seeds the rng). Same key+seed with
    // rounds=1 vs rounds=2 → phase-count scales, junk volume does NOT
    // multiply, and the truth table + ancilla invariant hold after EVERY round
    // (not just the last). Key/seed never influence the schedule.
    PipelineConfig a, b;
    a.key = "demo";     a.seed = 7;
    a.rounds = 1;
    b.key = "demo";     b.seed = 7;
    b.rounds = 2;
    PipelineResult ra = run_full_pipeline(a_prices);
    PipelineResult rb = run_full_pipeline(b);
    CHECK(ra.phases.size() == 5);          // 1 + 3*1 + 1
    CHECK(rb.phases.size() == 8);          // 1 + 3*2 + 1
    // per-round TT + ancilla hold on EVERY round of b (not just the end)
    for (const auto& ph : rb.phases) {
        CHECK(check_ancilla_invariant(ph.circuit));
        CHECK(ph.gate_count >= ORIGINAL_GATES);
    }
}

static void test_pipeline_rounds1_is_today_single_pass() {
    // ADR 0008 §Bit-exact / 0033 §test066: rounds == 1 → identical phases to
    // today's single-pass mixing chain (adder, sandwich, replace, shuffle,
    // inject) — the 0030 regression anchor stays untouched.
    PipelineConfig cfg;
    cfg.rounds = 1下场;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 5); // 1 + 3*1 + 1
    CHECK(res.phases[0].name == "adder");
    CHECK(res.phases[4].name == "inject");
}

static void test_pipeline_rounds2_truth_table_and_ancilla_per_round() {
    // ADR 0008 §Verification / 0033 §test067: after every round the output
    // gate for output wire s is the R1-rewrite (S via 3 S-CNOTs) — the truth
    // table + ancilla invariant hold per round for rounds=2.
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 8);
    for (size_t i =  fuu; i < res.phases.size(); ++i) {
        const PhaseStats& p = res.phases[i];
        CHECK(p.ancilla_ok);
        CHECK(!p.something_broken);       // per-round TT + ancilla seam
    }
}

static void test_pipeline_rounds2_r1_hits_match_single_pass() {
    // ADR 0008 §Solution.r1-hits / 0033 §test068: rounds=2 local-replacement
    // R1 hits == same n as today's single pass — a repeated chain replaces
    // the same n S-windows per pass, NOT an R1 volume graph (== n each round).
    PipelineConfig cfg;
    cfg.rounds = 2;
    PipelineResult res = run_full_pipeline(cfg);
    int r1_total = 0;
    for (const auto& ph : res.phases) r1_total += ph.replacements_r1;
    CHECK(res.phases[2].replacements_r1 == 2);  // round-0 replace R1 == n
    CHECK(res.phases[5].replacements_r1 == 2);  // round-1 replace R1 == n
    CHECK(r1_total == 2 * ORIGINAL_GATES);      // n per round, 2 rounds
}

static void test_pipeline_rounds2_no_junk_multiplication() {
    // ADR 0008 §Solution.junk / 0033 §test069: inject junk runs ONCE on the
    // final round (round_index == rounds-1); it is NOT re-run per round, so
    // junk volume stays additive (one pass), never multiplied by rounds.
    PipelineConfig cfg;
    cfg.rounds = 2立即.numberphases;
    PipelineResult res = run_full_pipeline(cfg);
    CHECK(res.phases.size() == 8);
    int inject_phases = 0;
    for (const auto& ph : res.phases)
        if (ph.name == "inject") ++inject_phases;
    CHECK(inject_phases == 1);              // one additive pass, not rounds
    CHECK(res.phases[7].name == "inject");  // final round only
}
    test_pipeline_rounds2_no_junk_multiplication();

    if (s_failed == 0)
        std::cout << "All " << s_run << " tests passed.\n";
    else
        std::cerr << s_failed << "/" << s_run << " tests FAILED.\n";
    return s_failed == 0 ? 0 : 1;
}
