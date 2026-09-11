# Execution Summary: 0020-r2-core-carry-replacement

- **Date**: 2026-09-10
- **Spec/ADR Path**: `docs/adr/0005-r2-carry-replacement-and-metrics.md`
- **Ticket Path**: `docs/ticket/0020-r2-core-carry-replacement.md`
- **Branch**: (uncommitted work on current branch)

---

## Overview
- **Commits**: 0 (uncommitted; builds on commit `81643eb`)
- **Files Changed**: 4 files (`+307` / `-70`)

## Key Changes
1. **`ReplaceResult` struct** added to `local_replace.hpp` — holds `r1_hits` and `r2_hits` separately (replaces old `int` return type).
2. **`LocalReplaceConfig::enable_r2`** added (default `false`) — controls R2 pass.
3. **R2 Pass 2** implemented in `local_replace.cpp` — scans for 3-Toffoli same-target windows with 3 distinct controls, replaces with canonical 5-gate sequence using borrowed wire `z`.
4. **`pipeline.cpp`** updated for new `ReplaceResult` return type; `enable_r2 = false` by default.

## Verification
- **Typecheck**: Pass (no compiler errors)
- **Tests**: 8/8 test suites pass (79 local_replace tests + all others)
- **Truth table**: PASS on all bare adder configurations tested
- **Ancilla invariant**: PASS on all tested configurations

## Code Review Findings
- **Standards**: Renamed `T` → `tgt` (mysterious name smell); removed redundant default assignment in `pipeline.cpp`. No hard violations.
- **Spec**: z selection uses general "no existing gate targets" approach instead of strict `≥ 4n+1` formula (more general, correct for non-VBE circuits). R2 hit counts after R1 differ from ADR's R2-only numbers (expected: R1 reduces available windows).

## Implementation Notes
- **z selection**: Scans backwards for last non-output, non-targeted wire (general approach; works for VBE adder and hand-built test circuits).
- **R2 hit counts (R1 runs first)**: n=2 → r2=2; n=4 → r2=4. (ADR's 2n−1 numbers assumed R2 in isolation.)
- **Gate-type delta**: R1 adds +2 Toffoli/hit; R2 adds +2 CNOT/hit — distinguishable in PhaseStats.
- **Degenerate case**: extra_ancilla=0 → r2=0, no crash, no circuit modification.

## Files Modified
| File | Change |
|------|--------|
| `include/local_replace.hpp` | `ReplaceResult` struct, `enable_r2` config, new function signature |
| `src/local_replace.cpp` | R2 pass, `match_r2` helper, general z selection |
| `src/pipeline.cpp` | Updated for `ReplaceResult` return type |
| `tests/test_local_replace.cpp` | R2 tests (hand-built, bare adder, combined, degenerate, distinguishability) |
