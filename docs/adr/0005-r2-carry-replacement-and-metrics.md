# ADR 0005 — R2 carry-majority replacement + per-phase structural metrics

> Extensions to the local-replacement MVP (ADR 0004): a second rewrite (R2) targeting the carry-majority
> triple — the first use of the borrow-1-zero-ancilla contract (B) — and lightweight per-phase
> structural metrics to make depth and gate-type evolution observable at every phase boundary.
> Synthesized from grilling session (2026-09-10); terminology follows `docs/DEFINITION.md`.

---

## Problem Statement

R1 (ADR 0004) validates the 0-ancilla special case of the contract-B local-replacement framework, but
only touches sum-section S-triples and uses no borrowed wire. Two gaps remain:

1. **Contract B unexercised** — the definition allows borrowing one pre-declared zero-ancilla wire
   during a window, restoring it to 0 before the window ends. Without exercising this path, the
   framework's real generative capability is unproven.
2. **No per-phase structure observability** — the pipeline records gate count, wire count, and
   ancilla invariant at each boundary, but not depth levels or gate-type composition. Without this,
   it is impossible to see how sandwich, replace, and shuffle change the *structure* (as opposed to
   just volume) of the circuit.

R2 and the metrics extension fill both gaps without enlarging the MVP scope.

---

## Solution

A second local-rewrite pattern `R2` is added to the existing `replace` phase, running after R1 in
the same single invocation. R2 matches a new window type — the **carry-majority 3-Toffoli triple**
(forward carry chain and reverse uncompute) — and rewrites each match using one borrowed
zero-ancilla wire `z`, restoring `z = 0` before the window ends.

**R2 prerequisite:** the circuit must have at least one extra-ancilla wire (`extra_ancilla >= 1`
in `make_nbit_adder` or `PipelineConfig`). When no extra wire is available (`extra_ancilla = 0`),
the R2 pass reports zero hits — this is correct, not an error. All R2 verification and unit tests
use `extra_ancilla >= 1` (bare adder tests use `make_nbit_adder(n, 1)`; pipeline default is
`extra_ancilla = 4`).

In parallel, lightweight structural metrics are appended to `PhaseStats` at each phase boundary and
exposed in the CLI phase table, making gate-type evolution (depth, CNOT/Toffoli/NOT counts) visible
per phase.

Full replace phase flow becomes:

```
apply_local_replace:
  Pass 1 — R1:  S-triple (3×CNOT) → 5-gate rewrite  (0 new wires)
  Pass 2 — R2:  carry 3-Toffoli    → 5-gate rewrite  (1 borrowed wire, restored)
  both single-pass, non-overlapping, no global topo-sort
```

`enable_replace_r2` (default `false`) controls whether Pass 2 runs; `enable_replace = false`
disables the entire replace phase including R2.

---

## Canonical Rewrite R2 (the carry-majority pattern)

**Window (match condition)** — all three must hold:

- Three **strictly adjacent** `Toffoli` gates in the topological order (no gate in between).
- All three target the **same wire `T`**.
- The six control slots contain exactly **three distinct wires** `a, b, ci` (each appearing in two
  of the three Toffoli — the exact identity of which Toffoli pairs which two controls does not
  matter; the rule is pure structural shape).

**Equivalence table:**

| Name   | Gate sequence                                                              | vs. original                       |
|--------|----------------------------------------------------------------------------|------------------------------------|
| Identity (original) | `Toffoli(a,b,T); Toffoli(a,ci,T); Toffoli(b,ci,T)`               | 3 Toffoli, 0 CNOT, 0 new wires   |
| **R2** | `CNOT(a,z); Toffoli(b,z,T); Toffoli(ci,z,T); Toffoli(b,ci,T); CNOT(a,z)` | 3 Toffoli, +2 CNOT, borrow z     |

**Equivalence argument (z lifetime explicit):**

1. `CNOT(a,z)` — writes `z = a` (z is now non-zero when `a = 1`; this is the temporary dirty state).
2. `Toffoli(b,z,T)` — T ⊕= b ∧ z = b ∧ a.
3. `Toffoli(ci,z,T)` — T ⊕= ci ∧ z = ci ∧ a.
4. `Toffoli(b,ci,T)` — T ⊕= b ∧ ci.
5. `CNOT(a,z)` — z = a ⊕ a = 0 (z restored; lifetime over).

