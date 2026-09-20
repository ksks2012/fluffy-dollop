# 0029 — ADR 0007 (Keyed PRNG) + config/helper spec (M6 open)

**What to build:** Ratify ADR 0007 (Keyed PRNG seeding — deterministic replay) and pin the config
surface (`PipelineConfig::key`, `make_rng(cfg)` helper) as a specifiable contract before any
implementation lands. Milestone M6 open.

**Blocked by:** (none) — project history at HEAD `8c99d36` + uncommitted M5 working tree

**Status:** implemented (2026-09-17)

- [x] ADR `docs/adr/0007-keyed-prng-seeding.md` written per grilling Q1–Q8:
      key bytes + domain `"local-mixing-v1"` → `std::seed_seq` → `std::mt19937`
  - [x] Single shared stream, phase order unchanged
        (`sandwich → replace [Sound/R1/R2 read no rng] → shuffle → inject`)
  - [x] Precedence: `--key` > `--seed` > default 42; both given ⇒ not an error (optional warning)
  - [x] Key = any non-empty string; empty `--key=` ⇒ error exit (never silently "no key")
  - [x] Key ONLY seeds the rng; pass enablement/params stay public config/CLI
  - [x] Explicit non-CSPRNG / no-security-claims scope line (engineering PRNG)
  - [x] Out-of-scope list: key-derived pass schedule (M6.1+), hex/fixed-length key,
        `random_device`, per-phase rng streams (with the one-line "re-evaluate if a future
        replace-pass rng consumer appears" note)
  - [x] ADR 0001/0002/0006 bodies untouched (references only in CONTEXT/CHANGELOG)
- [x] Spec seams pinned (for 0030): `run_full_pipeline` rng-construction point, single
      `make_rng(cfg)` helper, CLI `--key=` parse location, `tests/test_pipeline.cpp`
- [x] `PipelineConfig::key` (`std::string`, default `""`) and `make_rng` signature documented in the
      ticket so 0030 can implement without re-interpreting

> Acceptance of this ticket = ADR 0007 merged + seams listed. Implementation lands in 0030; this
> ticket intentionally carries no circuit-level behavior yet.