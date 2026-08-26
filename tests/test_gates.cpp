// Simple inline test runner — no external dependencies
#include <iostream>
#include <vector>
#include "gate.h"

static int s_run = 0, s_failed = 0;

#define CHECK(expr)                                                      \
    do {                                                                 \
        ++s_run;                                                         \
        if (!(expr)) {                                                   \
            std::cerr << "FAIL [line " << __LINE__ << "]: " #expr "\n"; \
            ++s_failed;                                                  \
        }                                                                \
    } while (0)

// ---------------------------------------------------------------------------
// ToffoliGate tests
// ---------------------------------------------------------------------------
static void test_toffoli_both_controls_zero() {
    std::vector<int> w = {0, 0, 1};
    ToffoliGate(0, 1, 2).evaluate(w);
    CHECK(w[2] == 1);  // no flip — controls are 0,0
}

static void test_toffoli_one_control_zero_a() {
    std::vector<int> w = {1, 0, 0};
    ToffoliGate(0, 1, 2).evaluate(w);
    CHECK(w[2] == 0);  // no flip — control2 is 0
}

static void test_toffoli_one_control_zero_b() {
    std::vector<int> w = {0, 1, 0};
    ToffoliGate(0, 1, 2).evaluate(w);
    CHECK(w[2] == 0);  // no flip — control1 is 0
}

static void test_toffoli_both_controls_one_flips_zero_target() {
    std::vector<int> w = {1, 1, 0};
    ToffoliGate(0, 1, 2).evaluate(w);
    CHECK(w[2] == 1);  // flip 0 → 1
}

static void test_toffoli_both_controls_one_flips_one_target() {
    std::vector<int> w = {1, 1, 1};
    ToffoliGate(0, 1, 2).evaluate(w);
    CHECK(w[2] == 0);  // flip 1 → 0
}

static void test_toffoli_does_not_touch_other_wires() {
    std::vector<int> w = {1, 1, 0, 0, 0};
    ToffoliGate(0, 1, 2).evaluate(w);
    CHECK(w[0] == 1);
    CHECK(w[1] == 1);
    CHECK(w[3] == 0);
    CHECK(w[4] == 0);
}

static void test_toffoli_self_inverse() {
    std::vector<int> w = {1, 1, 0};
    ToffoliGate g(0, 1, 2);
    g.evaluate(w);
    CHECK(w[2] == 1);
    g.evaluate(w);
    CHECK(w[2] == 0);  // two applications restore original
}

// ---------------------------------------------------------------------------
// CnotGate tests
// ---------------------------------------------------------------------------
static void test_cnot_control_zero_no_flip() {
    std::vector<int> w = {0, 1};
    CnotGate(0, 1).evaluate(w);
    CHECK(w[1] == 1);  // no XOR — control is 0
}

static void test_cnot_control_one_flips_target_zero() {
    std::vector<int> w = {1, 0};
    CnotGate(0, 1).evaluate(w);
    CHECK(w[1] == 1);
}

static void test_cnot_control_one_flips_target_one() {
    std::vector<int> w = {1, 1};
    CnotGate(0, 1).evaluate(w);
    CHECK(w[1] == 0);
}

static void test_cnot_does_not_touch_control() {
    std::vector<int> w = {1, 0};
    CnotGate(0, 1).evaluate(w);
    CHECK(w[0] == 1);  // control wire unchanged
}

static void test_cnot_self_inverse() {
    std::vector<int> w = {1, 0};
    CnotGate g(0, 1);
    g.evaluate(w);
    CHECK(w[1] == 1);
    g.evaluate(w);
    CHECK(w[1] == 0);
}

// ---------------------------------------------------------------------------
// NotGate tests
// ---------------------------------------------------------------------------
static void test_not_flips_zero() {
    std::vector<int> w = {0};
    NotGate(0).evaluate(w);
    CHECK(w[0] == 1);
}

static void test_not_flips_one() {
    std::vector<int> w = {1};
    NotGate(0).evaluate(w);
    CHECK(w[0] == 0);
}

static void test_not_does_not_touch_other_wires() {
    std::vector<int> w = {0, 1, 0};
    NotGate(0).evaluate(w);
    CHECK(w[0] == 1);
    CHECK(w[1] == 1);
    CHECK(w[2] == 0);
}

static void test_not_self_inverse() {
    std::vector<int> w = {0};
    NotGate g(0);
    g.evaluate(w);
    CHECK(w[0] == 1);
    g.evaluate(w);
    CHECK(w[0] == 0);
}

// ---------------------------------------------------------------------------
// Polymorphic dispatch tests (Gate* interface)
// ---------------------------------------------------------------------------
static void test_polymorphic_toffoli() {
    std::vector<int> w = {1, 1, 0};
    Gate* g = new ToffoliGate(0, 1, 2);
    g->evaluate(w);
    CHECK(w[2] == 1);
    delete g;
}

static void test_polymorphic_cnot() {
    std::vector<int> w = {1, 0};
    Gate* g = new CnotGate(0, 1);
    g->evaluate(w);
    CHECK(w[1] == 1);
    delete g;
}

static void test_polymorphic_not() {
    std::vector<int> w = {0};
    Gate* g = new NotGate(0);
    g->evaluate(w);
    CHECK(w[0] == 1);
    delete g;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    // Toffoli
    test_toffoli_both_controls_zero();
    test_toffoli_one_control_zero_a();
    test_toffoli_one_control_zero_b();
    test_toffoli_both_controls_one_flips_zero_target();
    test_toffoli_both_controls_one_flips_one_target();
    test_toffoli_does_not_touch_other_wires();
    test_toffoli_self_inverse();
    // CNOT
    test_cnot_control_zero_no_flip();
    test_cnot_control_one_flips_target_zero();
    test_cnot_control_one_flips_target_one();
    test_cnot_does_not_touch_control();
    test_cnot_self_inverse();
    // NOT
    test_not_flips_zero();
    test_not_flips_one();
    test_not_does_not_touch_other_wires();
    test_not_self_inverse();
    // Polymorphic
    test_polymorphic_toffoli();
    test_polymorphic_cnot();
    test_polymorphic_not();

    if (s_failed == 0)
        std::cout << "All " << s_run << " tests passed.\n";
    else
        std::cerr << s_failed << "/" << s_run << " tests FAILED.\n";

    return s_failed == 0 ? 0 : 1;
}
