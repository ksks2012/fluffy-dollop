# 0004 — Sandwiching (CNOT chain)

**What to build:** Random CNOT chain insertion before and after a critical section. Chains are self-inverse (reverse order restores original values). After sandwiching, exhaustive truth-table check confirms functional equivalence.

**Critical section definition:** For the 2-bit adder MVP, the critical section is the **entire adder circuit** — all gates between the primary inputs and primary outputs. This is the simplest and most robust choice for a small circuit.

**Fixed wire budget for sandwiching:**
- Wires 0–8: adder (5 inputs + 3 outputs + 1 working ancilla `carry_mid`)
- Wires 9–9+K−1: K extra ancilla for the CNOT chain (all zero-initialized, never touched by the adder)

**Sandwiching operation — pseudocode:**

```
// extra_ancilla = [9, 10, ..., 9+K-1], all init to 0

// 1. Generate forward chain of N CNOT gates
chain = []
for i in 0..N-1:
    ctrl = random_choice(extra_ancilla)
    tgt  = random_choice(extra_ancilla \ {ctrl})
    chain.append((ctrl, tgt))

// 2. Prepend forward chain before gate index 0
for i in 0..N-1:
    circuit.insert(pos=0, CNOT(chain[i].ctrl, chain[i].tgt))

// 3. Append reversed chain after last gate
for i in N-1 downto 0:
    circuit.append(CNOT(chain[i].ctrl, chain[i].tgt))
```

Why equivalence is guaranteed:
- All extra ancilla wires start at 0, so every `CNOT(ctrl=0, tgt)` in the forward chain is a no-op.
- The reversed chain (same gates, reversed order) is therefore also a no-op.
- Wires 0–8 (adder computation) are never touched by the chain → truth table unchanged.
- Extra ancilla wires: `0 → forward chain → 0 → adder (ignored) → 0 → reversed chain → 0` ✓

**Blocked by:** 0002

**Status:** done

- [x] Random CNOT chain generator (random wire pairs from available wires)
- [x] Insert forward chain before first gate of critical section
- [x] Insert reversed chain after last gate of critical section
- [x] Truth-table check: all 32 inputs still produce correct output
- [x] **Ancilla invariant check**: ancilla wires remain 0 after sandwiching
- [x] Gate count printed before and after