Net effect on `T`: `T ⊕= (a ∧ b) ⊕ (a ∧ ci) ⊕ (b ∧ ci)` = maj(a,b,c), identical to the original
3-Toffoli window. `a, b, ci` are read-only (never written by R2). `z` is non-zero only between
steps 1–4 and is explicitly zeroed in step 5. `check_ancilla_invariant` verifies `z = 0` at the
phase boundary.

**Window scope:** the structural matcher does **not** distinguish forward carry chain from reverse
uncompute — both emit identical 3-Toffoli triples on the same target with the same control set, and
the R2 rewrite is valid on both. On the bare `make_nbit_adder(n, extra >= 1)` this gives `2n−1`
hits per size (`n = 2 → 3`, `n = 4 → 7`); the reverse-uncompute windows count toward the total.

**Borrowed wire `z`:** selected once at the start of the R2 pass — the first extra-ancilla wire
in the circuit (index `≥ 4n+1`, i.e. within the `[4n+1 .. 4n+extra_ancilla]` pool declared at
circuit construction). Deterministic (lowest index); `rng` is **not** consumed (retained only for
signature uniformity). Once selected, `z` is shared across all R2 windows in the same pass (serial
replacement; each window writes and restores `z`). If no extra-ancilla wire exists
(`extra_ancilla = 0`), R2 reports zero hits (no error, no circuit extension). This is a correct
degenerate case — not a failure.

---

## Per-phase structural metrics

Append to `PhaseStats` (all computed at the same moment as `check_ancilla_invariant`):

| Field                | Type       | Meaning                                                      |
|----------------------|------------|--------------------------------------------------------------|
| `depth_levels`       | `int`      | Number of topological depth levels in the circuit at this boundary (= `max_depth`, consistent with `compute_depth_levels`) |
| `count_cnot`         | `int`      | Number of `CnotGate` in the circuit                          |
| `count_toffoli`      | `int`      | Number of `ToffoliGate` in the circuit                       |
| `count_not`          | `int`      | Number of `NotGate` in the circuit (always 0 in current gate library; reserved for future use) |

These fields are always filled (including non-replace phases).

**Replace-phase hit tracking:** `PhaseStats` gains `int replacements_r1 = 0` and
`int replacements_r2 = 0` in addition to the existing `int replacements = 0` (total =
`replacements_r1 + replacements_r2`). The replace phase fills all three fields. The CLI displays:

- R2 off: `hits=N` (from `replacements`, which equals `replacements_r1`)
- R2 on: `hits=N+M` (from `replacements_r1 + replacements_r2`)

The split fields enable the `hits=N+M` display without ambiguity. The total `replacements` is the
sum and continues to be used for any aggregate assertions (e.g. `replacements >= 1`).

**CLI suffix:** each phase line gains a compact suffix after the existing columns:

```
replace  gates=57  wires=21  overhead=1.73x  ancilla=OK  hits=4+7  depth=16  toffoli=41  cnot=16
```

---

## Core Verification

Performed against `make_nbit_adder(n, extra_ancilla=1)` via one-off program (not committed);
regression authority is the test suite (ticket 0020), not this backdrop.

**R2 standalone (no R1), extra_ancilla=1:**

| n | hits | Toffoli before→after | CNOT before→after | TT | ancilla | wires (before→after) |
|---|------|----------------------|--------------------|----|---------|----------------------|
| 2 | 3 (2n−1) | 9 → 9 (+0) | 6 → 12 (+6) | PASS (32 inputs) | PASS | 10 → 10 (unchanged) |
| 4 | 7 (2n−1) | 21 → 21 (+0) | 12 → 26 (+14) | PASS (512 inputs) | PASS | 18 → 18 (unchanged) |

("unchanged" = `num_wires()` identical before and after R2; the extra wire exists from construction
and is not added by R2.)

**R1 + R2 combined, extra_ancilla=1:**

| n | R1 hits | R2 hits | Toffoli before→after | CNOT before→after |
|---|---------|---------|----------------------|--------------------|
| 2 | 2 | 3 | 9 → 13 (+4) | 6 → 18 (+12) |
| 4 | 4 | 7 | 21 → 29 (+8) | 12 → 38 (+26) |

R1 moves Toffoli count upward; R2 moves CNOT count upward — in opposite directions, ensuring
gate-type composition is distinguishable from both R1-only and R2-only baselines when both are
enabled.

**extra_ancilla=0 degenerate case:** when the circuit has no extra-ancilla wires
(`make_nbit_adder(n, 0)`), R2 has no suitable `z` and correctly reports zero hits. This is a valid
configuration, not an error.

