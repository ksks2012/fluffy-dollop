# CHANGELOG

Milestone and version history for the Local Mixing MVP.
Consolidated from archived tickets per the DOCUMENTATION-LIFECYCLE convention.
Current system state lives in `CONTEXT.md`; terminology in `DEFINITION.md`.

---

## Milestone 4 — R2 Carry-Majority Replacement + Per-Phase Structural Metrics (2026-09-11)

Second local rewrite (R2): carry-majority reconstruction with borrowed zero-ancilla wires, plus
structural metrics (depth / gate-type counts) at every phase boundary.

- **R2 core (ADR 0005)**: matched X-Tof-X triples (target `z`) rewritten in place with one half-toffoli
  window which is equivalent iff `z` is a touchable ancilla; hits `== n` on the bare adder when R1 runs
  first (ticket 0020; 81-test `test_local_replace` suite).
- **Pipeline + CLI integration (ticket 0021)**: `--replace-r2` phase (R2-off by default), `extra_ancilla`
  requirement (extra=0 → r2=0 correctly), production hit counts `r1 == n`, `r2 == n`, total `== 2n`;
  CLI displays `hits=N+M` (literal format) and lists `--replace-r2` in `Phases enabled`.
- **Per-phase structural metrics (ticket 0022)**: `PhaseStats` gains `depth_levels`, `count_cnot`,
  `count_toffoli`, `count_not` (filled at every phase boundary via `measure_structural`); CLI suffix
  `depth=D toffoli=T cnot=C` always shown; optional `--verbose-metrics` prints the final-circuit
  `gates_per_depth` histogram.
- **Active sandwich gap pinned and default changed (ticket 0024)**: ADR 0002's `F⁻¹∘Adder∘F≡Adder`
  claim was verified false for chains touching computation wires (default run FAILed TT/ancilla before
  any R2 work). Default `sandwich_mode` is now **Safe** (equivalence-guaranteed); Active is experimental
  with a CLI warning and pinned KNOWN-BROKEN regression tests.
- Tests: 8/8 suites green (pipeline + local_replace + gates/circuit/sandwich/shuffle/inject_junk).

**References:** ADR `0005-r2-carry-replacement-and-metrics.md` (0002 amended); tickets 0020–0024.

## Milestone 3 — Local Replacement (R1) Validation Token (2026-09-06)

Introduced a minimal sliver of generation mixing into the MVP: a single fixed, hand-written,
provably-equivalent local rewrite (R1) of matched CNOT-triple windows.

- New `apply_local_replace` phase (`local_replace.hpp/.cpp`, ticket 0017): single-pass, non-overlapping
  scan for strictly-adjacent same-target CNOT triples; each window rewritten in place as
  `Toffoli(a,b,s); CNOT(a,s); CNOT(b,s); CNOT(c,s); Toffoli(a,b,s)` (+2 Toffoli, 0 new wires).
  Net effect identical (`s ⊕= a ⊕ b ⊕ c`); exhaustive 2-bit/4-bit truth tables PASS.
- Pipeline integration (ticket 0018): `replace` phase with `PipelineConfig::enable_replace` (default
  true), `PhaseStats::replacements` (CLI prints `hits=N`), per-phase ancilla verification.
- **Phase-order amendment (ADR 0004):** replace runs `sandwich → replace → shuffle → inject`.
  Measurement showed shuffle destroys the strict-adjacency windows (hits 2/4 → 0), so the original
  Q4 order was amended, not silently changed; full-pipeline hits are deterministically `== n`.
- CLI `--no-replace` to disable the phase (ticket 0019).
- Tests: new `test_local_replace` suite (28 tests) + pipeline-level replace/integration tests; added
  `Circuit::remove_gates` seam. All 8 suites green.
- Docs: ADR 0004, `CONTEXT.md` (Phase 3 + non-goal reconciliation), this changelog.

**References:** ADR `0004-minimal-local-replacement-r1.md`; tickets 0016 (superseded), 0017–0019.

## Milestone 2 — Active Sandwiching Mode (2026-08-28)

> **Correction (2026-09-11, ticket 0024):** this milestone claimed Active mode "preserv[es] the
> 32-input truth table and zero-ancilla invariant" via `F⁻¹ ∘ Adder ∘ F`. That claim is **false** for
> chains touching computation wires — conjugation does not preserve a non-linear circuit. The default
> was changed to **Safe** in Milestone 4 (2026-09-11); Active is experimental and known to FAIL TT / ancilla.

Introduced a genuine mixing sandwiching mode alongside the prior no-op baseline.

- Introduced `SandwichMode` (`Safe | Active`) and extended `apply_sandwich` (expand–contract migration; all existing call sites forced to `Safe` first, CI green throughout).
- **Active mode** (now the pipeline default): CNOT chain control wires drawn from primary inputs and targets from primary outputs, producing real value-level mixing (`input XOR adder_partial_result`) while preserving the 32-input truth table and zero-ancilla invariant. Correctness by `F⁻¹ ∘ Adder ∘ F` equivalence.
- **Safe mode** retained as a no-op baseline for isolated downstream-phase testing.
- `PipelineConfig.sandwich_mode` (default `Active`) + `sandwich_chain_length`; CLI `--sandwich-mode=active|safe`; `--no-sandwich` still fully skips the phase.
- Scramble verification (forward-only intermediate perturbation) + Safe/Active truth-table, ancilla-invariant, determinism, gate-count-delta tests.
- Docs updated: `DEFINITION.md` (Sandwiching), `CONTEXT.md` (Phase 2), `README.md` (CLI + sample output). ADR 0002 records the decision.

**References:** ADR `0002-active-sandwiching-mode.md`; tickets 0008–0011.

## Milestone 1 — Local Mixing MVP (2026-08-26)

Functional end-to-end pipeline for the Local Mixing MVP, validated on a 2-bit ripple-carry adder.

- Polymorphic reversible gate hierarchy (`Gate` → `ToffoliGate`, `CnotGate`, `NotGate`) with in-place `evaluate(vector<int>&)` model.
- Adjacency-list `Circuit` class; canonical 2-bit ripple-carry adder with pre-allocated zero-ancilla wires.
- Topological sort + depth-level computation (same-depth gates independent).
- Sandbox/prereq transforms: CNOT-chain sandwiching, depth-level shuffling, zero-ancilla junk injection.
- 6 test suites (gates, circuit, sandwich, shuffle, inject_junk, pipeline) — all green.
- `.hpp`/`.cpp` separation per Coding Standards.

**References:** ADR `0001-local-mixing-mvp-spec.md`; tickets 0001–0007.
