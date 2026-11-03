---
Title: M8 Close — Docs, summary, archive 0035, milestone wrap-up
Status: ready-for-agent
Milestone: M8
Category: Docs/Closeout
Priority: low
Created: 2026-09-22
Updated: 2026-09-22
Blocked by: 0036
Addresses: ADR 0009
Related: ADR 0007, ADR 0008, ticket 0012, ticket 0035
---
## Problem Statement
M8 (target-factory seam + keyed-perm $P_k$) implementation and oracle tests are expected complete under 0036. Close out milestone per M7 tri-block (0032→0033→0034) pattern: update docs, summarize execution, and archive the merged spec ticket.

## Tasks
1. **README**: document `--target=adder|perm` (default adder), key behavior (deterministic per key, orthogonal to rounds), perm `ancilla_ok` vacuous-true note if appropriate (concise, user-facing).
2. **CONTEXT**: add a short decision/close line for M8 (target-factory seam, adder 13/13 unchanged, keyed-perm oracle).
3. **CHANGELOG**: add Milestone 8 entry summarizing target-factory seam + keyed-perm oracle (non-breaking for adder).
4. **DEFINITION** (if needed): clarify target/factory seam vs pipeline phases (optional but consistent with ADR 0009 terminology).
5. **Summary**: write execution summary in `docs/summary/` mirroring M7 style (what changed, verification: adder regression + perm oracle + ctest green).
6. **Archive 0035**: move pointer `docs/ticket/0035-keyed-perm-target-factory-seam.md` to `docs/archive/` (or mark archived) consistent with M7 (0025–0034). Keep ADR 0009 canonical in `docs/adr/`.
7. **Verification**: full `ctest` green; adder 13/13 regression unchanged; perm oracle present; shell stream orthogonality holds (per 0036).

## Out of Scope
No code changes beyond doc updates. No re-merging ADR 0009 (already canonical).

## Done Criteria
- README/CHANGELOG/CONTEXT updated; summary written.
- 0035 pointer archived under `docs/archive/` (naming consistent with existing archive entries).
- All tests green; M8 closeout complete.
