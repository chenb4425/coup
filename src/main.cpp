// ============================================================
// main_test.cpp  –  quick smoke-test / simulation
// ============================================================
#include "coup.hpp"
#include <iostream>
#include <random>
#include <cassert>

using namespace coup;

// ── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    bool verbose = (argc > 1 && std::string(argv[1]) == "-v");

    std::cout << "═══ Coup game engine unit tests ═══\n\n";

    testIncome();
    testForeignAidBlockedByDuke();
    testChallengeBlockSucceeds();
    testMandatoryCoupAt10Coins();
    testLegalMovesNeverEmpty();
    testAlwaysTerminates();

    std::cout << "\nAll tests passed.\n\n";

    
    return 0;
}
