# ADR 0002 — Active Sandwiching Mode

> **CORRECTION (2026-09-11, ticket 0024):** the claim in Solution § "Correctness is guaranteed
> mathematically" — `F⁻¹ ∘ Adder ∘ F ≡ Adder` for an arbitrary CNOT chain `F` drawn from all circuit
> wires — is **withdrawn as false**. Conjugating a non-linear circuit by a CNOT chain that touches
> its computation lines does NOT preserve the circuit: the ripple-carry adder computes on scrambled
> `A/B/Cin`, and a linear `F⁻¹` cannot cancel errors introduced while the adder ran on scrambled
> inputs. Only **Safe** mode (chart drawn exclusively from always-zero extra ancilla, so `F ≡ I` on
> the computation path) guarantees equivalence. **Active mode is demoted to experimental** and is
> NOT the pipeline default; it does not preserve the truth table (verified failing) and can break
> the zero-ancilla invariant at the phase boundary. Awaiting a conjugation-sound redesign in a
> separate deferred ticket. Affected tests were updated accordingly (ticket 0024 pin tests).

> Spec: sandwich hardening strengthening — enabling real mixing of intermediate values.
> Synthesized from grilling session outcomes. Terminology follows `docs/DEFINITION.md`.

---

## Problem Statement

The current sandwiching implementation is functionally correct but does not fulfill its stated intent ("scramble intermediate values"). It draws its random CNOT chain exclusively from dedicated zero-init ancilla wires (wires 9+). Because those wires are always 0, the entire chain evaluates as a no-op: it never touches the computation path (wires 0–8). The result is an obfuscation step that inflates gate count but provides essentially zero mixing of real intermediate values.

Operators need a sandwiching mode that genuinely perturbs intermediate wire states while preserving the pipeline's hard constraint (functional equivalence for all 32 inputs) and the ancilla invariant.

---

## Solution

Introduce an **Active** sandwiching mode alongside the existing no-op behavior (renamed **Safe**). In Active mode, the random CNOT chain's control/target wires are drawn from all currently-existing circuit wires (indices `0 .. num_wires-1`), including the adder computation lines and the working ancilla `carry_mid`.

Correctness is guaranteed mathematically, not by keeping ancillary wires untouched: a chain of CNOT gates `F = g₁ ∘ g₂ ∘ … ∘ g_N`, where each gate is self-inverse, has an exact inverse `F⁻¹ = g_N ∘ … ∘ g₂ ∘ g₁` — precisely the same (control, target) pairs executed in reverse order. The full pipeline `F⁻¹ ∘ Adder ∘ F` is therefore exactly equivalent to `Adder`. Ancilla wires may become temporarily non-zero mid-circuit (this is exactly the scrambling we want) but, because the composed transform is the identity, they return to 0 by the time the phase ends. Safety is verified at the phase boundary by the existing truth-table and final ancilla-invariant checks.

> ⚠️ **Superseded by the correction banner above.** `F⁻¹ ∘ Adder ∘ F ≡ Adder` holds only when `F`
> acts as the identity on every wire the adder reads or writes (Safe mode). It does NOT hold for a
> general Active-mode chain that touches computation lines. See ticket 0024.

Both modes attach the forward chain before the first gate and the reversed chain after the last gate, with the shared pairs unmodified. Only the wire-selection strategy differs.

---

## User Stories

1. As a developer, I want `apply_sandwich` to accept a `SandwichMode` (`Safe | Active`), so that the behavior of the two wire-selection strategies is explicit at the call site.
2. As a developer, I want Active mode to draw chain wires from all current circuit wires (`0 .. num_wires-1`), so that the computation path is genuinely scrambled.
3. As a developer, I want Active mode to ignore the `extra_ancilla` argument (even when empty or containing invalid indices) without error, so that the call site does not need to conditionally construct a wire list.
4. As a developer, I want Safe mode to continue using exactly the `extra_ancilla` list as its wire pool, and to fail observably (assert or clear runtime check) when fewer than 2 wires are provided, so that the existing no-op baseline remains available for regression comparison.
5. As a developer, I want Active mode not to call `extend_wires`, so that no extra zero wires are introduced to dilute the scrambling effect.
6. As a developer, I want the forward/reversed chain mirroring and insertion-order rules to hold in Active mode exactly as in Safe mode, so that the `F⁻¹ ∘ Adder ∘ F` equivalence is preserved.
7. As a developer, I want `PipelineConfig` to expose `sandwich_mode = SandwichMode::Safe` (default Safe since ticket 0024; Active is experimental and off by default) and `sandwich_chain_length`, so that the configuration surface for sandwching is explicit and isolated.
8. As a developer, I want the full pipeline to pass the exhaustive truth-table and final ancilla-invariant checks with Safe as the default mode, so that the real-world end-to-end path is validated.
9. As a developer, I want a defensive guard when Active mode has fewer than 2 usable wires (currently impossible for the 2-bit adder), so that future small circuits fail clearly rather than generating an invalid CNOT.
10. As a developer, I want the scramble effect to be observable in a test (via a manually-assembled forward-only circuit), without adding production debug hooks, so that Active mode is verified to actually perturb intermediate wires.

---

## Implementation Decisions

