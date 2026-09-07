#include "local_replace.hpp"
#include "gate.h"
#include <memory>

// ---------------------------------------------------------------------------
// R1 window matching — helper for gate inspection
// ---------------------------------------------------------------------------
static const CnotGate* as_cnot(const Gate& g) {
    return dynamic_cast<const CnotGate*>(&g);
}

// ---------------------------------------------------------------------------
// apply_local_replace — see header for the contract.
// ---------------------------------------------------------------------------
int apply_local_replace(Circuit& circuit, const LocalReplaceConfig& cfg, std::mt19937& rng) {
    (void)cfg;   // v1: no configurable knobs
    (void)rng;   // v1: every match is fixed to R1; rng kept only for signature uniformity

    const auto& gates = circuit.gates_ref();
    int replacements = 0;
    int i = 0;
    const int n = circuit.gate_count();

    while (i + 2 < n) {
        const CnotGate* c0 = as_cnot(*gates[i]);
        const CnotGate* c1 = as_cnot(*gates[i + 1]);
        const CnotGate* c2 = as_cnot(*gates[i + 2]);

        int s = c0 && c1 && c2 ? c0->target() : -1;
        bool same_target = c0 && c1 && c2
                        && c1->target() == s && c2->target() == s;

        int a = c0 ? c0->control() : -1;
        int b = c1 ? c1->control() : -1;
        int cc = c2 ? c2->control() : -1;
        bool distinct = same_target && a != b && b != cc && a != cc;

        if (!distinct) {
            ++i;
            continue;
        }

        // Replace the 3-gate window at position i with the 5-gate R1 sequence.
        circuit.remove_gates(i, 3);
        circuit.insert_gate(i,     std::make_unique<ToffoliGate>(a, b, s));
        circuit.insert_gate(i + 1, std::make_unique<CnotGate>(a, s));
        circuit.insert_gate(i + 2, std::make_unique<CnotGate>(b, s));
        circuit.insert_gate(i + 3, std::make_unique<CnotGate>(cc, s));
        circuit.insert_gate(i + 4, std::make_unique<ToffoliGate>(a, b, s));

        ++replacements;

        // Non-overlapping scan: resume *after* the new sub-circuit (5 gates).
        i += 5;
    }
    return replacements;
}