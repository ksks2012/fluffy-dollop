# ADR 0008 — Multi-round mixing (rounds; M7)

> Spec: upgrade the single-pass local-mixing pipeline into a **multi-round** shake — the same
> functionality-preserving chain (sandwich → replace → shuffle) repeated `rounds` times, with junk
> injection applied only on the final round. Built on the keyed single-stream rng of ADR 0007.
> Synthesized from grilling session (2026-09-19); terminology follows `docs/DEFINITION.md`.
> This ADR does NOT modify ADR 0001 / 0002 / 0006 / 0007 bodies — rounds is a new orthogonal
> config axis; the pass machinery (and the keyed rng contract) is untouched and referenced only.

---

## Problem Statement

The current pipeline runs the mixing chain exactly **once**: `adder → sandwich → replace
(Sound→R1→R2) → shuffle → inject`. "One shake of the salami" leaves structural information that a
second shake would further diffuse: depth-level ordering survives one shuffle, and a single
sandwich+replace pass only partially obscures the adder's shape. The Local Mixing paradigm (ADR 0001)
is fundamentally *iterative* — local functionality-preserving transforms applied repeatedly. M7 adds
the **rounds** dimension so a user can run `R` repeated mixing shakes, deterministically
(R same ∵ key/seed + same config ⇒ bit-identical) and *without* changing any key (rounds is
orthogonal to the keyed PRNG — the key selects the rng stream, the rounds count is independent
public configuration).

---

## Solution

### Config surface

| Item | Rule |
|------|------|
| `PipelineConfig::rounds` | `int rounds = 1` — new public config field (default 1 = single pass). |
| CLI | `--rounds=N` (`N ≥ 1`; `N < 1` / unparseable ⇒ error + non-zero exit — never silent clamp). |
| CLI absent | default `rounds = 1` ⇒ **bit-exact identical** to today's single-pass run (regression anchor). |
| Orthogonality | `rounds` and `--key` / `--seed` are fully orthogonal (ADR 0007): key only seeds the rng; rounds never reads key-derived schedule; rounds never re-seeds; pass enablement/parameters stay public config (never key/round-derived). |

### Per-round schedule

- One round = the existing local-mixing chain, **with `inject` deferred to the final round**:

  ```
  for r in 0 .. rounds-1:
      sandwich (Safe) → replace (Sound→R1→R2) → shuffle
      if r == rounds-1:   inject (junk, final-round only)
  ```

- `rounds == 1` ⇒ identical to today (inject is the final/only round). `rounds >= 2` adds
  `(rounds-1)` extra (sandwich→replace→shuffle) chains, then inject once at the end.
- **Single shared rng stream** (ADR 0007): exactly ONE `mt19937` built at pipeline start
  (`make_rng(cfg)`); all rounds draw from the SAME stream reference — NEVER rebuilt per round.
  Same config + same key ⇒ bit-identical across runs. Different rounds with same key ⇒
  distinguishable (each round consumes rng for sandwich/shuffle).
- `PhaseStats::round_index` = `int round_index = 0` — new field attributing a phase to its round
  (0 = adder/first-round phases, `rounds-1` = final round incl. inject). Phase `name` values stay
  UNCHANGED (`"adder"`, `"sandwich"`, `"replace"`, `"shuffle"`, `"inject"`) — no per-round string
  mangling; attribution lives in `round_index` only.
- **Phase count formula (multi-round):** `phases.size() == 1 + rounds*3 + 1`
  (adder + R×(sandwich,replace,shuffle) + inject). E.g. `rounds=1` ⇒ 5 (today); `rounds=2` ⇒ 8,
  round_index sequence `{0,0,0,0, 1,1,1,1}` (adder + round-0's 3 phases → 0; round-1's 3 +
  inject → 1). *(Correction from grilling: this is `1+3R+1 = 3R+2`, NOT `1+4R`.)*

---

## Verification contract

### Where verification runs (test-side consumer, NOT pipeline-internal)

- **Every round boundary** is verified for truth-table equivalence AND zero-ancilla invariant
  (ADR 0001/0002 seam): the circuit after each round must still be TT-equivalent to the original
  adder and keep all non-output ancilla zero. Round-1's verify == today's post-pipeline verify
  (same exhaustive `2^(2n+1)` truth-table check; n=4 → 512 cases).
- **Test-side only (consumer):** per-round verification lives in `tests/test_pipeline.cpp`, NOT in
  `run_full_pipeline` — the pipeline remains a pure transformation (M4–M6 precedent: verification
  is a consumer concern, never the pipeline's hot path). `rounds` loop is a pure transformation
  producer; `tests` (and optionally a final `truth_table PASS` in main, as today) are the verifiers.
- For `rounds >= 2`: verify after each round boundary (round 0..rounds-1) — TT + ancilla invariant
  both PASS at every boundary; a failure identifies the exact round. `main.cpp` keeps printing the
  final truth-table result only (unchanged shape).

### Test contracts (ticket 0033)

| Case | Expectation |
|------|-------------|
| `rounds=1` | Gate sequence bit-identical to today's single-pass run (regression / zero-delta). |
| `rounds=2` | TT-equivalent + ancilla-invariant at EACH round boundary; gate count > single-pass (structure distinguishes rounds). |
| Same config + same key, rerun | Bit-identical (rounds orthogonal to replay determinism). |
| CLI `--rounds=0` / `-1` / `abc` | Error + non-zero exit (never silent clamp). |
| Different key, same config, `rounds=2` | At least one gate differs (rng-consuming config; ADR 0007 distinguishability pattern). |

### Scope

**In scope:** `rounds` public config + CLI; per-round sandwich→replace→shuffle with inject on the
final round; single shared rng stream across rounds; `PhaseStats::round_index`; per-round TT+ancilla
verification (test-side). Milestone M7.

**Out of scope (M7.1+, deferred):** key-derived round schedules (rounds count or per-round
variation chosen by the key — explicitly NOT in this ADR; key only seeds rng per ADR 0007); per-round
pass-config variation (rounds all run the same pass set; per-round pass toggles out of scope);
rounds interacting with `--sound`/`--replace-r2` beyond today's per-round semantics; hex/key-length
or other key-strength rules (unchanged, ADR 0007).

---

## References

- ADR 0001 (MVP spec) / ADR 0002 (Active sandwiching) / ADR 0006 (Sound replacement) — phase
  machinery referenced, bodies untouched.
- ADR 0007 (Keyed PRNG seeding) — single shared rng stream + `make_rng` seam carried into every
  round; rounds adds no rng consumer beyond existing phases.
- ADR 0004 (R1) / ADR 0005 (R2) — local replacement passes referenced; run per round unchanged.

---

*Created: 2026-09-19*
*Milestone: M7*
*Source: grilling session Q1–Q8 (2026-09-19); rounds-orthogonal-to-key per Q1/Q2/Q6; inject
final-round only per Q2; per-round verification test-side per Q3/Q5; phase-count correction
`3R+2` per Q4; CLI strictness per Q6.*
