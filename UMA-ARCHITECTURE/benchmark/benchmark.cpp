// ============================================================
//  UMA — Benchmark
//  FILE: benchmark.cpp
//  DESC: Measures UMA overhead vs raw C++.
//        Tests: allocation speed, scope cleanup speed,
//               buffer safety check speed.
// ============================================================

#include "../msf/include/msf.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip>

using Clock = std::chrono::high_resolution_clock;
using Ms    = std::chrono::duration<double, std::milli>;

// silence engine logs during benchmark
static void run_benchmark() {
    msf::engine().set_verbose(false);

    const int ITERATIONS = 100000;

    std::cout << "============================================\n";
    std::cout << "  UMA Benchmark — " << ITERATIONS << " iterations\n";
    std::cout << "============================================\n\n";

    // ── BENCHMARK 1: Raw C++ int allocation ───────────────────
    auto t0 = Clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        volatile int x = i * 2;
        (void)x;
    }
    auto t1 = Clock::now();
    double raw_ms = Ms(t1 - t0).count();

    // ── BENCHMARK 2: msf::Int allocation ─────────────────────
    auto t2 = Clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        msf::Scope s;
        msf::Int x(i * 2, "x", 0);
        volatile int v = x.get(0);
        (void)v;
    }
    auto t3 = Clock::now();
    double msf_ms = Ms(t3 - t2).count();

    // ── BENCHMARK 3: Raw array access ─────────────────────────
    auto t4 = Clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        int arr[10];
        arr[i % 10] = i;
        volatile int v = arr[i % 10];
        (void)v;
    }
    auto t5 = Clock::now();
    double raw_arr_ms = Ms(t5 - t4).count();

    // ── BENCHMARK 4: msf::SafeArray access ───────────────────
    auto t6 = Clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        msf::Scope s;
        msf::SafeArray<int, 10> arr("arr", 0);
        arr.at(i % 10, 0) = i;
        volatile int v = arr.at(i % 10, 0);
        (void)v;
    }
    auto t7 = Clock::now();
    double msf_arr_ms = Ms(t7 - t6).count();

    // ── BENCHMARK 5: Raw ptr ──────────────────────────────────
    auto t8 = Clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        int* p = new int(i);
        volatile int v = *p;
        (void)v;
        delete p;
    }
    auto t9 = Clock::now();
    double raw_ptr_ms = Ms(t9 - t8).count();

    // ── BENCHMARK 6: msf::Ptr ────────────────────────────────
    auto t10 = Clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        msf::Scope s;
        msf::Ptr<int> p(new int(i), "p", 0);
        volatile int v = *p.get(0);
        (void)v;
    }
    auto t11 = Clock::now();
    double msf_ptr_ms = Ms(t11 - t10).count();

    // ── Results ───────────────────────────────────────────────
    auto pct = [](double raw, double safe) -> double {
        return ((safe - raw) / raw) * 100.0;
    };

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "┌─────────────────────────┬──────────┬──────────┬──────────────┐\n";
    std::cout << "│ Test                    │ Raw C++  │ MSF/UMA  │ Overhead     │\n";
    std::cout << "├─────────────────────────┼──────────┼──────────┼──────────────┤\n";

    std::cout << "│ Int allocation          │ "
              << std::setw(6) << raw_ms     << "ms │ "
              << std::setw(6) << msf_ms     << "ms │ "
              << std::setw(8) << pct(raw_ms, msf_ms) << "% overhead │\n";

    std::cout << "│ Array access            │ "
              << std::setw(6) << raw_arr_ms << "ms │ "
              << std::setw(6) << msf_arr_ms << "ms │ "
              << std::setw(8) << pct(raw_arr_ms, msf_arr_ms) << "% overhead │\n";

    std::cout << "│ Ptr alloc/free          │ "
              << std::setw(6) << raw_ptr_ms << "ms │ "
              << std::setw(6) << msf_ptr_ms << "ms │ "
              << std::setw(8) << pct(raw_ptr_ms, msf_ptr_ms) << "% overhead │\n";

    std::cout << "└─────────────────────────┴──────────┴──────────┴──────────────┘\n\n";

    std::cout << "Note: Overhead includes Stack Table registration,\n"
              << "      Magic Engine monitoring, and Bridge coordination.\n"
              << "      Goal: reduce this to <5% with optimization.\n\n";
}

int main() {
    run_benchmark();
    return 0;
}
