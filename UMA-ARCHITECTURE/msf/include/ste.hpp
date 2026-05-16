#pragma once
// ============================================================
//  UMA — Unambiguous Memory Acknowledgment
//  FILE: ste.hpp
//  DESC: Stack Table Entry. Every variable gets one of these.
// ============================================================

#include <string>
#include <cstdint>

namespace msf {

enum class StorageClass {
    STATIC,     // lives for program lifetime
    DYNAMIC,    // scope-bound, Stack Table managed
    CONSTANT    // immutable; heap allowed, GC handles it
};

enum class STEStatus {
    ACTIVE,
    BORROWED,
    MOVED,
    EXPIRED,
    LEAKED,
    GC_PENDING  // GC marked it, waiting for collection
};

struct STE {
    uint64_t     var_id;
    std::string  var_name;
    std::string  type_name;
    uint32_t     scope_depth;
    uint64_t     owner_id;
    uint32_t     borrow_count;
    uint32_t     ref_count;
    StorageClass storage;
    STEStatus    status;
    uint32_t     declared_line;
    bool         heap_allowed;    // only true for CONSTANT
    bool         gc_managed;      // GC thread owns cleanup
    bool         ownership_lock;  // ownership engine holds this

    STE()
        : var_id(0), scope_depth(0), owner_id(0),
          borrow_count(0), ref_count(0),
          storage(StorageClass::DYNAMIC),
          status(STEStatus::EXPIRED),
          declared_line(0), heap_allowed(false),
          gc_managed(false), ownership_lock(false) {}

    STE(uint64_t id, const std::string& name,
        const std::string& type, uint32_t scope,
        uint64_t owner, StorageClass sc, uint32_t line)
        : var_id(id), var_name(name), type_name(type),
          scope_depth(scope), owner_id(owner),
          borrow_count(0), ref_count(1),
          storage(sc), status(STEStatus::ACTIVE),
          declared_line(line),
          heap_allowed(sc == StorageClass::CONSTANT),
          gc_managed(sc == StorageClass::CONSTANT),
          ownership_lock(sc != StorageClass::CONSTANT)
    {}

    bool is_freeable() const {
        return ref_count == 0 && borrow_count == 0
            && status != STEStatus::MOVED
            && status != STEStatus::EXPIRED;
    }

    bool is_accessible() const {
        return status == STEStatus::ACTIVE
            || status == STEStatus::BORROWED;
    }

    static std::string status_str(STEStatus s) {
        switch(s) {
            case STEStatus::ACTIVE:     return "ACTIVE";
            case STEStatus::BORROWED:   return "BORROWED";
            case STEStatus::MOVED:      return "MOVED";
            case STEStatus::EXPIRED:    return "EXPIRED";
            case STEStatus::LEAKED:     return "LEAKED";
            case STEStatus::GC_PENDING: return "GC_PENDING";
        }
        return "UNKNOWN";
    }
};

} // namespace msf
