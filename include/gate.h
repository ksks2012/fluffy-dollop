#pragma once

#include <vector>

// ---------------------------------------------------------------------------
// Gate — abstract base class (in-place model)
// Each derived gate reads from and writes to the shared wire vector directly.
// ---------------------------------------------------------------------------
class Gate {
public:
    virtual ~Gate() = default;
    virtual void evaluate(std::vector<int>& wires) const = 0;
};

// ---------------------------------------------------------------------------
// ToffoliGate — flips wires[target] iff wires[control1] == 1 AND
//               wires[control2] == 1
// ---------------------------------------------------------------------------
class ToffoliGate : public Gate {
public:
    ToffoliGate(int control1, int control2, int target)
        : control1_(control1), control2_(control2), target_(target) {}

    void evaluate(std::vector<int>& wires) const override {
        if (wires[control1_] == 1 && wires[control2_] == 1)
            wires[target_] ^= 1;
    }

    int control1() const { return control1_; }
    int control2() const { return control2_; }
    int target()   const { return target_;   }

private:
    int control1_, control2_, target_;
};

// ---------------------------------------------------------------------------
// CnotGate — XORs wires[target] with wires[control]
// ---------------------------------------------------------------------------
class CnotGate : public Gate {
public:
    CnotGate(int control, int target)
        : control_(control), target_(target) {}

    void evaluate(std::vector<int>& wires) const override {
        wires[target_] ^= wires[control_];
    }

    int control() const { return control_; }
    int target()  const { return target_;  }

private:
    int control_, target_;
};

// ---------------------------------------------------------------------------
// NotGate — unconditionally flips wires[target]
// ---------------------------------------------------------------------------
class NotGate : public Gate {
public:
    explicit NotGate(int target) : target_(target) {}

    void evaluate(std::vector<int>& wires) const override {
        wires[target_] ^= 1;
    }

    int target() const { return target_; }

private:
    int target_;
};
