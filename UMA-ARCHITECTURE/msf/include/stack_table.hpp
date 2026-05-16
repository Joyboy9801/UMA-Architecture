#pragma once
// ============================================================
//  UMA — Unambiguous Memory Acknowledgment
//  FILE: stack_table.hpp
//  DESC: Central registry. Both GC and Ownership read/write
//        here. Thread-safe via mutex.
// ============================================================

#include "ste.hpp"
#include <unordered_map>
#include <vector>
#include <iostream>
#include <mutex>
#include <sstream>

namespace msf {

// ── Error reporting ───────────────────────────────────────────
struct MSFError {
    std::string code;
    std::string message;
    uint32_t    line;
    std::string var_name;

    void print() const {
        std::cerr << "\n[MSF_ERROR::" << code << "]\n"
                  << "  Variable : " << var_name << "\n"
                  << "  Line     : " << line << "\n"
                  << "  Detail   : " << message << "\n";
    }
};

// ── Stack Table ───────────────────────────────────────────────
class StackTable {
public:

    void register_var(const STE& entry) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (table_.count(entry.var_id)) {
            emit("DUPLICATE_VAR_ID",
                 "Variable already registered.",
                 entry.declared_line, entry.var_name);
            return;
        }
        table_[entry.var_id] = entry;
        scope_index_[entry.scope_depth].push_back(entry.var_id);
        log("Registered: " + entry.var_name +
            " [id=" + std::to_string(entry.var_id) +
            " scope=" + std::to_string(entry.scope_depth) +
            " type=" + entry.type_name + "]");
    }

    STE* get(uint64_t var_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        return get_unlocked(var_id);
    }

    // ── Borrow ────────────────────────────────────────────────
    bool borrow(uint64_t var_id, uint32_t line) {
        std::lock_guard<std::mutex> lock(mtx_);
        STE* s = get_unlocked(var_id);
        if (!s) { emit("UNKNOWN_VAR", "Variable not found.", line, "?"); return false; }
        if (!s->is_accessible()) {
            emit("BORROW_INVALID",
                 "Cannot borrow — status: " + STE::status_str(s->status),
                 line, s->var_name);
            return false;
        }
        s->borrow_count++;
        s->status = STEStatus::BORROWED;
        log("Borrowed: " + s->var_name + " [borrows=" + std::to_string(s->borrow_count) + "]");
        return true;
    }

    bool release_borrow(uint64_t var_id, uint32_t line) {
        std::lock_guard<std::mutex> lock(mtx_);
        STE* s = get_unlocked(var_id);
        if (!s) return false;
        if (s->borrow_count == 0) {
            emit("BORROW_UNDERFLOW", "No borrow to release.", line, s->var_name);
            return false;
        }
        s->borrow_count--;
        if (s->borrow_count == 0) s->status = STEStatus::ACTIVE;
        log("Borrow released: " + s->var_name);
        return true;
    }

    // ── Ownership move ────────────────────────────────────────
    bool move_ownership(uint64_t var_id, uint64_t new_owner, uint32_t line) {
        std::lock_guard<std::mutex> lock(mtx_);
        STE* s = get_unlocked(var_id);
        if (!s) return false;
        if (s->status == STEStatus::MOVED) {
            emit("USE_AFTER_MOVE", "Already moved. Cannot move again.", line, s->var_name);
            return false;
        }
        if (s->borrow_count > 0) {
            emit("MOVE_WHILE_BORROWED", "Cannot move while borrowed.", line, s->var_name);
            return false;
        }
        s->owner_id = new_owner;
        s->status   = STEStatus::MOVED;
        log("Moved: " + s->var_name + " -> owner " + std::to_string(new_owner));
        return true;
    }

    // ── Decrement ref count (GC uses this) ────────────────────
    void decrement_ref(uint64_t var_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        STE* s = get_unlocked(var_id);
        if (!s || s->ref_count == 0) return;
        s->ref_count--;
        if (s->ref_count == 0 && s->borrow_count == 0
            && s->status == STEStatus::ACTIVE) {
            s->status = STEStatus::GC_PENDING;
            log("GC_PENDING: " + s->var_name);
        }
    }

