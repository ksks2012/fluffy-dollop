# 0010 — Contract: make Active the pipeline default + CLI switch

**What to build:** Flip the production configuration to Active by default and expose a CLI switch. Contract step: `PipelineConfig.sandwich_mode` (default `Active`) becomes the single source of truth; the pipeline and driver consume the configured mode. Tests that are only sandwiching setup (shuffle, inject_junk) continue to pass `Safe` explicitly to preserve their no-op assumption.

**Blocked by:** 0009

**Status:** done

- [ ] `PipelineConfig` exposes `SandwichMode sandwich_mode = SandwichMode::Active;` and `int sandwich_chain_length = 8;` (retain `sandwich_ancilla` for Safe)
- [ ] `pipeline.cpp` / `main.cpp` consume `cfg.sandwich_mode` (forward, never interpret)
- [ ] CLI adds `--sandwich-mode=active|safe` alongside existing `--no-sandwich`; precedence: `--no-sandwich` fully skips the sandwich phase regardless of mode switch
- [ ] Test: specifying both `--no-sandwich` and `--sandwich-mode=active` must fully skip the sandwich phase (precedence verified, no ambiguity)
- [ ] If a temporary default value for `mode` was introduced earlier, now remove it — force explicit passing so the implicit-Safe trap is not left behind
- [ ] `test_pipeline.cpp` asserts default `sandwich_mode == Active` and that the end-to-end pipeline passes truth table + ancilla invariant under Active
- [ ] `test_shuffle.cpp` / `test_inject_junk.cpp` continue to pass `Safe` explicitly (no-op sandwich setup preserved)
- [ ] Full test suite green with Active as the production default
