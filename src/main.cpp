#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>
#include <random>
#include <algorithm>
#include "gate.h"
#include "circuit.hpp"
#include "adder.hpp"
#include "pipeline.hpp"

static void print_phase(const PhaseStats& p) {
    std::cout << "  " << std::left << std::setw(10) << p.name
              << "  gates=" << std::setw(4) << p.gate_count
              << "  wires=" << std::setw(3) << p.wire_count
              << "  overhead=" << std::fixed << std::setprecision(2) << p.overhead << "x"
              << "  ancilla=" << (p.ancilla_ok ? "OK" : "FAIL");
    if (p.replacements > 0) {
        if (p.replacements_sound > 0)
            std::cout << "  hits=" << p.replacements_sound << "+"
                      << p.replacements_r1
                      << (p.replacements_r2 > 0 ? "+" + std::to_string(p.replacements_r2) : "");
        else if (p.replacements_r2 > 0)
            std::cout << "  hits=" << p.replacements_r1 << "+" << p.replacements_r2;
        else
            std::cout << "  hits=" << p.replacements;
    }
    std::cout << "  depth=" << p.depth_levels
              << " toffoli=" << p.count_toffoli
              << " cnot=" << p.count_cnot;
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    std::cout << "=== Gate hierarchy demo ===\n";
    { std::vector<int> w={1,1,0}; ToffoliGate(0,1,2).evaluate(w);
      std::cout << "  Toffoli(1,1,?) -> wires[2]=" << w[2] << "  (flipped)\n"; }
    { std::vector<int> w={1,0};   CnotGate(0,1).evaluate(w);
      std::cout << "  CNOT(1,?)     -> wires[1]=" << w[1] << "  (flipped)\n"; }
    { std::vector<int> w={0};     NotGate(0).evaluate(w);
      std::cout << "  NOT(0)        -> wires[0]=" << w[0] << "  (flipped)\n"; }

    std::cout << "\n=== Full local-mixing pipeline ===\n";

    PipelineConfig cfg;
    int bits_arg = cfg.n;          // default from PipelineConfig (2)
    bool verbose_metrics = false;  // --verbose-metrics (ticket 0022)
    bool has_key = false, has_seed = false;  // ADR 0007 precedence hint

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if      (arg == "--no-sandwich") cfg.enable_sandwich = false;
        else if (arg == "--no-shuffle")  cfg.enable_shuffle  = false;
        else if (arg == "--no-replace")  cfg.enable_replace  = false;
        else if (arg == "--replace-r2")  cfg.enable_replace_r2  = true;
        else if (arg == "--sound")       cfg.enable_sound       = true;
        else if (arg == "--no-inject")   cfg.enable_inject   = false;
        else if (arg.substr(0, 16) == "--sandwich-mode=") {
            std::string m = arg.substr(16);
            if      (m == "active") cfg.sandwich_mode = SandwichMode::Active;
            else if (m == "safe")   cfg.sandwich_mode = SandwichMode::Safe;
            // else: unrecognised value ignored; default (Safe) kept (ticket 0024)
        }
        else if (arg.substr(0, 7) == "--seed=") {
            has_seed = true;
            cfg.seed = std::stoi(arg.substr(7));
        }
        else if (arg.substr(0, 6) == "--key=") {
            has_key = true;
            cfg.key = arg.substr(6);
            // ADR 0007: an empty / whitespace-only key is a user error, never "no key".
            bool blank = true;
            for (unsigned char ch : cfg.key)
                if (ch > ' ' && ch != 0x7f) { blank = false; break; }
            if (cfg.key.empty() || blank) {
                std::cerr << "error: --key= requires a non-empty string value\n";
                return 1;
            }
        }
        else if (arg.substr(0, 7) == "--bits=") {
            bits_arg = std::stoi(arg.substr(7));
        }
        else if (arg.substr(0, 16) == "--extra-ancilla=") {
            cfg.extra_ancilla = std::stoi(arg.substr(16));
        }
        else if (arg == "--verbose-metrics") {
            verbose_metrics = true;
        }
        // --no-sandwich takes precedence over --sandwich-mode
    }

    // ADR 0007 precedence: --key > --seed > default 42 (key wins when both given).
    if (has_key && has_seed && !cfg.key.empty())
        std::cerr << "warning: --key overrides --seed (ADR 0007 precedence)\n";

    // Validate --bits: only 2 and 4 are supported in this release.
    if (bits_arg != 2 && bits_arg != 4) {
        std::cerr << "error: --bits=" << bits_arg
                  << " is not supported; use 2 or 4\n";
        return 1;
    }
    cfg.n = bits_arg;

    std::cout << "Seed: " << cfg.seed
              << "  bits=" << cfg.n
              << "  extra-ancilla=" << cfg.extra_ancilla
              << (cfg.key.empty() ? "" : "  key=\"" + cfg.key + "\"")
              << "\n";
    std::cout << "Phases enabled:"
              << (cfg.enable_sandwich ? " sandwich" : "")
              << (cfg.enable_replace  ? " replace"  : "")
              << (cfg.enable_replace_r2 ? " --replace-r2" : "")
              << (cfg.enable_sound    ? " --sound" : "")
              << (cfg.enable_shuffle  ? " shuffle"  : "")
              << (cfg.enable_inject   ? " inject"   : "")
              << "\n";
    if (cfg.enable_sandwich) {
        std::cout << "Sandwich mode: "
                  << (cfg.sandwich_mode == SandwichMode::Active ? "active" : "safe")
                  << "\n";
        if (cfg.sandwich_mode == SandwichMode::Active)
            std::cout << "  WARNING: Active sandwich does NOT guarantee truth-table / ancilla "
                      << "equivalence (known defect — see ADR 0002 amend, ticket 0024).\n"
                      << "  Use --sandwich-mode=safe for guaranteed TT.\n";
    }

    // -------------------------------------------------------------------------
    // Depth analysis on the base adder (no extra ancilla, no pipeline phases).
    // Shows the shuffle space: gates per topological depth level.
    // -------------------------------------------------------------------------
    {
        auto base = make_nbit_adder(cfg.n, 0);
        auto depths = base.compute_depth_levels();
        int max_d = depths.empty() ? 0 : *std::max_element(depths.begin(), depths.end());
        std::vector<int> per_level(max_d + 1, 0);
        for (int d : depths) per_level[d]++;
        std::cout << "\nBase adder depth analysis (" << base.gate_count()
                  << " gates, max depth " << max_d << "):\n";
        for (int lvl = 1; lvl <= max_d; ++lvl)
            if (per_level[lvl] > 0)
                std::cout << "  depth " << std::setw(2) << lvl
                          << " : " << std::setw(3) << per_level[lvl] << " gate(s)\n";
    }

    std::cout << "\n  Phase         Gates   Wires   Overhead   Ancilla\n";
    std::cout << "  ------------  ------  ------  ---------  -------\n";

    auto result = run_full_pipeline(cfg);
    for (const auto& p : result.phases) print_phase(p);

    // Ticket 0022: --verbose-metrics prints per-depth histogram of the final circuit
    if (verbose_metrics) {
        const auto depths = result.circuit.compute_depth_levels();
        int maxd = depths.empty() ? 0 : *std::max_element(depths.begin(), depths.end());
        std::vector<int> per_level(maxd + 1, 0);
        for (int d : depths) ++per_level[d];
        std::cout << "\nGates per depth (final circuit):\n";
        for (int lvl = 1; lvl <= maxd; ++lvl)
            if (per_level[lvl] > 0)
                std::cout << "  depth " << std::setw(2) << lvl
                          << " : " << std::setw(3) << per_level[lvl] << " gate(s)\n";
    }

    bool tt_ok = check_truth_table(result.circuit);
    const int n_inputs = result.circuit.num_inputs();
    const int combos   = 1 << n_inputs;
    std::cout << "\nFinal truth-table: "
              << (tt_ok ? "PASS" : "FAIL")
              << " (all " << combos << " inputs checked)\n";

    const auto& last = result.phases.back();
    bool all_ancilla = true;
    for (const auto& p : result.phases) if (!p.ancilla_ok) { all_ancilla = false; break; }

    std::cout << "\nSummary:\n"
              << "  Original gate count : " << result.phases[0].gate_count << "\n"
              << "  Final gate count    : " << last.gate_count   << "\n"
              << "  Final wire count    : " << last.wire_count   << "\n"
              << "  Final overhead      : " << std::fixed << std::setprecision(2) << last.overhead << "x\n"
              << "  Ancilla invariant   : " << (all_ancilla ? "OK (all phases)" : "FAIL") << "\n"
              << "  Truth table         : " << (tt_ok ? "PASS" : "FAIL") << "\n";

    return (tt_ok && all_ancilla) ? 0 : 1;
}
