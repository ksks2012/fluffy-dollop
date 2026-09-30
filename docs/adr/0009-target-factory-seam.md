# ADR 0009 — Target factory seam (keyed-perm standalone; M8)

> Decision: put the multi-round mixing shell of ADR 0008 behind a **target-factory seam** so the
> same shell can be driven by either the adder (the long-standing default) or a **keyed
> permutation** `P_k` (n data lines, `extra_ancilla = 0`) as a second, symmetric standalone
> target — with a keyed-permutation truth-table oracle as the new acceptance anchor. Switching
> the target must **not** shift the RNG stream position the mixing shell runs at (bit-identical
> shell stream consumption between `--target=adder` and `--target=perm`). The adder path stays
> the default and its 13/13 truth-table regression is unchanged.

---

## Problem Statement

Today the multi-round mixing pipeline is **adder-only**: `run_full_pipeline` builds the target
circuit by hardcoded `make_nbit_adder(n, extra_ancilla)` (src/pipeline.cpp §run_full_pipeline) and
every PhaseStats/junk observation is therefore an observation *of the adder's mixing*. M7 (ADR 0008)
delivered the rounds/junk-shell against that one target. But the shell's entire value proposition is
"repeatable, sound, junk-honest mixing on *whatever* reversible circuit you hand it" — and that claim
is currently **untestable**, because there is no second target wired in. Add a **second target —
a keyed permutation `P_k` on n data lines with zero extra ancilla** — driven by the *same* shell
(zero diff in the mixing pipeline), so "the shell works on any target" becomes an observable,
testable, oracle-anchored statement instead of hand-waving.

## Solution

Introduce a **target-factory seam** at the very top of the pipeline: the circuit under mixing is
selected by a `--target=adder|perm` CLI flag, dispatched through a single factory (`make_target_circuit`)
which returns either the classic adder or a new `make_nbit_keyed_perm(n, extra_ancilla=0)`. Default
remains **adder**. `--target=perm` produces *the same shell* (same rounds loop, same final-round junk
injection on the same pool seam, same PhaseStats contract) applied to a keyed permutation of the n
data lines. The permutation is seeded from `--key=` (ADR 0007 stream) and verified against a
keyed-permutation truth-table oracle. The adder path stays bit-for-bit identical (13/13 regression
anchor is untouched); the perm path is the new acceptance story.

## User Stories

1. As a user, I want to run the mixing shell against a **keyed permutation** target in addition to
   the adder, so that I can see the shell demonstrably mix a *different* reversible circuit.
2. As a user, I want `--target=perm` to use the **same rounds loop / same final-round junk
   injection / same PhaseStats** as `--target=adder`, so that the "same shell, new target" claim is
   literally the same pipeline code (zero diff).
3. As a user, I want `--target=adder` to remain the default, so that existing invocations/regressions
   are unchanged.
4. As a user, I want the keyed permutation to accept `extra_ancilla = 0` by default, so that the
   permutation is a pure n-line keyed reordering of the data lines (ancilla pool = 0) — the honest
   degenerate case of the same factory contract the adder uses.
5. As a user, I want the permutation's junk to be measured on the **same B-pool/junk seam** as the
   adder (pipeline-injected, not factory-claimed), so that junk volume/hits are comparable across
   targets.
6. As a user, I want `make_nbit_keyed_perm` to share the exact factory signature shape with
   `make_nbit_adder` (both return `Circuit`; both take `(n, extra_ancilla)`), so the seam is a true
   target-swap rather than a second pipeline.
7. As a user, I want `--key=` to seed the permutation's key derivation (ADR 0007 stream), so that the
   permutation is repeatable/reproducible per key exactly like the adder's rounds are.
8. As a user, I want a **keyed-permutation truth-table oracle** in the tests, so that `--target=perm`
   is verified against a true P_k oracle (not just "runs without crashing").
9. As a user, I want the adder's 13/13 regression to stay green with `--target=adder`, proving the
   perm work caused zero regression on the long-standing target.
