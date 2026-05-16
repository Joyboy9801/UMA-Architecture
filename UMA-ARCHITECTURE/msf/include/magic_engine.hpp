#pragma once
// ============================================================
//  UMA — Unambiguous Memory Acknowledgment
//  FILE: magic_engine.hpp
//  DESC: The Magic Engine. Brain of UMA. Monitors every
//        variable from before first assignment. Detects
//        buffer overflows, use-after-free, scope violations.
//        Coordinates Bridge between GC and Ownership.
// ============================================================

#include "bridge.hpp"
#include <atomic>
#include <iostream>
#include <string>
#include <unordered_set>

namespace msf {

class MagicEngine {
public:

    MagicEngine()
        : current_scope_(0), next_var_id_(1),
          running_(true), total_vars_(0),
          errors_caught_(0)
    {
        bridge_ = std::make_unique<Bridge>(table_);
        bridge_->start_gc();
        std::cout << "[MagicEngine] *** UMA Initialized ***\n"
                  << "[MagicEngine] Stack Table: ONLINE\n"
                  << "[MagicEngine] Bridge:      ONLINE\n"
                  << "[MagicEngine] GC Thread:   ONLINE\n"
                  << "[MagicEngine] Monitoring:  ACTIVE\n\n";
    }

    ~MagicEngine() { shutdown(); }

    // ── Scope management ──────────────────────────────────────
    void open_scope() {
        current_scope_++;
        std::cout << "[MagicEngine] Scope OPEN  depth=" << current_scope_ << "\n";
    }

    void close_scope() {
        if (current_scope_ == 0) {
            std::cerr << "[MagicEngine] ERROR: Cannot close global scope.\n";
            return;
        }
        std::cout << "[MagicEngine] Scope CLOSE depth=" << current_scope_ << "\n";

        // Release ownership locks for all vars at this scope
        // (Bridge notified via ownership_release in the close)
        table_.close_scope(current_scope_);
        current_scope_--;
    }

    // ── Variable declaration ───────────────────────────────────
    uint64_t declare(const std::string& name,
                     const std::string& type,
                     StorageClass sc,
                     uint32_t line) {

        // Pre-declaration safety checks
        check_type_safety(name, type, line);

        uint64_t id = next_var_id_++;
        total_vars_++;

        STE entry(id, name, type, current_scope_,
                  /*owner=*/current_scope_, sc, line);
        table_.register_var(entry);

        // Notify bridge about ownership
        if (sc != StorageClass::CONSTANT) {
            bridge_->ownership_lock(id, name);
        }

        return id;
    }

    // ── Variable access check ──────────────────────────────────
    bool access(uint64_t var_id, uint32_t line) {
        STE* s = table_.get(var_id);
        if (!s) {
            error("UNKNOWN_VARIABLE",
                  "Variable ID " + std::to_string(var_id) + " not in Stack Table.",
                  line, "?");
            return false;
        }
        if (!s->is_accessible()) {
            error("USE_AFTER_FREE",
                  "Variable '" + s->var_name + "' is no longer alive. Status: "
                  + STE::status_str(s->status),
                  line, s->var_name);
            return false;
        }
        return true;
    }

    // ── Borrow ────────────────────────────────────────────────
    bool borrow(uint64_t var_id, uint32_t line) {
        return table_.borrow(var_id, line);
    }

    bool release_borrow(uint64_t var_id, uint32_t line) {
        bool ok = table_.release_borrow(var_id, line);
        if (ok) bridge_->ownership_release(var_id, "borrow_released");
        return ok;
    }

    // ── Ownership move ────────────────────────────────────────
    bool move(uint64_t var_id, uint64_t new_owner, uint32_t line) {
        bridge_->ownership_release(var_id, "pre_move");
        bool ok = table_.move_ownership(var_id, new_owner, line);
        if (ok) bridge_->ownership_lock(new_owner, "post_move");
        return ok;
    }

    // ── Buffer overflow check ─────────────────────────────────
    bool check_buffer_write(uint64_t var_id,
                            size_t buf_size,
                            size_t write_size,
                            uint32_t line) {
        STE* s   = table_.get(var_id);
        std::string name = s ? s->var_name : "unknown";

        if (write_size > buf_size) {
            error("BUFFER_OVERFLOW_RISK",
                  "Write size (" + std::to_string(write_size)
                  + ") exceeds buffer capacity ("
                  + std::to_string(buf_size) + ").",
                  line, name);
            return false;
        }
        return true;
    }

    // ── Force a GC cycle now ───────────────────────────────────
    int force_gc() { return bridge_->force_gc(); }

    // ── Decrement ref (for GC-managed constants) ───────────────
    void decrement_ref(uint64_t var_id) {
        table_.decrement_ref(var_id);
    }

    // ── Shutdown ───────────────────────────────────────────────
    void shutdown() {
        if (!running_) return;
        running_ = false;

        bridge_->stop_gc();

        std::cout << "\n[MagicEngine] ===== UMA Shutdown Report =====\n";
        std::cout << "[MagicEngine] Total variables tracked : " << total_vars_ << "\n";
        std::cout << "[MagicEngine] Errors caught           : " << errors_caught_ << "\n";
        std::cout << "[MagicEngine] GC collections          : "
                  << bridge_->total_collected() << "\n";
        std::cout << "[MagicEngine] Conflicts resolved      : "
                  << bridge_->conflicts_resolved() << "\n";

        bool clean = table_.final_sweep();
        table_.dump();

        std::cout << "[MagicEngine] Memory status           : "
                  << (clean ? "CLEAN" : "LEAKS DETECTED") << "\n";
        std::cout << "[MagicEngine] ===== End Report =====\n\n";
    }

    void set_verbose(bool v)    { table_.set_verbose(v); }
    void dump()                  { table_.dump(); }
    uint32_t scope()       const { return current_scope_; }
    uint64_t total_vars()  const { return total_vars_; }
    uint64_t errors()      const { return errors_caught_; }

private:
    StackTable                   table_;
    std::unique_ptr<Bridge>      bridge_;
    uint32_t                     current_scope_;
    std::atomic<uint64_t>        next_var_id_;
    std::atomic<bool>            running_;
    std::atomic<uint64_t>        total_vars_;
    std::atomic<uint64_t>        errors_caught_;

    void error(const std::string& code, const std::string& msg,
               uint32_t line, const std::string& name) {
        errors_caught_++;
        MSFError{ code, msg, line, name }.print();
    }

    void check_type_safety(const std::string& name,
                           const std::string& type,
                           uint32_t line) {
        // Warn on dangerous raw types
        if (type == "char[]" || type == "int[]"
            || type.find("*") != std::string::npos) {
            std::cerr << "[MagicEngine] MSF_WARNING::UNSAFE_TYPE\n"
                      << "  Variable : " << name << " at line " << line << "\n"
                      << "  Type     : " << type << "\n"
                      << "  Advice   : Use msf::SafeArray or msf::Ptr instead.\n";
        }
    }
};

} // namespace msf
