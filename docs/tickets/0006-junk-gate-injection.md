# 0006 — Junk gate injection

**What to build:** Insert Toffoli gates on zero-valued ancilla wires. Because at least one control is always 0, the gate acts as identity on observable computation while inflating circuit volume. After injection, exhaustive truth-table check confirms functional equivalence.

**Junk gate invariant:** Every injected Toffoli gate must have **at least one control wire** that is a known-zero ancilla wire. This guarantees the gate flips no observable bits.

**Blocked by:** 0002

**Status:** ready-for-agent

- [ ] Junk gate generator: Toffoli with at least one zero-ancilla control wire
- [ ] Insert junk gates at valid positions (respect topological ordering — must not precede any gate that writes to its input wires)
- [ ] Truth-table check: all 32 inputs still produce correct output
- [ ] **Ancilla invariant check**: ancilla wires remain 0 after injection
- [ ] Gate count printed before and after
