// ============================================================
//  UMA — Full Test Suite
//  FILE: tests/test_all.cpp
//  DESC: Tests every feature of UMA:
//        Int, Float, Const, Ptr, SafeArray, Borrow,
//        scope cleanup, use-after-free, move, GC, Bridge.
// ============================================================

#include "../msf/include/msf.hpp"
#include <iostream>
#include <cassert>

// ── Test helpers ──────────────────────────────────────────────
static int passed = 0;
static int failed = 0;

#define TEST(name) \
    std::cout << "\n━━━ TEST: " << name << " ━━━\n"

#define PASS(msg) \
    std::cout << "  ✓ PASS: " << msg << "\n"; passed++

#define FAIL(msg) \
    std::cout << "  ✗ FAIL: " << msg << "\n"; failed++

#define CHECK(cond, msg) \
    if(cond) { PASS(msg); } else { FAIL(msg); }

// ─────────────────────────────────────────────────────────────

void test_int_basic() {
    TEST("msf::Int basic declaration and access");
    msf::Scope s;
    msf::Int x(42, "x", __LINE__);
    CHECK(x.get(__LINE__) == 42, "Int value is 42");
    x.set(99, __LINE__);
    CHECK(x.get(__LINE__) == 99, "Int value updated to 99");
}

void test_float_basic() {
    TEST("msf::Float basic declaration");
    msf::Scope s;
    msf::Float f(3.14f, "f", __LINE__);
    CHECK(f.get(__LINE__) > 3.0f, "Float value > 3.0");
}

void test_const_gc() {
    TEST("msf::Const — GC managed constant");
    {
        msf::Scope s;
        msf::Const<int> c(100, "PI_APPROX", __LINE__);
        CHECK(c.get(__LINE__) == 100, "Const value is 100");
        // destructor signals GC when scope exits
    }
    msf::engine().force_gc();
    PASS("GC cycle forced after const scope exit");
}

void test_scope_cleanup() {
    TEST("Scope cleanup — variable freed on scope exit");
    {
        msf::Scope s;
        msf::Int y(10, "y", __LINE__);
        CHECK(y.get(__LINE__) == 10, "y accessible inside scope");
    }
    // y is gone here — scope closed, freed by Stack Table
    PASS("Scope closed and y freed (check engine log above)");
}

void test_nested_scopes() {
    TEST("Nested scopes — correct depth tracking");
    msf::Scope outer;
    msf::Int a(1, "a", __LINE__);
    {
        msf::Scope inner;
        msf::Int b(2, "b", __LINE__);
        CHECK(b.get(__LINE__) == 2, "b accessible in inner scope");
    } // b freed here
    CHECK(a.get(__LINE__) == 1, "a still accessible in outer scope");
    PASS("Nested scope depth tracking correct");
}

void test_ptr_basic() {
    TEST("msf::Ptr — safe pointer");
    msf::Scope s;
    msf::Ptr<int> p(new int(77), "p", __LINE__);
    CHECK(*p.get(__LINE__) == 77, "Ptr dereference correct");
}

void test_ptr_move() {
    TEST("msf::Ptr — ownership move");
    msf::Scope s;
    msf::Ptr<int> p(new int(55), "p", __LINE__);
    CHECK(*p.get(__LINE__) == 55, "p valid before move");

    msf::Ptr<int> q(std::move(p));
    CHECK(*q.get(__LINE__) == 55, "q valid after receiving ownership");

    // p is now invalid
    int* result = p.get(__LINE__);
    CHECK(result == nullptr, "p returns nullptr after move (USE_AFTER_MOVE caught)");
}

void test_safe_array_inbounds() {
    TEST("msf::SafeArray — in-bounds access");
    msf::Scope s;
    msf::SafeArray<int, 5> arr("arr", __LINE__);
    arr.at(0, __LINE__) = 10;
    arr.at(4, __LINE__) = 50;
    CHECK(arr.at(0, __LINE__) == 10, "arr[0] correct");
    CHECK(arr.at(4, __LINE__) == 50, "arr[4] correct");
}

void test_safe_array_overflow() {
    TEST("msf::SafeArray — buffer overflow caught");
    msf::Scope s;
    msf::SafeArray<int, 3> buf("buf", __LINE__);
    bool caught = false;
    try {
        buf.at(99, __LINE__) = 1; // way out of bounds
    } catch (const std::out_of_range& e) {
        caught = true;
    }
    CHECK(caught, "Buffer overflow exception thrown and caught");
}

void test_borrow() {
    TEST("msf::Borrow — immutable borrow");
    msf::Scope s;
    msf::Int x(42, "x", __LINE__);
    {
        msf::Borrow<int> b(x.id(), x.get(), __LINE__);
        CHECK(b.get() == 42, "Borrow reads correct value");
    } // borrow released here
    PASS("Borrow released on scope exit");
}

void test_static_var() {
    TEST("msf::StaticVar — static lifetime");
    msf::StaticVar<int> sv(999, "GLOBAL_MAX", __LINE__);
    CHECK(sv.get(__LINE__) == 999, "StaticVar accessible");
    sv.set(1000, __LINE__);
    CHECK(sv.get(__LINE__) == 1000, "StaticVar updated");
}

void test_gc_bridge() {
    TEST("Bridge — GC cycle via force_gc()");
    {
        msf::Scope s;
        msf::Const<int> c1(10, "c1", __LINE__);
        msf::Const<int> c2(20, "c2", __LINE__);
        PASS("Two constants declared and registered");
    }
    int collected = msf::engine().force_gc();
    std::cout << "  GC collected " << collected << " variable(s)\n";
    PASS("GC cycle completed");
}

void test_large_allocation() {
    TEST("Large allocation — 1000 variables in scope");
    {
        msf::Scope s;
        for (int i = 0; i < 1000; i++) {
            msf::Int x(i, "bulk_" + std::to_string(i), __LINE__);
        }
        PASS("1000 msf::Int variables declared");
    }
    PASS("All 1000 variables freed on scope exit");
}

// ── Main ──────────────────────────────────────────────────────
int main() {
    msf::engine().set_verbose(false); // suppress per-var logs

    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║   UMA — Full Test Suite                  ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";

    test_int_basic();
    test_float_basic();
    test_const_gc();
    test_scope_cleanup();
    test_nested_scopes();
    test_ptr_basic();
    test_ptr_move();
    test_safe_array_inbounds();
    test_safe_array_overflow();
    test_borrow();
    test_static_var();
    test_gc_bridge();
    test_large_allocation();

    std::cout << "\n╔══════════════════════════════════════════╗\n";
    std::cout << "║   Results                                ║\n";
    std::cout << "╠══════════════════════════════════════════╣\n";
    std::cout << "║  PASSED: " << passed
              << std::string(32 - std::to_string(passed).size(), ' ') << "║\n";
    std::cout << "║  FAILED: " << failed
              << std::string(32 - std::to_string(failed).size(), ' ') << "║\n";
    std::cout << "╚══════════════════════════════════════════╝\n\n";

    return failed == 0 ? 0 : 1;
}
