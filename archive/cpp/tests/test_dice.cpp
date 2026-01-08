#include "engine/dice.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace battle;

void test_d6_range() {
    DiceRoller roller(12345);

    for (int i = 0; i < 10000; ++i) {
        u8 roll = roller.roll_d6();
        assert(roll >= 1 && roll <= 6);
    }
    std::cout << "[PASS] test_d6_range" << std::endl;
}

void test_distribution() {
    DiceRoller roller(42);
    int counts[7] = {0};

    const int iterations = 60000;
    for (int i = 0; i < iterations; ++i) {
        counts[roller.roll_d6()]++;
    }

    // Each value should appear roughly 1/6 of the time
    double expected = iterations / 6.0;
    for (int i = 1; i <= 6; ++i) {
        double diff = std::abs(counts[i] - expected) / expected;
        assert(diff < 0.05); // Within 5%
    }
    std::cout << "[PASS] test_distribution" << std::endl;
}

void test_quality_test() {
    DiceRoller roller(999);

    // Quality 4+ should hit about 50%
    auto [hits, sixes] = roller.roll_quality_test(1000, 4, 0);
    double hit_rate = hits / 1000.0;
    assert(hit_rate > 0.45 && hit_rate < 0.55);
    std::cout << "[PASS] test_quality_test (hit rate: " << hit_rate << ")" << std::endl;
}

void test_defense_test() {
    DiceRoller roller(777);

    // Defense 4+, no AP, should save about 50%
    u32 wounds = roller.roll_defense_test(1000, 4, 0, 0, false);
    double wound_rate = wounds / 1000.0;
    assert(wound_rate > 0.45 && wound_rate < 0.55);
    std::cout << "[PASS] test_defense_test (wound rate: " << wound_rate << ")" << std::endl;
}

int main() {
    std::cout << "=== Dice Tests ===" << std::endl;

    test_d6_range();
    test_distribution();
    test_quality_test();
    test_defense_test();

    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
