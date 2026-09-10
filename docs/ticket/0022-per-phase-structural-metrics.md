# 0022 — Per-phase structural metrics (PhaseStats fields + CLI suffix)

**What to build:** Add structural metrics fields to `PhaseStats`, computed at every phase boundary
(at the same timing as `check_ancilla_invariant`), and display them compactly in the CLI phase table.
This ticket handles depth/cnot/toffoli suffix only; hit display (`hits=N` / `hits=N+M`) is owned by
ticket 0021.

**Blocked by:** 0018 (pipeline phase structure).

**Status:** ready-for-agent

---

**Checklist:**

- [ ] `PhaseStats` gains:
  - `int depth_levels = 0`  — topological depth level count (= max depth from `compute_depth_levels`)
  - `int count_cnot = 0`    — number of `CnotGate` in the circuit
  - `int count_toffoli = 0` — number of `ToffoliGate`
  - `int count_not = 0`     — number of `NotGate` (always 0 in current library; reserved)
  - `int replacements_r1 = 0` — R1 hit count (filled only by replace phase)
  - `int replacements_r2 = 0` — R2 hit count (filled only by replace phase)
- [ ] `depth_levels`, `count_cnot`, `count_toffoli`, `count_not` filled at every phase boundary
      (including non-replace phases); `replacements_r1/r2` filled only at the replace phase boundary
- [ ] CLI phase table gains compact suffix after existing columns (this ticket):
      `depth=D toffoli=T cnot=C` (always shown, not gated behind a flag)
- [ ] `--verbose-metrics` flag (optional, default off): prints the full `gates_per_depth` vector
      after the phase table; without the flag, only the aggregate suffix is shown
- [ ] No `hits=...` display in this ticket — that is ticket 0021's responsibility

**Verification (Done gate):**

- [ ] After adder phase on n=2 (bare, no replace yet): `depth_levels == 5`, `count_toffoli == 9`,
      `count_cnot == 6`, `count_not == 0`
- [ ] After R1+R2 on bare n=4, extra_ancilla=1: `count_toffoli == 29` (+8 from R1),
      `count_cnot == 38` (+26 from R2) — per ADR 0005 combined-delta table
- [ ] After replace phase, `replacements_r1 == n`, `replacements_r2 == 2n−1` (bare adder,
      extra_ancilla >= 1); total `replacements == 3n−1`
- [ ] All existing test suites still green

---

*Source: grilling session Q4; ADR 0005 Implementation Decision 4.*
