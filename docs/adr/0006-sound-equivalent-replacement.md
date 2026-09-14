# ADR 0006 — Sound equivalent replacement (intermediate-value scrambling, local-replace family)

> Spec: a sound successor to the deprecated legacy Active sandwich for the intent of scrambling
> intermediate wire values, implemented as a third local-replace pattern (Pass 0: Sound) rather
> than a third sandwich mode. Synthesized from grilling session (2026-09-12); terminology follows
> `docs/DEFINITION.md`. Builds on ADR 0004 (R1) and ADR 0005 (R2); see the append note on ADR 0002.

---

## Problem Statement

The legacy Active sandwich (`SandwichMode::Active`) aimed to scramble the computation path's
intermediate values by wrapping the **entire** adder in `F⁻¹ ∘ Adder ∘ F` with a chain `F` drawn
from all wires. ADR 0002 withdrew the `F⁻¹ ∘ Adder ∘ F ≡ Adder` claim as false: conjugation does not
preserve a non-linear circuit when `F` touches wires the adder reads (A/B/Cin are eagerly consumed),
and Active is known to FAIL the truth table and the zero-ancilla invariant. It is NOT repaired in
place.

The *intent* — genuinely perturb intermediate wire states while preserving the hard equivalence
constraint — remains valuable and is the foundation for future keyed / multi-round symmetric paths.
This ADR delivers that intent through a mechanism that is correct by construction.

Key insight that fixes the root cause: the error in the legacy approach was **conjugating by a chain
that touches live inputs before they are consumed**. A correct approach must only rewrite **local
windows** whose interface equivalence can be proven, and must never touch the window's read-only
control wires as write targets.

---

## Solution

Add a third local-replace pattern, **Sound**, as **Pass 0** of `apply_local_replace` (running before
R1 and R2). Sound matches the same window type as R1 — three strictly-adjacent `CNOT` gates, all
targeting the same wire `s`, with three pairwise-distinct controls `a, b, c` (the sum-section
S-triples of the VBE adder) — and rewrites each window with a **verified equivalent sequence** that
borrows one zero-initialized ancilla `z` and restores it to 0 by the end of the window.

### Correctness contract (the soundness rule)

1. **Match structure:** a window is eligible only if it is single-target-write — exactly one wire
   `s` is written by the window, and all other wires the window touches are **read-only** inside the
   window.
2. **Replacement:** the rewritten sequence implements the **same net interface effect** as the
   original (for R1-family windows: `s ⊕= a ⊕ b ⊕ c`, with `a, b, c` unchanged).
3. **Borrowed ancilla:** a rewrite may borrow one extra zero-initialized ancilla `z`; `z` must be
   restored to `0` before the window ends, and no control or other architectural wire is secretly
   modified.
4. **Proof obligation:** the rewrite's equivalence is established by **window-level exhaustive
   enumeration** (all `2^{k}` assignments over the window wires plus `z`), embedded in a full-circuit
   truth-table check — NOT by asserting a conjugation identity.
5. **Forbidden:** wrapping the whole circuit in an outer `F⁻¹ ∘ C ∘ F`; any claim that "conjugation
   preserves equivalence" without proving the commutation / structure required. "Conjugation" may
   appear only as a heuristic description, never as the correctness argument.

### Canonical Sound rewrite (the T-c pattern)

For a window `W0 = CNOT(a,s); CNOT(b,s); CNOT(c,s)` (net `s ⊕= a⊕b⊕c`), the **sound v1** rewrite is:

```
T-c:  CNOT(a,z); CNOT(z,s); CNOT(a,z); CNOT(b,s); CNOT(c,s)
```

| Name | Gate sequence | vs. original |
|------|---------------|--------------|
| Identity (original) | `CNOT(a,s); CNOT(b,s); CNOT(c,s)` | 3 CNOT, 0 new wires |
| **Sound (T-c)** | `CNOT(a,z); CNOT(z,s); CNOT(a,z); CNOT(b,s); CNOT(c,s)` | 3 CNOT (+2 routing CNOT through z), borrow z |

**Equivalence argument (z lifetime explicit):**

1. `CNOT(a,z)` — writes `z = a` (z now holds a shadow of control `a`; non-zero exactly when `a = 1`).
2. `CNOT(z,s)` — `s ⊕= z = a` (the `a` contribution is routed through `z` instead of a direct `CNOT(a,s)`).
3. `CNOT(a,z)` — `z = a ⊕ a = 0` (z restored; the borrow contract closes before the window ends).
4. `CNOT(b,s)` — `s ⊕= b`.
5. `CNOT(c,s)` — `s ⊕= c`.

Net effect on `s`: `s ⊕= a ⊕ b ⊕ c` — identical to the original window. `a, b, c` are read-only
(never written: `CNOT(a,z)` target is `z`, `CNOT(z,s)` target is `s`). `z` is non-zero only between
steps 1–3 (exactly when `a = 1`), giving the intermediate-value perturbation the legacy Active
mode promised but failed to deliver soundly.

**Verification (window-level enumeration, performed 2026-09-12):** a one-off program compared W0 vs
T-c over all 16 assignments of `(a,b,c,s)` (`z` fixed 0 at entry, as required): outputs identical on
all 16; final `z = 0` in all; mid-window `z == 1` in exactly the 8 cases with `a == 1`.
Regression authority is the test suite (ticket 0026), not this backdrop.

**Window scope:** identical to R1 — both forward-pass and any later S-triple occurrences are
eligible; the structural matcher is shared with R1 (same `match_r1` shape). Sound hits are counted
in `replacements_sound`.

