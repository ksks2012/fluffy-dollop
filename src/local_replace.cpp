#include "local_replace.hpp"
#include "gate.h"
#include <algorithm>
#include <memory>

// ---------------------------------------------------------------------------
// Gate inspection helpers
// ---------------------------------------------------------------------------
static const CnotGate* as_cnot(const Gate& g) {
    return dynamic_cast<const CnotGate*>(&g);
}

static const ToffoliGate* as_toffoli(const Gate& g) {
    return dynamic_cast<const ToffoliGate*>(&g);
}

// ---------------------------------------------------------------------------
// R1: collect window info at position i (3 consecutive CNOTs).
// Returns true and fills params if match found.
// ---------------------------------------------------------------------------
static bool match_r1(const std::vector<std::unique_ptr<Gate>>& gates, int i,
                     int& s, int& a, int& b, int& cc) {
    const CnotGate* c0 = as_cnot(*gates[i]);
    const CnotGate* c1 = as_cnot(*gates[i + 1]);
    const CnotGate* c2 = as_cnot(*gates[i + 2]);
    if (!c0 || !c1 || !c2) return false;
    s = c0->target();
    if (c1->target() != s || c2->target() != s) return false;
    a  = c0->control();
    b  = c1->control();
    cc = c2->control();
    return (a != b && b != cc && a != cc);
}

// ---------------------------------------------------------------------------
// R2: collect window info at position i (3 consecutive Toffolis).
// Returns true and fills params if match found:
//   same target T, six control slots contain exactly 3 distinct wires.
// ---------------------------------------------------------------------------
static bool match_r2(const std::vector<std::unique_ptr<Gate>>& gates, int i,
                     int& tgt, int& a, int& b, int& ci) {
    const ToffoliGate* t0 = as_toffoli(*gates[i]);
    const ToffoliGate* t1 = as_toffoli(*gates[i + 1]);
    const ToffoliGate* t2 = as_toffoli(*gates[i + 2]);
    if (!t0 || !t1 || !t2) return false;
    tgt = t0->target();
    if (t1->target() != tgt || t2->target() != tgt) return false;

    // Collect 6 control slots, find exactly 3 distinct wires.
    int controls[6] = {
        t0->control1(), t0->control2(),
        t1->control1(), t1->control2(),
        t2->control1(), t2->control2()
    };
    // Deduplicate into sorted distinct set.
    int d[3]; int nd = 0;
    for (int k = 0; k < 6 && nd < 3; ++k) {
        bool seen = false;
        for (int j = 0; j < nd; ++j)
            if (d[j] == controls[k]) { seen = true; break; }
        if (!seen) d[nd++] = controls[k];
    }
    if (nd != 3) return false;
    a  = d[0];
    b  = d[1];
    ci = d[2];
    return true;
}

// ---------------------------------------------------------------------------
// apply_local_replace — see header for the contract.
// ---------------------------------------------------------------------------
ReplaceResult apply_local_replace(Circuit& circuit, const LocalReplaceConfig& cfg, std::mt19937& rng) {
    (void)rng;   // v2: fixed rewrite patterns; rng kept only for signature uniformity

    ReplaceResult result;

    // ---- Pass 1: R1 (S-triple CNOT windows) ----
    {
        const auto& gates = circuit.gates_ref();
        int i = 0;
        const int n = circuit.gate_count();

        while (i + 2 < n) {
            int s, a, b, cc;
            if (!match_r1(gates, i, s, a, b, cc)) {
                ++i;
                continue;
            }

            circuit.remove_gates(i, 3);
            circuit.insert_gate(i,     std::make_unique<ToffoliGate>(a, b, s));
            circuit.insert_gate(i + 1, std::make_unique<CnotGate>(a, s));
            circuit.insert_gate(i + 2, std::make_unique<CnotGate>(b, s));
            circuit.insert_gate(i + 3, std::make_unique<CnotGate>(cc, s));
            circuit.insert_gate(i + 4, std::make_unique<ToffoliGate>(a, b, s));

            ++result.r1_hits;
            i += 5;   // non-overlapping
        }
    }

    // ---- Pass 2: R2 (carry-majority 3-Toffoli windows) ----
    if (cfg.enable_r2) {
        // z = first extra-ancilla wire: last non-output wire that no existing
        // gate targets (guaranteed zero throughout execution). Scanning from the
        // end picks the highest-indexed safe wire, which for the VBE adder is
        // an extra-ancilla wire (index >= 4n+1). For hand-built test circuits
        // it picks any unused wire. If none exists, z stays -1.
        const auto& outs = circuit.output_wires();
        const auto& gates = circuit.gates_ref();
        const int num_w = circuit.num_wires();
        int z = -1;
        for (int w = num_w - 1; w >= circuit.num_inputs(); --w) {
            bool is_out = std::find(outs.begin(), outs.end(), w) != outs.end();
            if (is_out) continue;
            bool is_targeted = false;
            for (const auto& g : gates)
                if (g->target_wire() == w) { is_targeted = true; break; }
            if (!is_targeted) { z = w; break; }
        }

        if (z >= 0) {
            const auto& gates = circuit.gates_ref();
            int i = 0;
            const int n = circuit.gate_count();

            while (i + 2 < n) {
                int tgt, a, b, ci;
                if (!match_r2(gates, i, tgt, a, b, ci)) {
                    ++i;
                    continue;
                }

                // Replace 3-gate window with canonical R2 5-gate sequence.
                circuit.remove_gates(i, 3);
                circuit.insert_gate(i,     std::make_unique<CnotGate>(a, z));
                circuit.insert_gate(i + 1, std::make_unique<ToffoliGate>(b, z, tgt));
                circuit.insert_gate(i + 2, std::make_unique<ToffoliGate>(ci, z, tgt));
                circuit.insert_gate(i + 3, std::make_unique<ToffoliGate>(b, ci, tgt));
                circuit.insert_gate(i + 4, std::make_unique<CnotGate>(a, z));

                ++result.r2_hits;
                i += 5;   // non-overlapping
            }
        }
        // z not found → r2_hits stays 0 (correct degenerate case).
    }

    return result;
}
