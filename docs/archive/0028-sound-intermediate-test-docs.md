# 0028 — Sound intermediate-state test + docs (M5 close)

**What to build:** Prove the Sound pass actually scrambles intermediate wire values soundly (the
agenda legacy Active failed): a test that demonstrates `z != 0` mid-window for a reproducible input
while the completed rewrite leaves `z == 0` and the whole circuit stays equivalent — then record the
new phase in CONTEXT/CHANGELOG/README and close milestone M5.

**Blocked by:** 0027 (Sound pipeline / CLI wiring)

**Status:** implemented (2026-09-15)

- [x] T5 test (test-local half-sequence recombination, per ADR 0006 Testing Decisions): assemble a
      circuit where one Sound window is replaced by only the T-c prefix that writes `z` (through
      `CNOT(a,z)`), evaluate, assert `z == 1` at that point; then run the full rewrite and assert
      `z == 0` at window end and full TT equivalence
  - [x] No production partial-evaluate API introduced (all oracle logic stays inside the test)
- [x] Regenerate the `z != 0` case from the same fixed seed as the pipeline tests so the existence
      argument is reproducible
- [x] CONTEXT.md: Sound row in phase table (replace-family Pass 0, T-c pattern, borrow-z,
      `enable_sound` default off) + decision table note
- [x] README.md: `--sound` flag and `hits=S+R1+R2` display documented
- [x] ADR/definition references consistent: `docs/DEFINITION.md` Mixing ops / MVP subset reflect
      Sound as the Active-intent carrier (no false "Active preserves TT" anywhere)
- [x] Full `ctest` green; milestone summary + CHANGELOG Milestone 5 written

> **Note:** the "Same fixed seed" acceptance maps to Sound's deterministic behavior (no rng reads);
> the T5 test's reproducible input is `a == 1` on a single-window circuit, exact for every seed.