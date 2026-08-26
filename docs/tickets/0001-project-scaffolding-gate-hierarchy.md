# 0001 — Project scaffolding + Gate hierarchy

**What to build:** CMake project setup and a polymorphic `Gate` class hierarchy with `ToffoliGate`, `CnotGate`, and `NotGate` derived classes. Each gate implements a virtual `evaluate(vector<int>& wires)` method using the **in-place model**: the gate mutates wire state directly (reads from and writes to the shared wire vector). A trivial driver program creates gates and evaluates them.

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [ ] CMakeLists.txt configured for C++17
- [ ] `Gate` base class with virtual `evaluate(vector<int>& wires)` (in-place mutation)
- [ ] `ToffoliGate(control1, control2, target)` — flips `wires[target]` iff both controls are 1
- [ ] `CnotGate(control, target)` — XORs `wires[target]` with `wires[control]`
- [ ] `NotGate(target)` — flips `wires[target]`
- [ ] Driver creates gates, initializes wire vector, evaluates, and prints results
