# Spec: Local Mixing MVP Implementation

> Synthesized from `CONTEXT.md`, `DEFINITION.md`, and grilling session decisions.
> All terminology follows `docs/DEFINITION.md`.

---

## Problem Statement

We need to implement and validate a working pipeline for the Local Mixing obfuscation technique on reversible logic circuits. No code exists yet. The project has fully defined architecture (12 accepted decisions in `CONTEXT.md`) but zero implementation. The immediate goal is to build, from scratch, a C++ MVP that:

1. Represents a reversible circuit as an adjacency-list DAG.
2. Constructs a 2-bit ripple-carry adder using a fixed reversible gate library (Toffoli, CNOT, NOT).
3. Applies three obfuscation transformations (sandwiching, shuffling, junk gate injection) while preserving functional equivalence.
4. Verifies correctness via exhaustive truth-table comparison over all 32 input combinations.

Without this MVP, there is no executable artifact to validate that the Local Mixing pipeline is implementable and functionally correct.

---

## Solution

Build a C++ program (`gate.*` + `circuit.*` + `main.cpp`) that:

1. Defines a polymorphic `Gate` class hierarchy (base + ToffoliGate, CnotGate, NotGate derived classes).
2. Builds a 2-bit ripple-carry adder circuit using an adjacency-list graph with pre-allocated zero-valued ancilla wires.
3. Executes a four-phase pipeline: Reversibility → Hardening (sandwiching) → Mixing (shuffling + junk injection) → Verification.
4. After each phase, prints the gate count and overhead ratio.
5. At the end, runs an exhaustive truth-table comparison confirming functional equivalence between the original and transformed circuit.

Deterministic randomness via `std::mt19937` with a recorded seed ensures reproducibility.

---

## User Stories

1. As a developer, I want the circuit to be represented as an adjacency-list DAG, so that I can efficiently traverse, insert gates, and compute topological depth levels.
2. As a developer, I want a polymorphic `Gate` class hierarchy, so that each gate type encapsulates its own evaluation logic and can be extended for future gadget replacement.
3. As a developer, I want a fixed reversible gate library (Toffoli, CNOT, NOT), so that I can construct the 2-bit adder directly without an irreversible-to-reversible converter.
4. As a developer, I want the 2-bit ripple-carry adder to be assembled from reversible gates (2 Toffoli + CNOTs), so that I have a known-correct baseline circuit for pipeline validation.
5. As a developer, I want pre-allocated zero-valued ancilla wires, so that junk gates and hardening operations have safe wires to operate on without violating the zero-ancilla invariant.
6. As a developer, I want topological depth level computation, so that I can identify which gates are independent and can be safely shuffled.
7. As a developer, I want random CNOT chain sandwiching (slice gates) inserted before and after critical sections, so that intermediate wire values are scrambled while preserving reversibility.
8. As a developer, I want depth-level shuffling that randomly permutes gates within the same topological depth level, so that the circuit's execution order is altered without affecting functionality.
9. As a developer, I want junk gate injection that inserts Toffoli gates on zero-valued ancilla wires, so that circuit volume increases while observable behavior remains unchanged.
10. As a developer, I want an exhaustive truth-table comparison over all 32 input combinations (A1, A0, B1, B0, Cin → S1, S0, Cout), so that I can confirm 100% functional equivalence after each phase.
11. As a developer, I want gate count and overhead ratio printed after each phase, so that I can measure the cost of each transformation.
12. As a developer, I want deterministic randomness with a recorded seed (`std::mt19937`), so that results are reproducible for debugging and verification.
13. As a developer, I want the project to build with CMake, so that I have a standard, cross-platform build system.
14. As a developer, I want each phase to be independently executable, so that I can test and debug phases in isolation.

---

## Implementation Decisions

1. **Gate class hierarchy (in-place model)**: `Gate` base class with virtual `evaluate(vector<int>& wires)` that mutates wire state in-place. Derived classes: `ToffoliGate`, `CnotGate`, `NotGate`. Each gate stores wire indices (control, target) and modifies `wires[target]` directly during evaluation. `Circuit::evaluate(input)` copies input into the wire vector, runs topologically sorted gates in sequence, then extracts output wires. Gadget replacement deferred.
2. **Circuit graph**: Adjacency list. Each node is a `Gate` instance. Edges represent wire connections between gates. The `Circuit` class manages the gate list, wire count (including ancilla), and topological sort.
3. **Ripple-carry adder construction (canonical)**: Direct assembly using reversible gates. Wire mapping: inputs 0–4 (A1, A0, B1, B0, Cin), outputs 5–7 (S1, S0, Cout), ancilla 8+ (zero-initialized). Circuit structure:
   - Toffoli(A0, B0, Cin) → carry to wire 8
   - Toffoli(A1, B1, wire 8) → Cout
   - CNOT(Cin, wire 5), CNOT(B0, wire 5), CNOT(A0, wire 5) → S0
   - CNOT(wire 8, wire 6), CNOT(B1, wire 6), CNOT(A1, wire 6) → S1
   - CNOT(A0, S0), CNOT(A1, S1) → copy sums to output wires
   Total: 2 Toffoli + 5 CNOT = 7 gates.
