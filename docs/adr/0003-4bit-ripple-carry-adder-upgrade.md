# ADR 0003 — 4-bit Ripple-Carry Adder Upgrade (Scalable Validation)

> Spec: upgrade the target circuit from 2-bit to 4-bit, generalizing the adder factory and
> verification helpers, while preserving the existing Local Mixing pipeline semantics.
> Synthesized from grilling session outcomes (decisions 1–8). Terminology follows `docs/DEFINITION.md`.

---

## Problem Statement

The Local Mixing MVP is validated only on a 2-bit ripple-carry adder. That circuit is so shallow
(small number of `Topological Depth Levels`, tiny shuffle space) that the pipeline's mixing phases
cannot be meaningfully exercised: the `Overhead Ratio` is large but the actual mixing effect of
`Shuffling` is near nil. Developers need a slightly larger target to confirm the existing Active
sandwich → shuffle → junk pipeline remains functionally equivalent and preserves the
zero-ancilla invariant as circuit size grows, and to observe whether depth levels and the shuffle
space begin to behave meaningfully.

Today the entire pipeline, its verification helpers, and its wire budgets are hardcoded to the
2-bit instance:

- `check_truth_table` / `check_ancilla_invariant` hardcode 32 input cases and the 2-bit
  `A1*2+A0 + B1*2+B0 + Cin` arithmetic and `[S1,S0,Cout]` output positions
  (`src/pipeline.cpp:13,25`).
- `run_full_pipeline` hardcodes `make_2bit_adder()` and `ORIGINAL_GATES = 15`
  (`src/pipeline.cpp:45,47`).
- `PipelineConfig` hardcodes `sandwich_ancilla = {9,10,11,12}` and `inject_ancilla = {13,14,15}`
  (`include/pipeline.hpp:18,26`).

This is a validation-artifact upgrade only: it does **not** pursue minimal gates, optimal adder
design, circuits larger than 4-bit, generation mixing, or any security/attack measurement.

---

## Solution

Generalize the adder construction and every verification/wire-budget dependency so the same
pipeline runs on both a 2-bit and a 4-bit reversible ripple-carry adder, with 2-bit preserved as a
fast regression baseline and 4-bit becoming the primary validation circuit.

A single generic `make_nbit_adder(n, extra_ancilla)` factory builds the adder via loops (same
CNOT-sum + Toffoli-majority + uncompute paradigm for each bit). The verification helpers and the
overhead baseline become parameterized by the circuit itself; wire budgets are derived from the
adder rather than hardcoded. The `cfg.seed`-driven `std::mt19937` stream stays the reproducible random source.

This is deliberately a *narrow* generalization: no new obfuscation phase, no semantic change to
Active sandwich / shuffle / junk, only adapting them to a larger wire count.

---

## User Stories

