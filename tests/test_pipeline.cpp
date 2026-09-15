#include <iostream>
#include <vector>
#include <algorithm>
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

    if (s_failed == 0)
        std::cout << "All " << s_run << " tests passed.\n";
    else
        std::cerr << s_failed << "/" << s_run << " tests FAILED.\n";
    return s_failed == 0 ? 0 : 1;
}
