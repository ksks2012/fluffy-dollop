# 0019 — CLI, tests & regression, docs reconciliation for R1

**What to build:** Surface the `replace` phase on the CLI (`--no-replace`), complete the full test
suite for local replacement (`test_local_replace.cpp`) including regression, and reconcile
`CONTEXT.md` so the narrowed generation-mixing non-goal reflects the MVP R1 scope.

**Phase order note:** per ADR 0004, the phase order is
`adder → sandwich → local replace → shuffle → inject` (amended from Q4's original
`shuffle → replace` after measurement showed shuffle destroys the strict-adjacency windows).

**Blocked by:** 0017 (core algorithm), 0018 (pipeline integration).

**Status:** done (CLI + tests + docs reconciled; full suite green)

---

**CLI:**
- [x] CLI gains `--no-replace` to disable the replace phase (maps to
      `PipelineConfig::enable_replace = false`); prints the `replace` phase in the "Phases enabled"
      list when enabled
- [x] Full pipeline with `--no-replace`: `replacements == 0` and the gate sequence is identical to
      a run with the replace phase skipped
- [x] CLI/observability output during normal runs shows `hits=N` for the replace phase (via 0018's
      `PhaseStats::replacements`), demonstrating the MVP success criterion is observable

**Tests (`test_local_replace.cpp`, new):**
- [x] A minimal hand-built circuit containing one S-triple documents the exact window shape and
      proves before/after interface behavior identical (hit count, gate counts, net effect on the
      touched wires)
- [x] Note on overlap with 0017: exact `replacements == n` (n=2→2, n=4→4) and the truth-table /
      invariant / Toffoli-delta assertions on *bare* adders are the **0017 golden tests**; 0019
      re-asserts at most a lightweight spot-check on the bare adder (acceptable double insurance)
- [x] The 0019 emphasis is on *integration* coverage not duplicated in 0017: the hand-built single
      S-triple unit test, `enable_replace=false` / `--no-replace` equivalence, and full-suite
      regression
- [x] `enable_replace=false` / `--no-replace` leaves the circuit identical to running without the
      replace phase
- [x] Regression: all existing test suites (gates, circuit, sandwich, shuffle, inject_junk,
      pipeline, adder) stay green — the new phase must not change sandwich/shuffle/inject behavior
      (8/8 suites pass)

**Docs reconciliation (per ADR 0004 Scope Reconciliation):**
- [x] `CONTEXT.md` §1 non-goal "Full generation mixing (replacement of local sub-circuits from
      large equivalence tables)" refined to state that automated/large-table generation mixing
      remains out of scope, while the single fixed hand-written provably-equivalent local rewrite
      (R1) is inside the MVP
- [x] `CONTEXT.md` §2 pipeline phases updated to list the new `replace` phase in the ordering
      `adder → sandwich → local replace → shuffle → inject`
- [x] `CHANGELOG.md` milestone entry notes the local-replacement validation token (if a milestone
      entry is appropriate)

---

*Source: grilling session Q1–Q6; ADR 0004; ticket 0016 (superseded).*