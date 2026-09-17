# 0031 — Keyed PRNG docs + milestone close (M6 close)

**What to build:** Document the keyed seeding feature in the durable docs and close milestone M6,
mirroring the M5 close pattern (CONTEXT one-liners, README flag example, CHANGELOG Milestone 6).

**Blocked by:** 0030 (make_rng + CLI + tests)

**Status:** ready

- [ ] README.md: `--key=<string>` flag documented (one line + one example), noting
      `--key=42` (string) ≠ `--seed=42` (int) and that no-`--key` preserves `--seed`/default-42
      behavior; precedence `--key` > `--seed` > 42
- [ ] `docs/CONTEXT.md`: phase-1/phase-2 randomness note updated — pipeline builds ONE
      `mt19937` from `--key`/`--seed`/42 (precedence), key-only-seeds-rng (pass toggles stay public),
      sound/R1/R2 read no rng (already stated), domain `"local-mixing-v1"`; ADR table row for 0007
- [ ] `docs/CHANGELOG.md`: Milestone 6 written (keyed deterministic replay; same-key bit-exact,
      different-key distinguishable; non-CSPRNG caveat; out-of-scope M6.1 key-derived schedules)
- [ ] References coherent: ADR 0001/0002/0006 bodies untouched; ADR 0007 titled and cross-referenced
      where relevant; `docs/DEFINITION.md` untouched unless a new term is needed (decide: no new
      glossary term — "key" stays a plain configuration field)
- [ ] Full `ctest` green; milestone summary to `docs/summary/`

> If README already shows `--seed` anywhere, keep it; `--key` is additive. Do not introduce hex/key
> length rules or security claims — engineering PRNG scope only (ADR 0007).