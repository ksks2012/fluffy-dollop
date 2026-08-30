# 0013 — Generic `make_nbit_adder(n)` factory + generalized verification

**What to build:** Introduce a single generic adder factory `make_nbit_adder(n, extra_ancilla)` that builds a reversible ripple-carry adder for any supported `n` via a per-bit loop (three CNOT gates for the sum, three Toffoli gates for the majority carry, and per-bit uncompute), while keeping `make_2bit_adder()` as a thin wrapper so the many existing call sites keep working unchanged. Generalize `check_truth_table` and `check_ancilla_invariant` so a single verification path validates any supported size by deriving `n` from the circuit's input count and reading outputs by `output_wires()` order. Preserve the 2-bit behavior as the regression baseline and stand up unit coverage for the new adder.

Equivalence here means functional equivalence (identical I/O truth table) plus the ancilla invariant (all non-output wires return to 0) — it does not promise identical wire count or gate-for-gate structure with the old 2-bit construction (the generic design uses ≤ 2 working wires).

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [ ] `make_nbit_adder(n, extra_ancilla)` produces the canonical wire layout contract: inputs `[0..n-1] = A_{n-1}..A_0`, `[n..2n-1] = B_{n-1}..B_0`, `[2n] = Cin`; outputs `output_wires() = [S_{n-1}..S_0, Cout]`
- [ ] Wire count is `3n + 4 + extra_ancilla` (inputs `2n+1`, outputs `n+1`, working ancilla `≤ 2`, plus extra); extra_ancilla defaults to 4
- [ ] The adder uses a main carry plus at most one staging working wire (total working ancilla ≤ 2), with all working ancilla uncomputed back to 0 at the end
- [ ] The per-bit gate sequence (how Cin seeds the initial carry, when Cout is written, how the staging wire hands the carry between main and staging) is captured next to the adder's source as a canonical comment, and is covered by an explicit spot-check in the adder test suite (the source comment + spot-check, not this ticket, are the authoritative canonical reference)
- [ ] Every test that asserts `num_wires() == 9` or `gate_count() == 15` (or any other hardcoded 2-bit wire/gate value) is updated to derive the expected value from the wire-contract formula (`3n+4+extra_ancilla`) or from the factory's returned circuit, so no stale literal remains
- [ ] `make_nbit_adder(4, ...)` produces a 4-bit ripple-carry adder whose outputs match `A + B + Cin` for every input
- [ ] The factory supports `n = 2` and `n = 4` and fails observably (clear assert/error) on unsupported sizes
- [ ] `make_2bit_adder()` is retained as a thin wrapper delegating to `make_nbit_adder(2, ...)`, and all existing call sites keep compiling/running unchanged
- [ ] `check_truth_table(c)` derives `n = (num_inputs - 1) / 2`, enumerates all `2^(2n+1)` inputs, computes `A + B + Cin` in a loop, and compares against `output_wires()` order
- [ ] `check_ancilla_invariant(c)` treats every non-primary-output wire as ancilla that must return to 0
- [ ] The 2-bit-specific checker copies are removed (no long-lived dual track); before removal, the generalized checker is run at `n = 2` and diffed against the old 32-case behavior to confirm equivalence
- [ ] Tests that reference the old hardcoded 2-bit gate count (15) or old wire indices are updated to derive expected counts from the adder rather than literal constants
- [ ] A new adder test suite (for `n = 2` and `n = 4`) asserts the wire-layout contract, input/`output_wires()` ordering, exhaustive truth-table correctness over all inputs, and that all working ancilla return to 0
- [ ] All existing 2-bit tests remain green after the migration
