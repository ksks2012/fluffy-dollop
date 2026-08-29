#include <iostream>
#include <vector>
#include <algorithm>
#include "pipeline.hpp"

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
    // adder + sandwich + shuffle + inject = 4 phase entries
    CHECK(r.phases.size() == 4);
}

static void test_pipeline_phase_names() {
    auto r = run_full_pipeline(default_cfg());
    CHECK(r.phases[0].name == "adder");
    CHECK(r.phases[1].name == "sandwich");
    CHECK(r.phases[2].name == "shuffle");
    CHECK(r.phases[3].name == "inject");
}

// ---------------------------------------------------------------------------
// Gate counts at each phase
// ---------------------------------------------------------------------------
static void test_pipeline_gate_count_adder() {
    auto r = run_full_pipeline(default_cfg());
    CHECK(r.phases[0].gate_count == 15);
}

static void test_pipeline_gate_count_sandwich() {
    // sandwich adds 2 * chain_len gates
    auto cfg = default_cfg();
    auto r = run_full_pipeline(cfg);
    CHECK(r.phases[1].gate_count == 15 + 2 * cfg.sandwich_chain_length);
}

static void test_pipeline_gate_count_shuffle_unchanged() {
    auto r = run_full_pipeline(default_cfg());
    // shuffle never changes gate count
    CHECK(r.phases[2].gate_count == r.phases[1].gate_count);
}

static void test_pipeline_gate_count_inject() {
    auto cfg = default_cfg();
    auto r = run_full_pipeline(cfg);
    CHECK(r.phases[3].gate_count == r.phases[2].gate_count + cfg.inject_count);
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
    auto r = run_full_pipeline(default_cfg());
    for (const auto& p : r.phases) {
        if (!p.ancilla_ok)
            std::cerr << "  ancilla violation at phase: " << p.name << "\n";
        CHECK(p.ancilla_ok);
    }
}

// ---------------------------------------------------------------------------
// Final truth table
// ---------------------------------------------------------------------------
static void test_pipeline_final_truth_table() {
    auto r = run_full_pipeline(default_cfg());
    CHECK(check_truth_table(r.circuit));
}

// ---------------------------------------------------------------------------
// Wire budget: final circuit covers wires 0..inject_ancilla.back()
// ---------------------------------------------------------------------------
static void test_pipeline_wire_budget() {
    auto cfg = default_cfg();
    int expected_max = *std::max_element(cfg.inject_ancilla.begin(),
                                          cfg.inject_ancilla.end()) + 1;
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
    // phases: adder + shuffle + inject = 3
    CHECK(r.phases.size() == 3);
    CHECK(r.phases[0].name == "adder");
    CHECK(r.phases[1].name == "shuffle");
    // No sandwich → no extra ancilla → shuffle won't change gate count
    CHECK(r.phases[0].gate_count == 15);
    CHECK(check_truth_table(r.circuit));
}

static void test_pipeline_no_shuffle() {
    PipelineConfig cfg;
    cfg.enable_shuffle = false;
    auto r = run_full_pipeline(cfg);
    CHECK(r.phases.size() == 3);
    CHECK(r.phases[1].name == "sandwich");
    CHECK(r.phases[2].name == "inject");
    CHECK(check_truth_table(r.circuit));
}

static void test_pipeline_no_inject() {
    PipelineConfig cfg;
    cfg.enable_inject = false;
    auto r = run_full_pipeline(cfg);
    CHECK(r.phases.size() == 3);
    CHECK(r.phases[2].name == "shuffle");
    CHECK(check_truth_table(r.circuit));
}

static void test_pipeline_adder_only() {
    PipelineConfig cfg;
    cfg.enable_sandwich = false;
    cfg.enable_shuffle  = false;
    cfg.enable_inject   = false;
    auto r = run_full_pipeline(cfg);
    CHECK(r.phases.size() == 1);
    CHECK(r.phases[0].gate_count == 15);
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
    // Both still correct
    CHECK(check_truth_table(r1.circuit));
    CHECK(check_truth_table(r2.circuit));
}

// ---------------------------------------------------------------------------
// Ticket 0010: default sandwich_mode is Active
// ---------------------------------------------------------------------------
static void test_pipeline_default_mode_is_active() {
    PipelineConfig cfg;
    CHECK(cfg.sandwich_mode == SandwichMode::Active);
}

// ---------------------------------------------------------------------------
// Ticket 0010: end-to-end pipeline with Active mode passes truth table
// ---------------------------------------------------------------------------
static void test_pipeline_active_mode_truth_table() {
    PipelineConfig cfg;
    // default cfg already has sandwich_mode=Active
    auto r = run_full_pipeline(cfg);
    CHECK(check_truth_table(r.circuit));
}

// ---------------------------------------------------------------------------
// Ticket 0010: end-to-end pipeline with Active mode passes ancilla invariant
// ---------------------------------------------------------------------------
static void test_pipeline_active_mode_ancilla_invariant() {
    PipelineConfig cfg;
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
    // adder + shuffle + inject = 3; no sandwich phase entry
    CHECK(r.phases.size() == 3);
    CHECK(r.phases[0].name == "adder");
    CHECK(r.phases[1].name == "shuffle");
    CHECK(check_truth_table(r.circuit));
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
    test_pipeline_final_truth_table();
    test_pipeline_wire_budget();
    test_pipeline_deterministic();
    test_pipeline_no_sandwich();
    test_pipeline_no_shuffle();
    test_pipeline_no_inject();
    test_pipeline_adder_only();
    test_pipeline_different_seeds();
    test_pipeline_default_mode_is_active();
    test_pipeline_active_mode_truth_table();
    test_pipeline_active_mode_ancilla_invariant();
    test_pipeline_no_sandwich_beats_mode();

    if (s_failed == 0)
        std::cout << "All " << s_run << " tests passed.\n";
    else
        std::cerr << s_failed << "/" << s_run << " tests FAILED.\n";
    return s_failed == 0 ? 0 : 1;
}
