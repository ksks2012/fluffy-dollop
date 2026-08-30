# 0012 — Fix Active sandwich semantics to ADR 0002 (all-wires sampling)

**What to build:** Make `apply_sandwich`'s Active mode draw each `(ctrl, tgt)` CNOT pair from the full set of the circuit's wires `[0, num_wires())` with `ctrl != tgt`, instead of the current restricted scheme (control wires limited to primary inputs, target wires limited to primary outputs). Update the code comments and the glossary entry to match ADR 0002, and verify Active scrambling actually perturbs intermediate values on a fixed seeded input while preserving functional equivalence and the ancilla invariant.

Note: the source of truth for Active semantics is ADR 0002 (all wires). The current implementation, its header comment, and the glossary entry that say "control = inputs / target = outputs" are a deviation and must be corrected — not the ADR.

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [ ] Active mode builds its control/target pools from all current circuit wires `[0, num_wires())`, enforcing `ctrl != tgt`, and does not call `extend_wires`
- [ ] Active mode no longer restricts control wires to primary inputs or target wires to primary outputs
- [ ] The scramble effect is verified by a test: a manually-assembled forward-only chain changes at least one intermediate wire value on a fixed seeded input versus the plain adder
- [ ] Full forward+reverse Active chain preserves the exhaustive 2-bit truth table
- [ ] All non-output wires return to 0 at the phase boundary (ancilla invariant) under Active mode
- [ ] Gate-count delta of Active sandwich is always `+2 * chain_length`
- [ ] (Optional hardening) Active's wire pool scales with `num_wires()`: e.g. after manually extending the circuit wires, Active still samples from the full current wire set — a guard against re-hardcoding a 2-bit wire count
- [ ] Safe-mode regression is unchanged: Safe still uses exactly the passed extra-ancilla pool and no-op behavior, confirming the all-wires change touches Active only
- [ ] The header comment describing Active mode is updated to "all wires", matching ADR 0002
- [ ] The glossary `Sandwiching` / `Slice Gate` Active-mode description is corrected to "all wires" (was control=input/target=output)
- [ ] All existing 2-bit tests remain green after the change
