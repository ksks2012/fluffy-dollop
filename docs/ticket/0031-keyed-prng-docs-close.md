# 0031 — Keyed PRNG docs + milestone close (M6 close)

**What to build:** Document the keyed seeding feature in the durable docs and close milestone M6,
mirroring the M5 close pattern (CONTEXT one-liners, README flag example, CHANGELOG Milestone 6).

**Blocked by:** 0030 (make_rng + CLI + tests)

**Status:** implemented (2026-09-17)

- [x] README.md: `--key=<string>` / `--seed=<int>` flags documented (one line + one example each),
      noting `--key=42` (string) ≠ `--seed=42` (int) and that no-`--key` preserves `--seed`/default-42
      behavior; precedence `--key` > `--seed` > 42; engineering-PRNG caveat; CLIs in "Running Tests"
- [x] `docs/CONTEXT.md`: phase-2 randomness note — pipeline builds ONE `mt19937` from
      `--key`/`--seed`/42 (precedence), key-only-seeds-rng (pass toggles stay public), Sound/R1/R2 read
      no rng, domain `"local-mixing-v1"`; decision rows 11 + 14 (ADR 0007); ADR index line
- [x] `docs/CHANGELOG.md`: Milestone 6 written (keyed deterministic replay; same-key bit-exact,
      different-key distinguishable; non-CSPRNG caveat; out-of-scope M6.1 key-derived schedules)
- [x] References coherent: ADR 0001/0002/0006 bodies untouched; ADR 0007 titled and cross-referenced;
      `docs/DEFINITION.md` untouched (no new glossary term — "key" stays a plain configuration field)
- [x] Full `ctest` green (11/11); milestone summary to `docs/summary/` (`0031-keyed-prng-seeding.md`)

> If README already shows `--seed` anywhere, keep it; `--key` is additive. Do not introduce hex/key
> length rules or security claims — engineering PRNG scope only (ADR 0007).