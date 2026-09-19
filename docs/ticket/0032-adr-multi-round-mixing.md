# 0032 — ADR 0008 (Multi-round mixing) + rounds loop seams spec (M7 open)

**What to build:** Ratify ADR 0008 (multi-round mixing — `PipelineConfig::rounds`, per-round
sandwich → replace → shuffle with inject final-round-only, per-round test-side verification) and pin
the loop / config-surface / PhaseStats seams as a specifiable contract before any implementation
lands. Milestone M7 open.

**Blocked by:** (none) — project history at HEAD `f90514c` (M6 close, 0031 merged)

**Status:** spec written (2026-09-19); implementation lands in 0033

- [x] ADR `docs/adr/0008-multi-round-mixing.md` written per grilling Q1–Q8 (ADR 0007 orthogonality +
      M6 ticketing pattern):
  - [x] `PipelineConfig::rounds` = `int rounds = 1` (public config; NOT key-derived)
  - [x] Per-round schedule (unchanged per-round phase chain): `sandwich → replace
        (Sound→R1→R2) → shuffle`, repeated R times; `inject` runs **only** on the final round
        boundary (`round_index == rounds-1`) — junk volume is additive, not multiplicative
  - [x] `rounds == 1` ⇒ bit-exact identical to today's single-pass output (regression anchor;
        inject position preserved)
  - [x] `--rounds=N` CLI (N ≥ 1; N < 1 / unparseable ⇒ error + non-zero exit, never silent clamp)
  - [x] Orthogonal to `--key` / `--seed` (ADR 0007): rounds change the mixer, key only seeds the
        one shared rng; single `mt19937` carried across all rounds (no per-round rebuild)
  - [x] **Verification contract (Q3/Q5):** truth-table + zero-ancilla invariant are checked **at
        every round boundary**, but the check lives **test-side** (`tests/test_pipeline.cpp`
        consumer), NOT inside `run_full_pipeline` — the pipeline stays a pure transformation
        (M4–M6 precedent). CLI prints final TT PASS once (unchanged shape)
  - [x] `PhaseStats::round_index` (`int`, default 0) — added field for per-round attribution;
        `PhaseStats::name` unchanged (no "r1/sandwich" string mangling — names stay stable)
  - [x] Phase count formula (contract, avoids off-by-one on tests):
        `phases.size() == 1 + rounds*3 + 1`  // adder + R×(sandwich,replace,shuffle) + inject
        Example rounds=2 → 8 phases; round_index sequence {0,0,0,0, 1,1,1,1}
        (adder+round0's three = index 0; round1's three + inject = index 1)
  - [x] Out-of-scope (written in ADR): key-derived rounds / per-round schedules (M7.1+);
        per-round pass enablement; anything beyond engineering replay sensitivity
- [x] Seams pinned (for 0033):
      `PipelineConfig::rounds` decl in `include/pipeline.hpp`; rounds loop location in
      `run_full_pipeline` (`src/pipeline.cpp`, wrapping existing sandwich/replace/shuffle calls,
      inject gated on `round_index == rounds-1`); CLI `--rounds=N` parse in `src/main.cpp`
      (alongside `--key=`, ADR 0007 parse site); `PhaseStats::round_index`; test anchor
      `tests/test_pipeline.cpp`
- [x] `Phasestats` / `PipelineConfig` seam contract written so 0033 can implement without
      re-interpreting (rounds field + round_index field + loop structure + CLI semantics above)

> Acceptance of this ticket = ADR 0008 merged + seams listed. Implementation lands in 0033; this
> ticket intentionally carries no behavior yet (M6 ADR-first pattern, mirrors 0029).
