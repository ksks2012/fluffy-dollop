# Local Mixing (MVP)

A C++17 mini project implementing the **Local Mixing** obfuscation technique on reversible logic circuits, targeting a 2-bit reversible ripple-carry adder.

Local Mixing is an obfuscation paradigm (inspired by symmetric ciphers/hash functions rather than lattices) that repeatedly applies **functionality-preserving transformations** to small local regions of a circuit, obscuring the original computation path while keeping input-output behavior identical.

**This MVP is a validation artifact, not a benchmark.** It proves the pipeline is implementable and functionally correct on a 15-gate adder. It does not measure obfuscation strength.

---

## Pipeline

The pipeline applies four phases to the canonical 2-bit adder:

```
adder (15 gates) → sandwich → shuffle → inject junk → verification
```

| Phase | Operation | Effect |
|-------|-----------|--------|
| **Reversibility** | Build 2-bit ripple-carry adder from reversible gates (2 Toffoli + 5 CNOT in the canonical spec; 15 gates in the actual majority-based implementation) | Reversible baseline |
| **Sandwiching** | Insert random self-inverse CNOT chains before/after the critical section | Scrambles intermediate values |
| **Shuffling** | Permute gates within the same topological depth level | Alters execution order |
| **Junk Injection** | Insert no-op Toffoli gates on zero-valued ancilla wires | Inflates circuit volume |
| **Verification** | Exhaustive truth-table (32 inputs) + ancilla invariant check | Confirms functional equivalence |

### Wire layout (canonical 2-bit adder)
- Wires **0–4**: primary inputs `A1 A0 B1 B0 Cin`
- Wires **5–7**: primary outputs `S1 S0 Cout`
- Wire **8**: `carry_mid` working ancilla (returns to 0)
- Wires **9+**: extra zero-initialized ancilla for sandwiching / junk injection

**Hard constraint:** the transformed circuit must be functionally equivalent to the original for every one of the 32 possible inputs.

---

## Building

```bash
cmake -S . -B build
cmake --build build
```

Requires CMake ≥ 3.16 and a C++17 compiler.

## Running

```bash
./build/gate_driver                          # full pipeline (Active sandwich, default)
./build/gate_driver --sandwich-mode=safe     # use Safe (no-op ancilla) sandwich
./build/gate_driver --sandwich-mode=active   # explicitly request Active sandwich
./build/gate_driver --no-sandwich            # skip sandwiching entirely
./build/gate_driver --no-shuffle             # skip shuffling
./build/gate_driver --no-inject              # skip junk injection
```

`--no-sandwich` takes precedence over `--sandwich-mode`: specifying both skips the phase entirely.

Example output (seed 42, Active mode — default):

```
  adder       gates=15    wires=9    overhead=1.00x  ancilla=OK
  sandwich    gates=31    wires=9    overhead=2.07x  ancilla=OK
  shuffle     gates=31    wires=9    overhead=2.07x  ancilla=OK
  inject      gates=43    wires=16   overhead=2.87x  ancilla=OK

Final truth-table: PASS (all 32 inputs correct)
```

Active mode leaves the wire count at 9 after sandwiching (no extra ancilla allocated) because it draws chain wires from the circuit's existing input and output wires rather than zero-initialized extras.

## Running Tests

```bash
cd build
ctest
```

6 test suites, all passing:
- `gates` — Toffoli / CNOT / NOT truth tables, self-inverse, polymorphism
- `circuit` — circuit class, 2-bit adder correctness, exhaustive truth-table, ancilla invariant
- `sandwich` — CNOT-chain sandwiching preserves equivalence
- `shuffle` — depth-level shuffling preserves equivalence, determinism
- `inject_junk` — junk gate invariant, truth-table, ancilla invariant
- `pipeline` — end-to-end full-pipeline verification

---

## Project Layout

```
include/   gate.h, circuit.hpp, adder.hpp, sandwich.hpp, shuffle.hpp,
           inject_junk.hpp, pipeline.hpp
src/       main.cpp, circuit.cpp, adder.cpp, sandwich.cpp, shuffle.cpp,
           inject_junk.cpp, pipeline.cpp
tests/     test_gates.cpp, test_circuit.cpp, test_sandwich.cpp,
           test_shuffle.cpp, test_inject_junk.cpp, test_pipeline.cpp
docs/      CONTEXT.md, DEFINITION.md, DOCUMENTATION-LIFECYCLE.md,
           adr/, tickets/, summary/
```

---

## Documentation

- **`docs/CONTEXT.md`** — single source of truth: scope, pipeline, architectural decisions, coding standards
- **`docs/DEFINITION.md`** — domain glossary (Local Mixing, Reversible Circuit, Ancilla Wire, Sandwiching, Shuffling, Junk Gate, Overhead Ratio, …)
- **`docs/DOCUMENTATION-LIFECYCLE.md`** — documentation management conventions
- **`docs/adr/0001-local-mixing-mvp-spec.md`** — MVP specification (user stories, implementation/testing decisions, out of scope)
- **`docs/tickets/`** — vertical-slice tickets 0001–0007
- **`docs/summary/`** — execution summaries per milestone

## Out of Scope (post-MVP)

- Full generation mixing (equivalence-table sub-circuit replacement)
- Nonlinear gadgetization
- Security / attack resistance evaluation
- Automatic irreversible-to-reversible conversion
- Circuits larger than 2-bit
