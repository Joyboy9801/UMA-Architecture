# UMA — Unambiguous Memory Acknowledgment

A parallel memory safety architecture combining Garbage Collection
and Ownership/Borrowing via a unified Stack Table and Magic Engine.

## Architecture

```
SOURCE CODE
    │
    ▼
BINARY CONVERSION
    │
    ▼
MAGIC ENGINE (monitors everything)
    │
    ├──────────────────────┐
    ▼                      ▼
GARBAGE COLLECTOR    OWNERSHIP/BORROWING
(handles constants)  (handles dynamic vars)
    │                      │
    └──────────┬───────────┘
               ▼
           THE BRIDGE
        (arbitrates conflicts)
               │
               ▼
          STACK TABLE
       (single source of truth)
               │
               ▼
          MSF LIBRARY
       (developer interface)
```

## Components

| File | Role |
|------|------|
| `msf/include/ste.hpp` | Stack Table Entry — data unit for every variable |
| `msf/include/stack_table.hpp` | Central registry, thread-safe |
| `msf/include/bridge.hpp` | GC ↔ Ownership arbitration + GC thread |
| `msf/include/magic_engine.hpp` | Brain — monitors all variable lifecycles |
| `msf/include/msf.hpp` | Developer API — Int, Float, Ptr, SafeArray, Borrow |

## MSF Types

```cpp
#include "msf/include/msf.hpp"

msf::Scope s;                              // open a tracked scope

msf::Int x(42, "x", __LINE__);            // tracked integer
msf::Float f(3.14f, "f", __LINE__);       // tracked float
msf::Const<int> c(100, "MAX", __LINE__);  // GC-managed constant
msf::Ptr<int> p(new int(7), "p", __LINE__); // safe pointer
msf::SafeArray<int, 10> arr("arr", __LINE__); // bounds-checked array
msf::Borrow<int> b(x.id(), x.get(), __LINE__); // immutable borrow
```

## Build

```bash
# Build everything
make all

# Run demo
./uma_demo

# Run tests
./uma_tests

# Run benchmark
./uma_benchmark

# Or run all at once
make run
```

## Errors Detected

- `BUFFER_OVERFLOW_RISK` — write exceeds array bounds
- `USE_AFTER_FREE` — access to expired variable
- `USE_AFTER_MOVE` — access after ownership transferred
- `MOVE_WHILE_BORROWED` — move attempted while borrowed
- `BORROW_OUTLIVES_SCOPE` — borrow alive when scope closes
- `MEMORY_LEAK` — variable alive at program exit
- `DUPLICATE_VAR_ID` — double registration in Stack Table

## Architecture Rules

1. **Ownership wins over GC** in conflicts — ownership is deterministic
2. **Constants only** may use heap — GC handles them safely
3. **Dynamic variables** go through Stack Table — no heap
4. **GC runs in background** — no full program pause
5. **Scope exit = immediate cleanup** — no waiting for GC

## NOTE :-)
**Use this code for developement, not expliotation**

---
*Architecture by JAI PARTHASARATHY M | MSF Library | Part of the UMA Architecture*
