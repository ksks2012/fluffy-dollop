# Glossary: Local Mixing Project

Domain-specific terminology used in this project.
Aligned with the Local Mixing obfuscation approach described by Vitalik Buterin (2026).

---

## Core Concepts

### Local Mixing
A circuit obfuscation paradigm that repeatedly applies **functionality-preserving transformations to small local regions** of a circuit.  
Each transformation slightly confuses and diffuses structural information. After sufficiently many rounds, the original computation path becomes hard to recover by inspection or simple algebraic analysis, while the input-output behavior remains identical.

Inspired by the design tradition of symmetric cryptography and hash functions (deliberate chaos rather than structured mathematical hardness). It does not rely on elliptic curves, factoring, or lattices.

In the full research vision, the strongest form of mixing replaces small sub-circuits with randomly chosen functionally equivalent alternatives (often precomputed in large tables).  
This MVP implements a simplified subset: sandwiching, depth-level shuffling, and zero-ancilla junk injection.

### Reversible Circuit
A logic circuit composed exclusively of invertible gates, so that every output pattern corresponds to exactly one input pattern.  
Reversibility is a prerequisite: irreversible gates destroy entropy and make secure mixing far harder.

### Obfuscation
The process of transforming a circuit so that its internal structure is difficult to understand or reverse-engineer, while preserving exact functional equivalence with the original circuit.

### Functional Equivalence
Two circuits produce identical outputs for every possible input combination.  
In this MVP it is verified by exhaustive enumeration over all `2^(2n+1)` inputs for an n-bit adder (32 for n=2; 512 for n=4), using the generalized `check_truth_table` helper that derives `n` from `circuit.num_inputs()`.

---

## Gates

### CNOT (Controlled-NOT)
A 2-wire reversible gate. The target bit is XORed with the control bit.  
Self-inverse: applying the same CNOT twice restores the original values.

### Toffoli Gate (CCNOT)
A 3-wire reversible gate (two controls, one target). The target is flipped if and only if both controls are 1.  
Universal for classical reversible computation when combined with NOT / CNOT.

### NOT Gate
A single-wire reversible gate that flips its input. Self-inverse.

### Slice Gate
A gate (or short chain of gates) inserted during the **sandwiching** step to scramble intermediate wire values.  
In this MVP, slice gates are random CNOT chains.  
The chain's wire-selection strategy is governed by `SandwichMode`:

- **Safe mode** — control/target wires are drawn exclusively from zero-initialized ancilla wires (`extra_ancilla`).  Every chain gate evaluates as a no-op, so functional equivalence is trivially preserved.  Used for isolated testing of later phases (shuffle, junk injection) where the sandwich must not alter observable wire values.
- **Active mode** *(default)* — control and target wires are drawn from **all** current circuit wires `[0, num_wires())` with `ctrl ≠ tgt` (see ADR 0002: `docs/adr/0002-active-sandwiching-mode.md`).  The chain never calls `extend_wires`.  Intermediate wire values are genuinely scrambled during execution; the ancilla invariant is preserved at the phase boundary by the `F⁻¹ ∘ Adder ∘ F` cancellation structure.

### Junk Gate
A redundant gate that does not change the observable input-output behavior of the circuit.  
Its purpose is to inflate volume and further dilute structural signals.  
In this MVP, junk gates are Toffoli gates whose controls include at least one wire that is known to be zero (ancilla).

### Gadget
A functionally equivalent but structurally more complex replacement for a single gate or small block of gates.  
Used in the gadgetization phase to break linear and value correlations between the original and transformed circuits.  
(Full nonlinear gadgets can expand a single gate by tens of thousands of gates; this MVP defers advanced gadgetization.)

---

## Circuit Structure

### Ancilla Wire
An auxiliary wire initialized to a known constant (normally 0) that is not part of the original primary inputs or outputs.  
Required for reversibility and for safe insertion of junk gates. Maintaining the “zero-ancilla” invariant is critical.

### Topological Depth Level
The depth of a gate is one plus the maximum depth of any of its immediate predecessors (inputs have depth 0).  
Gates that share the same depth level have no data dependence on each other and may be freely reordered.

### Local Region
A small contiguous or topologically limited portion of the circuit that is the target of a single mixing transformation.  
The “local” in Local Mixing refers to operating on these limited regions rather than performing a single global rewrite.

### Ripple-Carry Adder
A binary adder in which the carry-out of each bit position becomes the carry-in of the next.  
This MVP uses a generic reversible n-bit ripple-carry adder (VBE-style: forward majority-carry chain → XOR sums → reverse-order uncompute) built by `make_nbit_adder(n, extra_ancilla)`.  
Supported sizes: `n = 2` (15 gates, 9 wires) and `n = 4` (33 gates, 17 wires); formula: `9n-3` gates, `4n+1` base wires.  
The 4-bit instance is the primary validation circuit; the 2-bit instance is the fast regression baseline.

---

## Pipeline Phases

### Reversibility
Conversion (or direct construction) of the target circuit into an equivalent reversible circuit using only invertible gates (Toffoli, CNOT, NOT) and ancilla wires.

### Hardening
Techniques that protect the reversible circuit against two weaknesses:  
(1) leakage through ancilla wires that must stay zero, and  
(2) the fact that a reversible circuit can be run backwards.  
In this MVP the primary hardening method is **sandwiching** (insertion of random CNOT chains before and after critical sections).

### Gadgetization
Replacement of ordinary gates or wires by more complex gadgets so that the original wire values are never directly instantiated.  
This step is intended to eliminate the strong correlations (visible in heat-maps) that remain after naïve mixing.  
Deferred or heavily simplified in the current MVP.

### Mixing
The core iterative process of Local Mixing: repeatedly select a local region and apply a functionality-preserving transformation that destroys some structural information.  
Concrete operations used in this MVP include:  
- depth-level shuffling,  
- junk-gate injection,  
- sandwich CNOT chains.  

In the full research literature the most powerful mixing step replaces a small sub-circuit with a different but equivalent sub-circuit drawn from a large precomputed table.

### Sandwiching
A hardening technique that inserts reversible scrambling chains (slice gates) immediately before and after a target section.  
Because the chains are self-inverse when applied in reverse order, the net functional effect on the section is identity, while intermediate values are obscured.

### Shuffling
Reordering of gates that share the same topological depth level.  
Because such gates are data-independent, any permutation preserves functionality.

### Junk Gate Injection
Insertion of identity-preserving (or zero-preserving) gates for the sole purpose of increasing circuit size and diluting recognizable patterns.

---

## Metrics

### Overhead Ratio
`transformed_gate_count / original_gate_count`.  
Primary cost metric tracked after each pipeline phase.

---

*Last updated: 2026-08-26*  
*Source alignment: Vitalik Buterin, “Obfuscation (Part 3): Local Mixing”, 2026-08-21.*