# 0030 — Keyed PRNG seeding: `make_rng`, CLI `--key`, tests (M6 build)

**What to build:** Implement ADR 0007's derivation and surface it through the pipeline and CLI:
`PipelineConfig::key` (default `""` = use `seed`), a single `make_rng(cfg)` helper used by
`run_full_pipeline`, CLI `--key=<string>` (empty value → non-zero exit), and the deterministic-replay
/ distinguishability test suite. Milestone M6 main build.

**Blocked by:** 0029 (ADR 0007 + seams spec)

**Status:** implemented (2026-09-17)

- [x] `PipelineConfig::key` = `std::string` default `""`; `seed` untouched (`int seed = 42`)
- [x] `make_rng(cfg)` (single helper, in the `.cpp` seam from 0029):
      `!cfg.key.empty()` → `std::seed_seq`(key bytes + `"local-mixing-v1"` bytes) → `mt19937`;
      else `mt19937{cfg.seed}`
  - [x] Exactly one `mt19937` per pipeline run; references passed through existing consumer chain
        (`sandwich → replace [no-rng] → shuffle → inject`); zero consumer-signature changes
- [x] CLI `--key=`:
      value stored into `cfg.key`; empty/whitespace-only value → error message + non-zero exit
      (never treated as "no key")
  - [x] `--key` + `--seed` both given: `--key` wins, no error (optional one-line stderr warning);
        no-`--key` runs byte-identical to current `seed` behavior
- [x] Tests (`tests/test_pipeline.cpp`, existing deterministic-run pattern):
  - [x] Same key + same full `PipelineConfig` ran twice ⇒ gate sequences bit-identical
        (use identical config incl. `--sound off` etc. — do NOT vary pass toggles between the two runs)
  - [x] Different key + same config ⇒ at least one gate differs — MUST use an rng-consuming config
        (Safe sandwich + shuffle and/or inject ON), otherwise two keys may coincidentally pass
  - [x] `--seed`-only run (no key) reproduces today's seed-42 circuit (fallback regression)
  - [x] `--key` precedence over `--seed` (both set ⇒ key-derived, distinguishable from seed-only)
  - [x] Empty `--key=` → non-zero exit (CLI-level ctest `cli_key_empty_errors`, WILL_FAIL)
  - [x] Keyed run TT-equivalent (Safe config) — key change must not break equivalence
- [x] All pre-existing tests pass unchanged (they never pass `--key`; seed path unmodified)

> Grilling note (port to Done): the distinguishability test asserts gate-sequence inequality only —
> NOT depth/gate-count/hits metrics (flaky). `--key=42` (string) ≠ `--seed=42` (int): distinct
> derivations, both deterministic; document one example of each.