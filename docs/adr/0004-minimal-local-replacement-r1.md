# ADR 0004 — Minimal Local Replacement, Direction A (R1 only)

> Spec: bring a deliberately minimal sliver of "generation mixing" into the MVP — replace matched
> local windows with a single fixed, hand-written, provably-equivalent rewrite (R1). Synthesized
> from grilling session outcomes (decisions Q1–Q6). Terminology follows `docs/DEFINITION.md`.
> Canonical R1 spec: `docs/archive/tickets/0016-local-replacement-r1.md` (superseded ticket, retained
> as the canonical reference; implementation split across archived tickets 0017/0018/0019).

---

## Problem Statement

`CONTEXT.md` currently lists "Full generation mixing (replacement of local sub-circuits from large
equivalence tables)" as an explicit non-goal. The existing mixing phases — sandwich, shuffle, junk
injection — all mutate the circuit in ways that change *wires it touches* or *reorder/append*
gates, but none performs a *structural local rewrite*: replacing a matched sub-circuit with a
different-looking but equivalent one. That is the minimal essence of generation mixing.

This ADR pulls back into scope only the *smallest defensible* sliver: a single, fixed, hand-written,
provably-equivalent rewrite of one specific window pattern against the actual `make_nbit_adder`
gate sequence. It is a validation token that local replacement is meaningful — not a security
measurement, not an automated equivalence-table search.

Deliberately narrow: one rewrite (`R1`), one window pattern, single-pass, no new wire in the
canonical case, pipeline-integrated in the same shape as the existing phases.

---

## Solution

A new pipeline phase `local replace` sits between `sandwich` and `shuffle`. In one pass over the
current topological gate sequence, it finds every window matching the `R1` pattern and replaces it
in place with the `R1` equivalent sub-circuit. The replacement adds 2 Toffoli gates per window and
no new wires, so gate-type counts become distinguishable while functional equivalence and the
zero-ancilla invariant are preserved at every phase boundary.

Full phase order becomes:

```
adder → sandwich → local replace (R1) → shuffle → inject → verification
```

> **Order amendment (empirical, not silent):** the original grill decision Q4 placed `replace`
> *after* `shuffle`. During implementation, measurement showed that shuffle reduces full-pipeline
> hits to ~0: shuffle permutes gates within each depth level, and the three S-CNOTs of a window all
> write `s`, so they are serially dependent (WAW) and sit at *different* depths — after shuffle,
> unrelated same-depth gates get interleaved between them, destroying the strict adjacency the match
> requires. Sandwich preserves the windows (2/4 hits), shuffle destroys them (0 hits). Rerunning
> match-rate evaluation per Q4's own precondition ("若實測匹配率過低，再單開評估"), the phase was
> moved before shuffle — replacement now happens while the structure is still tight, keeping
> `replacements == n` deterministic on the full pipeline (n=2 → 2, n=4 → 4 across Safe/Active and
> all seeds tested). A stricter post-shuffle matcher (allowing unrelated intervening gates) remains a
> v2 option; not enabled here.

---

## Canonical Rewrite R1 (the single locked pattern)

**Window (match condition)** — all four must hold:

- Three **strictly adjacent** `CNOT` gates in the topological order (no other gate in between).
- All three target the **same wire `s`**.
- The three control wires are **pairwise distinct** (call them `a, b, c`); the ordering of the
  controls is arbitrary — matching must not assume the adder's specific `A/B/Cin` order.

**Equivalence table (two written forms):**

| Name   | Gate sequence                                             | vs. original                 |
|--------|-----------------------------------------------------------|------------------------------|
| Identity | `CNOT(a,s); CNOT(b,s); CNOT(c,s)`                      | original                     |
| **R1** | `Toffoli(a,b,s); CNOT(a,s); CNOT(b,s); CNOT(c,s); Toffoli(a,b,s)` | +2 Toffoli, 0 new wires |

