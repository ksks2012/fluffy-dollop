# 0033 — Multi-round loop: `rounds` in config + CLI `--rounds=N` + per-round verify (M7 build)

**What to build:** Implement the M7 loop per ADR 0008 and the seams pinned in 0032:
`PipelineConfig::rounds` (int, default 1), the `rounds` loop inside `run_full_pipeline`
(sandwich → replace → shuffle per round; inject **only** on the final round), `PhaseStats::round_index`
filed for per-round attribution, the CLI `--rounds=N` flag with strict validation, and the per-round
truth-table + ancilla verification **in the test consumer** (pipeline stays a pure transformation).
Milestone M7 main build.

**Blocked by:** 0032 (ADR 0008 + seams)

**Status:** open

## Config / CLI

- [ ] `PipelineConfig::rounds` = `int rounds = 1;` (in `include/pipeline.hpp`, alongside `n` /
      `extra_ancilla` / pass toggles; ADR 0008 §Config surface)
- [ ] CLI `--rounds=N` (`src/main.cpp`):
      - [ ] `N >= 1` ⇒ accepted, stored into `cfg.rounds`
      - [ ] `N < 1` (incl. `--rounds=0`, `--rounds=-2`), unparseable (non-integer), or missing
            value (`--rounds=`) ⇒ error line + non-zero exit — NEVER silent clamp, NEVER default-rewrite
            (mirrors ADR 0007 `--key=` empty-value rule, ticket 0030)
      - [ ] Flag absent ⇒ `rounds = 1` (default) — no change to today's invocations
      - [ ] No precedence interaction with `--key` / `--seed`: rounds is orthogonal (ADR 0008 §Q6;
            ADR 0007 precedence table untouched)

## Pipeline loop

- [ ] `run_full_pipeline` (in `src/pipeline.cpp`, seam from 0032): wrap the existing
      `sandwich → replace → shuffle` chain in a `for (int r = 0; r < cfg.rounds; r++)` loop,
      existing pass toggles / config honored identically in **every** round
- [ ] `inject` executes **only** when `r == rounds - 1` (final round) — junk volume stays additive
      once, not multiplicative per round (ADR 0008 §Solution)
- [ ] `rounds == 1` ⇒ loop runs once, inject fires on round 0, bit-exact identical gate sequence to
      today's single-pass output; base `adder` phase + phase ordering unchanged
- [ ] Single `mt19937` (ADR 0007 §Single shared stream) passed **by reference through all rounds** —
      rng is NEVER rebuilt per round; round count does not touch `make_rng` (0030 seam intact)

## PhaseStats

- [ ] `PhaseStats::round_index` = `int round_index = 0;` (in `include/pipeline.hpp`) — recorded on
      every `PhaseStats` produced by `make_phase_stats`; base `adder` → 0, per-round phases → that
      round's index (`r`), final-round `inject` → `rounds - 1`
- [ ] `PhaseStats::name` **unchanged** (`"adder"`, `"sandwich"`, `"replace"`, `"shuffle"`,
      `"inject"`) — names carry no round suffix; `round_index` is the only round attribution
      (ADR 0008 §Q4/A: no "r1/sandwich" string mangling — keeps existing phase-name assertions
      in tests green)
- [ ] **Phase count contract (for stats / metrics / tests):**
      `phases.size() == 1 + rounds*3 + 1`  (adder + R×(sandwich,replace,shuffle) + inject)
      — e.g. `rounds=2` ⇒ 8 phases (`round_index` {0,0,0,0, 1,1,1,1}); `rounds=1` ⇒ 5 (today)

## Tests (`tests/test_pipeline.cpp`)

- [ ] **Regression:** `rounds=1` run ⇒ gate sequence bit-identical to the existing default single-pass
      run (existing deterministic test unchanged)
- [ ] **Multi-round TT + ancilla per round boundary (test-side consumer, Q3/Q5/A):**
      for a `rounds>=2` run, after EACH round boundary verify: truth-table equivalence to the
      original adder (all `2^(2n+1)` inputs) AND zero-ancilla invariant (ADR 0001/0002 seam);
      failure message identifies the round index. Pipeline itself stays a pure transformation
      (no verification inside `run_full_pipeline`; ADR 0008 §Verification contract)
- [ ] `rounds=2` ⇒ `phases.size() == 8` and the phase-name sequence (ignoring round_index) equals
      today's 5-phase names; each round leaves TT + ancilla PASS
- [ ] CLI validation (ctest WILL_FAIL pattern, as 0030):
      - [ ] `--rounds=0` ⇒ error + non-zero exit
      - [ ] `--rounds=-1` ⇒ error + non-zero exit
      - [ ] `--rounds=abc` (unparseable) ⇒ error + non-zero exit
- [ ] Orthogonality (ADR 0008): same `rounds` + same key/seed stream ⇒ bit-exact replay across runs;
      different `rounds` ⇒ distinguishable; `rounds` change never interacts with `--key` / `--seed`
- [ ] All pre-existing tests pass unchanged (they never pass `--rounds`; default `rounds=1` path
      is the regression anchor)

> Grilling note (port to Done): per-round TT is test-side ONLY — `run_full_pipeline` hot path carries
> no truth-table work (same consumer/precedence decision as 0030 and ADR 0007). Inject-final-round
> means `rounds=2`'s gate count is NOT 2× the single-pass count — junk adds once; assert structural
> difference / distinguishability, not junk-count multiplication.
