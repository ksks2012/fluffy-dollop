---
title: M7 — Multi-round mixing (ADR 0008)
status: closed
milestone: M7
tickets: 0032, 0033, 0034
adr: ADR 0008
---

# Milestone 7 — Multi-round mixing (ADR 0008)

**Dates:** M6 close (0030) → M7 close (0034)
**Spec:** `docs/adr/0008-multi-round-mixing.md` + tickets 0032–0034
**Branch:** main

## Overview

**Commits (M7 atom):** 4 (`fb43a50` ADR 0008 + `faf5245` feat + `af89649` docs-close; `f90514c` ADR-doc parent)
**Files changed:** 9 (`src/pipeline.cpp`, `src/main.cpp`, `include/pipeline.hpp`, `tests/test_pipeline.cpp`, `CMakeLists.txt`, 3 docs, 1 ticket)

## Key Changes

* `run_full_pipeline` now wraps the (sandwich → replace → shuffle) chain in a `rounds` loop —
  the adder runs on round 0 (phase 0), each mixing shake repeats the full chain, junk junk
  inject runs **final round only** (additive pass, never × rounds) — ADR 0008 §Solution.
* `PhaseStats::round_index` — every phase carries its round (adder → 0, per-round → that
  round, inject → rounds-1); phase names carry NO round suffix (ADR 0008 §Q4).
* CLI `--rounds=N` (N ≥ 1; N < 1 / unparseable → error + non-zero exit) — orthogonal to
  key/seed/seed (ADR 0008 §Q6/A; ADR 0007 §Orthogonality).
* `docs/` close: README `--rounds=` row; CONTEXT decision-row 12 (ADR 0008); CHANGELOG
  §Milestone 7 block.

## Verification

* Typecheck: ✅ `cmake --build build` clean (0 warnings, 0 errors)
* Tests: ✅ `ctest` — 13/13 (incl. `cli_rounds_zero_errors` / `cli_rounds_empty_errors`
  WILL_FAIL — the CLI red seams, expected-fail)

## Code Review Findings

* **Standards:** No issues — the rounds seam landed IN the existing 5-phase shape (adder
  outside loop, inject final-round gate), rounds=1 the bit-exact today's-5-anchor regression
  preserved (0030/0031 anchor untouched). TDD red-first sequence lands in the runner exactly
  as the M5 seam body book.
* **Spec:** Fully implemented (ADR 0008 §Solution 100%): 1+3R+1 phases (rounds=1 → 5;
  rounds=2 → 8); per-round TT + ancilla invariant verified per round (0033 §test063-069);
  rounds orthogonal to key/seed (ADR 0007 §Q6).

## Milestone Closing

M1–M7 all landed. M7 = hyperborn multi-round mixing (ADR 0008 + tickets 0032/0033/0034).