1. **`SandwichMode` enum** — add `enum class SandwichMode { Safe, Active }`, declared in the `sandwich` header (alongside `apply_sandwich`), so that the mode is co-located with its consuming function.
2. **`apply_sandwich` signature** — extend the existing in-place function with a `SandwichMode mode` parameter, keeping the existing `Circuit&` in-place modification semantics (no change to const/return semantics; consistent with `shuffle` and `inject_junk`):
   - `apply_sandwich(Circuit&, extra_ancilla, chain_length, mode, rng)`
   - The `extra_ancilla` parameter is retained: used only by Safe mode, ignored by Active mode.
3. **Wire-selection strategy** — Active mode draws control/target wires from `[0, circuit.num_wires())` with `ctrl != tgt` and does not call `extend_wires`. Safe mode keeps the existing logic: `extend_wires` to cover `extra_ancilla`, then draw from `extra_ancilla` only. Safe mode with `extra_ancilla.size() < 2` must fail observably (assert or explicit runtime check with a clear error message); this is encoded as an acceptance criterion in the implementing ticket.
4. **Chain construction & insertion** — unchanged from current behavior: build N random `(ctrl, tgt)` pairs; prepend the forward chain before gate index 0 (in forward order via reverse-index insertion); append the reversed chain (same pairs, reverse execution order) after the last gate. Pairs are never modified.
5. **Defensive guard** — Active mode asserts/fails if fewer than 2 usable wires exist (not applicable to the current 2-bit adder, which has 9 wires). This guard is reserved for future small circuits and must not be removed by implementers.
6. **`PipelineConfig`** — replace/augment the sandwich section with `SandwichMode sandwich_mode = SandwichMode::Safe;` (default Safe since ticket 0024) and `int sandwich_chain_length = 8;`. The `sandwich_ancilla` list is retained and passed through as `extra_ancilla` (used only when mode is Safe). The pipeline layer merely forwards the config values; it does not interpret the mode.
7. **CLI** — expose a sandwich-mode switch (e.g. `--sandwich-mode=active|safe`) in addition to the existing `--no-sandwich`, `--no-shuffle`, `--no-inject`. Safe is the default since ticket 0024; Active prints a warning that it does not guarantee TT equivalence. Precedence: `--no-sandwich` takes priority over the mode switch — specifying it must fully skip the sandwich phase regardless of `--sandwich-mode`, avoiding ambiguous simultaneous settings.
8. **Existing call sites** — `apply_sandwich` callers (pipeline, main, all sandwich/pipeline tests) must pass the new `mode` argument; Safe is passed where prior no-op behavior is intended, preserving a clean regression baseline.
9. **Randomness** — keep the recorded `std::mt19937` (seed default 42); Active mode consumes the same RNG stream.

---

## Testing Decisions

- **What is a good test**: tests verify external observable behavior — functional equivalence (exhaustive 32-input truth table), the ancilla invariant at the phase boundary, gate-count deltas, determinism, and chain mirroring — not internal implementation details of wire pooling. Safe mode with fewer than 2 `extra_ancilla` wires must fail observably.
- **Seam**: a single seam — the `apply_sandwich` function — which is an extension of an existing, stable, correctly-leveled seam. No new production abstraction is introduced (the wire-pool logic is thin and not worth exposing separately).
- **Modules tested / prior art**:
  - `test_sandwich.cpp` (existing) — extend with:
    - Safe-mode regression: all existing tests (gate count, extent, mirroring, determinism, truth table, ancilla invariant, stacking) must still pass with `mode=Safe`.
    - Active-mode truth table + ancilla invariant + determinism + gate-count delta (always `+2*chain_len`).
    - Active-mode scramble verification: manually assemble a forward-only circuit (prepend the chain, no reverse) and assert that the intermediate wire state differs from the plain adder for a fixed seeded input, while the full forward+reverse circuit remains equivalent.
  - `test_pipeline.cpp` (existing) — assert that the default `sandwich_mode` is `Safe` and that the end-to-end pipeline passes truth table and ancilla invariant under Safe (ticket 0024); Active is pinned as KNOWN-BROKEN (must NOT preserve TT).

---

## Out of Scope

- Weighted or otherwise constrained wire-selection strategies.
- Excluding specific wires from the Active chain (no per-wire blacklist).
- Nonlinear gadgetization or equivalence-table replacement.
- Any security / attack-resistance measurement.
- Automatic irreversible-to-reversible conversion or circuits larger than 2-bit.

---

## Further Notes

- Documentation updates (deferred to the ticket implementing this ADR, per DOCUMENTATION-LIFECYCLE). The implementing ticket's Done criteria must explicitly list the files to update, so they are not missed:
  - `DEFINITION.md` — clarify the Sandwiching entry (Safe vs Active distinction)
  - `CONTEXT.md` — Phase 2 sandwiching description
  - `README.md` — CLI (`--sandwich-mode`) and sample output
  - record this decision as ADR 0002 (this file); the implementing ticket references it.
- The MVP spec (ADR 0001) remains in force and is not superseded; this ADR extends its Sandwiching decision (#5).
- Expected overhead with `chain_length = 8` for the 15-gate adder: sandwich alone always adds exactly `2 * chain_length = 16` gates; the overall overhead ratio depends on the subsequent shuffle and junk phases, so the final ratio is not fixed at 3x. More importantly, Active mode produces real perturbation of intermediate computation wires rather than pure no-op decoration.

---

*Created: 2026-08-28*
*Source: grilling session, decisions 1–7; to-spec synthesis.*
