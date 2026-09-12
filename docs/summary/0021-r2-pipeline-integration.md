# 0021 — R2 pipeline integration + CLI + hit display

**Status:** implemented, all tests green (ctest 8/8)
**Commits:** 0 unpushed preliminary (uncommitted working tree; builds on commit `81643eb`)
**Spec:** `docs/archive/tickets/0021-r2-pipeline-integration.md`; canonical: `docs/adr/0005-r2-carry-replacement-and-metrics.md`

## What was built

- `PipelineConfig::enable_replace_r2` (default `false`), wired from `pipeline.cpp` into
  `LocalReplaceConfig::enable_r2`; `PhaseStats` gains `replacements_r1` / `replacements_r2`
  (per ADR 0005 Decision 5); total `replacements = r1 + r2`.
- CLI `--replace-r2` switch; `print_phase` shows `hits=N` (R2 off) or `hits=N+M` (R2 on);
  "Phases enabled" lists `--replace-r2` when active. `--no-replace` disables the whole phase.
- R2 Pass 2 z-wire selection rewritten to **index-based** (ADR 0005 Decision 3): first
  non-output wire with index ≥ 4n+1, where `n = (num_inputs−1)/2`. This was the integration
  bug found in 0021: the previous "not targeted by any gate" heuristic picked no z after the
  Safe sandwich (its CNOTs target extra wires), making pipeline r2 == 0. The rewrite is robust
  across sandwich modes.

## Hit-count reality (measured, deterministic)

- Replace phase always runs R1 first; R2 is the second pass, so ADR 0005's R2-standalone count
  (2n−1) does **not** hold in production. The bare adder (gold standard) gives `r2 == n`
  deterministically:
  - n=2, extra=1: r1=2, r2=2, total=4
  - n=4, extra=1: r1=4, r2=4, total=8
- With a Safe sandwich (seed 42): n=2 → r2=3, n=4 → r2=6 (seed/config-dependent) → full-pipeline
  tests use the policy floor (r1 == n exact, r2 > 0, TT + ancilla PASS at every boundary).
- `apply_sandwich` requires ≥ 2 extra ancilla wires → "extra_ancilla=1 Safe sandwich" is impossible.

## Verification

- `tests/test_pipeline.cpp`: 6 new R2 tests (default off, bare n=2/n=4, extra=0 degenerate,
  R2-off ≡ R1-only, no-replace) — passed (216 total pipeline tests).
- `tests/test_local_replace.cpp`: 2 extra assertions tightened to exact `r2 == n` — passed
  (81 total). Full `ctest`: 8/8.

## Review findings fixed

- Spec: hits format made literal `hits=N+M`; Phases-enabled lists literal `--replace-r2`.
- Spec: ticket 0021 Done gates corrected (were R2-standalone counts; now bare-adder gold
  standard with an explicit correction note).
- Standards: `n_bits` → `n`. Dup-lookup test helper + replacement-stats data clump noted as
  non-blocking judgement calls.

## Notes for next tickets

- **0023 (tests+docs)**: its Done gates still cite stale ADR R1+R2 totals (`hits=5/11`, r2=3/7);
  must be rewritten to the measured bare-adder gold standard (r1=2,r2=2,total=4 / r1=4,r2=4,total=8).
- **0022 (per-phase structural metrics)**: ADR 0005 Decision 4 (`depth_levels`, count_not/cnot/
  toffoli at every boundary) is that ticket's scope, not 0021's.

## Post-hoc note: Active sandwich gap (ticket 0024)

While validating R2, the default (`Active`) sandwich was found to violate the truth table and the
ancilla invariant at the sandwich boundary — a pre-existing defect (reproduced at committed HEAD
`2820b25`, before any R2 work). Root cause: ADR 0002's `F⁻¹ ∘ Adder ∘ F ≡ Adder` is false for CNOT
chains touching computation wires (conjugation does not preserve a non-linear circuit); earlier tests
only checked TT under Safe mode. Fixed in ticket 0024: default `sandwich_mode` is now **Safe**
(equivalence-guaranteed), Active is demoted to experimental with pinned KNOWN-BROKEN regression
tests, ADR 0002 amended. All R2 correctness assertions already ran under Safe, so R2 logic is
unaffected.