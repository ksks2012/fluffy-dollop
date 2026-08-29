# 0009 — Active mode implementation + dedicated tests

**What to build:** Implement the Active wire-selection strategy inside `apply_sandwich`, and add comprehensive Active-mode tests. Active mode is not yet wired into any production path (that happens in the next ticket), so correctness is proven in isolation via explicitly-`Active` tests; CI remains green throughout.

**Blocked by:** 0008

**Status:** ready-for-agent

- [ ] Active mode draws control/target wires from all current wires `[0, num_wires())` with `ctrl != tgt`
- [ ] Active mode does NOT call `extend_wires` and ignores the `extra_ancilla` argument (empty/invalid indices tolerated without error)
- [ ] Defensive guard: Active mode fails clearly if fewer than 2 usable wires exist (reserved for future small circuits; must not be removed)
- [ ] Active mode preserves forward/reversed chain mirroring and insertion-order rules exactly as Safe mode (`F⁻¹ ∘ Adder ∘ F` equivalence)
- [ ] Tests (explicit `mode=Active`): exhaustive truth-table, final ancilla invariant, determinism (same seed → same structure), gate-count delta always `+2*chain_len`
- [ ] Scramble verification: manually assemble a forward-only circuit and assert intermediate wire state differs from the plain adder for a fixed seeded input, while the full forward+reverse circuit remains equivalent
- [ ] Safe-mode edge case (not Active-specific): `extra_ancilla.size() < 2` must fail observably (assert or clear runtime error)
