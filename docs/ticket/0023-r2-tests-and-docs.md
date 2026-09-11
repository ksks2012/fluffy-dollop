# 0023 — R2 tests, CONTEXT reconciliation, CHANGELOG

**What to build:** Complete the test coverage for R2 and the metrics extension, reconcile
`CONTEXT.md` and `CHANGELOG.md`, and ensure ADR 0005 is properly referenced. This is the final
documentation bookkeeping ticket for Milestone 4.

**Blocked by:** 0020 (R2 core), 0021 (R2 pipeline), 0022 (metrics).

**Status:** implemented

---

**Tests (`tests/test_local_replace.cpp` and/or `tests/test_pipeline.cpp`):**

- [x] Hand-built single R2 3-Toffoli window: match → replacement → hits=1; 3→5 gates;
      `z` restored to 0; functionally equivalent to original 3-Toffoli on all 8 inputs
      (`test_r2_hand_built_window`)
- [x] Bare n=2, extra_ancilla=1: R1+R2 → r1==2, r2==2, total==4; TT PASS, ancilla PASS,
      `num_wires()` unchanged (`test_r2_bare_adder_hit_counts`)
- [x] Bare n=4, extra_ancilla=1: R1+R2 → r1==4, r2==4, total==8; TT PASS, ancilla PASS,
      `num_wires()` unchanged (`test_r2_bare_adder_hit_counts`); R2 gate deltas
      (`+2*r1` Toffoli, `+2*r2` CNOT) in `test_r2_combined_gate_distinguishability`
- [x] **Degenerate case:** bare n=4, extra_ancilla=0 + R2 on: `replacements_r2 == 0`; no crash;
      `num_wires()` unchanged; TT PASS, ancilla PASS (`test_pipeline_r2_no_extra_ancilla`)
- [x] `enable_replace_r2 = false` (default): same output as R1-only; no regressions
      (`test_pipeline_r2_off_same_as_r1_only`)
- [x] Full pipeline (n=2, Safe, seed=42): TT PASS, ancilla PASS at every boundary;
      `replacements_r1 == 2`, `replacements_r2 == 3`, total == 5 — measured; use floor
      (`r2 > 0`, `total >= 5` sanity) if sandwich mode changes (`test_pipeline_r2_safe_exact_hit_counts`,
      `test_pipeline_r2_on_bare_4bit`)
- [x] Full pipeline (n=4, Safe, seed=42): TT PASS, ancilla PASS;
      `replacements_r1 == 4`, `replacements_r2 == 6`, total == 10 (`test_pipeline_r2_safe_exact_hit_counts`)
- [x] Metrics: after adder phase n=2 bare, `depth_levels == 8`, `count_toffoli == 9`,
      `count_cnot == 6` (`test_pipeline_metrics_adder_phase`)
- [x] Regression: all 0017–0022 test suites still green (full `ctest` pass)

> **Correction note (2026-09-11):** the original Done-gate numbers above were copied from the ADR 0005
> R2-standalone table and do not survive the R1-first pipeline (same pattern as tickets 0021 / 0022):
> bare n=2 R2-only is 2 (_not_ 3) ⇒ R1+R2 total 4 (_not_ 5); bare n=4 R2-only is 4 (_not_ 7) ⇒ total 8
> (_not_ 11). Full-pipeline Safe-sandwich (seed 42) measured counts differ from bare: n=4 `r2 == 6`
> (_not_ 7), total 10. Metrics `depth_levels` is **8** (_not_ 5) — corrects the stale 0022-era number.

**Docs reconciliation:**

- [x] `CONTEXT.md` §1 non-goal: R2 extends the local-replacement sliver (still MVP scope,
      not automated/large-table generation)
- [x] `CONTEXT.md` §2 Phase 3: R2 as a second pass within the replace phase
      (carry-majority 3-Toffoli, borrow 1 zero-ancilla, requires `extra_ancilla >= 1`)
- [x] `CONTEXT.md` §3 decision table: row 13 for R2 borrow contract (B, z from extra pool `≥ 4n+1`)
- [x] `CHANGELOG.md`: Milestone 4 entry (R2 carry-majority replacement, structural metrics,
      per-pattern hit tracking) — present from ticket 0022/0024 work
- [x] ADR 0005 reference confirmed: `docs/adr/0005-r2-carry-replacement-and-metrics.md` exists
      and is the canonical spec (referenced from CONTEXT.md and CHANGELOG.md)

---

*Source: grilling session Q1–Q6; ADR 0005; tickets 0020–0022.*
