# 0021 — R2 pipeline integration + CLI + hit display

**What to build:** Wire R2 (Pass 2 in `apply_local_replace`) into the pipeline with a new
`PipelineConfig::enable_replace_r2` toggle (default `false`) and a CLI `--replace-r2` switch. The
replace phase runs R1 then R2 when both are enabled; the phase entry remains a single `replace`
row in the phase table. The `hits=N` / `hits=N+M` display logic is owned entirely by this ticket.

**Blocked by:** 0020 (R2 core algorithm).

**Status:** ready-for-agent

---

**Checklist:**

- [ ] `PipelineConfig` gains `bool enable_replace_r2 = false;` (maps to `--replace-r2`)
- [ ] `apply_local_replace` receives `enable_r2` from config and only runs Pass 2 when enabled
- [ ] Pass 2 fill `PhaseStats::replacements_r1` and `PhaseStats::replacements_r2` from the
      split hit counts returned by `apply_local_replace` (per ADR 0005 Decision 5); total
      `replacements = r1 + r2`
- [ ] `print_phase` (in `main.cpp` or `pipeline.cpp`) displays:
  - R2 off: `hits=N` (from `replacements_r1`, which equals `replacements`)
  - R2 on: `hits=N+M` (from `replacements_r1 + replacements_r2`)
- [ ] `--replace-r2` is listed in the "Phases enabled" output when active
- [ ] `--no-replace` continues to disable the entire phase (both R1 and R2)
- [ ] When `enable_replace_r2 = false`, the pipeline output is identical to the R1-only pipeline
      (no regressions on existing tests)

**Verification (Done gate):**

- [ ] Bare adder n=2, extra_ancilla=1, Safe sandwich: TT PASS, ancilla PASS;
      `replacements_r1 == 2`, `replacements_r2 == 3`, total `== 5`
- [ ] Bare adder n=4, extra_ancilla=1, Safe sandwich: TT PASS, ancilla PASS;
      `replacements_r1 == 4`, `replacements_r2 == 7`, total `== 11`
- [ ] Bare adder n=4, extra_ancilla=0, R2 on: `replacements == 0`; TT PASS, ancilla PASS
- [ ] `--replace-r2` off (default): same output as R1-only pipeline (same-seed deterministic)
- [ ] All existing test suites (0017–0020) still green; no regressions

**Full pipeline test policy (issue #7):**

Full pipeline tests (with sandwich + shuffle + inject) may have different hit counts from bare
adder depending on sandwich mode and seed. The Done gate for full pipeline tests is:
- TT PASS at every phase boundary
- ancilla PASS at every phase boundary
- `replacements >= 1` (sanity floor when R2 is on)
- `replacements_r1 >= 1` when R2 is on (R1 runs first, should always find S-triples)

Exact hit counts are asserted only on bare adder unit tests (the gold standard).

---

*Source: grilling session Q1–Q6; ADR 0005; ticket 0016 (superseded).*
