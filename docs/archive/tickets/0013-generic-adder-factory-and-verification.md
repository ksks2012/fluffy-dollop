# 0013 — Generic `make_nbit_adder(n)` factory + generalized verification

**What to build:** Introduce a single generic adder factory `make_nbit_adder(n, extra_ancilla)` that builds a reversible **VBE-style ripple-carry** adder for any supported `n` in three phases:
1. **Forward carry chain** — for each bit `i = 0..n-1`, compute `carry_{i+1} = maj(A_i, B_i, C_i)` (three Toffoli gates) into an intermediate carry wire, where `C_0 = Cin` and `C_i = carry_i` for `i ≥ 1`; the final `carry_n = Cout` is written to the output wire, so `n-1` distinct carry wires `carry_1..carry_{n-1}` are allocated.
2. **Sums** — for each bit `i = 0..n-1`, compute `S_i = A_i ^ B_i ^ C_i` (three CNOT gates) into the `S_i` output wire.
3. **Reverse uncompute** — re-emit each `maj` in reverse creation order to restore every intermediate carry wire to 0 (guaranteeing the zero-ancilla invariant).
Keep `make_2bit_adder()` as a thin wrapper so the many existing call sites keep working unchanged. Generalize `check_truth_table` and `check_ancilla_invariant` so a single verification path validates any supported size by deriving `n` from the circuit's input count and reading outputs by `output_wires()` order. Preserve the 2-bit behavior as the regression baseline and stand up unit coverage for the new adder.

Equivalence here means functional equivalence (identical I/O truth table) plus the ancilla invariant (all non-output wires return to 0) — it deliberately does **not** promise gate-for-gate structural identity with the old 2-bit construction. For `n = 2` the only structural guarantees are the **9-wire** base count (`4·2+1`) and the deterministic gate count `9n-3 = 15`; the emitted gate order may differ from the old hardcoded order (still a valid topological order), and tests must assert only functional equivalence, the ancilla invariant, 9 wires, and the derived `9n-3` count — never the old order.

**Blocked by:** None — can start immediately.

**Status:** implemented (make_nbit_adder(n, extra_ancilla) + generalized check_truth_table/check_ancilla_invariant; 2-bit + 4-bit covered)

- [ ] `make_nbit_adder(n, extra_ancilla)` produces the canonical wire layout contract: inputs `[0..n-1] = A_{n-1}..A_0`, `[n..2n-1] = B_{n-1}..B_0`, `[2n] = Cin`; outputs `output_wires() = [S_{n-1}..S_0, Cout]`
- [ ] Wire count is `4n + 1 + t + extra_ancilla` (inputs `2n+1`, outputs `n+1`, working `(n-1)+t` intermediate carries + optional temp, plus extra); extra_ancilla defaults to 4. For `n=2`, base is 9 wires, matching the existing 2-bit adder
- [ ] The adder uses one intermediate carry wire per bit (`n-1` total, plus at most one optional temp `t∈{0,1}`), computes the sums and the carry-out, then reverse-uncomputes all intermediate carry wires to 0 (VBE-style, matching the existing 2-bit paradigm); no size-independent working budget is assumed
- [ ] The gate sequence (how `Cin` seeds `C_0`, the per-bit majority chain into the intermediate carry wires, when `Cout` is written, and the reverse-order uncompute) is captured next to the adder's source as a canonical comment, and is covered by an explicit spot-check in the adder test suite (the source comment + spot-check, not this ticket, are the authoritative canonical reference)
- [ ] Every test that asserts `num_wires() == 9` or `gate_count() == 15` (or any other hardcoded 2-bit wire/gate value) is updated to derive the expected value from the wire-contract formula (`4n+1+t+extra_ancilla`) or from the factory's returned circuit, so no stale literal remains
- [ ] `make_nbit_adder(4, ...)` produces a 4-bit ripple-carry adder whose outputs match `A + B + Cin` for every input
- [ ] The factory supports `n = 2` and `n = 4` and fails observably (clear assert/error) on unsupported sizes
- [ ] `make_2bit_adder()` is retained as a thin wrapper delegating to `make_nbit_adder(2, ...)`, and all existing call sites keep compiling/running unchanged
- [ ] `check_truth_table(c)` derives `n = (num_inputs - 1) / 2`, enumerates all `2^(2n+1)` inputs, computes `A + B + Cin` in a loop, and compares against `output_wires()` order
- [ ] `check_ancilla_invariant(c)` treats every non-primary-output wire as ancilla that must return to 0
- [ ] The 2-bit-specific checker copies are removed (no long-lived dual track); before removal, the generalized checker is run at `n = 2` and diffed against the old 32-case behavior to confirm equivalence
- [ ] Tests that reference the old hardcoded 2-bit gate count (15) or old wire indices are updated to derive expected counts from the adder rather than literal constants
- [ ] A new adder test suite (for `n = 2` and `n = 4`) asserts the wire-layout contract, input/`output_wires()` ordering, exhaustive truth-table correctness over all inputs, and that all working ancilla return to 0
- [ ] All existing 2-bit tests remain green after the migration