1. As a developer, I want a single generic `make_nbit_adder(n, extra_ancilla)` factory, so that both the 2-bit and the 4-bit adder are produced by one parameterized code path instead of duplicated canonical sequences.
2. As a developer, I want `make_nbit_adder(2, ...)` to behave equivalently to the existing 2-bit adder, reached through a thin retained `make_2bit_adder()` wrapper, so that existing tests can migrate without re-deriving semantics.
3. As a developer, I want `make_nbit_adder(4, ...)` to produce a correct 4-bit reversible ripple-carry adder whose outputs match integer addition `A + B + Cin` for every input, so that I have a trusted larger validation target.
4. As a developer, I want the factory to support `n = 2` and `n = 4` and to fail observably on unsupported `n`, so that unsupported sizes raise a clear error rather than silently misbehaving.
5. As a developer, I want the factory's wire layout to follow a fixed contract — inputs `[0..n-1] = A_{n-1}..A_0`, `[n..2n-1] = B_{n-1}..B_0`, `[2n] = Cin`; outputs `output_wires() = [S_{n-1}..S_0, Cout]` — so that consumers rely on one stable contract instead of physical wire indices.
6. As a developer, I want the adder's wire count to follow a predictable per-size formula with intermediate carries kept on dedicated `O(n)` working wires (not carried on a size-independent budget), so that the wire budget is explicit and the working wires are all uncomputed to 0 at the end.
7. As a developer, I want the 4-bit adder to use one intermediate carry wire per bit (`n-1` in total, plus at most one optional temp), all uncomputed back to 0 in reverse order after computing the sums and the carry-out, so that the adder matches the existing 2-bit "compute carry → use it → reverse-uncompute" paradigm and is straightforward to verify.
8. As a developer, I want Active sandwiching to draw each `(ctrl, tgt)` pair from the circuit's own `[0, num_wires())` with `ctrl != tgt` (per the already-locked Active semantics), so that raising the wire count automatically gives Active sandwiching a larger, real mixing surface with no semantic change.
9. As a developer, I want `check_truth_table(c)` to derive `n = (num_inputs - 1) / 2`, enumerate all `2^(2n+1)` input combinations, compute the expected sum `A + B + Cin` in a loop, and compare against `output_wires()` order, so that a single generalized checker validates any supported `n`.
10. As a developer, I want `check_ancilla_invariant(c)` to treat every non-primary-output wire as ancilla that must return to 0, so that working wires and extra ancilla are treated identically (per decision 4).
11. As a developer, I want the 2-bit-specific `check_truth_table`/`check_ancilla_invariant` replaced by the generalized versions with no 2-bit-only copies retained, so that there is a single verification code path.
12. As a developer, I want `PipelineConfig` to carry `n` and `extra_ancilla` as first-class fields, so that the pipeline's circuit size and ancilla budget are explicit and one `run_full_pipeline(cfg)` entry serves both sizes.
13. As a developer, I want `run_full_pipeline` to build the adder via `make_nbit_adder(cfg.n, cfg.extra_ancilla)` and to record the overhead baseline as the resulting adder's own gate count, so that the overhead ratio is computed per size rather than from a hardcoded 15.
14. As a developer, I want the `sandwich_ancilla` / `inject_ancilla` wire selections derived from the adder's non-input/non-output wires (or configured as a bare extra-ancilla count) rather than hardcoded `{9..12}` / `{13..15}`, so that Safe sandwich and junk receive a valid zero-ancilla pool on any size.
15. As a developer, I want a CLI switch `--bits=N` (e.g. `--bits=2`, `--bits=4`) alongside `--extra-ancilla=N` and `--sandwich-mode=...`, so that an operator can choose the target size without rebuilding.
16. As a developer, I want the full pipeline to pass the exhaustive truth table and the ancilla invariant at every phase boundary on the 4-bit adder with Active sandwich as the default, so that the end-to-end real path is validated on the larger circuit.
17. As a developer, I want the 2-bit pipeline and its regression tests to remain green after generalization, so that the original validation baseline is preserved.
18. As a developer, I want the 4-bit circuit to exhibit observably more `Topological Depth Levels` and a larger per-level shuffle space than the 2-bit circuit, so that the pipeline's mixing phases can be meaningfully exercised and validated.

---

## Implementation Decisions

1. **Generic adder factory** — add `Circuit make_nbit_adder(int n, int extra_ancilla = 4)` (or an equivalently-named `make_ripple_carry_adder`). It uses the same VBE-style reversible ripple-carry paradigm as the existing 2-bit adder, generalized by a loop: forward over bits computing the intermediate carry `C_{i+1} = maj(A_i, B_i, C_i)` into a per-bit carry wire (three Toffoli gates), then the sums `S_i = A_i ^ B_i ^ C_i` (three CNOT gates), then a final `Cout` written to the output, then a **reverse-order uncompute** of all intermediate carry wires to restore them to 0. The exact gate sequence — including how `Cin` seeds `C_0`, when `Cout` is written, and the reverse-uncompute order — is written down concretely next to the adder's source as a canonical comment with an accompanying spot-check in the adder test suite (source comment + spot-check are authoritative). **`make_2bit_adder()` is retained as a thin wrapper delegating to `make_nbit_adder(2, ...)`**, so the ~40 existing call sites (tests, pipeline) are not rewritten in this step; the wrapper is the single place that pins the 2-bit entry point. **"Equivalence" here means functional equivalence (identical I/O truth table) and the ancilla invariant (all non-output wires return to 0) — it deliberately does **not** promise structural identity, and for `n = 2` we guarantee only these two properties plus the wire count.** For `n = 2` the construction yields base wire count `4·2+1 = 9` (matching the original 9-wire 2-bit adder) and a deterministic gate count of `9n-3 = 15`, with the same gate set (controls/targets) as the original but emitted in a possibly different (still-valid) topological order. No `n = 2`-specific branch is used; the same VBE loop builds every size. Existing 2-bit tests that assert functional equivalence, the ancilla invariant, 9 wires, or the derived gate count continue to hold; tests that depend on the *specific order* of the old hardcoded gates are not guaranteed and must assert only the canonical spot-check (see decision 3).
2. **Wire contract** — the canonical layout is fixed as: input wires `[0..n-1] = A_{n-1}..A_0`, `[n..2n-1] = B_{n-1}..B_0`, `[2n] = Cin`; output wires `output_wires() = [S_{n-1}..S_0, Cout]`. The working-ancilla region sits between the outputs and the extra-ancilla region. Physical wire numbering is irrelevant to consumers; only the `output_wires()` ordering is contractual.
3. **Working ancilla budget (amended): `(n-1) + t` carry wires** — the generic adder is a VBE-style ripple carry with **`n-1` intermediate carry wires plus at most one optional temp wire** (`t ∈ {0,1}`), all reverse-uncomputed to 0.
   - **Carry-chain meaning**: the running carry is `C_0 := Cin` (the input carry on input wire `2n` seeds the chain). For each bit `i = 0..n-1`, the forward pass computes `carry_{i+1} = maj(A_i, B_i, C_i)` where `C_i = Cin` for `i = 0` and `C_i = carry_i` for `i ≥ 1`. There are exactly **`n-1` distinct carry wires** `carry_1 .. carry_{n-1}` — one per carry that must persist as a working wire — because `carry_0 = Cin` already lives on an input wire and `carry_n = Cout` is written to the *output* wire, not a working wire.
   - **Uncompute**: after the sums are formed, the `n-1` carry wires are cleared to 0 by re-emitting each `maj` in **reverse order** (reverse of the forward creation order) using the self-inverse Toffoli majority.
   - **Wire budget**: base = `(2n+1) inputs + (n+1) outputs + ((n-1)+t) working = 4n + 1 + t`; total = `4n + 1 + t + extra_ancilla`. The construction here fixes `t = 0`, so the working region is the `4n+1`-base; a temp wire is reserved by the formula but unused by the current implementation.
   - This is `O(n)` working ancilla, **not** a size-independent budget. It supersedes the earlier "≤ 2 working, `3n + 4`" estimate: the double-buffer (main + ≤1 staging) approach was shown to be unworkable for `n ≥ 3` because clearing a carry's wire requires its ancestor carry to remain intact, which cannot be satisfied with a size-independent number of working wires. This is a deliberate amendment for correctness and consistency with the existing 2-bit construction, not a hidden constraint change.
