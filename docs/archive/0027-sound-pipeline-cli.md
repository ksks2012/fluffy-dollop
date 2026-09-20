# 0027 — Sound pipeline / CLI wiring

**What to build:** Expose Sound as a first-class pipeline configuration and CLI option: construct the
circuit, run the phase order sandwich → replace (Sound→R1→R2) → shuffle → inject, record Sound hits in
the phase stats, and print them in the phase table. Prove the full pipeline passes the truth table and
ancilla invariant with Sound on, and that Sound composes with R1/R2 toggles.

**Blocked by:** 0026 (Sound core Pass 0)

**Status:** implemented (2026-09-15)

- [x] `PipelineConfig` gains `bool enable_sound = false` (default off), passed through to the replace
      phase; `--no-replace` disables Sound together with R1/R2 (master switch)
- [x] CLI: `--sound` flag (symmetric to `--replace-r2`), default off
- [x] `PhaseStats` gains `int replacements_sound = 0`, filled by the replace phase when enabled; CLI
      `hits=` suffix becomes `hits=S+R1+R2` when `--sound` is on
- [x] Seed behavior: enabling Sound does not consume the `mt19937` stream (rng untouched by the
      Sound pass), so later-phase regression behavior is stable

**Verification (Done gate):**

- [x] Full pipeline n=2 and n=4, Safe sandwich, Sound on: TT PASS + ancilla PASS at every phase
      boundary; `replacements_sound >= 1` (bare-adder deterministic count registered in 0026 reused)
- [x] Combinations: Sound × {R1 on/off, R2 on/off} each pass TT + ancilla (T6)
- [x] `--no-replace` with `--sound`: replace phase fully skipped (no Sound, no R1/R2), pipeline green
- [x] `--sound` off: identical to M4 output incl. `r1 == n` (regression T2)
- [x] Full pipeline suite green