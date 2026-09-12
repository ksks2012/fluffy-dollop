# 0015 — CLI switches, observability, and documentation reconciliation

**What to build:** Let an operator choose the target adder size and ancilla budget from the command line, and surface per-size observability that demonstrates the 4-bit circuit exercises the mixing phases more meaningfully than 2-bit. Reconcile the project's high-level documentation so it reflects 4-bit as the primary validation circuit, 2-bit as fast regression, and the corrected Active sandwich semantics.

**Blocked by:** 03 (pipeline supports `n` and `extra_ancilla`; Active semantics corrected by 01).

**Status:** implemented (--bits / --extra-ancilla / --sandwich-mode; per-size observability; docs reconciled)

- [ ] CLI accepts `--bits=N` (e.g. `--bits=2`, `--bits=4`) alongside `--extra-ancilla=N` and `--sandwich-mode=...`, defaulting to 2-bit
- [ ] CLI rejects unsupported `--bits` values (e.g. `3`, `0`, `8`) with a clear error and a non-zero exit, rather than silently ignoring or misbehaving
- [ ] The driver prints per-phase gate count, wire count, and overhead derived from the selected adder (no hardcoded constants), for both sizes
- [ ] The driver prints depth-level count and per-level gate distribution, making the larger shuffle space of the 4-bit circuit observable
- [ ] A 2-bit vs 4-bit comparison run confirms the 4-bit circuit has observably more topological depth levels and larger per-level shuffle space
- [ ] A manual/observed run with `--bits=4` passes truth table and ancilla invariant with Active sandwich as default
- [ ] `CONTEXT.md` — update §1 non-goal "circuits larger than 2-bit" to reflect 4-bit as the primary validation circuit and 2-bit as fast regression; note the generic adder and generalized checker; update Positioning
- [ ] `DEFINITION.md` — confirm/complete the ADR 0003-related glossary (`Functional Equivalence` exhaustive cases now `2^(2n+1)`, `Ripple-Carry Adder` parameterized `n`, add a reference to the generic factory). The Active/Safe sandwich all-wires wording stays the responsibility of ticket 01; this ticket only verifies it is consistent and does not rewrite the same entry independently
- [ ] `README.md` — document CLI (`--bits=N`, `--extra-ancilla=N`) and sample output
- [ ] ADR 0001 and ADR 0002 remain in force (not superseded); the scope boundary is narrowed to "larger than 4-bit" for this step only
