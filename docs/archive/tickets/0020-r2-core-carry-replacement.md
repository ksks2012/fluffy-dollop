# 0020 — R2 core algorithm (carry-majority replacement)

**What to build:** Extend `apply_local_replace` with a second pass (Pass 2) that matches the R2
carry-majority 3-Toffoli window and rewrites each match using one borrowed zero-ancilla wire,
restoring `z = 0` before the window ends. Same single-pass, non-overlapping, in-place, no-new-wires
contract as R1.

**Blocked by:** 0017 (R1 core — this ticket extends the same function).

**Status:** implemented (see docs/summary/0020-r2-core-carry-replacement.md; note: R1-first yields r2==n, not ADR 2n−1)

---

**Locked R2 spec (from ADR 0005):**

- Window: three strictly adjacent `Toffoli` gates, same target `T`, controls are 3 distinct wires
  `a, b, ci`.
- Replace: `CNOT(a,z); Toffoli(b,z,T); Toffoli(ci,z,T); Toffoli(b,ci,T); CNOT(a,z)`.
- `z`: first extra-ancilla wire (index `≥ 4n+1`), selected once at pass start, shared across all
  windows; fixed (no `rng` consumed); re-zeroed within each window.
- Non-overlapping scan: resume after the 5-gate block.
- **Prerequisite:** circuit must have `extra_ancilla >= 1`. When `extra_ancilla = 0`, R2 correctly
  reports zero hits (valid degenerate case, not an error).

**Checklist:**

- [ ] `LocalReplaceConfig` gains `bool enable_r2 = false` (default off)
- [ ] Pass 2 added to `apply_local_replace`: scans for 3-Toffoli same-target triples with 3
      distinct controls; replaces each with the canonical 5-gate R2 sequence
- [ ] `z` selection: at pass start, first extra-ancilla wire (`index >= 4n+1`); if none exists,
      R2 reports zero hits and returns immediately (no error, no `extend_wires`)
- [ ] `z` is shared across all R2 windows in the same pass (serial replacement; each window
      writes and restores `z`)
- [ ] Pass 2 runs after Pass 1 (R1), each single-pass, non-overlapping
- [ ] Returns hit counts for R1 and R2 separately: out-param or struct `{ int r1_hits; int r2_hits; }`
      (so callers can populate `PhaseStats::replacements_r1/r2` — per ADR 0005 Decision 5)

**Verification (Done gate — proves R2 correctness on the real adder):**

- [ ] `make_nbit_adder(2, 1)` + R2: `replacements == 3` (2n−1); `check_truth_table` PASS;
      `check_ancilla_invariant` PASS; `num_wires()` unchanged
- [ ] `make_nbit_adder(4, 1)` + R2: `replacements == 7` (2n−1); TT PASS; ancilla PASS;
      `num_wires()` unchanged
- [ ] `make_nbit_adder(4, 1)` + R1+R2: `replacements == 11` (4+7); TT PASS; ancilla PASS
- [ ] `make_nbit_adder(4, 0)` + R2: `replacements == 0` (no extra wire); no crash, no
      `extend_wires` called; `num_wires()` unchanged
- [ ] Gate-type distinguishability: R2 on n=4 gives Toffoli +0, CNOT +14 (per ADR 0005 table)

**Notes:**

- The R2 canonical is already verified against `make_nbit_adder(2,1)` and `(4,1)` by one-off
  program (ADR 0005 Core Verification). Regression authority is this ticket's test suite.
- R2 matches both forward carry chain and reverse uncompute windows (same structural shape;
  ADR 0005 window scope).
- The `extra_ancilla=0` degenerate case is a required Done gate: it proves R2 is safe on circuits
  that lack a borrowable wire.

---

*Source: grilling session Q1–Q6; ADR 0005; ticket 0016 (superseded, R1 spec).*
