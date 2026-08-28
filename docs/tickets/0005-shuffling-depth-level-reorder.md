# 0005 — Shuffling (depth-level reorder)

**What to build:** Random permutation of gates within the same topological depth level. Because the in-place evaluation model processes gates sequentially, and same-depth gates share no data dependencies (no wire overlap in their read/write sets), any permutation produces identical output. After shuffling, recompute depth levels as a sanity check, then verify via truth-table.

**Why shuffling preserves execution order:**
- The in-place model evaluates gates one by one in list order.
- Gates at the same depth level operate on disjoint wire sets (proven by topological sort).
- Therefore, swapping two same-depth gates does not change the wire state seen by any subsequent gate.
- The output after processing all gates is identical regardless of intra-level permutation.

**Blocked by:** 0003, 0004

**Status:** done

- [x] Group gates by topological depth level
- [x] Randomly permute gate order within each level
- [x] Write permuted sequence back to circuit gate list
- [x] Recompute depth levels — must be unchanged (sanity check)
- [x] Truth-table check: all 32 inputs still produce correct output
- [x] **Ancilla invariant check**: ancilla wires remain 0 after shuffling
- [x] Gate count printed before and after
