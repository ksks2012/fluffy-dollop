# 0018 — Pipeline integration for local replacement (R1)

**What to build:** Wire the standalone `apply_local_replace` from 0017 into the pipeline as a new
phase `replace`, running **after `shuffle` and before `inject`**, following the same phase shape as
sandwich/shuffle/inject: a `PipelineConfig::enable_replace` toggle (default true), a `PhaseStats`
entry carrying the replacement count, per-phase ancilla verification, and the phase listed as
`replace` in the pipeline's phase enumeration/output. No CLI changes in this ticket (0019).

**Blocked by:** 0017 (the core `apply_local_replace` algorithm and its algorithm-level verification).

**Status:** ready-for-agent

---

- [ ] `PipelineConfig` gains `bool enable_replace = true;` (no `replace_max` — deferred to a
      second version per ADR 0004/Q6)
- [ ] `run_full_pipeline` phase order becomes `adder → sandwich → shuffle → local replace → inject`;
      the R1 phase is named `replace` and runs the `apply_local_replace` from 0017
- [ ] The `replace` phase participates in the per-phase ancilla invariant verification like the
      other phases (ancilla invariant must hold entering and exiting the phase)
- [ ] `PhaseStats` gains `int replacements = 0;` (default; other phases leave it 0); the `replace`
      phase fills it with the actual hit count from `apply_local_replace`
- [ ] `print_phase` / the phase summary shows `hits=N` when `replacements > 0` (N = the hit count)
- [ ] When `enable_replace == false`, the pipeline's gate sequence is identical to running without
      the replace phase (skip entirely)

**Verification (this ticket's Done gate — proves the phase integrates without breaking the
pipeline):**
- [ ] Full pipeline on bare n=2 and n=4 (with Active sandwich default): `check_truth_table` PASS
      and `check_ancilla_invariant` PASS at every phase boundary including the new `replace` phase
- [ ] The `replace` phase hit count in a full run is **at least 1** (`replacements >= 1`); note the
      exact-hit `== n` guarantee holds only on the *bare* adder (0017). After shuffle + Active
      sandwich, S-triple adjacency is generally preserved but not guaranteed in the future, so the
      full-pipeline assertion must not be pinned to exactly n
- [ ] `enable_replace=false` produces exactly the same gate sequence as a run with the replace phase
      skipped entirely (no replace-phase side effects)
- [ ] Same-seed comparison: with a fixed seed and `enable_replace=false` versus `true`, every phase
      *before* `replace` (adder, sandwich, shuffle) has identical gate sequences and counts; the
      only difference is gates added by the `replace` phase onward. (Do not compare against a preset
      "0014-era baseline" — Active sandwich is seed-dependent.)

---

*Source: grilling session Q1–Q6; ADR 0004; ticket 0016 (superseded).*