10. As a user, I want the CLI `--target` validation to reject unknown targets with a clear error and a
    usage line listing both choices, so that typos are caught, not silently ignored.
11. As a user, I want `ancilla_ok`/ancilla-invariant reporting on `--target=perm` to be the
    vacuous-but-honest `true` (0 ancilla lines → invariant holds trivially), so that perm does not
    misreport an ancilla pool it does not have.
12. As a user, I want junk volume/hits on the perm target to come from the pipeline's junk pool (the
    same inject seam as the adder), so that the "junk is shell-owned, not target-owned" story is
    observable for both targets.
13. As a developer, I want the perm target to live in its own module (`make_nbit_keyed_perm`) rather
    than being inlined into the pipeline, so that the pipeline stays target-agnostic and the seam
    stays a one-line dispatch.
14. As a developer, I want the perm factory to be the *only* new code path (pipeline/inject/PhaseStats
    untouched), so that the milestone is a small, reviewable atom.
15. As a maintainer, I want an ADR 0009 recording the target-factory seam decision (with
    perm-vs-adder table), so that future targets (e.g. reversible subtractor, NIC keyed block) follow
    the same seam instead of forking the pipeline.
16. As a maintainer, I want the M8 delivery (ticket 0035 spec, 0036 implementation, 0037 docs-close)
    documented in the same shape as M7's 0032/0033/0034 tri-block, so that milestone close is uniform.

## Implementation Decisions

- **Target-factory seam at the top of the pipeline.** `run_full_pipeline` stops hardcoding the adder.
  A factory `make_target_circuit(cfg)` dispatches on `cfg.target` (`"adder" | "perm"`, default
  `"adder"`) and returns the target `Circuit` the shell then mixes. This is the **only** seam the
  feature adds — no per-target branches inside the rounds loop, no per-target inject, no
  per-target PhaseStats.
- **New module: `make_nbit_keyed_perm`.** A factory with the *same shape* as the adder:
  `Circuit make_nbit_keyed_perm(int n, int extra_ancilla = 0)`. Default `extra_ancilla = 0` → pure
  n-line keyed permutation of the data lines (output wires = all n data lines; no factory-claimed
  ancilla pool). `extra_ancilla > 0` is wired through but is a legal, degenerate extension (the
  same license the adder already carries for its extra wires).
- **Key derivation reuses the ADR 0007 stream, but does NOT consume from the plumbing shell's RNG
  stream.** `--key=` seeds the keyed PRNG (ADR 0007) which derives the permutation `P_k` over the n
  lines. The keyed permutation is deterministic per key; `--rounds` stays orthogonal to `--key`
  exactly as in ADR 0008 (same orthogonality contract). The permutation stream is **read through its
  own `seed_seq(key_bytes, "local-mixing-keyed-perm-v1")`-shaped derivation** — the perm factory
  derives `P_k`, and the shell's rounds loop / sandwich / replace / shuffle / final-round inject
  then read **their** stream position **orthogonal** to that. In other words: switching the target
  from adder to perm must **not** shift the RNG stream position the mixing shell runs at — so
  `--target=adder` and `--target=perm` produce *bit-identical* shell stream consumption. If a future
  target later needs to consume from the *same* stream the shell runs on, that is a new ADR, not an
  amendment here.
- **Junk pool is pipeline-owned, identical for both targets.** The final-round junk injection uses
  the *same* pool seam as the adder today (the B-pool place where the pipeline injects junk so
  `junk_hits`/`junk_volume` are read). The perm adds **no** pool of its own: its junk volume comes
  from the shell's inject, so perm vs adder junk is measured on the same seam and is comparable.
  (Explicitly NOT a "perm has its own pioneered junk pool" story — see Out of Scope.)
- **Ancilla semantics for the perm.** `make_nbit_keyed_perm(n, extra_ancilla=0)` returns an n-line
  circuit with **no factory ancilla pool** (all wires are output/data wires). Therefore
  `ancilla_ok` on the perm target is **vacuously true** (0 ancilla lines → the ancilla-invariant
  holds trivially). This is the honest report, not a made-up pool: the perm genuinely has no ancilla
  to claim, and the junk it shows is the shell's junk pool.