**Equivalence argument:** the two `Toffoli(a,b,s)` each contribute `a ∧ b` to `s` and cancel;
the three middle `CNOT`s contribute `a ⊕ b ⊕ c`. Net effect is `s ⊕= a ⊕ b ⊕ c`, identical to the
window. `a, b, c` are read-only controls; the *update* to `s` is the same as the original window
(`s`'s own pre-window value need not be 0 — the equivalence holds for any initial `s`).
Functionally equivalent, reversible, no new wire.

**Scanning:** single pass, **non-overlapping** — after a replacement, the next window search
starts *after* the new sub-circuit, so no matched span is scanned twice.

---

## Core Verification (performed, not assumed)

Verified end-to-end with a one-off program (not committed) against the real `make_nbit_adder`;
regression authority going forward is the test suite (tickets 0017/0019), not this backdrop:

- Hit: `make_nbit_adder(2,0)` has **2** S-triple windows; `make_nbit_adder(4,0)` has **4**.
- Exhaustive truth table on the R1-replaced adder: n=2 (32 cases) **PASS**, n=4 (512 cases) **PASS**.
- `check_ancilla_invariant` on the R1-replaced adder: **PASS** for both sizes.
- Gate-type distinguishability: n=4 Toffoli **21→29**, CNOT unchanged (12); n=2 Toffoli **9→13**.
- Wire count unchanged (9 and 17) — R1 borrows **0** ancilla wires (decision B's 0-ancilla special case).

---

## Implementation Decisions

1. **Structural contract (B)** — a local replacement may borrow at most one pre-declared
   zero-ancilla wire, computing equivalently *inside* the window and restoring it to 0 before the
   window ends, to preserve the phase-boundary ancilla invariant. R1 is the **0-ancilla special
   case** (borrows zero wires).
2. **Pattern strategy** — exclude pure reordering/commutation rewrites (they overlap with shuffle
   and cannot make gate-type counts distinguishable). First version: only the P2 family (rewrites
   that change the gate-type multiset). R2 (borrowing 1 ancilla, different window) is deferred.
3. **Canonical source** — equivalence forms are derived by the agent against the actual adder
   sequence, verified by program against the real circuit, then locked as canonical after review.
4. **R1 only** — single pattern locked for the first version (see Canonical Rewrite section).
5. **Phase shape — single pass (A)** — each matching window is replaced exactly once; no rescan,
   no multi-round iteration. A `--replace-max` limit is deferred to a second version.
6. **Pipeline position** — `adder → sandwich → local replace (R1) → shuffle → inject → verification`.
   *(Amended from the grill session's original `shuffle → replace` order — see the Order amendment
   note in the Solution section: shuffle destroys the strict-adjacency windows, so replace runs while
   the structure is still tight.)*
7. **Same phase shape as existing phases** — `apply_local_replace(Circuit&, ..., rng)` with a
   `PipelineConfig` toggle, a CLI switch, `PhaseStats` entry, and per-phase ancilla verification.
8. **Matching rule** — strictly adjacent triples of CNOT, same target, pairwise-distinct controls,
   arbitrary control order; no wide-window matching.
9. **Insertion** — in-place at the original window position; **no** global re-topological-sort.
   The 5-gate sub-circuit's internal ordering is statically valid (the two Toffoli's WAW
   dependency on `s` is ordered), and the window's external net effect is equivalent, so the
   surrounding gates' dependencies are preserved.
10. **Config** — add `bool enable_replace = true` only (`--no-replace` disables). No `replace_max`.
11. **Observability** — add `int replacements = 0` to `PhaseStats`; R1 fills the actual hit count;
    `print_phase` shows `hits=N` when `replacements > 0`.
12. **Verification** — full-circuit `2^(2n+1)` truth table and per-phase ancilla invariant; both
    2-bit and 4-bit must remain green.

---

## Scope Reconciliation

`CONTEXT.md` §1 non-goal "Full generation mixing (replacement of local sub-circuits from large
equivalence tables)" is refined to: *automated/large-table generation mixing remains out of scope;
a single fixed, hand-written, provably-equivalent local rewrite (R1) is inside the MVP.* No security
scope is touched. ADR 0001/0002/0003 remain in force.

---

## Out of Scope

- R2 (borrowing 1 ancilla; different window), P1 (pure reordering).
- Multi-round scans, `replace_max`, automatic equivalence-table enumeration, nonlinear gadgetization.
- Security/attack-resistance evaluation.
- Circuits larger than 4-bit.

---

## Testing Decisions

- **Good test**: a test verifies external observable behavior — window hit counts on the real
  adder, exhaustive truth-table equivalence, per-phase ancilla invariant, and gate-type-count
  distinguishability (`toffoli_after == toffoli_before + 2 * replacements`) — not matching
  implementation internals.
- **Seam**: the new phase function `apply_local_replace` plus the existing generalized
  `check_truth_table` / `check_ancilla_invariant` as oracle. Implemented via the existing
  unit-test harness (adding `test_local_replace.cpp`).

---

## Further Notes

- Robust gate-type assertion: prefer `toffoli_count_after == toffoli_count_before + 2 * replacements`
  over an absolute `21→29`, so the test survives minor adder-gate-count changes.
- `replacements == n` for a bare adder (n=2 → 2, n=4 → 4) is the deterministic hit expectation.

---

*Created: 2026-09-06*
*Source: grilling session, decisions Q1–Q6; to-spec synthesis.*