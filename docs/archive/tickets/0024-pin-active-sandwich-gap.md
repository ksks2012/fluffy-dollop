# 0024 — Pin Active sandwich functional-equivalence gap

**What to build:** The Active sandwich mode is mathematically unsound: ADR 0002's claim
`F⁻¹ ∘ Adder ∘ F ≡ Adder` is false for any CNOT chain `F` that touches the adder's computation
lines. Conjugating the adder by a general CNOT chain does not preserve a non-linear circuit —
the adder computes on scrambled `A/B/Cin`, and a linear `F⁻¹` cannot cancel non-linear errors.
Only Safe mode (`F` acts on always-zero wires the adder never reads, i.e. `F ≡ I` on the
computation path) guarantees equivalence. Verified: default run (`Active`) FAILs truth-table and
ancilla invariant at the sandwich boundary on the committed HEAD, before any R2 work.

Deliverables (in order):
1. **Pin the defect with regression tests** so a "fix" that silently greens Active without a
   sound redesign is caught.
2. **Change the default `sandwich_mode` to `Safe`** — the default pipeline must be the
   equivalence-guaranteed path. Active is demoted to experimental.
3. **Amend ADR 0002** — withdraw the `F⁻¹ ∘ Adder ∘ F ≡ Adder` claim; state Safe is the
   equivalence-guaranteed path; note Active awaits a sound redesign (separate ticket).

**Blocked by:** None.

**Status:** implemented (see docs/summary/0024-note in 0021 summary; ADR 0002 correction)

---

**Checklist:**

- [ ] Regression `test_pipeline_active_known_broken_truth_table_pinned`: full pipeline with
      `sandwich_mode = Active` (seed 42) → `check_truth_table` is FALSE and at least one phase
      boundary reports `ancilla_ok == false`. Documented as KNOWN-BROKEN; the assert must flip
      to expect PASS only when Active is redesigned to be conjugation-sound.
- [ ] Regression `test_pipeline_safe_preserves_truth_table_and_ancilla`: full pipeline with
      `sandwich_mode = Safe` → `check_truth_table` PASS and every phase boundary
      `ancilla_ok == true`. Anchors Safe as the equivalence path.
- [ ] `PipelineConfig::sandwich_mode` default changes from `Active` to `Safe`; header comment and
      all tests asserting the old default updated (`test_pipeline_default_mode_is_active` inverts
      to `test_pipeline_default_mode_is_safe`).
- [ ] CLI prints an explicit warning when `--sandwich-mode=active` is requested ("does not
      guarantee truth-table / ancilla equivalence; experimental — see ADR 0002 amend / ticket
      0024"), reminding to use `--sandwich-mode=safe` for guaranteed TT.
- [ ] All remaining full-pipeline truth-table / ancilla assertions stay forced to Safe with a
      comment citing ticket 0024 (no correctness assertion may run under Active).

**Verification (Done gate):**

- [ ] Repro command `./build/gate_driver` (default) shows `Ancilla invariant: OK (all phases)`
      and `Truth table: PASS`, with `Sandwich mode: safe`.
- [ ] `./build/gate_driver --sandwich-mode=active` prints the experimental warning and FAILs TT
      (current known behavior, pinned by test).
- [ ] `ctest` 8/8 passes (pin tests included).
- [ ] ADR 0002 amended (correction banner); README / CONTEXT / sandwich.hpp / CHANGELOG no longer
      present Active as TT-guaranteed; ticket 0012's "preserves truth table" acceptance marked
      blocked by this ticket.

---

**Out of scope:** redesigning Active into a conjugation-sound mode (separate ticket: "Conjugation-
sound Active sandwich", deferred; must NOT be merged into R2 metrics work). R2 / 0021 work is
unaffected — its validation path is already Safe.

*Source: defect investigation 2026-09-11; ADR 0002; README running examples.*