# 0002 — Circuit class + 2-bit adder + evaluation

**What to build:** An adjacency-list `Circuit` class that manages gates and wires. Construct a 2-bit ripple-carry adder from reversible gates with pre-allocated zero-valued ancilla wires. Implement `Circuit::evaluate()` using the in-place model. Verify correctness via exhaustive truth-table comparison over all 32 input combinations.

**Canonical 2-bit reversible adder construction:**

Fixed wire layout — **9 wires total**:

| Wire | Role | Init |
|------|------|------|
| 0 | A1 (input) | input |
| 1 | A0 (input) | input |
| 2 | B1 (input) | input |
| 3 | B0 (input) | input |
| 4 | Cin (input) | input |
| 5 | S1 (output) | 0 |
| 6 | S0 (output) | 0 |
| 7 | Cout (output) | 0 |
| 8 | carry_mid (working ancilla) | 0, **must return to 0** |

Gate sequence — **15 gates (9 Toffoli + 6 CNOT)**:

```
// Step 1: S0 = A0 ⊕ B0 ⊕ Cin
CNOT(ctrl=1, tgt=6)          // S0 ^= A0
CNOT(ctrl=3, tgt=6)          // S0 ^= B0
CNOT(ctrl=4, tgt=6)          // S0 ^= Cin  → S0 = A0⊕B0⊕Cin ✓

// Step 2: carry_mid = majority(A0, B0, Cin)  [A0∧B0 ⊕ A0∧Cin ⊕ B0∧Cin]
Toffoli(c1=1, c2=3, tgt=8)   // carry_mid ^= A0∧B0
Toffoli(c1=1, c2=4, tgt=8)   // carry_mid ^= A0∧Cin
Toffoli(c1=3, c2=4, tgt=8)   // carry_mid ^= B0∧Cin  → carry_mid = maj(A0,B0,Cin) ✓

// Step 3: S1 = A1 ⊕ B1 ⊕ carry_mid
CNOT(ctrl=0, tgt=5)          // S1 ^= A1
CNOT(ctrl=2, tgt=5)          // S1 ^= B1
CNOT(ctrl=8, tgt=5)          // S1 ^= carry_mid  → S1 = A1⊕B1⊕carry_mid ✓

// Step 4: Cout = majority(A1, B1, carry_mid)
Toffoli(c1=0, c2=2, tgt=7)   // Cout ^= A1∧B1
Toffoli(c1=0, c2=8, tgt=7)   // Cout ^= A1∧carry_mid
Toffoli(c1=2, c2=8, tgt=7)   // Cout ^= B1∧carry_mid  → Cout = maj(A1,B1,carry_mid) ✓

// Step 5: uncompute carry_mid → restore wire 8 to 0
Toffoli(c1=3, c2=4, tgt=8)   // undo step 2c
Toffoli(c1=1, c2=4, tgt=8)   // undo step 2b
Toffoli(c1=1, c2=3, tgt=8)   // undo step 2a  → carry_mid = 0 ✓
```

Spot-checks:

| A1 A0 | B1 B0 | Cin | S1 S0 | Cout | Notes |
|-------|-------|-----|-------|------|-------|
| 0 1 | 0 1 | 0 | 1 0 | 0 | 1+1=2 ✓ |
| 1 1 | 1 1 | 1 | 1 1 | 1 | 3+3+1=7 ✓ |
| 1 0 | 0 1 | 1 | 0 0 | 1 | 2+1+1=4 ✓ |
| 0 0 | 0 0 | 0 | 0 0 | 0 | 0+0=0 ✓ |

**Blocked by:** 0001

**Status:** ready-for-agent

- [ ] `Circuit` class with adjacency-list gate storage
- [ ] Wire management with pre-allocated zero ancilla wires
- [ ] 2-bit ripple-carry adder constructed per canonical specification above
- [ ] `Circuit::evaluate(vector<int> input)` — copies input into wire state, runs gates in-place, returns output slice
- [ ] Exhaustive truth-table check: all 32 inputs produce correct S1, S0, Cout
- [ ] **Ancilla invariant check**: after evaluation, all ancilla wires must remain 0
- [ ] Gate count printed
