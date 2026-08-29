# CONTEXT: Local Mixing MVP

> Single source of truth for the Local Mixing MVP.
> This document consolidates the project scope, pipeline, and accepted architectural decisions.
> All terminology follows `definition.md`.

---

## 1. Project Overview

**Goal**  
Implement a Minimum Viable Product (MVP) of the **Local Mixing** obfuscation approach on reversible logic circuits, using a 2-bit ripple-carry adder as the target.

Local Mixing (as defined in `definition.md`) is a paradigm that repeatedly applies functionality-preserving transformations to small local regions of a circuit, progressively diffusing and obscuring structural information while keeping input-output behavior identical. It draws inspiration from the design tradition of symmetric cryptography and hash functions rather than lattices or elliptic curves.

**This MVP implements only a simplified subset** of the full Local Mixing pipeline:
- Reversibility
- Hardening via sandwiching
- Simplified mixing (depth-level shuffling + zero-ancilla junk injection)

**Non-goals (explicitly out of scope)**  
- Full generation mixing (replacement of local sub-circuits from large equivalence tables)
- Advanced / nonlinear gadgetization
- Any form of security evaluation or attack resistance measurement
- Automatic irreversible-to-reversible conversion of arbitrary circuits
- Circuits larger than 2-bit for the initial pipeline validation

**Hard constraint**  
The transformed circuit must be functionally equivalent to the original for every possible input.

**Positioning of the 2-bit adder**  
The 2-bit circuit is used solely to validate correctness of the pipeline and the implementation of each phase. Its shallow depth means the practical mixing effect is limited; it is **not** intended as a benchmark for obfuscation strength.

---

## 2. Pipeline Phases

### Phase 1: Reversibility
- Represent the circuit as a directed acyclic graph (adjacency list).
- Use a fixed reversible gate library (Toffoli, CNOT, NOT).
- Construct the 2-bit ripple-carry adder directly with reversible gates (primarily 2 Toffoli gates + CNOT gates) instead of performing generic irreversible-to-reversible conversion.

### Phase 2: Hardening
- Primary technique: **Sandwiching** (`apply_sandwich` in `sandwich.hpp`).
- Insert random CNOT chains (slice gates) before and after critical sections.  Applying the same pairs in reverse order after the circuit forms the `F⁻¹ ∘ Adder ∘ F` structure that guarantees functional equivalence.
- Two wire-selection modes are supported (see `SandwichMode` in `sandwich.hpp` and `docs/adr/0002-active-sandwiching-mode.md`):
  - **Safe mode** — wires drawn from zero-initialized extra ancilla only; every chain gate is a no-op. Useful for isolated testing of downstream phases.
  - **Active mode** *(pipeline default)* — control wires from primary inputs, target wires from primary outputs.  Intermediate output wires carry `input_value XOR adder_partial_result`, providing genuine value-level mixing while preserving the 32-input truth table and the zero-ancilla invariant at every phase boundary.
- `PipelineConfig::sandwich_mode` defaults to `SandwichMode::Active`; pass `--sandwich-mode=safe` on the CLI to switch.

### Phase 3: Mixing (Simplified)
- **Shuffling**: Reorder gates that share the same topological depth level. Gates at the same depth are independent and may be freely permuted.
- **Junk Gate Injection**: Insert Toffoli gates that touch at least one zero-valued ancilla wire. Because one control is 0, the gate acts as an identity on the observable computation while increasing circuit volume.
- These operations constitute the MVP’s local, functionality-preserving transformations. They are a deliberate simplification of the full Local Mixing ideal (which includes repeated replacement of small sub-circuits by equivalent alternatives).

### Phase 4: Verification
- Exhaustive truth-table comparison over all 2⁵ = 32 input combinations (A1, A0, B1, B0, Cin).
- Confirm that outputs (S1, S0, Cout) are identical to the original circuit.
- Record gate count before and after each phase to compute the overhead ratio.

**Gadgetization** is deferred and explicitly out of scope for this MVP.

---

## 3. Architectural Decisions

| #  | Decision                  | Choice                              | Rationale |
|----|---------------------------|-------------------------------------|---------|
| 1  | Circuit representation    | Adjacency list                      | Natural for sparse circuits; supports efficient topological sort and gate insertion. |
| 2  | Gate data structure       | Inheritance / polymorphism (`Gate` base + derived classes) | Extensible; allows later addition of gadgets via virtual dispatch if needed. |
| 3  | Reversibility strategy    | Fixed reversible gate library       | 2-bit adder can be constructed directly; avoids complexity of a general converter at MVP stage. |
| 4  | Adder architecture        | Ripple-carry                        | Simplest reversible form; small depth makes pipeline effects easy to observe and debug. |
| 5  | Hardening method          | Random CNOT sandwich chains         | Self-inverse, simple to insert/remove, preserves reversibility, integrates cleanly with later shuffling. |
| 6  | Junk gate strategy        | Toffoli on zero-valued ancilla      | Identity-preserving when a control is 0; harder for trivial cleanup passes to remove than plain identity pairs. Requires strict maintenance of the zero-ancilla invariant. |
| 7  | Shuffling algorithm       | Topological depth level             | Correct by construction (same-depth gates are independent). Sufficient for a small circuit. |
| 8  | Mixing scope              | Simplified only (shuffle + junk + sandwich) | Full generation mixing and nonlinear gadgets are out of scope; keeps MVP focused and implementable. |
| 9  | Equivalence testing       | Exhaustive truth table (32 cases)   | Trivial runtime; gives 100 % functional correctness guarantee for the 2-bit instance. |
| 10 | Ancilla management        | Pre-allocate a fixed pool of zero-initialized ancilla wires | Guarantees the zero invariant required by junk gates and hardening; exact count to be decided in implementation Spec. |
| 11 | Randomness                | Deterministic and reproducible (`std::mt19937` or equivalent + recorded seed) | Essential for debugging and for repeating verification runs. |
| 12 | Project structure         | `gate.*` + `circuit.*` + `main.cpp` | Separates core abstractions from the phase pipeline; sufficient for MVP. |

---

## 4. Coding Standards

### .hpp / .cpp Separation

| Rule | Detail |
|------|--------|
| `.hpp` (headers) | Class declarations, inline trivial methods (≤ 3 lines, no complex control flow), `constexpr` / `consteval` functions, template implementations. |
| `.cpp` (source) | Non-trivial method implementations, free functions, `main()`, anything with loops / conditionals / allocations. |
| Header-only acceptable | Small utility classes or gate types where all methods are trivial getters and single-expression evaluators (e.g., current `Gate` hierarchy). |
| When to split | When a method exceeds ~5 lines, uses local variables, or has branching — move implementation to `.cpp`. |
| Include guard | `#pragma once` for all headers. |
| Forward declarations | Prefer forward declarations in headers to minimize include dependencies. |

---

## 5. File Structure (Planned)
