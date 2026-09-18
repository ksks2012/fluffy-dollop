# Execution Summary: 0031 Keyed PRNG seeding (Milestone 6)

- **Date**: 2026-09-17
- **Spec/ADR Path**: `docs/adr/0007-keyed-prng-seeding.md`
- **Ticket Paths**: `docs/ticket/0029-adr-keyed-prng.md`,
  `docs/ticket/0030-keyed-prng-implementation.md`,
  `docs/ticket/0031-keyed-prng-docs-close.md`
- **Branch**: `main` (uncommitted working tree for this milestone)

---

## Overview

M6 upgrades the single-scalar-seed demo into a **keyed, replayable engineering PRNG seeding**
contract (ADR 0007). The pipeline builds exactly one `std::mt19937` from `--key` (non-empty string
bytes + domain `"local-mixing-v1"` → `std::seed_seq`) > `--seed` > default 42. Same key + same config
⇒ bit-identical circuit; different key ⇒ different random choices across sandwich / shuffle / inject.
Key only seeds the stream — it is NOT a pass-selection language. No security claims (engineering
PRNG); key-derived pass schedules deferred to M6.1.

## Key Changes

1. **`PipelineConfig::key`** (`std::string`, default `""`), `seed` untouched (`int seed = 42`).
2. **`make_rng(cfg)`** single construction seam in `src/pipeline.cpp`; `run_full_pipeline` now uses it
   (one engine per run, consumer chain unchanged: `sandwich → replace [Sound/R1/R2 read no rng] →
   shuffle → inject`).
3. **CLI** (`src/main.cpp`): `--key=<string>` (empty / whitespace-only value → error, non-zero exit),
   `--seed=<int>`, precedence warning when both given. Precedence scan folded into the parse loop
   (also fixes a short-unknown-flag `substr` crash found in review).
4. **Tests** (`tests/test_pipeline.cpp`, +7): same-key bit-exact gate-type+target sequences;
   different-key distinguishable under rng-consuming config; `--seed`-only fallback equals literal
   `mt19937(seed)`; key precedence over seed; keyed TT + ancilla equivalence.
   **CLI-level ctests** (CMakeLists): `cli_key_empty_errors` (WILL_FAIL), `cli_key_then_seed`,
   `cli_seed_only`.
5. **Docs**: README `--key`/`--seed` + `--key=42` ≠ `--seed=42` note; CONTEXT phase-2 randomness
   contract + decision rows 11/14 + ADR index; CHANGELOG Milestone 6. ADR 0001/0002/0006 bodies
   untouched; DEFINITION untouched.

## Verification

- **Build**: fresh `cmake -B build && cmake --build build` — 0 errors, 0 warnings
- **Tests**: `ctest --test-dir build` — **11/11 passed** (8 suites + 3 CLI-level); `test_pipeline`
  = All 360 tests passed
- **Determinism demo**: `gate_driver --bits=4 --key=keyA` run twice → identical md5; `--key=keyB` →
  different hash (replay + distinguishability confirmed end-to-end)
- **Typecheck**: N/A (no typecheck tool configured for C++ in this project)

## Code Review Findings

- **Standards**: no hard violations. Judgement calls — `#include <random>` in header (inherent, mt19937
  is a typedef); duplicated precedence/derivation text across docs; `gates_signature` twin-vector return;
  nascent `dynamic_cast` switch (no `Gate::type()` tag today). All minor.
- **Spec**: two defects found in review and fixed — (1) short unknown CLI flag (`-x`) crashed via
  `substr` out_of_range; (2) whitespace-only `--key=` check only covered `' '`/`'\t'`, now uses
  `ch > ' ' && ch != 0x7f` (covers control whitespace). Milestone summary (this file) added. Else spec
  fully implemented: derivation, precedence, single-stream, empty-key error, observation tests, docs.