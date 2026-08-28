# 0007 — Full pipeline integration + overhead

**What to build:** Wire all phases sequentially: Reversibility (0002) → Hardening/sandwiching (0004) → Mixing/shuffling (0005) + junk injection (0006) → Verification. Print gate count and overhead ratio after each phase. Deterministic randomness via `std::mt19937` with recorded seed. Optional phase toggle mechanism for isolated debugging.

**Fixed wire budget (must be consistent across all phases):**
- Wires 0–4: primary inputs (A1, A0, B1, B0, Cin)
- Wires 5–7: primary outputs (S1, S0, Cout)
- Wire 8: `carry_mid` working ancilla (must be 0 at each phase boundary)
- Wires 9+: extra ancilla for sandwiching / junk injection (all 0 at each phase boundary)
- Total ancilla checked at every phase boundary: wires 8, 9, 10, ...

**Blocked by:** 0005, 0006

**Status:** done

- [x] Sequential phase execution in `main.cpp`
- [x] Overhead ratio printed after each phase (`transformed / original gate count`)
- [x] Final exhaustive truth-table verification over full pipeline
- [x] **Ancilla invariant check** at each phase boundary
- [x] Deterministic seed printed at program start (`std::mt19937`, seed = 42)
- [x] End-to-end demo produces correct output
- [x] (Optional) Phase toggle via command-line flags (`--no-sandwich`, `--no-shuffle`, `--no-inject`)
