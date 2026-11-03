---
Title: M8 Build — Target factory seam + keyed-perm P_k + CLI --target + oracle tests
Status: ready-for-agent
Milestone: M8
Category: Build/Implementation
Priority: medium
Created: 2026-09-22
Updated: 2026-09-22
Blocked by: 0035
Addresses: ADR 0009
Related: ADR 0007, ADR 0008, ticket 0012
---
## Problem Statement
ADR 0009 (canonical register at `docs/adr/0009-target-factory-seam.md`) defines the target-factory seam and keyed-perm `P_k` (seed_seq self-derived, does NOT consume the shell RNG stream), key via cfg/CLI, `ancilla_ok` vacuous-true for perm, junk on the same pipeline inject seam, and a 2^n keyed-perm truth-table oracle as acceptance anchor. The adder remains default with 13/13 regression unchanged. Ticket 0035 is a pointer to that ADR.

## Goal
Implement the seam + keyed permutation + CLI flag + oracle tests with zero behavioral change to the adder path and no per-target forks in the mixing shell.

## Implementation Tasks
1. **PipelineConfig**: add `target` (enum/string) with default `"adder"`. `--key=` already exists per ADR 0007 surface; ensure `cfg.key` flows to target factory when present.
2. **Target factory**: `make_target_circuit(cfg)` dispatches on `cfg.target` (`"adder"|"perm"`) and returns the target `Circuit`. Default `"adder"`. Unknown values → reject (non-zero exit) per CLI behavior.
3. **Keyed perm factory**: `make_nbit_keyed_perm(n, extra_ancilla = 0, /* key */ ...)` implementing $P_k$ per ADR 0009 (bijection over n data lines; `extra_ancilla == 0` → no factory ancilla pool). Signature must accept key (derive via `seed_seq(key_bytes, "local-mixing-keyed-perm-v1")`) and must **not** consume the shell's mt19937/RNG stream used by the mixing rounds loop.
4. **CLI**: `--target=adder|perm`. Invalid → non-zero exit + usage. Default `adder` preserves existing invocations.
5. **Shell agnosticism**: pipeline/inject/rounds loop, junk injection, PhaseStats aggregation remain target-agnostic (no per-target branches). Shell RNG stream position is identical for `--target=adder` and `--target=perm` under identical non-target config.
6. **Ancilla semantics**: perm has no factory ancilla pool → `ancilla_ok` vacuously true (inject-before/after semantics consistent with ADR 0009).

## Testing Requirements (must satisfy)
- **Adder regression**: 13/13 truth-table/regression set unchanged for `--target=adder` (byte/bit-identical behavior vs baseline).
- **Perm oracle**: keyed-permutation 2^n truth-table oracle verifies `P_k` for the exercised n (deterministic per key; different keys produce distinguishable permutations as per ADR 0007/0009).
- **Shell stream orthogonality**: under identical inject/config, switching target does **not** shift the mixing shell's RNG stream consumption (observable via shell-derived gate stream positions or equivalent lightweight assertion where feasible).
- **Junk comparability**: same inject configuration → `junk_hits`/`junk_volume` comparable/read on the same pipeline inject seam for both targets (no target-owned junk pool).
- **CLI validation**: unknown `--target` → non-zero exit.
- **Determinism**: same key+config+target → bit-identical; rounds orthogonal to key per ADR 0008/0009.

## Out of Scope
- No perm factory-claimed junk pool. No changes to pipeline.cpp/inject/junk phases beyond the single dispatch seam. Sandwich semantics (ticket 0012) unchanged (BLOCKED-by-0024 context). No manual `--target=perm` end-to-end “smoke” beyond unit/oracle tests unless required by existing test harness.

## Done Criteria
- All new/affected tests pass; full test suite (`ctest`/project tests) green.
- Adder path bit-identical to pre-M8 for default target.
- No per-target branches inside rounds loop/inject (verified by inspection).
- Perm TT oracle present and deterministic.
