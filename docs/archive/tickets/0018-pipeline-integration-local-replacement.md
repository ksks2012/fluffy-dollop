# 0018 — Pipeline integration for local replacement (R1)

**What to build:** Wire the standalone `apply_local_replace` from 0017 into the pipeline as a new
phase `replace`, running **after `sandwich` and before `shuffle`**, following the same phase shape as
sandwich/shuffle/inject: a `PipelineConfig::enable_replace` toggle (default true), a `PhaseStats`
entry carrying the replacement count, per-phase ancilla verification, and the phase listed as
`replace` in the pipeline's phase enumeration/output. No CLI changes in this ticket (0019).

**Phase order note:** the grill decision Q4 originally placed `replace` after `shuffle`. During
implementation, measurement showed shuffle destroys the strict-adjacency windows (hits drop 2/4 → 0,
because the three S-CNOTs of a window sit at different depths and shuffle interleaves unrelated
same-depth gates between them), while sandwitch preserves them (still 2/4). Per Q4's own
"若實測匹配率過低，再單開評估" precondition the order was amended to `sandwich → replace → shuffle`
(ADR 0004, marked as an amendment, not a silent change). With this order, full-pipeline hits are
deterministically `== n` for both sizes and all modes/seeds tested.

**Blocked by:** 0017 (the core `apply_local_replace` algorithm and its algorithm-level verification).

**Status:** done (merged into pipeline implementation; tests green)

---

- [x] `PipelineConfig` gains `bool enable_replace = true;` (no `replace_max` — deferred to a
      second version per ADR 0004/Q6)
- [x] `run_full_pipeline` phase order becomes `adder → sandwich → local replace → shuffle → inject`;
      the R1 phase is named `replace` and runs the `apply_local_replace` from 0017
- [x] The `replace` phase participates in the per-phase ancilla invariant verification like the
      other phases (ancilla invariant must hold entering and exiting the phase)
- [x] `PhaseStats` gains `int replacements = 0;` (default; other phases leave it 0); the `replace`
      phase fills it with the actual hit count from `apply_local_replace`
- [x] `print_phase` / the phase summary shows `hits=N` when `replacements > 0` (N = the hit count)
- [x] When `enable_replace == false`, the pipeline's gate sequence is identical to running without
      the replace phase (skip entirely)

**Verification (this ticket's Done gate — proves the phase integrates without breaking the
pipeline):**
- [x] Full pipeline on bare n=2 and n=4 (with Active sandwich default): `check_truth_table` PASS
      and `check_ancilla_invariant` PASS at every phase boundary including the new `replace` phase
- [x] The `replace` phase hit count in a full run is `== n` (n=2 → 2, n=4 → 4) because replace runs
      before shuffle while the S-triple windows are still intact (deterministic across Safe/Active
      and all tested seeds)
- [x] `enable_replace=false` produces exactly the same gate sequence as a run with the replace phase
      skipped entirely (no replace-phase side effects)
- [x] Same-seed comparison: with a fixed seed and `enable_replace=false` versus `true`, every phase
      *before* `replace` (adder, sandwich) has identical gate sequences and counts; the only
      difference is gates added by the `replace` phase onward. (Do not compare against a preset
      "0014-era baseline" — Active sandwich is seed-dependent.)

---

*Source: grilling session Q1–Q6; ADR 0004; ticket 0016 (superseded).*