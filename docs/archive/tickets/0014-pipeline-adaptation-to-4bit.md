# 0014 — Pipeline adaptation to 4-bit (config, single entry, wire budget)

**What to build:** Make the existing pipeline run on any supported adder size through one entry point. Extend `PipelineConfig` with `n` (default 2) and `extra_ancilla` (default 4), have `run_full_pipeline(cfg)` build the circuit via `make_nbit_adder(cfg.n, cfg.extra_ancilla)`, compute the overhead baseline from that adder's own gate count, and derive the Safe-sandwich and junk wire pools from the adder's non-input/non-output zero wires instead of hardcoded wire indices. The end-to-end pipeline must pass the exhaustive truth table and the ancilla invariant at every phase boundary on the 4-bit adder with Active sandwich as the default, while keeping the 2-bit path green.

**Blocked by:** 01 (Active sandwich semantics corrected to all wires), 02 (generic `make_nbit_adder` + generalized verification).

**Status:** implemented (PipelineConfig n/extra_ancilla; run_full_pipeline on 4-bit; wire pools derived from circuit)

- [ ] `PipelineConfig` exposes `int n = 2;` and `int extra_ancilla = 4;` alongside the existing sandwich mode / chain length / phase toggles
- [ ] `run_full_pipeline(cfg)` builds the circuit via `make_nbit_adder(cfg.n, cfg.extra_ancilla)` and remains the single pipeline entry (no parallel size-specific entry)
- [ ] The `Overhead Ratio` baseline is the built adder's own gate count, replacing the hardcoded 2-bit constant
- [ ] The Safe-sandwich and junk wire pools are derived as the adder's non-input/non-output zero wires (chosen default: after adder construction, scan for wires that are not primary inputs or outputs and are currently 0/reusable; `extend_wires` happens only inside the Safe-sandwich path if more wires are requested), replacing hardcoded fixed indices — one consistent derivation, not two
- [ ] Full 4-bit pipeline (Active sandwich → shuffle → junk) passes the exhaustive truth table for all `2^9 = 512` inputs
- [ ] Ancilla invariant holds at every phase boundary on the 4-bit pipeline
- [ ] Active sandwich on the 4-bit circuit uses the corrected all-wires wire selection with no semantic change
- [ ] 2-bit pipeline and its regression tests remain green
- [ ] Full test suite (all existing suites plus the new 4-bit coverage) passes

**Merge-order note:** ticket 01 must be merged before (or bundled with) the 4-bit Active-pipeline work in this ticket. Do not merge 02 + 03 alone while Active still uses the restricted input/output wire selection; the 4-bit Active pipeline must run against the corrected all-wires semantics.