4. **Topological depth computation**: BFS from input wires. Depth of a gate = 1 + max(depth of all predecessor gates). Gates at the same depth level are independent.
5. **Sandwiching**: A "critical section" is any contiguous subsequence of gates in the topological order whose intermediate wire values carry information that should be obscured. For the 2-bit adder MVP, the critical section is the **entire adder circuit** (all gates between input and output). The sandwiching operation: (a) generate a random CNOT chain of configurable length, (b) insert the chain **before** the first gate of the critical section, (c) insert the **reversed** chain **after** the last gate. Because CNOT is self-inverse, the reversed chain cancels the forward chain's effect on the final output.
6. **Shuffling**: Group gates by topological depth level. Within each level, randomly permute gate order. Because the in-place evaluation model processes gates sequentially and same-depth gates share no data dependencies (no wire overlap in read/write sets), any permutation produces identical output. The permuted sequence is written back to the adjacency list. After shuffling, recompute depth levels to confirm they remain unchanged (sanity check).
7. **Junk gate injection**: For each injection point, create a `ToffoliGate` where at least one control wire is a known-zero ancilla wire. Insert into the circuit at a valid position (respects topological ordering).
8. **Ancilla management**: Pre-allocate a fixed pool of zero-initialized wires (exact count determined during adder construction, estimated at 2–4 wires for junk gate insertion).
9. **Randomness**: `std::mt19937` seeded with a fixed value (e.g., 42). Seed printed at program start for reproducibility.
10. **Verification**: Enumerate all 32 input combinations. For each, evaluate both the original and transformed circuit. Compare all output bits. Additionally, verify the **ancilla invariant**: after evaluation, all ancilla wires must remain 0. Report pass/fail and the first failing input if any.

---

## Testing Decisions

- **What is a good test**: Tests verify external behavior (output correctness) of each phase, not internal implementation details. The exhaustive truth-table comparison is the primary correctness oracle.
- **Modules to test**:
  - `Circuit::evaluate()` — produces correct output for known input
  - `Circuit::topological_sort()` — produces valid ordering
  - `Circuit::compute_depth_levels()` — assigns correct depth to each gate
  - `sandwiching()` — inserts slice gates and preserves functional equivalence
  - `shuffling()` — reorders gates and preserves functional equivalence
  - `junk_injection()` — adds junk gates and preserves functional equivalence
  - Full pipeline — end-to-end truth-table comparison
- **Test prior art**: No existing tests. This is a greenfield project. Testing approach follows standard C++ practice: a separate test binary (or `main.cpp` test mode) that exercises each phase and asserts correctness.
- **Test strategy**: Each phase wraps with a truth-table check. If any phase breaks equivalence, the program halts with the failing input and expected vs. actual outputs.

---

## Out of Scope

- Full generation mixing (replacement of local sub-circuits from large equivalence tables).
- Nonlinear gadgetization.
- Security evaluation or attack resistance measurement.
- Automatic irreversible-to-reversible conversion of arbitrary circuits.
- Circuits larger than 2-bit.
- Unit test framework (Google Test, Catch2) — MVP uses inline assertions.
- CMake test integration (`ctest`) — deferred to post-MVP.

---

## Further Notes

- The 2-bit adder's shallow depth (estimated 4–6 depth levels) means shuffling will have limited visual impact. This is expected and acceptable for pipeline validation.
- The zero-ancilla invariant is critical: if any phase accidentally modifies ancilla wire values, junk gate injection will silently corrupt the circuit. The truth-table check will catch this, but ancilla state should also be printed for debugging.
- Future work (post-MVP): add gadget replacement, support larger circuits, add formal equivalence checking, integrate with a real issue tracker.

---

*Created: 2026-08-26*
*Source: CONTEXT.md decisions #1–#12, DEFINITION.md glossary, grilling session outcomes.*
