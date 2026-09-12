# Execution Summary: 0023-r2-tests-and-docs

- **Date**: 2026-09-12
- **Spec/ADR Path**: `docs/adr/0005-r2-carry-replacement-and-metrics.md`
- **Ticket Path**: `docs/archive/tickets/0023-r2-tests-and-docs.md`
- **Branch**: `main`

---

## Overview
- **Commits**: `8c8e9d4` (docs: README, CONTEXT, CHANGELOG) on top of the metrics commit `59c5aa1`
- **Files Changed**: `tests/test_pipeline.cpp` (+32, R2 Safe exact-hit-count tests),
  README, CONTEXT, CHANGELOG, ticket 0023

## Key Changes
1. **R2 Safe-pipeline gold standard tests** (`test_pipeline_r2_safe_exact_hit_counts`): full
   pipeline with Safe sandwich, seed=42 — n=2: `r1==2, r2==3, total==5`; n=4: `r1==4, r2==6,
   total==10`; TT PASS + ancilla PASS at every boundary. This pins the *Safe-pipeline* counts
   (sandwich mode and seed affect window availability after mixing, so these are seed-specific).
2. **Bare-adder gold standard** (already in `test_local_replace.cpp:240/249`): R1-first ⇒
   `r1 == n`, `r2 == n`, combined total `== 2n` (n=2 → 4, n=4 → 8). Comment at
   `test_pipeline.cpp:667`: "R1-first: r2 == n, not 2n−1".
3. **Docs reconciliation (ticket 0023)**: CONTEXT §1 non-goal, §2 Phase 3 (R2 second pass,
   carry-majority 3-Toffoli, borrow-1-zero-ancilla), §3 decision row 13; CHANGELOG Milestone 4;
   README CLI examples replaced with actual measured 4-bit Safe output.

## Verification
- **Typecheck**: green
- **Tests**: local_replace 81/81, pipeline 280/280, `ctest` 8/8 all pass
- **CLI (measured, 4-bit Safe)**: `replace gates=57 depth=16 toffoli=29 cnot=28 hits=4+4`;
  `inject gates=69 toffoli=41 cnot=28 wires=24 overhead=2.09x`; TT PASS 512

## Code Review Findings (spec axis)
- **(c) CONTEXT §2 R2 window description wrong**: claimed the R2 matcher looked for
  "X-Tof-X triples (a Toffoli flanked by one CNOT on each side)". Contradicts ADR 0005's canonical
  window — three **strictly adjacent** `Toffoli`, same target, controls from three distinct wires
  (the CNOT-flanked form is the *rewrite output*, not the match). Fixed in CONTEXT and mirrored by
  the ADR 0005 correction banner (2026-09-12).
- **(a, minor)** no Safe-pipeline depth/toffoli/cnot assertions in the metrics test; metrics
  structural values asserted indirectly via the adder-phase test.
- **(b)** README/CHANGELOG edits slightly exceed the ticket's docs scope (accepted; they were the
  only stale user-facing examples).

## Documentation Lifecycle (concentrate pass, 2026-09-12)
- `DEFINITION.md`: Active-mode default claim corrected → **Safe is the default** (ticket 0024,
  ADR 0002); the withdrawn `F⁻¹ ∘ Adder ∘ F` cancellation claim removed from the Sandwiching /
  Slice-Gate entries; R1/R2 added to the MVP subset and Mixing operations; ripple-carry adder wire
  count clarified (base `4n+1` vs pipeline default `extra_ancilla=4` → 4-bit = 21 wires).
- `CONTEXT.md` §2: R2 matcher description corrected (see finding c above).
- `ADR 0005`: correction banner added (append-only, per DOCUMENTATION-LIFECYCLE) — the
  `2n−1` R2-standalone figures assume R2 in isolation; integrated R1-first gold standard is
  `r2 == n`, total `== 2n`.
- Tickets: 0013/0014/0015/0020 marked `implemented` (were stale `ready-for-agent`).
- 0024 note remains in the 0021 summary (`docs/summary/0021-...md` lines 53–60) + ADR 0002 correction.

---

**Next:** ticket 0012 (`fix-active-sandwich-semantics`) is still `blocked-by-0024`; a
conjugation-sound Active-sandwich redesign is deferred. Completed tickets 0013–0024 were archived
to `docs/archive/tickets/` (milestone sweep, 2026-09-12); only 0012 remains active in `docs/ticket/`.