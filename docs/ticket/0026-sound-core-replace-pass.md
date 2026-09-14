# 0026 — Sound core pass (Pass 0 in the replace phase)

**What to build:** Add the Sound local replacement as Pass 0 of `apply_local_replace` (running before
R1, R2): match R1-family windows (three strictly-adjacent CNOT, same target, three pairwise-distinct
controls — the existing `match_r1` shape), rewrite each with the fixed verified T-c canonical sequence
borrowing one zero-initialized ancilla `z`, and restore `z` at the end of each window. When the
circuit has no extra-ancilla wire, report zero Sound hits without error or wire extension.

**Blocked by:** 0025 (ADR 0006 + verified canonical)

**Status:** ready-for-agent

- [ ] `LocalReplaceConfig` gains `bool enable_sound = false` (opt-in, default off)
- [ ] Pass 0 in `apply_local_replace`: scans the same 3-CNOT same-target window shape as R1
      (reuse/refactor `match_r1` so Sound and R1 consume the same cursor), rewrites each matched
      window with the ADR 0006 canonical sequence `CNOT(a,z); CNOT(z,s); CNOT(a,z); CNOT(b,s);
      CNOT(c,s)` (byte-for-byte identical to the verified ADR template; no `rng` reads)
- [ ] `z` selection: same rule as R2 (first extra-ancilla wire, index ≥ 4n+1, chosen once at pass
      start, shared across Sound windows); if none exists (`extra_ancilla = 0`) → zero hits, no
      crash, no `extend_wires`
- [ ] Returns Sound hit count separately (out-param or widened `ReplaceResult`) so callers can
      populate `PhaseStats::replacements_sound`
- [ ] R1/R2 behavior unchanged when `enable_sound` is off (M4 regression: `r1 == n` still holds);
      with Sound on, no `r1 == n` assertion (hits may drop), hard conditions are TT PASS + ancilla
      PASS

**Verification (Done gate — proves Sound correctness on the real adder):**

- [ ] `make_nbit_adder(n, extra_ancilla=1)` + Sound on · R1/R2 off: `replacements_sound ==` expected
      window count (exact numbers registered in this ticket on first green run); TT PASS; ancilla
      PASS; `num_wires()` unchanged
- [ ] n=2 and n=4 both covered
- [ ] `make_nbit_adder(4, 0)` + Sound on: `replacements_sound == 0`; no crash; `num_wires()` unchanged
- [ ] Window-level exhaustive enumeration (T3): original window vs T-c rewrite equal on all
      assignments of the window wires plus `z`; final `z == 0`; at least one case with mid-window
      `z != 0` (a==1)
- [ ] Full `test_local_replace` suite green; no-Sound runs keep existing `r1 == n` assertions green