4. **Working = ordinary zero wire** — the pipeline does not distinguish working ancilla from extra ancilla. Any non-primary-output wire that is 0 at a phase boundary is a valid zero wire for Safe sandwich and junk injection. This is guaranteed by the per-phase ancilla invariant and removes adder-internal knowledge from the pipeline.
5. **Parameterized verification** — replace `check_truth_table(Circuit&)` and `check_ancilla_invariant(Circuit&)` with versions that derive `n = (num_inputs - 1) / 2`, guard against unsupported `n` (clear assert/error for `n` outside `{2, 4}`), enumerate `1 << (2n+1)` input patterns, compute `sum = A + B + Cin` with loops over the `n`-bit A and B fields, and compare the extracted `[S_{n-1}..S_0, Cout]` against `output_wires()` in order. No 2-bit-only copies are retained.
6. **Overhead baseline** — `PipelineConfig` / pipeline records the original adder's gate count (`make_nbit_adder(cfg.n, ...).gate_count()`) as the denominator for all `Overhead Ratio` calculations, replacing the hardcoded `ORIGINAL_GATES = 15`.
7. **`PipelineConfig` fields** — add `int n = 2;` and `int extra_ancilla = 4;`. Keep `sandwich_mode = Active`, `sandwich_chain_length = 8`, and the phase toggles. The `sandwich_ancilla` / `inject_ancilla` lists are no longer hardcoded as fixed indices: they are derived after adder construction as the non-input/non-output zero wires, or receive the bare `extra_ancilla` count. `run_full_pipeline(cfg)` builds the circuit with `make_nbit_adder(cfg.n, cfg.extra_ancilla)`.
8. **Single pipeline entry** — `run_full_pipeline(cfg)` remains the sole entry; `cfg.n` selects the size. No parallel `run_full_pipeline_n` entry is introduced.
9. **Wire-budget invariance** — remove reliance on the fixed `{9,10,11,12}` / `{13,14,15}` indices. Safe sandwich's `apply_sandwich` currently auto-extends wires to fit its requested `extra_ancilla` (observed at `src/sandwich.cpp:47`); after the upgrade the pool comes from the adder's actual zero wires, so the hardcoded indices disappear from production config.
10. **Randomness** — keep the deterministic `std::mt19937` stream seeded from `cfg.seed`; the 4-bit path consumes the same reproducible RNG stream, preserving determinism and seed-replay.
11. **Scope extension (reconciliation)** — this upgrade deliberately extends the prior "circuits larger than 2-bit" out-of-scope statements (ADR 0001 §Out of Scope, ADR 0002 §Out of Scope, `CONTEXT.md` §1 non-goals). It narrows that boundary to "larger than 4-bit" for this step. ADR 0001 and ADR 0002 remain in force and are **not** superseded; this ADR refines their scope boundary. No *security* scope is expanded.

---

## Testing Decisions

