# Archived Tickets

Completed tickets from the Local Mixing MVP development, archived after merge per the
[DOCUMENTATION-LIFECYCLE](../DOCUMENTATION-LIFECYCLE.md) convention (Tickets & Issues → Archive & Knowledge Extraction).

Durable knowledge and current project state live in `../CONTEXT.md`; milestone history in `../CHANGELOG.md`.
These ticket files are retained for historical rationale only — do not treat them as live documentation.

## Summary

| Range | Milestone | Scope |
|-------|-----------|-------|
| 0001–0007 | Local Mixing MVP | Gate hierarchy, Circuit + 2-bit adder, topological sort/depth, sandwiching, shuffling, junk injection, full-pipeline integration |
| 0008–0011 | Active Sandwiching | `SandwichMode` expand/contract, Active-mode implementation, default pipelining + CLI, docs update |
| 0013–0016 | n-bit upgrade + R1 spec | Generic `make_nbit_adder(n, extra_ancilla)` + generalized verification, 4-bit pipeline adaptation, CLI observability, R1 local-replacement canonical spec (0016, superseded → 0017/0018/0019) |
| 0017–0019 | Local Replacement (R1) | Core S-triple rewrite, pipeline integration, CLI/tests/docs |
| 0020–0024 | R2 + Metrics + Active pin | R2 carry-majority rewrite, pipeline integration, per-phase structural metrics, tests/docs reconciliation, Active-mode gap pinned (0024; Safe is default) |

Active tickets live in `docs/ticket/` (currently 0012 — blocked-by-0024, awaiting a conjugation-sound Active-sandwich redesign). Completed ranges above are closed milestones; current state is in `../CONTEXT.md`, milestone history in `../CHANGELOG.md`.