    // ── GC collect: marks GC_PENDING entries as EXPIRED ───────
    int gc_collect() {
        std::lock_guard<std::mutex> lock(mtx_);
        int collected = 0;
        for (auto& [id, s] : table_) {
            if (s.status == STEStatus::GC_PENDING
                && s.gc_managed
                && s.borrow_count == 0
                && !s.ownership_lock) {
                s.status   = STEStatus::EXPIRED;
                s.ref_count = 0;
                log("GC collected: " + s.var_name);
                collected++;
            }
        }
        return collected;
    }

    // ── Scope exit ────────────────────────────────────────────
    void close_scope(uint32_t depth) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = scope_index_.find(depth);
        if (it == scope_index_.end()) return;
        log("Closing scope depth " + std::to_string(depth));

        for (uint64_t vid : it->second) {
            STE* s = get_unlocked(vid);
            if (!s) continue;
            if (s->borrow_count > 0) {
                emit("BORROW_OUTLIVES_SCOPE",
                     "Still borrowed when scope closed.",
                     s->declared_line, s->var_name);
                continue;
            }
            if (s->status == STEStatus::MOVED) {
                log("Skipping moved: " + s->var_name);
                continue;
            }
            if (s->status == STEStatus::EXPIRED
                || s->status == STEStatus::GC_PENDING) {
                continue; // GC handles these
            }
            s->status    = STEStatus::EXPIRED;
            s->ref_count = 0;
            log("Freed: " + s->var_name);
        }
        scope_index_.erase(it);
    }

    // ── Final sweep ───────────────────────────────────────────
    bool final_sweep() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::cout << "\n[StackTable] ===== Final Sweep =====\n";
        bool clean = true;
        for (auto& [id, s] : table_) {
            if (s.status == STEStatus::ACTIVE && s.ref_count > 0) {
                s.status = STEStatus::LEAKED;
                emit("MEMORY_LEAK",
                     "Variable alive at program exit.",
                     s.declared_line, s.var_name);
                clean = false;
            }
        }
        if (clean) std::cout << "[StackTable] Clean. No leaks.\n";
        std::cout << "[StackTable] ===== Sweep Done =====\n\n";
        return clean;
    }

    // ── Stats ─────────────────────────────────────────────────
    struct Stats {
        size_t total, active, borrowed, moved, expired, leaked, gc_pending;
    };

    Stats stats() const {
        std::lock_guard<std::mutex> lock(mtx_);
        Stats s{};
        s.total = table_.size();
        for (const auto& [id, e] : table_) {
            switch(e.status) {
                case STEStatus::ACTIVE:     s.active++;     break;
                case STEStatus::BORROWED:   s.borrowed++;   break;
                case STEStatus::MOVED:      s.moved++;      break;
                case STEStatus::EXPIRED:    s.expired++;    break;
                case STEStatus::LEAKED:     s.leaked++;     break;
                case STEStatus::GC_PENDING: s.gc_pending++; break;
            }
        }
        return s;
    }

    void dump() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::cout << "\n[StackTable] ===== Dump =====\n";
        for (const auto& [id, s] : table_) {
            std::cout << "  [" << id << "] "
                      << s.var_name << " : " << s.type_name
                      << " | scope=" << s.scope_depth
                      << " | " << STE::status_str(s.status)
                      << " | borrows=" << s.borrow_count
                      << " | refs=" << s.ref_count
                      << (s.gc_managed ? " | GC" : " | OWN")
                      << "\n";
        }
        std::cout << "[StackTable] ===== End =====\n\n";
    }

    void set_verbose(bool v) { verbose_ = v; }

private:
    mutable std::mutex mtx_;
    std::unordered_map<uint64_t, STE> table_;
    std::unordered_map<uint32_t, std::vector<uint64_t>> scope_index_;
    bool verbose_ = true;

    STE* get_unlocked(uint64_t id) {
        auto it = table_.find(id);
        return it == table_.end() ? nullptr : &it->second;
    }

    void emit(const std::string& code, const std::string& msg,
              uint32_t line, const std::string& name) const {
        MSFError{ code, msg, line, name }.print();
    }

    void log(const std::string& msg) const {
        if (verbose_)
            std::cout << "[StackTable] " << msg << "\n";
    }
};

} // namespace msf
