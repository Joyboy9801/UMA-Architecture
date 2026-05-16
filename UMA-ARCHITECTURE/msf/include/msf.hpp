#pragma once
// ============================================================
//  UMA — Unambiguous Memory Acknowledgment
//  FILE: msf.hpp
//  DESC: The MSF library. Developer-facing types.
//        #include "msf.hpp" then use msf::Int, msf::Ptr etc.
//        All types auto-register in the Stack Table.
// ============================================================

#include "magic_engine.hpp"
#include <stdexcept>
#include <array>
#include <string>

namespace msf {

// ── Global engine singleton ───────────────────────────────────
inline MagicEngine& engine() {
    static MagicEngine instance;
    return instance;
}

// ── RAII scope guard ──────────────────────────────────────────
struct Scope {
    Scope()  { engine().open_scope(); }
    ~Scope() { engine().close_scope(); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};

// ── msf::Int — memory-safe integer ───────────────────────────
class Int {
public:
    Int(int value, const std::string& name, uint32_t line)
        : value_(value), name_(name)
    {
        id_ = engine().declare(name, "int", StorageClass::DYNAMIC, line);
    }

    int get(uint32_t line = 0) const {
        engine().access(id_, line);
        return value_;
    }

    void set(int v, uint32_t line = 0) {
        engine().access(id_, line);
        value_ = v;
    }

    uint64_t id() const { return id_; }

    ~Int() {
        std::cout << "[MSF::Int] Destructed: " << name_ << "\n";
    }

private:
    int         value_;
    uint64_t    id_;
    std::string name_;
};

// ── msf::Float ────────────────────────────────────────────────
class Float {
public:
    Float(float value, const std::string& name, uint32_t line)
        : value_(value), name_(name)
    {
        id_ = engine().declare(name, "float", StorageClass::DYNAMIC, line);
    }

    float get(uint32_t line = 0) const {
        engine().access(id_, line);
        return value_;
    }

    void set(float v, uint32_t line = 0) {
        engine().access(id_, line);
        value_ = v;
    }

    ~Float() {
        std::cout << "[MSF::Float] Destructed: " << name_ << "\n";
    }

private:
    float       value_;
    uint64_t    id_;
    std::string name_;
};

// ── msf::Const<T> — immutable constant, GC managed ───────────
template<typename T>
class Const {
public:
    Const(T value, const std::string& name, uint32_t line)
        : value_(value), name_(name)
    {
        // Constants go to GC — heap allowed
        id_ = engine().declare(name, "const<T>",
                               StorageClass::CONSTANT, line);
    }

    const T& get(uint32_t line = 0) const {
        engine().access(id_, line);
        return value_;
    }

    ~Const() {
        // Signal GC to collect
        engine().decrement_ref(id_);
        std::cout << "[MSF::Const] GC signaled: " << name_ << "\n";
    }

private:
    T           value_;
    uint64_t    id_;
    std::string name_;
};

// ── msf::Ptr<T> — safe pointer, ownership tracked ─────────────
template<typename T>
class Ptr {
public:
    Ptr(T* raw, const std::string& name, uint32_t line)
        : ptr_(raw), name_(name), moved_(false)
    {
        id_ = engine().declare(name, "ptr<T>",
                               StorageClass::DYNAMIC, line);
    }

    // Move constructor — transfers ownership
    Ptr(Ptr&& other)
        : ptr_(other.ptr_), name_(other.name_),
          id_(other.id_), moved_(false)
    {
        engine().move(other.id_, id_, 0);
        other.ptr_   = nullptr;
        other.moved_ = true;
    }

    // No copy — only move
    Ptr(const Ptr&)            = delete;
    Ptr& operator=(const Ptr&) = delete;

    T* get(uint32_t line = 0) const {
        if (moved_) {
            std::cerr << "[MSF::Ptr] MSF_ERROR::USE_AFTER_MOVE\n"
                      << "  Variable: " << name_
                      << " at line " << line << "\n";
            return nullptr;
        }
        engine().access(id_, line);
        return ptr_;
    }

    T& operator*()  { return *get(); }
    T* operator->() { return get(); }

    ~Ptr() {
        if (!moved_ && ptr_) {
            delete ptr_;
            std::cout << "[MSF::Ptr] Freed: " << name_ << "\n";
        }
    }

private:
    T*          ptr_;
    std::string name_;
    uint64_t    id_;
    bool        moved_;
};

// ── msf::SafeArray<T,N> — bounds-checked array ───────────────
template<typename T, size_t N>
class SafeArray {
public:
    SafeArray(const std::string& name, uint32_t line) : name_(name) {
        id_ = engine().declare(
            name,
            "safe_array<T," + std::to_string(N) + ">",
            StorageClass::DYNAMIC, line);
    }

    T& at(size_t index, uint32_t line = 0) {
        engine().check_buffer_write(id_, N, index + 1, line);
        if (index >= N) {
            throw std::out_of_range(
                "[MSF::SafeArray] BUFFER_OVERFLOW at index "
                + std::to_string(index)
                + " (capacity=" + std::to_string(N) + ")");
        }
        return data_[index];
    }

    const T& at(size_t index, uint32_t line = 0) const {
        if (index >= N) {
            throw std::out_of_range(
                "[MSF::SafeArray] BUFFER_OVERFLOW at index "
                + std::to_string(index));
        }
        return data_[index];
    }

    constexpr size_t size() const { return N; }

    ~SafeArray() {
        std::cout << "[MSF::SafeArray] Freed: " << name_ << "\n";
    }

private:
    std::array<T, N> data_{};
    std::string      name_;
    uint64_t         id_;
};

// ── msf::Borrow<T> — RAII immutable borrow ───────────────────
template<typename T>
class Borrow {
public:
    Borrow(uint64_t var_id, const T& ref, uint32_t line)
        : ref_(ref), var_id_(var_id)
    {
        engine().borrow(var_id_, line);
    }

    const T& get() const { return ref_; }

    ~Borrow() {
        engine().release_borrow(var_id_, 0);
    }

    Borrow(const Borrow&) = delete;
    Borrow& operator=(const Borrow&) = delete;

private:
    const T& ref_;
    uint64_t var_id_;
};

// ── msf::StaticVar<T> — static lifetime variable ─────────────
template<typename T>
class StaticVar {
public:
    StaticVar(T value, const std::string& name, uint32_t line)
        : value_(value), name_(name)
    {
        id_ = engine().declare(name, "static<T>",
                               StorageClass::STATIC, line);
    }

    T get(uint32_t line = 0) const {
        engine().access(id_, line);
        return value_;
    }

    void set(T v, uint32_t line = 0) {
        engine().access(id_, line);
        value_ = v;
    }

private:
    T           value_;
    uint64_t    id_;
    std::string name_;
};

} // namespace msf
