# 0034 — Multi-round docs + milestone close (M7 close)

**Status:** Closed

**What to build:** Document the multi-round feature in the durable docs and close milestone M7,
mirroring the M6 close pattern (0031): README flag example, CONTEXT one-liner rows + ADR index line,
CHANGELOG Milestone 7, and the milestone summary to `docs/summary/`.

**Blocked by:** 0033 (rounds loop + CLI + tests)

**Status:** open

- [x] README.md: `--rounds=N` documented (one line + one example each) — default 1; `--rounds` is
      orthogonal to `--key`/`--seed` (rounds = how many mixing shakes; key = which rng stream);
      `--rounds=2` example shown as a distinguishable, TT-equivalent multi-shake run; CLI table /
      "Running Tests" section kept coherent
- [x] `docs/CONTEXT.md`:
      - [x] phase-2 randomization + mixing note — multi-round schedule: `rounds` wraps
            (sandwich → replace → shuffle) per round, inject runs **only** on the final round;
            single shared `mt19937` stream (ADR 0007) reused across ALL rounds — never rebuilt
            per round; rounds is orthogonal to key (key never decides rounds; rounds never re-seeds)
      - [x] decision rows 15 (ADR 0008) with §2 §P8-style one-liner; ADR index line
      - [x] `STRUCTURE.md` (if it lists run_full_pipeline phases) — add rounds loop line; phase
            names unchanged, per-round attribution via `PhaseStats::round_index`
- [x] `docs/CHANGELOG.md`: **Milestone 7** written — multi-round mixing (MVP multi-shake): `rounds`
      config/CLI (`--rounds=N`; `N<1` → error), per-round sandwich→replace→shuffle, junk injection
      final-round only, per-round truth-table + ancilla verification (test-side consumer),
      `rounds=1` bit-exact regression anchor, round/`rounds` orthogonal to key/seed (ADR 0007)
- [x] References coherent: ADR 0007/0008 bodies untouched (cross-refs only); ADR 0001/0002/0006
      bodies untouched (rounds never re-touches passes — Addresses via refs, not rewrites);
      `docs/DEFINITION.md` untouched (no new glossary term — "round" stays a domain §2 mixing
      concept described in ADR 0008 / CONTEXT, no new entry needed unless §2 already references
      multi-round; if it does, keep that line, don't expand)
- [x] Full `ctest` green (now counting 0033's new tests; all pre-existing unchanged); milestone
      summary to `docs/summary/` (`0034-multi-round-mixing.md`)

> If README already shows `--rounds` anywhere from 0033's CLI work, keep it consistent; this ticket
> is the durable-docs close only. Do not introduce per-round pass schedules or key-derived rounds
> (out of scope, ADR 0008).

---
**Status:** Closed — af89649 (docs-close 0034); upper-doc anchors verified in README/CONTEXT/CHANGELOG/ADR 0008; ctest 13/13 green.
