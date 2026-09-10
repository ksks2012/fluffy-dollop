# 0023 — R2 tests, CONTEXT reconciliation, CHANGELOG

**What to build:** Complete the test coverage for R2 and the metrics extension, reconcile
`CONTEXT.md` and `CHANGELOG.md`, and ensure ADR 0005 is properly referenced. This is the final
documentation bookkeeping ticket for Milestone 4.

**Blocked by:** 0020 (R2 core), 0021 (R2 pipeline), 0022 (metrics).

**Status:** ready-for-agent

---

**Tests (`tests/test_local_replace.cpp` and/or `tests/test_pipeline.cpp`):**

- [ ] Hand-built single R2 3-Toffoli window: match → replacement → hits=1; 3→5 gates;
      `z` restored to 0; functionally equivalent to original 3-Toffoli on all 8 inputs
- [ ] Bare n=2, extra_ancilla=1: R2-only hits=3; R1+R2 hits=5; TT PASS, ancilla PASS,
      `num_wires()` unchanged
- [ ] Bare n=4, extra_ancilla=1: R2-only hits=7; R1+R2 hits=11; TT PASS, ancilla PASS,
      `num_wires()` unchanged
- [ ] **Degenerate case:** bare n=4, extra_ancilla=0 + R2 on: `replacements == 0`; no crash;
      `num_wires()` unchanged; TT PASS, ancilla PASS
- [ ] `enable_replace_r2 = false` (default): same output as R1-only; no regressions
- [ ] Full pipeline (n=2, Safe, seed=42): TT PASS, ancilla PASS at every boundary;
      `replacements >= 5` (sanity floor); `replacements_r1 == 2`, `replacements_r2 == 3`
      (exact counts on Safe sandwich for bare adder are reproducible — verify once, then use floor
      if sandwich mode changes)
- [ ] Full pipeline (n=4, Safe, seed=42): TT PASS, ancilla PASS;
      `replacements_r1 == 4`, `replacements_r2 == 7`
- [ ] Metrics: after adder phase n=2 bare, `depth_levels == 5`, `count_toffoli == 9`,
      `count_cnot == 6`
- [ ] Regression: all 0017–0022 test suites still green (full `ctest` pass)

**Docs reconciliation:**

- [ ] `CONTEXT.md` §1 non-goal: note R2 extends the local-replacement sliver (still MVP scope,
      not automated/large-table generation)
- [ ] `CONTEXT.md` §2 Phase 3: add sentence describing R2 as a second pass within the replace
      phase (carry-majority 3-Toffoli, borrow 1 zero-ancilla, requires `extra_ancilla >= 1`)
- [ ] `CONTEXT.md` §3 decision table: add row for R2 borrow contract (B, 0-ancilla → 1-borrow;
      z from extra pool `≥ 4n+1`)
- [ ] `CHANGELOG.md`: add Milestone 4 entry (R2 carry-majority replacement, structural metrics,
      per-pattern hit tracking)
- [ ] ADR 0005 reference confirmed: `docs/adr/0005-r2-carry-replacement-and-metrics.md` exists
      and is the canonical spec

---

*Source: grilling session Q1–Q6; ADR 0005; tickets 0020–0022.*
