// ============================================================
//  UMA — Unambiguous Memory Acknowledgment
//  FILE: main.cpp
//  DESC: Main demo. Shows UMA working like a real program.
//        Compile: g++ -std=c++17 -pthread -o uma main.cpp
// ============================================================

#include "msf/include/msf.hpp"
#include <iostream>

// ── Simulated game object using MSF types ─────────────────────
void simulate_game_object() {
    std::cout << "\n[Demo] Game object simulation\n";
    msf::Scope game_scope;

    msf::Int    health(100, "player_health", __LINE__);
    msf::Int    score(0,   "player_score",  __LINE__);
    msf::Float  speed(5.5f, "player_speed", __LINE__);
    msf::Const<int> MAX_HEALTH(100, "MAX_HEALTH", __LINE__);

    health.set(80, __LINE__); // took damage
    score.set(250, __LINE__); // scored points

    std::cout << "[Demo] health=" << health.get(__LINE__)
              << " score="  << score.get(__LINE__)
              << " speed="  << speed.get(__LINE__)
              << " max="    << MAX_HEALTH.get(__LINE__) << "\n";
}

// ── Simulated buffer handling ─────────────────────────────────
void simulate_buffer_ops() {
    std::cout << "\n[Demo] Buffer operations\n";
    msf::Scope buf_scope;

    msf::SafeArray<int, 8> packet("network_packet", __LINE__);

    // Safe writes
    for (int i = 0; i < 8; i++) {
        packet.at(i, __LINE__) = i * 10;
    }

    std::cout << "[Demo] Packet: ";
    for (int i = 0; i < 8; i++) {
        std::cout << packet.at(i) << " ";
    }
    std::cout << "\n";

    // Overflow attempt — caught
    try {
        packet.at(20, __LINE__) = 999;
    } catch (const std::out_of_range& e) {
        std::cout << "[Demo] Overflow caught: " << e.what() << "\n";
    }
}

// ── Simulated ownership transfer ──────────────────────────────
void simulate_ownership() {
    std::cout << "\n[Demo] Ownership transfer\n";
    msf::Scope own_scope;

    msf::Ptr<int> resource(new int(42), "resource", __LINE__);
    std::cout << "[Demo] resource=" << *resource.get(__LINE__) << "\n";

    msf::Ptr<int> new_owner(std::move(resource));
    std::cout << "[Demo] new_owner=" << *new_owner.get(__LINE__) << "\n";

    // resource is now invalid
    int* gone = resource.get(__LINE__);
    if (!gone) std::cout << "[Demo] resource correctly invalid after move\n";
}

// ── Simulated borrow ──────────────────────────────────────────
void simulate_borrow() {
    std::cout << "\n[Demo] Borrow and release\n";
    msf::Scope s;

    msf::Int data(512, "data", __LINE__);

    {
        msf::Borrow<int> reader(data.id(), data.get(), __LINE__);
        std::cout << "[Demo] borrowed value=" << reader.get() << "\n";
    } // borrow released here

    // data still owned by original, can be modified
    data.set(1024, __LINE__);
    std::cout << "[Demo] data after borrow released=" << data.get(__LINE__) << "\n";
}

// ── Main ──────────────────────────────────────────────────────
int main() {
    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║  UMA — Unambiguous Memory Acknowledgment ║\n";
    std::cout << "║  MSF Library Demo                        ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";

    simulate_game_object();
    simulate_buffer_ops();
    simulate_ownership();
    simulate_borrow();

    std::cout << "\n[Demo] All simulations complete.\n";
    std::cout << "[Demo] UMA engine shutting down...\n\n";

    return 0;
}
