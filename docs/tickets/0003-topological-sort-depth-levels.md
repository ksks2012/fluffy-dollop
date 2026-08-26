# 0003 — Topological sort + depth levels

**What to build:** `Circuit::topological_sort()` and `Circuit::compute_depth_levels()`. Topological sort produces a valid gate ordering. Depth levels assign each gate an integer depth (input wires = 0, gate = 1 + max predecessor depth). Gates at the same depth level are independent.

**Blocked by:** 0002

**Status:** ready-for-agent

- [ ] `Circuit::topological_sort()` returns a valid topological ordering
- [ ] `Circuit::compute_depth_levels()` assigns correct depth to each gate
- [ ] Verification: sorted order respects all wire dependencies
- [ ] Verification: gates at same depth share no data dependencies
