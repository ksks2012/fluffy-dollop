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
- One fixed, hand-written, provably-equivalent **local replacement** (R1) — a minimal sliver of generation mixing (see ADR 0004)

**Non-goals (explicitly out of scope)**  
- Full generation mixing (automated replacement of local sub-circuits from large equivalence tables) — *a single fixed, hand-written, provably-equivalent local rewrite (R1) is inside the MVP scope (ADR 0004)*
- Advanced / nonlinear gadgetization
- Any form of security evaluation or attack resistance measurement
- Automatic irreversible-to-reversible conversion of arbitrary circuits
- Circuits larger than 4-bit for this validation step (ADR 0001 scope boundary narrowed to >4-bit; see ADR 0001 and 0002, which remain in force)

**Hard constraint**  
The transformed circuit must be functionally equivalent to the original for every possible input.

**Positioning**  
The **4-bit ripple-carry adder** (33 gates, 21 wires, 512-input truth table) is the **primary validation circuit**: it has enough topological depth to make the shuffle phase's reordering space observable and the Active sandwich's scrambling effect non-trivial. The **2-bit adder** (15 gates, 9 wires, 32-input truth table) serves as the **fast regression baseline** and is used in isolated unit tests for each pipeline phase. Both sizes use the generic `make_nbit_adder(n)` factory (VBE-style, `n ∈ {2, 4}`) and the generalized `check_truth_table` / `check_ancilla_invariant` helpers that derive `n` from `num_inputs()`.

---

## 2. Pipeline Phases

### Phase 1: Reversibility
- Represent the circuit as a directed acyclic graph (adjacency list).
- Use a fixed reversible gate library (Toffoli, CNOT, NOT).
- Build the n-bit ripple-carry adder via `make_nbit_adder(n, extra_ancilla)` (VBE-style: forward carry chain → sums → reverse uncompute; `n ∈ {2, 4}`).

### Phase 2: Hardening
- Primary technique: **Sandwiching** (`apply_sandwich` in `sandwich.hpp`).
- Insert random CNOT chains (slice gates) before and after critical sections.  Applying the same pairs in reverse order after the circuit forms the `F⁻¹ ∘ Adder ∘ F` structure that guarantees functional equivalence.
- Two wire-selection modes are supported (see `SandwichMode` in `sandwich.hpp` and `docs/adr/0002-active-sandwiching-mode.md`):
  - **Safe mode** — wires drawn from zero-initialized extra ancilla only; every chain gate is a no-op. Useful for isolated testing of downstream phases.
  - **Active mode** *(pipeline default)* — control and target wires drawn from **all** current circuit wires `[0, num_wires())` with `ctrl ≠ tgt`.  The chain does not extend the wire count.  Intermediate wire values are genuinely scrambled during execution; the `F⁻¹ ∘ Adder ∘ F` structure preserves the zero-ancilla invariant at every phase boundary.
- `PipelineConfig::sandwich_mode` defaults to `SandwichMode::Active`; pass `--sandwich-mode=safe` on the CLI to switch.

### Phase 3: Mixing (Simplified)

Pipeline order: `adder → sandwich → local replace (R1) → shuffle → junk injection` (local replace is
described in ADR 0004).

- **Local Replacement (R1)**: matches strictly-adjacent triples of CNOT gates in the topological
  order (same target wire `s`, pairwise-distinct controls `a, b, c`, arbitrary control order) and
  rewrites each window in place as `Toffoli(a,b,s); CNOT(a,s); CNOT(b,s); CNOT(c,s); Toffoli(a,b,s)`.
  Net effect is identical (`s ⊕= a ⊕ b ⊕ c`); the rewrite adds 2 Toffoli gates per window and
  borrows 0 ancilla wires. Runs **before** shuffle: shuffle rearranges gates within each depth level,
  and since the three S-CNOTs of a window are serially dependent (all write `s`), they sit at
  different depths — shuffle would interleave unrelated same-depth gates and destroy the strict
  adjacency the match requires (ADR 0004 order amendment). `replacements == n` (n=2 → 2, n=4 → 4)
  holds deterministically when it runs before shuffle.
- **Shuffling**: Reorder gates that share the same topological depth level. Gates at the same depth are independent and may be freely permuted.
- **Junk Gate Injection**: Insert Toffoli gates that touch at least one zero-valued ancilla wire. Because one control is 0, the gate acts as an identity on the observable computation while increasing circuit volume.
- These operations constitute the MVP’s local, functionality-preserving transformations. They are a deliberate simplification of the full Local Mixing ideal (which includes repeated replacement of small sub-circuits by equivalent alternatives).

### Phase 4: Verification
- Exhaustive truth-table comparison over all `2^(2n+1)` input combinations (`n` bits per operand + Cin).
  - 2-bit: `2^5 = 32` inputs; 4-bit: `2^9 = 512` inputs.
- Confirm that outputs (S_{n-1}..S_0, Cout) are identical to the original circuit.
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
| 8  | Mixing scope              | Simplified (shuffle + junk + sandwich) + one fixed local replacement (R1) | Full automated generation mixing and nonlinear gadgets are out of scope; R1 (ADR 0004) is a deliberately minimal, hand-written, provably-equivalent sliver that validates the local-replacement concept; keeps MVP focused and implementable. |
| 9  | Equivalence testing       | Exhaustive truth table (`2^(2n+1)` cases)   | Trivial runtime for n≤4; gives 100 % functional correctness guarantee. Generalized checker derives n from `num_inputs()`. |
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

## 5. Documentation Structure

| Path | Purpose |
|------|---------|
| `docs/CONTEXT.md` | Single source of truth (this file) — current system state, decisions, coding standards |
| `docs/DEFINITION.md` | Domain glossary (Local Mixing, Reversible Circuit, Ancilla Wire, Sandwiching, Shuffling, Junk Gate, Overhead Ratio, …) |
| `docs/CHANGELOG.md` | Milestone/version history |
| `docs/DOCUMENTATION-LIFECYCLE.md` | Documentation management conventions |
| `docs/adr/` | Architecture Decision Records (accepted; superseded ones remain archived) |
| `docs/archive/tickets/` | Archived completed tickets (historical rationale only) |
| `docs/summary/` | Execution summaries per milestone |

Durable knowledge is distilled into `CONTEXT.md` / `DEFINITION.md`; raw tickets are archived to `docs/archive/tickets/` after merge.
