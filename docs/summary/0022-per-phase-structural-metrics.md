# Execution Summary: 0022-per-phase-structural-metrics

- **Date**: 2026-09-11
- **Spec/ADR Path**: `docs/adr/0005-r2-carry-replacement-and-metrics.md`
- **Ticket Path**: `docs/ticket/0022-per-phase-structural-metrics.md`
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 0 new (uncommitted working tree on top of `2280ae3`)
- **Files Changed**: 13 tracked files + 1 untracked ticket (`+345` / `-94`)

## 📝 Key Changes
- **PhaseStats structural metrics (ticket 0022)**: added `depth_levels`, `count_cnot`,
  `count_toffoli`, `count_not` (appended after `replacements_r2` so existing positional
  initializers stay valid). Filled at every phase boundary via the `measure_structural` +
  `make_phase_stats` helpers in `src/pipeline.cpp` (same timing as `check_ancilla_invariant`).
- **CLI (0022)**: `print_phase` always shows `depth=D toffoli=T cnot=C`; new `--verbose-metrics`
  flag (default off) prints the final-circuit `gates_per_depth` histogram after the phase table.
- **TDD on preset seams**: 3 tests written first (`test_pipeline_metrics_adder_phase`,
  `test_pipeline_metrics_partition_at_every_boundary`, `test_pipeline_metrics_after_r1_r2`) with
  measured ground truth — the ticket's original Done-gate numbers were stale ADR R2-standalone
  values (max depth 5→8; n=4 cnot 38→20); corrected in the ticket with an explicit correction note.
- **0024 restoration (discovered during build verification)**: the working tree had silently lost the
  entire 0024 change set (the file was being edited on a stashed/committed state). Restored the
  Safe default, pin tests, CLI warning, ADR 0002 amend, and all doc updates
  (README/CONTEXT/CHANGELOG/ticket 0012/summary 0021). Root-cause pattern documented in the 0021 summary.

## 🧪 Verification
- **Typecheck**: 🟢 Pass (clean compile)
- **Tests**: 🟢 Pass — pipeline suite **260 tests**, local_replace 81, `ctest` 8/8.
- **CLI**: default (Safe) run shows `Ancilla invariant: OK` + `Truth table: PASS` + `Sandwich mode: safe`;
  `--sandwich-mode=active` prints the 0024 warning and FAILs TT (as pinned); `--bits=2 --extra-ancilla=1
  --no-sandwich --replace-r2` prints `hits=2+2 depth=11 toffoli=13 cnot=10`; `--verbose-metrics` prints the histogram.

## 🔍 Code Review Findings
- **Standards**: 0 hard violations; judgement-call smells — Duplicated Code (`PhaseMetrics` mirrors
  `PhaseStats`), Data Clumps (`make_phase_stats` positional params), Shotgun Surgery (Active-mode
  correction across 9 files, justified), Speculative Generality (`count_not` reserved), Mysterious
  Name (`measure_structural`).
- **Spec**: fully implemented per corrected Done gates. One finding fixed: README example output was
  hand-edited and violated the `toffoli+cnot == gate_count` partition invariant → replaced with the
  actual measured 4-bit Safe output (replace gates=57 depth=16 toffoli=29 cnot=28; inject gates=69 toffoli=41).

## 📦 Recent Commits
- `2280ae3` feat(pipeline): integrate R2 carry-majority replacement into pipeline with CLI support and tests
- `2820b25` feat(local-replace): implement R2 carry-majority replacement and update related structures and tests
- `81643eb` feat(adr): add ADR 0005 for R2 carry-majority replacement and structural metrics

---

**Next tickets:** `docs/ticket/0023-r2-tests-and-docs.md` — Done gates still cite stale ADR R1+R2
totals (`hits=5/11`, r2=3/7); rewrite to the measured bare-adder gold standard (r1=2,r2=2,total=4 /
r1=4,r2=4,total=8). The deferred "Conjugation-sound Active sandwich" redesign and ticket 0012 remain
blocked by 0024.