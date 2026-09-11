# 0022 — Per-phase structural metrics (PhaseStats fields + CLI suffix)

**What to build:** Add structural metrics fields to `PhaseStats`, computed at every phase boundary
(at the same timing as `check_ancilla_invariant`), and display them compactly in the CLI phase table.
This ticket handles depth/cnot/toffoli suffix only; hit display (`hits=N` / `hits=N+M`) is owned by
ticket 0021.

**Blocked by:** 0018 (pipeline phase structure).

**Status:** implemented

---

**Checklist:**

- [x] `PhaseStats` gains:
  - `int depth_levels = 0`  — topological depth level count (= max depth from `compute_depth_levels`)
  - `int count_cnot = 0`    — number of `CnotGate` in the circuit
  - `int count_toffoli = 0` — number of `ToffoliGate`
  - `int count_not = 0`     — number of `NotGate` (always 0 in current library; reserved)
  - `int replacements_r1 = 0` — R1 hit count (filled only by replace phase)
  - `int replacements_r2 = 0` — R2 hit count (filled only by replace phase)
- [x] `depth_levels`, `count_cnot`, `count_toffoli`, `count_not` filled at every phase boundary
      (including non-replace phases); `replacements_r1/r2` filled only at the replace phase boundary.
      Implemented via `make_phase_stats` helper in `src/pipeline.cpp` (calls `measure_structural` at
      the same timing as `check_ancilla_invariant`)
- [x] CLI phase table gains compact suffix after existing columns (this ticket):
      `depth=D toffoli=T cnot=C` (always shown, not gated behind a flag)
- [x] `--verbose-metrics` flag (optional, default off): prints the full `gates_per_depth` vector
      after the phase table; without the flag, only the aggregate suffix is shown
- [x] No `hits=...` display in this ticket — that is ticket 0021's responsibility

**Verification (Done gate):**

- [x] After adder phase on n=2 (bare, no replace yet): `depth_levels == 8`, `count_toffoli == 9`,
      `count_cnot == 6`, `count_not == 0`
- [x] After R1+R2 on bare n=4, extra_ancilla=1: `count_toffoli == 29` (+2n=+8 from R1),
      `count_cnot == 20` (+2n=+8 from R2)
- [x] After replace phase, `replacements_r1 == n`, `replacements_r2 == n` (bare adder,
      extra_ancilla >= 1); total `replacements == 2n`
- [x] All existing test suites still green (pipeline + local_replace + ctest 8/8)

> **Correction note (2026-09-11):** the original Done-gate numbers were copied from the ADR 0005
> R2-standalone table and do not survive the R1-first pipeline. Measured ground truth in the
> pipeline (R1 runs before R2, z = first extra wire ≥ 4n+1): max depth is **8** (n=2) / **16** (n=4),
> _not_ 5; R2 deltas are **+2n CNOT** (r2 == n, _not_ 2n−1) ⇒ n=4 count_cnot == 20, _not_ 38;
> cumulative `replacements == 2n`, _not_ 3n−1. Same correction pattern as ticket 0021.

---

*Source: grilling session Q4; ADR 0005 Implementation Decision 4.*