- **What is a good test**: tests verify external observable behavior — exhaustive functional-equivalence (truth table) for all `2^(2n+1)` inputs, the zero-ancilla invariant at every phase boundary, gate-count/overhead derivation, determinism, and the wire-count contract — not internal wire-pool or carry-staging implementation details.
- **Seam**: primarily the top-level pipeline — `run_full_pipeline(cfg)` with a 4-bit config, plus the parameterized `check_truth_table(c)` / `check_ancilla_invariant(c)` as the correctness oracle. These are existing, correctly-leveled, high seams; the generic `make_nbit_adder(n, extra)` factory is the next-level seam exercised through the pipeline and directly for the adder unit. No new production seam is introduced beyond the generic factory (which is the natural home for the loop-based construction and is not wrapped in a further abstraction).
- **Test-migration strategy (decision 9)**: the codebase's inline 2-bit truth-table / ancilla loops (e.g. in `test_sandwich.cpp`, `test_circuit.cpp`, `test_inject_junk.cpp`) are replaced by the **shared parameterized helper** (the generalized `check_truth_table(c)` / `check_ancilla_invariant(c)`). 2-bit tests are migrated onto `make_nbit_adder(2, ...)` (through the `make_2bit_adder()` wrapper) and the shared checker. New 4-bit coverage is added through the same helpers. **No long-lived dual track is permitted**: inline 2-bit-only truth-table loops must not remain alongside the shared helper once the migration lands — they are the migration target, not co-existing duplicates. Before the 2-bit-specific copies are deleted, run the generalized checker at `n = 2` and diff its result against the old 32-case behavior once; only after that diff confirms equivalence is the old logic removed. This preserves the 2-bit regression as a hard precondition of the migration.
- **Modules tested / prior art**:
  - `test_pipeline.cpp` (existing) — extend with a 4-bit full-pipeline run (`cfg.n = 4`) asserting phases present, per-phase ancilla invariant, final truth table, wire-count `4n+1+extra` (with `t = 0`), determinism, and that different seeds diverge while both remain correct. Existing 2-bit assertions that reference the literal gate count `15` are updated to derive the expected count from the 2-bit adder's actual gate count (via the factory) rather than a literal.
  - New `test_adder.cpp` (new, following the pattern of `test_circuit.cpp` / `test_gates.cpp`) — for `n = 2` and `n = 4`, assert the canonical wire layout contract, the input/`output_wires()` ordering, and exhaustive truth-table correctness over all inputs via the parameterized checker; assert all working ancilla return to 0.
  - `test_sandwich.cpp` / `test_inject_junk.cpp` / `test_shuffle.cpp` (existing) — migrate inline truth-table loops to the shared checker and keep behavior green; add a 4-bit Active-sandwich truth-table + ancilla-invariant case to confirm size-agnostic sandwiching.
  - CLI/driver — a manual/observed run with `--bits=4` confirms gate count, overhead, and depth-level statistics are printed and the pipeline passes verification.

---

## Out of Scope

- Minimal gate count / optimal reversible adder design.
- Circuits larger than 4-bit (8-bit and beyond deferred).
- Generation mixing / equivalence-table sub-circuit replacement.
- Nonlinear gadgetization.
- Any security, attack-resistance, or obfuscation-strength measurement.
- Changes to the semantics of Active sandwich, shuffle, or junk injection (only adapting them to a larger wire count).
- A formal equivalence prover (exhaustive enumeration remains the oracle).

---

## Further Notes

- **Documentation updates** (per DOCUMENTATION-LIFECYCLE; deferred to the implementing ticket's Done criteria, which must list the files):
  - `CONTEXT.md` — §1 change the non-goal "circuits larger than 2-bit" to reflect 4-bit as the primary validation circuit and 2-bit as fast regression; §2/§3 note the generic adder and generalized checker; §1 "Positioning" now speaks of the 4-bit (and 2-bit) adder.
  - `DEFINITION.md` — update `Functional Equivalence` (exhaustive cases now `2^(2n+1)`), `Ripple-Carry Adder` (parameterized `n`), and the sandbox entry to reference the generic factory.
  - `README.md` — CLI (`--bits=N`, `--extra-ancilla=N`) and sample output.
  - record this decision as ADR 0003 (this file); the implementing ticket references it.
- **4-bit adder gate count**: the loop-based construction produces a larger gate count than the 2-bit hardcoded 15; all literal `15` references in tests/CLI/output must become derived from the adder, not constants.
- **Working-ancilla amendment**: the VBE-style construction uses `n-1` intermediate carry wires plus at most one optional temp (all reverse-uncomputed to 0), giving base `4n + 1 + t` wires. This replaces the earlier "≤ 2 working / `3n + 4`" estimate (decision 3/6 partially abandoned for `n ≥ 3`; decisions 4 and 5 preserved; decision 7's formula updated to `4n + 1 + t + extra`). Per-bit carry chains make a size-independent working budget impossible, so `O(n)` carry wires are the chosen, documented approach rather than the Cuccaro/Cdkm minimum-ancilla adder (deferred as a future optional optimization).

---

*Created: 2026-08-30*
*Source: grilling session, decisions 1–8; to-spec synthesis.*
