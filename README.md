# Local Mixing (MVP)

A C++17 mini project implementing the **Local Mixing** obfuscation technique on reversible logic circuits, supporting n-bit ripple-carry adders (`n ∈ {2, 4}`).

Local Mixing is an obfuscation paradigm (inspired by symmetric ciphers/hash functions rather than lattices) that repeatedly applies **functionality-preserving transformations** to small local regions of a circuit, obscuring the original computation path while keeping input-output behavior identical.

**This MVP is a validation artifact, not a benchmark.** The **4-bit adder** (33 gates, 512-input truth table) is the primary validation circuit demonstrating the pipeline at a meaningful scale; the **2-bit adder** (15 gates, 32-input truth table) serves as the fast regression baseline.

---

## Pipeline

The pipeline applies four phases to an n-bit reversible ripple-carry adder:

```
adder (9n-3 gates) → sandwich → local replace (Sound→R1→R2) → shuffle → inject junk → verification
```

| Phase | Operation | Effect |
|-------|-----------|--------|
| **Reversibility** | Build n-bit adder via `make_nbit_adder(n)` (VBE-style: forward carry → sums → reverse uncompute; `9n-3` gates) | Reversible baseline |
| **Sandwiching** | Insert random self-inverse CNOT chains before/after the circuit | Scrambles intermediate values. Safe (default): extra ancilla only, equivalence guaranteed. Active (experimental): all wires, does NOT guarantee TT (ticket 0024) |
| **Local Replacement** | Rewrite matched CNOT-triple / X-Tof-X windows (Sound→R1→R2) | Adds Toffoli/CNOT volume; Sound `--sound` yields `hits=S[+R1][+R2]` |
| **Shuffling** | Permute gates within the same topological depth level | Alters execution order; larger for n=4 |
| **Junk Injection** | Insert no-op Toffoli gates on zero-valued ancilla wires | Inflates circuit volume |
| **Verification** | Exhaustive truth-table (`2^(2n+1)` inputs) + ancilla invariant | Confirms functional equivalence |

### Wire layout contract (`make_nbit_adder(n, extra_ancilla)`)
- Wires **[0..n-1]**: primary inputs `A_{n-1}..A_0` (MSB first)
- Wires **[n..2n-1]**: primary inputs `B_{n-1}..B_0` (MSB first)
- Wire **[2n]**: `Cin`
- Wires **[2n+1..3n]**: primary outputs `S_{n-1}..S_0` (MSB first)
- Wire **[3n+1]**: `Cout`
- Wires **[3n+2..4n]**: `carry_1..carry_{n-1}` working ancilla (reverse-uncomputed to 0)
- Wires **[4n+1..]**: extra zero-initialized ancilla for sandwiching / junk injection

For n=2: 9 total wires. For n=4: 17+`extra_ancilla` wires.

**Hard constraint:** the transformed circuit must be functionally equivalent to the original for every one of the `2^(2n+1)` possible inputs.

---

## Building

```bash
cmake -S . -B build
cmake --build build
```

Requires CMake ≥ 3.16 and a C++17 compiler.

## Running

```bash
./build/gate_driver                          # 2-bit, Safe sandwich (default)
./build/gate_driver --bits=4                 # 4-bit adder (primary validation circuit)
./build/gate_driver --bits=2 --replace-r2    # 2-bit, enable R2 replacement (needs extra ancilla)
./build/gate_driver --bits=2 --sound          # 2-bit, enable Sound T-c pass (needs extra ancilla; scrambles intermediate z mid-window, restored)
./build/gate_driver --bits=4 --sound --replace-r2   # 4-bit, Sound + R2 combined
./build/gate_driver --extra-ancilla=8        # wider ancilla pool
./build/gate_driver --sandwich-mode=active   # experimental Active (warning; NOT TT-safe)
./build/gate_driver --no-replace             # disable local replacement (Sound+R1+R2)
./build/gate_driver --no-sandwich            # skip sandwiching
./build/gate_driver --no-shuffle             # skip shuffling
./build/gate_driver --no-inject              # skip junk injection
./build/gate_driver --verbose-metrics        # print per-depth gate histogram of final circuit
./build/gate_driver --seed=7                 # set the legacy scalar seed (default 42)
./build/gate_driver --key="project-foo"      # keyed PRNG: deterministic, replayable (ADR 0007)
./build/gate_driver --rounds=2              # multi-shake mixing: 2 full (sandwich -> replace -> shuffle) passes (ADR 0008); junk inject final round only, additive
```

