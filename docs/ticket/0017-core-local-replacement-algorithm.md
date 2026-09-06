# 0017 — Core local-replacement algorithm (R1)

**What to build:** Implement the standalone local-replacement engine for the locked R1 pattern:
scan a `Circuit`'s topological gate sequence for the R1 window, and replace each match in place with
the R1 equivalent sub-circuit (five gates; +2 Toffoli per window; 0 new wires; single pass,
non-overlapping). Same phase shape as the other phases (in-place `Circuit&`, `std::mt19937&`, config
struct), with no pipeline or CLI dependency, so it can be built and verified in isolation.
Canonical R1 spec lives in ticket 0016 (superseded).

**Blocked by:** None — 0012/0013/0014 (generic adder, generalized checkers, pipeline config) already landed.

**Status:** ready-for-agent

**Locked R1 spec (from 0016):**
- Window: three **strictly adjacent** `CNOT` gates in the topological order (no gate in between),
  all three targeting the same wire `s`, with three pairwise-distinct controls `a, b, c`
  (control order arbitrary — must not assume adder's A/B/Cin ordering).
- Replace with: `Toffoli(a,b,s); CNOT(a,s); CNOT(b,s); CNOT(c,s); Toffoli(a,b,s)`.
- Net effect `s ⊕= a⊕b⊕c` (the two Toffoli's `a∧b` contributions cancel). Reversible, 0 new wires.
- Scan: single pass, non-overlapping — after a replacement, continue scanning *after* the new
  sub-circuit so no matched span is rescanned.

---

- [ ] `apply_local_replace(Circuit&, const LocalReplaceConfig&, std::mt19937&)` (in-place `Circuit&`,
      like the other phases; `LocalReplaceConfig` may be the shared phase-config struct or minimal
      and optional) scans the topological gate sequence for strictly-adjacent triples of `CNOT`
      gates that share one target with pairwise-distinct, order-arbitrary controls
- [ ] For each match, replace in place with the exact five-gate sequence
      `Toffoli(a,b,s); CNOT(a,s); CNOT(b,s); CNOT(c,s); Toffoli(a,b,s)`
- [ ] Single pass: exactly one replacement per matching window, no rescan of the replaced span, no
      multi-round iteration
- [ ] Replacement introduces **zero** new wires (0-ancilla); the window's external interface net
      effect is equivalent; **no** global re-topological-sort is performed
- [ ] Note (v1): `std::mt19937&` is retained only to keep the signature uniform with the other
      phases; it is **not used** in v1 — every match is fixed to R1, no Identity drawing, no random
      substitution
- [ ] Returns/reports the number of replacements made (hit count) so the caller can observe it

**Verification (this ticket's Done gate — proves hit, equivalence, and distinguishability at the
algorithm level, on the real adder):**
- [ ] `make_nbit_adder(2,0)` yields `replacements == 2`; `make_nbit_adder(4,0)` yields
      `replacements == 4`
- [ ] After R1 on bare n=2 and n=4: `check_truth_table` PASS and `check_ancilla_invariant` PASS
- [ ] Gate-type distinguishability (robust form): `toffoli_count_after == toffoli_count_before
      + 2 * replacements` for both sizes (not an absolute 21→29)
- [ ] Wire count unchanged (9 and 17) after replacement

---

*Source: grilling session Q1–Q6; ADR 0004; ticket 0016 (superseded).*