- **Camera/PhaseStats observables unchanged in shape.** `PhaseStats` (junk_hits, junk_volume, ancilla_ok,
  round_index) keep the exact same fields/contract (ADR 0008); the perm simply drives them for a
  different target. `gate_count`/junk volume for perm = same shell inject path as adder, so the two
  targets' junk observations are the same magnitude, from the same code.
- **CLI: `--target=adder|perm`.** New flag in main.cpp (+ `--target` shown in usage); validation
  rejects unknown targets with a usage-referencing error. Default `adder` keeps all existing
  invocations/regressions byte-compatible.
- **No changes to pipeline.cpp/inject/junk phases.** `make_target_circuit` swaps the target *before*
  the shell runs; the rounds loop, sandwich/replace/shuffle, final-round inject, and PhaseStats
  aggregation are untouched (zdi = zero diff in the mixing pipeline). This is the core of the ADR:
  the shell is genuinely target-agnostic.

## Testing Decisions

- **Good test = external behavior only.** A good test asserts the observable contract: does a
  target circuit realize the oracle (adder truth table / keyed-perm truth table)? do junk_hits and
  junk_volume read the same shell-inject count for both targets? does ancilla_ok report honestly
  (adder: non-vacuous over its ancilla; perm: vacuous-true over zero ancilla)? Tests do **not** poke
  into internal round-count plumbing or assert implementation details; they assert the oracle truth
  table and the PhaseStats on the seam.
- **Modules tested:** `make_target_circuit` dispatch; `make_nbit_keyed_perm` oracle (new);
  adapter regression (`--target=adder` = adder 13/13 unchanged); perm PhaseStats (junk pool seam
  comparable to adder). Existing adder tests are the regression anchor and must keep passing
  unmodified.
- **Prior art (existing tests this mirrors):** the adder's 13/13 regression (truth-table oracle +
  ancilla-invariant checks, `tests/test_adder.cpp`-shape) and the M7 rounds oracle (`tests/test_*`
  from ticket 0033) are the templates the perm oracle replicates — same harness, new table.

## Out of Scope

- **No "perm-native junk" story.** The perm does **not** pioneer its own junk pool or claim
  self-holding ancilla; its junk volume/hits come from the shell's inject seam (same as adder).
  Any framing that "the permutation itself produces junk" is explicitly out of scope for M8.
- **No per-target branching in the pipeline.** The rounds loop, inject, and PhaseStats are untouched;
  the perm is *not* a second pipeline. If a future target needs actual per-target pipeline behavior,
  that is a new ADR, not this one.
- **No change to adder internals.** `make_nbit_adder` and the adder 13/13 tests are regression
  anchors, not modified.
- **No CLI beyond `--target`.** rounds/key/seed flags already exist (M7); this ADR only adds
  `--target`. No config-file, no multi-target-in-one-run.
- Ticket 0012 (`fix-active-sandwich-semantics`, BLOCKED-by 0024) is out of scope; the perm target
  does not touch sandwiching semantics.

## Further Notes

- The M8 story, per the grilling session's final framing: **"the same mixing shell, applied to a
  keyed permutation, with equivalent observability"** — not "the permutation rewrites the shell".
- Adder remains the strongest full-table regression anchor; the perm is the narrative anchor that
  makes "shell is target-agnostic" testable.
- Wiring/terminology follows `docs/DEFINITION.md` (rounds, junk, ancilla, B-pool, PhaseStats, ADR
  0007 keyed stream, ADR 0008 rounds) — see those docs before implementing.

---
*Merged 2026-09-22: the full ADR 0009 spec previously split across the register stub and ticket 0035 is now canonical in this single ADR register; ticket 0035 is archived as a pointer.*
*Created: 2026-09-22 · Milestone M8 · Decision: ADR 0009 (this file) · Spec: merged here from ticket 0035 · Supersedes: n/a — the adder remains the default target and its 13/13 regression is unchanged.*
