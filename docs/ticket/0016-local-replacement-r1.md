# 0016 — Minimal Local Replacement (R1)

**Status:** superseded — split into 0017 (core algorithm), 0018 (pipeline integration), 0019 (CLI, tests & regression, docs reconciliation). This file retains the locked R1 spec as the canonical reference.

**What to build:** Add a new pipeline phase `local replace (R1)` that performs a minimal, single:
hand-written, provably-equivalent local rewrite. It scans the topological gate sequence for the R1
window pattern, and for each match replaces it in place with the R1 equivalent sub-circuit
(five gates; +2 Toffoli per window; 0 new wires), in a single pass. The phase runs **after
`shuffle` and before `inject`**, following the same phase shape as sandwich/shuffle/inject
(`apply_local_replace(Circuit&, ..., rng)` + `PipelineConfig::enable_replace` + CLI `--no-replace`
+ `PhaseStats::replacements` + per-phase ancilla verification). `rng` appears only to keep the
signature uniform with other phases; it is unused in v1 — every match is fixed to R1, no Identity
drawing. This is a validation token only —
no security claim, no automated equivalence-table search.

**Blocked by:** None (0012/0013/0014 already landed).

## Canonical rewrite R1 (the single locked pattern)

**Window (match condition):**
- Three **strictly adjacent** CNOT gates in the topological order (no gate in between)
- All three target the same wire `s`
- The three control wires are pairwise distinct (call them `a, b, c`); **control order is
  arbitrary** — matching must not assume the adder's specific A/B/Cin ordering

**Equivalence table:**

| Name | Gate sequence |
|------|---------------|
| Identity (original) | `CNOT(a,s); CNOT(b,s); CNOT(c,s)` |
| R1 | `Toffoli(a,b,s); CNOT(a,s); CNOT(b,s); CNOT(c,s); Toffoli(a,b,s)` |

**Equivalence:** the two `Toffoli(a,b,s)` each contribute `a∧b` to `s` and cancel; the three
middle CNOTs contribute `a⊕b⊕c`. Net effect `s ⊕= a⊕b⊕c` — identical to the window. `a,b,c` are
read-only controls; `s`'s net value unchanged. Reversible, **0 new wires** (the borrow-1-ancilla
contract B's 0-ancilla special case).

**Scanning:** single pass, **non-overlapping** — after a replacement, continue scanning *after*
the new sub-circuit, so no matched span is rescanned.

---

- [ ] `apply_local_replace(Circuit&, const LocalReplaceConfig&, std::mt19937&)` (or equivalent
      naming) scans for strictly-adjacent 3-CNOT windows sharing one target with pairwise-distinct,
      order-arbitrary controls, and replaces each match in place with the exact five-gate R1
      sequence `Toffoli(a,b,s); CNOT(a,s); CNOT(b,s); CNOT(c,s); Toffoli(a,b,s)`
- [ ] Single pass: exactly one replacement per matching window; no rescan of the replaced span;
      no multi-round iteration
- [ ] Replacement introduces **zero** new wires (0-ancilla); the window's external interface net
      effect is equivalent; **no** global re-topological-sort is performed
- [ ] `PipelineConfig` gains `bool enable_replace = true`; CLI gains `--no-replace` to disable it;
      the phase is listed in "Phases enabled" as `replace`
- [ ] `run_full_pipeline` phase order becomes `adder → sandwich → shuffle → local replace → inject`
      (R1 phase name `replace`, running after shuffle and before inject)
- [ ] `PhaseStats` gains `int replacements = 0` (default; other phases leave it 0); R1 fills the
      actual hit count; `print_phase` shows `hits=N` when `replacements > 0`
- [ ] `test_local_replace.cpp` (new):
  - [ ] On a bare `make_nbit_adder(2,0)`, replacements == 2; on `make_nbit_adder(4,0)`,
        replacements == 4
  - [ ] After R1 on bare n=2 and n=4: `check_truth_table` PASS and `check_ancilla_invariant` PASS
  - [ ] Gate-type distinguishability (robust form): `toffoli_count_after == toffoli_count_before
        + 2 * replacements` for both sizes (not an absolute 21→29)
  - [ ] `enable_replace=false` leaves the circuit identical to running without the replace phase
  - [ ] A minimal hand-built circuit containing one S-triple proves the identified window shape,
        before/after interface behavior identical
- [ ] Full pipeline with `--no-replace` yields `replacements == 0` and the same gate sequence as a
      run with the replace phase skipped
- [ ] Existing test suites (gates, circuit, sandwich, shuffle, inject_junk, pipeline, adder) stay
      green — the new phase must not change sandwich/shuffle/inject behavior when enabled

---

*Source: grilling session decisions Q1–Q6; ADR 0004.*