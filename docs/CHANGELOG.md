# CHANGELOG

Milestone and version history for the Local Mixing MVP.
Consolidated from archived tickets per the DOCUMENTATION-LIFECYCLE convention.
Current system state lives in `CONTEXT.md`; terminology in `DEFINITION.md`.

---

## Milestone 6 — Keyed PRNG Seeding (2026-09-17)

Deterministic, key-derived replay built on ADR 0007: a single `std::mt19937` per run is now seeded
from a user-supplied **key** (non-empty string) instead of only the scalar `seed`. Same key + same
config ⇒ **bit-identical circuit**; different key ⇒ different random choices across every
rng-consuming phase (sandwich chain, shuffle, junk injection). The legacy `seed` path is untouched —
no-`--key` runs reproduce today's circuits exactly.

- **Derivation (ADR 0007, ticket 0030)**: `make_rng(cfg)` — empty `key` → `mt19937(cfg.seed)`;
  non-empty `key` → `std::seed_seq`(key bytes + domain `"local-mixing-v1"`). Exactly one engine per
  run, passed by reference through `sandwich → replace (Sound/R1/R2 read no rng) → shuffle → inject`;
  zero consumer-signature changes. Precedence `--key` > `--seed` > 42; `--key` + `--seed` ⇒ warning
  (not an error). Empty / whitespace-only `--key=` ⇒ error, non-zero exit (CLI ctest
  `cli_key_empty_errors`, WILL_FAIL).
- **Key is not a config language (ADR 0007 scope control)**: key only seeds the rng stream; pass
  enablement and parameters (`--sound`, `--replace-r2`, inject count, …) stay public CLI/config. Keyed
  streams are an **engineering PRNG** — no security claims; key-derived pass schedules are deferred
  (M6.1+).
- **Tests (ticket 0030)**: same-key run twice ⇒ gate-type+target sequences bit-identical;
  different keys under an rng-consuming config (Safe sandwich + shuffle + inject ON, same toggles)
  ⇒ at least one gate differs; `--seed`-only fallback equals literal `mt19937(seed)`; key precedence
  over seed; keyed run preserves truth-table + ancilla equivalence. Note `--key=42` (string) ≠
  `--seed=42` (int).
- Docs: ADR 0007 written (Q1–Q8 grilling), CONTEXT decision table + phase-2 randomness note, README
  `--key` / `--seed` + precedence, CHANGELOG this milestone.
- Tests: 11/11 ctest green (8 suites + 3 CLI-level).

**References:** ADR `0007-keyed-prng-seeding.md`; tickets 0029–0031.

## Milestone 5 — Sound Equivalent Replacement (2026-09-15)

Sound carrier of the legacy Active mode's "scramble intermediate values" intent: a third local-replace
pattern that is correction-by-construction (ADR 0006), replacing the withdrawn `F⁻¹∘Adder∘F`
conjugation claim with a window-level-verified T-c rewrite.

- **Sound core (ADR 0006, ticket 0026)**: new Pass 0 in `apply_local_replace`; matches the R1-family
  S-triple window and rewrites each in place as `CNOT(a,z); CNOT(z,s); CNOT(a,z); CNOT(b,s);
  CNOT(c,s)` (T-c), borrowing `z` as a shadow of `a` restored to 0 by window end. `ReplaceResult`
  gains `sound_hits`; `LocalReplaceConfig::enable_sound` (default off). `z` reuses R2's first
  extra-ancilla selection (`index ≥ 4n+1`); `extra_ancilla = 0` → 0 hits. Canonical sequence proven
  by window-level exhaustive enumeration (all 16 assignments, final `z == 0`).
- **Pipeline + CLI integration (ticket 0027)**: `PipelineConfig::enable_sound` (default off), CLI
  `--sound` (symmetric to `--replace-r2`), `PhaseStats::replacements_sound`, CLI `hits=S+R1+R2`; the
  `--no-replace` master switch disables Sound together with R1/R2. Enabling Sound consumes no `mt19937`.
- **Intermediate-state proof + docs (ticket 0028)**: T5 half-sequence recombination test asserts
  `z == 1` mid-window for a reproducible input (`a == 1`) while the completed window restores `z == 0`;
  CONTEXT / CHANGELOG / README updated with Sound as the replace-family Pass 0.
- Measured (bare adder, extra_ancilla ≥ 1, no sandwich): n=2 → `sound=2 r1=0 total=2`; n=4 → `sound=4
  r1=0 total=4` (Sound consumes every S-triple). Full Safe pipeline with `--sound` and `--replace-r2`:
  n=2 → `hits=2+0`, n=4 → `hits=4+0+6`; TT + ancilla PASS.
- Tests: 8/8 suites green.

**References:** ADR `0006-sound-equivalent-replacement.md` (ADR 0002 appended); tickets 0025–0028.

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
- **Active mode** (tentatively made the pipeline default at the time): CNOT chain control wires drawn from primary inputs and targets from primary outputs, aiming for real value-level mixing (`input XOR adder_partial_result`) and claiming truth-table / zero-ancilla preservation via `F⁻¹ ∘ Adder ∘ F` equivalence.
- **Safe mode** retained as a no-op baseline for isolated downstream-phase testing.
- `PipelineConfig.sandwich_mode` (default was `Active`) + `sandwich_chain_length`; CLI `--sandwich-mode=active|safe`; `--no-sandwich` still fully skips the phase.

> See the correction banner above — and Milestone 4 — for the final word: Active does **not** preserve
> the truth table / ancilla invariant, is experimental, and the default is now **Safe**.
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