**Borrowed wire `z`:** same selection rule as R2 (ADR 0005 Decision 3) — the first extra-ancilla
wire (index `≥ 4n+1`), fixed, no `rng` consumption, shared across all Sound windows in the pass.
If no extra-ancilla wire exists (`extra_ancilla = 0`), Sound reports zero hits (correct degenerate
case, not an error).

**Fixed template (no `rng`):** v1 uses the single canonical T-c sequence above; `rng` is retained in
the signature for uniformity but NOT read (matching R1/R2). Template families / keyed derivation are
deferred to M5.2+.

---

## Interaction model with R1 / R2

- **Pass order inside `apply_local_replace`:** Pass 0 Sound → Pass 1 R1 → Pass 2 R2.
- **Independent scanning:** each pass is single-pass, non-overlapping, left-to-right; after rewriting
  a window, scanning resumes after the new block's length and never backtracks.
- **No shared interval bookkeeping:** passes do not maintain overlapping-interval sets across passes;
  each pass re-scans the current gate sequence, and position shifts are absorbed by the full re-scan.
- **R1 hits drop (accepted):** because Sound runs first and consumes some S-triple windows, R1 hit
  counts on the same circuit are no longer guaranteed `== n`. Hard acceptance conditions are TT PASS
  + ancilla PASS; `r1 == n` is only asserted when Sound is OFF (M4 regression).

---

## CLI / config surface

| Item | Value |
|------|-------|
| `PipelineConfig::enable_sound` | `bool enable_sound = false;` (opt-in, default off; M4 behavior unchanged) |
| CLI flag | `--sound` (symmetric to `--replace-r2`) |
| `PhaseStats` field | `int replacements_sound = 0;` (fills alongside `replacements_r1` / `replacements_r2`) |
| Master switch | `--no-replace` disables the entire replace phase (Sound, R1, R2) |

When `--sound` is off, all existing expectations (including `r1 == n`) hold unchanged.

---

## Implementation Decisions

1. **Pass 0 within `apply_local_replace`** — Sound is the third pattern of the local-replace family;
   it is NOT a `SandwichMode` value. `SandwichMode` and `apply_sandwich` are untouched.
2. **`enable_sound` (default `false`)** — opt-in, mirroring `enable_replace_r2`; preserves M4
   behavior as the default and keeps hit expectations stable unless explicitly requested.
3. **Shared matcher** — Sound reuses the `match_r1` structural shape (3 adjacent CNOT, same target,
   3 distinct controls); the matcher is generalized to report window position (already the case)
   with the Sound pass consuming from the same cursor.
4. **Fixed canonical template** — the single verified T-c sequence; no `rng` reads in v1.
5. **`replacements_sound` in `PhaseStats`** — always filled when the replace phase runs; the CLI
   `hits=` suffix becomes `hits=S+R1+R2` when `--sound` is on.
6. **`--no-replace` master switch** — disables Sound alongside R1/R2 (single gate for the phase).

---

## Scope Reconciliation

`CONTEXT.md` §2 Phase 3 / §3 decision table get a row for Sound (Pass 0, T-c pattern, borrow-z,
`enable_sound` default off). `README.md` documents `--sound` and the `hits=` display. Old-Active
narrative: `SandwichMode::Active` stays experimental / known-broken in the code and docs; it is not
repaired in place (ADR 0002 stands, appended below).

---

## Out of Scope

- Conjugation-by-construction claims (`F⁻¹ W F ≡ W` without structure); any whole-circuit outer
  `F⁻¹ C F`.
- Template families, keyed / multi-round schedules (M6–M7).
- Rewrites targeting carry-section Toffoli windows (non-linear; deferred to M5.2).
- Relaxing R1/R2 match patterns or adding new patterns.
- 8-bit circuits; formal correctness proofs beyond exhaustive enumeration; attack-resistance claims.

---

## Testing Decisions

- **Good test:** window-level exhaustive enumeration (T3, includes `z` as a wire), full-circuit TT +
  ancilla at every boundary (T4/T6), an intermediate-state existence test (T5) that proves some
  reproducible input yields `z != 0` mid-window while the complete rewrite ends `z = 0` and remains
  equivalent, plus M4 regression (T2) with `--sound` off.
- **T5 approach:** test-local half-sequence recombination — assemble a circuit where one window is
  replaced by only the T-c **prefix** that writes `z` (e.g. through step 1, `CNOT(a,z)`), evaluate,
  assert `z == 1`; then the full rewrite asserts `z == 0` and TT equivalence. No production
  partial-evaluate API is introduced. (Same spirit as `test_active_scramble`, but window-local.)
- **Seam:** `apply_local_replace` remains the single seam; Sound is Pass 0 inside it. Existing
  `check_truth_table` / `check_ancilla_invariant` are the oracles.
- **Expectations:** no-Sound ⇒ `r1 == n` unchanged; with-Sound ⇒ hard TT/ancilla PASS, `r1 >= 0`,
  `replacements_sound ==` deterministic window count on bare adders with `extra_ancilla >= 1`
  (exact numbers locked after ticket 0026 implementation and registered in the ticket).

---

## Further Notes

- The mid-window `z == 1 ⟺ a == 1` property is the exact deliverable the legacy Active advertised
  ("scramble intermediate values") — now with a *sound* construction and a window-level proof.
- `--sound` does not consume the `mt19937` stream, so enabling it does not perturb the seed
  sequence seen by later phases (shuffle/junk) — keeping regression behavior clean.

---

*Created: 2026-09-12*
*Source: grilling session Q1–Q9 (2026-09-12); correctness contract per grilling Q1; T-c canonical verified by window-level enumeration (all 16 assignments) and locked into this ADR; ADR 0004 / ADR 0005.*