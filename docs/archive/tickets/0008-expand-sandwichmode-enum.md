# 0008 — Expand: SandwichMode enum + force Safe at all call sites

**What to build:** Introduce the `SandwichMode` (`Safe | Active`) vocabulary and extend `apply_sandwich` to accept a `mode` parameter, while keeping every existing call site's behavior byte-for-byte identical (all pass `SandwichMode::Safe`). This is the expand step of an expand–contract migration: CI stays green, no production path changes behavior yet.

**Blocked by:** None — can start immediately.

**Status:** done

- [ ] `enum class SandwichMode { Safe, Active }` declared co-located with `apply_sandwich`
- [ ] `apply_sandwich` signature extended to accept a `SandwichMode mode` parameter (in-place `Circuit&` semantics preserved). Target signature — **no default value** so the compiler forces every call site to be updated:
  ```
  void apply_sandwich(Circuit&,
                      const std::vector<int>& extra_ancilla,
                      int chain_length,
                      SandwichMode mode,
                      std::mt19937& rng);
  ```
- [ ] All existing call sites (approx. 20: sandwich, shuffle, inject_junk, pipeline, main tests) explicitly pass `SandwichMode::Safe`, preserving the current extra-ancilla no-op behavior
- [ ] Build passes; existing 6 test suites remain green (behavior unchanged)
- [ ] After compiling, search the whole project for `apply_sandwich` to confirm no old-signature call site was missed (rule out "changed 18, missed 2")
- [ ] No residual temporary default value for `mode` is left behind for the contract ticket to remove
