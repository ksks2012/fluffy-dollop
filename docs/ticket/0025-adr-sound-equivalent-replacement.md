# 0025 — ADR 0006: Sound equivalent replacement (spec + verified canonical)

**What to build:** Lock the soundness contract for intermediate-value scrambling (the M5 successor to
the deprecated legacy Active sandwich) as an ADR, with a window-level-verified canonical rewrite.
Sound is the third local-replace pattern (Pass 0 in the replace phase), NOT a third sandwich mode.

**Blocked by:** None — can start immediately.

**Status:** implemented (ADR 0006 + T-c window-level verification + ADR 0002 append, 2026-09-12)

---

**Delivered:**

- [x] `docs/adr/0006-sound-equivalent-replacement.md` — soundness contract (single-target-write
      window, read-only controls, optional borrowed zero ancilla restored to 0, proof by window-level
      enumeration, whole-circuit outer `F⁻¹ C F` forbidden)
- [x] Canonical T-c rewrite verified against the R1-family window by exhaustive enumeration (all 16
      assignments of a/b/c/s, z fixed 0 at entry; final z=0; mid-window z==1 exactly when a==1):
      `CNOT(a,z); CNOT(z,s); CNOT(a,z); CNOT(b,s); CNOT(c,s)` ≡ `CNOT(a,s); CNOT(b,s); CNOT(c,s)`
- [x] `docs/adr/0002` append-only note: legacy Active remains experimental / known-broken, intent
      carried forward by Sound; not repaired in place
- [x] Locked names: `PipelineConfig::enable_sound` (default false), CLI `--sound`, `PhaseStats::replacements_sound`, `--no-replace` master switch

**Acceptance criteria for implementation downstream (0026):** every code path must reproduce the
verified gate sequence byte-for-byte; no `rng` reads; no-window mixing with the two passes below.