`--no-sandwich` takes precedence over `--sandwich-mode`.  
The **default sandwich mode is `safe`** (equivalence-guaranteed). Requesting `--sandwich-mode=active` prints a
warning: Active does NOT guarantee truth-table / ancilla equivalence (known defect, ticket 0024; ADR 0002 amend).
Unsupported `--bits` values (e.g. 3, 0, 8) print an error and exit non-zero.

**Keyed seeding (ADR 0007).** One `std::mt19937` per run is seeded from the CLI in this order:

1. `--key=<string>` — non-empty string bytes + domain `"local-mixing-v1"` → `std::seed_seq` → stream. Same key + same config ⇒ **bit-identical** circuit; different key ⇒ different random choices. Requirement-seeding applies to all rng phases (sandwich chain, shuffle, junk).
2. `--seed=<int>` — legacy scalar `mt19937(seed)` (default 42).

`--key` **overrides** `--seed` when both are given (stderr warning; not an error). An empty / whitespace-only
`--key=` is an error (non-zero exit). Note `--key=42` (string) is a **different** derivation than `--seed=42`
(int); both are deterministic. This is an engineering PRNG — no security claims (see ADR 0007; key does not
select which phases run).

Example output — **4-bit, Safe sandwich** (seed 42):

```
Seed: 42  bits=4  extra-ancilla=4
Phases enabled: sandwich shuffle inject
Sandwich mode: safe

Base adder depth analysis (33 gates, max depth 16):
  depth  1 :   8 gate(s)
  depth  2 :   5 gate(s)
  depth  3 :   2 gate(s)
  depth  4 :   2 gate(s)
  ...
  depth 16 :   1 gate(s)

  Phase         Gates   Wires   Overhead   Ancilla
  ------------  ------  ------  ---------  -------
  adder       gates=33    wires=21   overhead=1.00x  ancilla=OK  depth=16 toffoli=21 cnot=12
  sandwich    gates=49    wires=21   overhead=1.48x  ancilla=OK  depth=16 toffoli=21 cnot=28
  replace     gates=57    wires=21   overhead=1.73x  ancilla=OK  hits=4    depth=16 toffoli=29 cnot=28
  shuffle     gates=57    wires=21   overhead=1.73x  ancilla=OK  depth=16 toffoli=29 cnot=28
  inject      gates=69    wires=24   overhead=2.09x  ancilla=OK  depth=16 toffoli=41 cnot=28

Final truth-table: PASS (all 512 inputs checked)
```

Compare with **2-bit** (8 depth levels, 32 inputs) to observe the larger shuffle space and input coverage of the 4-bit circuit.

## Running Tests

```bash
cd build
ctest
```

8 unit test suites plus 3 CLI-level checks (`cli_key_empty_errors` expects the non-zero exit for `--key=`), all passing:
- `gates` — Toffoli / CNOT / NOT truth tables, self-inverse, polymorphism
- `circuit` — circuit class, 2-bit adder correctness, exhaustive truth-table, ancilla invariant
- `sandwich` — CNOT-chain sandwiching, Safe and Active modes, wire-pool scaling
- `shuffle` — depth-level shuffling preserves equivalence, determinism
- `inject_junk` — junk gate invariant, truth-table, ancilla invariant
- `pipeline` — end-to-end 2-bit and 4-bit pipeline verification
- `adder` — n-bit adder wire-contract, gate-sequence spot-check, truth-table (n=2, n=4)
- `local_replace` — Sound (T-c), R1 (S-triple) and R2 (carry-majority) local rewrites, structural metrics

---

## Project Layout

```
include/   gate.h, circuit.hpp, adder.hpp, sandwich.hpp, shuffle.hpp,
           inject_junk.hpp, pipeline.hpp, local_replace.hpp
src/       main.cpp, circuit.cpp, adder.cpp, sandwich.cpp, shuffle.cpp,
           inject_junk.cpp, pipeline.cpp, local_replace.cpp
tests/     test_gates.cpp, test_circuit.cpp, test_sandwich.cpp,
           test_shuffle.cpp, test_inject_junk.cpp, test_pipeline.cpp,
           test_local_replace.cpp
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
- Circuits larger than 4-bit
