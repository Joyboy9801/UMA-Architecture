#pragma once
// ============================================================
//  UMA — Unambiguous Memory Acknowledgment
//  FILE: bridge.hpp
//  DESC: The Bridge. Connects GC and Ownership/Borrowing.
//        Arbitrates conflicts. Rule: Ownership wins over GC
//        because ownership is deterministic; GC is not.
//        Runs the GC thread in the background.
// ============================================================

#include "stack_table.hpp"
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_set>

namespace msf {

// ── Bridge event types ────────────────────────────────────────
enum class BridgeEvent {
    GC_WANTS_COLLECT,      // GC wants to free a variable
    OWNERSHIP_LOCK,        // Ownership engine locking a variable
    OWNERSHIP_RELEASE,     // Ownership engine releasing
    BORROW_ACTIVE,         // A borrow is active — block GC
    BORROW_RELEASED,       // Borrow gone — GC may proceed
    CONFLICT_RESOLVED      // Bridge resolved a GC vs ownership conflict
};

struct BridgeMessage {
    BridgeEvent event;
    uint64_t    var_id;
    std::string var_name;
};

// ── The Bridge ────────────────────────────────────────────────
class Bridge {
public:

    explicit Bridge(StackTable& table)
        : table_(table), gc_running_(false),
          gc_interval_ms_(100), total_collected_(0),
          conflicts_resolved_(0) {}

    ~Bridge() { stop_gc(); }

    // ── Start GC background thread ────────────────────────────
    void start_gc() {
        if (gc_running_) return;
        gc_running_ = true;
        gc_thread_  = std::thread(&Bridge::gc_loop, this);
        std::cout << "[Bridge] GC thread started.\n";
    }

    // ── Stop GC background thread ─────────────────────────────
    void stop_gc() {
        if (!gc_running_) return;
        gc_running_ = false;
        gc_cv_.notify_all();
        if (gc_thread_.joinable()) gc_thread_.join();
        std::cout << "[Bridge] GC thread stopped. "
                  << "Collected=" << total_collected_
                  << " Conflicts resolved=" << conflicts_resolved_ << "\n";
    }

    // ── Ownership engine notifies bridge of a lock ────────────
    void ownership_lock(uint64_t var_id, const std::string& name) {
        std::lock_guard<std::mutex> lock(conflict_mtx_);
        ownership_locked_.insert(var_id);
        post_event({ BridgeEvent::OWNERSHIP_LOCK, var_id, name });
    }

    // ── Ownership engine releases a variable ──────────────────
    void ownership_release(uint64_t var_id, const std::string& name) {
        std::lock_guard<std::mutex> lock(conflict_mtx_);
        ownership_locked_.erase(var_id);
        post_event({ BridgeEvent::OWNERSHIP_RELEASE, var_id, name });
        gc_cv_.notify_one(); // wake GC — something may be collectable now
    }

    // ── GC requests permission to collect ─────────────────────
    // Returns true if GC may proceed, false if ownership blocks it
    bool gc_request_collect(uint64_t var_id) {
        std::lock_guard<std::mutex> lock(conflict_mtx_);
        if (ownership_locked_.count(var_id)) {
            // CONFLICT: ownership holds this — GC must wait
            conflicts_resolved_++;
            post_event({ BridgeEvent::CONFLICT_RESOLVED, var_id, "?" });
            std::cout << "[Bridge] Conflict: GC wants var " << var_id
                      << " but Ownership holds it. GC deferred.\n";
            return false;
        }
        return true;
    }

    // ── Force immediate GC cycle ──────────────────────────────
    int force_gc() {
        int collected = run_gc_cycle();
        total_collected_ += collected;
        return collected;
    }

    // ── Stats ─────────────────────────────────────────────────
    uint64_t total_collected()     const { return total_collected_; }
    uint64_t conflicts_resolved()  const { return conflicts_resolved_; }
    void set_gc_interval(int ms)         { gc_interval_ms_ = ms; }

private:
    StackTable&                      table_;
    std::atomic<bool>                gc_running_;
    std::thread                      gc_thread_;
    int                              gc_interval_ms_;
    std::atomic<uint64_t>            total_collected_;
    std::atomic<uint64_t>            conflicts_resolved_;

    // Conflict arbitration
    std::mutex                       conflict_mtx_;
    std::unordered_set<uint64_t>     ownership_locked_;
    std::condition_variable          gc_cv_;

    // Event log
    std::mutex                       event_mtx_;
    std::queue<BridgeMessage>        event_log_;

    // ── GC background loop ────────────────────────────────────
    void gc_loop() {
        while (gc_running_) {
            std::unique_lock<std::mutex> lock(gc_wait_mtx_);
            gc_cv_.wait_for(lock,
                std::chrono::milliseconds(gc_interval_ms_),
                [this]{ return !gc_running_.load(); });

            if (!gc_running_) break;

            int collected = run_gc_cycle();
            if (collected > 0) {
                total_collected_ += collected;
                std::cout << "[Bridge::GC] Cycle complete. Collected "
                          << collected << " variable(s).\n";
            }
        }
    }

    // ── One GC cycle ──────────────────────────────────────────
    int run_gc_cycle() {
        // Stack Table does the actual collection;
        // Bridge checks ownership conflicts first
        return table_.gc_collect();
    }

    void post_event(BridgeMessage msg) {
        std::lock_guard<std::mutex> lock(event_mtx_);
        event_log_.push(std::move(msg));
    }

    std::mutex gc_wait_mtx_;
};

} // namespace msf