---

## Implementation Decisions

1. **R2 as second pass within the same `replace` phase** — single function call, R1 runs fully
   first (single-pass, non-overlapping), then R2 runs (single-pass, non-overlapping). Two independent
   patterns, no interaction; the S-triple windows (sums section) and 3-Toffoli windows (carry
   section) do not overlap in the VBE adder.

2. **`enable_replace_r2` (default `false`)** — preserves R1-only behavior as the default for
   backward compatibility; R2 is opt-in via `--replace-r2` on the CLI. `enable_replace = false`
   disables the entire phase (both R1 and R2).

3. **Borrowed wire `z`** — selected once at phase start: first extra-ancilla wire (index
   `≥ 4n+1`); deterministic, no `rng` consumption; shared across all R2 windows in the same pass;
   re-zeroed within each window (contract-B postcondition). If no extra-ancilla wire exists
   (`extra_ancilla = 0`), R2 reports zero hits (correct degenerate case, not an error).

4. **Structural metrics fields** — always filled at every phase boundary (no conditional); named
   `depth_levels`, `count_cnot`, `count_toffoli`, `count_not` in `PhaseStats`. Computed once per
   boundary, at the same timing as `check_ancilla_invariant`. Full `gates_per_depth` vector is
   optional (available via `--verbose-metrics` or programmatic access); not stored in `PhaseStats`
   for the MVP.

5. **Replace-phase hit tracking** — `PhaseStats` gains `replacements_r1` and `replacements_r2`
   fields (both default 0); the replace phase fills them with per-pattern hit counts. The CLI
   displays `hits=N` (R2 off) or `hits=N+M` (R2 on), reading directly from these fields. The
   total `replacements = r1 + r2` is the sum.

6. **Depth may change after replace** — R2 adds 5-gate blocks in place; local WAW chains on the
   target wire can extend the depth of that wire's path. The `depth_levels` field makes any such
   change visible; it is not guaranteed to stay constant.

7. **No new phase names** — R2 is internal to `replace`; the phase table shows a single `replace`
   entry with total `replacements = R1_hits + R2_hits`.

---

## Scope Reconciliation

`CONTEXT.md` §1 non-goal "Full generation mixing..." is unchanged — R2 is still a single fixed,
hand-written local rewrite; only the pattern is broader than R1.

§2 Phase 3 description updated to note R2 as a second pass within the replace phase (forward +
reverse-uncompute carry triples, borrow-1-zero-ancilla contract, requires `extra_ancilla >= 1`).

---

## Out of Scope

- Pattern list generalization beyond R1/R2 (a third pattern triggers the question of whether to
  move to a config-driven pattern list; deferred).
- Security/attack-resistance claims for the specific R2 replacement.
- Circuits larger than 4-bit.
- Dedicated `replace_max` / multi-round / identity-drawing for R2 (same deferral as R1, ticket 0016).

---

## Testing Decisions

- **Good test:** bare-adder hit counts (`2n−1` for R2 with `extra_ancilla >= 1`, `n` for R1),
  exhaustive truth-table equivalence, ancilla invariant at every phase boundary, gate-type
  distinguishability (R1 +2 Toffoli per hit, R2 +2 CNOT per hit — each measurable via the
  `replacements_r1`/`replacements_r2` split in `PhaseStats`).
- **Full pipeline gold standard:** bare adder tests (with `extra_ancilla >= 1`) are the primary
  correctness assertions. Full pipeline tests require TT PASS + ancilla PASS +
  `replacements >= expected_floor` (e.g. `>= 1` when R2 is on), not exact hit counts — sandwich
  mode and seed may affect window availability after mixing.
- **Seam:** `apply_local_replace` remains the single seam; R2 is added inside the same function.
  Existing `check_truth_table` / `check_ancilla_invariant` as oracle; new per-phase metrics fields
  provide structural observability tests.

---

## Further Notes

- Combined gate-type delta on n=4 with both R1+R2: Toffoli 21→29, CNOT 12→38 — the two types move
  in opposite directions, making the combined effect unmistakable in a metrics-based audit.
- `depth_levels` may increase or decrease after any phase (sandwich chains add depth; shuffle
  flattens within levels; replace adds local volume that may extend depth on the target wire's path).
  The `depth_levels` field makes all such changes visible.

---

*Created: 2026-09-10*
*Source: grilling session Q1–Q6; R2 canonical verified against make_nbit_adder(2,1) and (4,1); ADR 0004.*
