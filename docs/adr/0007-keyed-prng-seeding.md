# ADR 0007 — Keyed PRNG seeding (deterministic replay; M6)

> Spec: upgrade the single deterministic `seed → mt19937` demonstration into a keyed, replayable
> engineering PRNG seeding contract. Synthesized from grilling session (2026-09-15); terminology
> follows `docs/DEFINITION.md`. Builds on the existing seed handling in `run_full_pipeline`
> (`PipelineConfig::seed`, default 42). This ADR does NOT modify ADR 0002 (Active sandwich) or
> ADR 0006 (Sound) — the key stream is orthogonal to which passes run.

---

## Problem Statement

Today the pipeline is deterministic from a single `int seed` (`PipelineConfig::seed = 42`), seeding
one `std::mt19937` that is passed through every rng-consuming phase (Safe sandwich chain, shuffle,
junk injection) in phase order. That works for the toy demo, but a symmetric, keyed design needs a
distinct, user-supplied **key** that (1) fully determines the random decisions in a replayable way
(same config + same key ⇒ bit-identical circuit), and (2) changes the circuit structure when the key
changes. The key is NOT a pass-selection language — pass toggles/parameters stay public on the CLI /
`PipelineConfig`.

---

## Solution

### Key derivation

- A key is any **non-empty string** (`--key=...`). Its raw bytes feed a `std::seed_seq`, into which a
  fixed **domain separator string** is also mixed to avoid bare-hash stream collisions with other
  projects:
  ```
  seed_seq ← (key bytes …, "local-mixing-v1" bytes …)   → std::mt19937 rng
  ```
  (Implementation may feed key bytes one-by-one or packed 4-bytes-per-`uint32_t`; the ADR pins the
  *inputs* and the *algorithm*, not the packing.)
- Empty key (`--key=` with no value, or an empty string) is rejected at CLI parse time with a
  non-zero exit — never silently treated as "no key".

### Single shared stream

- The pipeline builds exactly **one** `mt19937` and passes it, by reference, through the phases in
  the existing order:
  `sandwich → replace (Sound/R1/R2 read NO rng) → shuffle → inject`.
- Identical `PipelineConfig` + identical key ⇒ identical gate sequence (bit-exact replay).
- No consumer signatures change (zero-touch contract for existing phases).

### Precedence

| Case | rng construction |
|------|------------------|
| `cfg.key` non-empty | `seed_seq` from key bytes + domain → `mt19937` |
| only `cfg.seed` set | exactly today's behavior: `mt19937(cfg.seed)` |
| neither set | default `seed = 42` (unchanged) |

- `--key` takes precedence over `--seed`; providing both is **not** an error (keeps scripts working;
  a stderr warning line is acceptable). No-`--key` runs are byte-for-byte identical to current
  behavior.

### Scope control

- The key only seeds the PRNG stream. Pass enablement and parameters (`--sound`, `--replace-r2`,
  `--no-inject`, `sandwich_chain_length`, `inject_count`, …) are public config / CLI, exactly as
  today (`seed` never decided pass toggles either).

### Config surface

| Item | Value |
|------|-------|
| `PipelineConfig::key` | `std::string key;` (default `""` = use `seed`) |
| `PipelineConfig::seed` | kept as-is (`int seed = 42`) |
| CLI | `--key=<string>`; empty value → error exit |
| Seeding helper | single `make_rng(cfg)` (or equivalent) implemented in one place, used by `run_full_pipeline` |

---

## Implementation Decisions

1. **Engineering PRNG, not a CSPRNG** — `std::mt19937` + `std::seed_seq`. No security claims; this
   is an obfuscation *validation* project (ADR 0001 scope: no attack resistance measurement). The
   forward contract is "rng flows down by reference"; swapping the algorithm/seed source later does
   not invalidate that contract.
2. **`std::string key` in `PipelineConfig`** — seed stays; precedence logic is centralized where the
   rng is built, so every caller (CLI/tests/CI) inherits it.
3. **Observation is the gate-sequence** — tests compare the final circuit's gate sequence (reusing
   the `test_pipeline_deterministic` / `test_pipeline_different_seeds` pattern) between two runs:
   same key ⇒ bit-identical; different key ⇒ at least one gate differs. Metrics (depth/count/hits)
   are NOT asserted to differ per key (they may or may not; would be flaky).
4. **rng-consuming config for the distinguishability test** — the different-key test must run with
   phases that actually consume rng (Safe sandwich + shuffle and/or inject ON), otherwise two keys
   may coincidentally give identical sequences.

---

## Scope Reconciliation

`CONTEXT.md` §1/§2 (single-sentence notes) and `README.md` `--key` flag; `CHANGELOG.md` Milestone 6.
No changes to ADR 0001 / 0002 / 0006 bodies (references only). Terminology unchanged: `seed` remains
the legacy scalar seeding path; `key` is the M6 string-derived path.

---

## Out of Scope

- CSPRNG / `std::random_device` seeding; any security or key-strength claim.
- Hex / fixed-length key encoding (accept any non-empty string; no hex parsing).
- Key-derived pass schedules / key bits selecting passes or parameters (**M6.1 and later**).
- Per-phase rng streams — M6 uses the single shared stream; if a future replace-pass rng consumer
  appears, re-evaluate per-phase derivation (`seed_seq{key, "phase-name"}`).
- Replacing `--seed`; migrating existing tests/config that set `cfg.seed`.

---

## Testing Decisions

- **Good test:** bit-exact same-key gate sequence (deterministic replay); different-key gate-sequence
  difference under an rng-consuming config; `--seed` fallback still yields today's circuits; `--key`
  precedence over `--seed`; empty `--key=` exits non-zero.
- **Seam:** `run_full_pipeline`'s rng construction point (the `make_rng` helper) + CLI parse section
  + `tests/test_pipeline.cpp`.
- **Not tested:** key strength, collision resistance, statistical randomness of key→circuit
  distribution (out of scope; engineering PRNG).
- **Expectations:** all existing tests (which use `seed` and never pass `--key`) pass unchanged.

---

## Further Notes

- `--key=42` (string bytes) is NOT the same derivation as `--seed=42` (integer) — both are
  deterministic, but distinct; the README/CHANGELOG document one example of each.
- "Different key ⇒ different structure" is the M6 *observable*; "key selects the pass schedule" is
  explicitly deferred to M6.1.

---

*Created: 2026-09-15*
*Source: grilling session Q1–Q8 (2026-09-15); key→seed_seq contract per grilling Q1/Q4/Q5; precedence
per Q3/Q7; single-stream sharing per Q5; scope control per Q2; observation per Q6; config surface per
Q7; ticket split per Q8.*