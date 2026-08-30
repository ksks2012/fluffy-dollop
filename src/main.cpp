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
              << "  ancilla=" << (p.ancilla_ok ? "OK" : "FAIL")
              << "\n";
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
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if      (arg == "--no-sandwich") cfg.enable_sandwich = false;
        else if (arg == "--no-shuffle")  cfg.enable_shuffle  = false;
        else if (arg == "--no-inject")   cfg.enable_inject   = false;
        else if (arg.substr(0, 16) == "--sandwich-mode=") {
            std::string m = arg.substr(16);
            if      (m == "active") cfg.sandwich_mode = SandwichMode::Active;
            else if (m == "safe")   cfg.sandwich_mode = SandwichMode::Safe;
        }
        else if (arg.substr(0, 4) == "--n=") {
            cfg.n = std::stoi(arg.substr(4));
        }
    }

    std::cout << "Seed: " << cfg.seed << "  n=" << cfg.n << "\n";
    std::cout << "Phases enabled:"
              << (cfg.enable_sandwich ? " sandwich" : "")
              << (cfg.enable_shuffle  ? " shuffle"  : "")
              << (cfg.enable_inject   ? " inject"   : "")
              << "\n";
    if (cfg.enable_sandwich)
        std::cout << "Sandwich mode: "
                  << (cfg.sandwich_mode == SandwichMode::Active ? "active" : "safe")
                  << "\n";

    std::cout << "  Phase         Gates   Wires   Overhead   Ancilla\n";
    std::cout << "  ------------  ------  ------  ---------  -------\n";

    auto result = run_full_pipeline(cfg);
    for (const auto& p : result.phases) print_phase(p);

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
