#include "engine/dice.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>

using namespace battle;

int main() {
    std::cout << "=== Dice Benchmarks ===" << std::endl;
    std::cout << std::endl;

    DiceRoller roller(12345);

    // Benchmark raw D6 rolls
    {
        const u64 iterations = 100'000'000;
        auto start = std::chrono::high_resolution_clock::now();

        u64 sum = 0;
        for (u64 i = 0; i < iterations; ++i) {
            sum += roller.roll_d6();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        double rolls_per_sec = iterations * 1000.0 / duration.count();

        std::cout << "Raw D6 Rolls:" << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Time: " << duration.count() << " ms" << std::endl;
        std::cout << "  Rate: " << std::fixed << std::setprecision(2)
                  << rolls_per_sec / 1e6 << " million/sec" << std::endl;
        std::cout << "  (Sum: " << sum << ")" << std::endl;
        std::cout << std::endl;
    }

    // Benchmark quality tests
    {
        const u64 iterations = 10'000'000;
        auto start = std::chrono::high_resolution_clock::now();

        u64 total_hits = 0;
        for (u64 i = 0; i < iterations; ++i) {
            auto [hits, sixes] = roller.roll_quality_test(10, 4, 0);
            total_hits += hits;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        double tests_per_sec = iterations * 1000.0 / duration.count();

        std::cout << "Quality Tests (10 dice each):" << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Time: " << duration.count() << " ms" << std::endl;
        std::cout << "  Rate: " << std::fixed << std::setprecision(2)
                  << tests_per_sec / 1e6 << " million/sec" << std::endl;
        std::cout << "  Avg hits: " << std::setprecision(2)
                  << (double)total_hits / iterations << std::endl;
        std::cout << std::endl;
    }

    // Benchmark roll_d6_target
    {
        const u64 iterations = 10'000'000;
        auto start = std::chrono::high_resolution_clock::now();

        u64 total_successes = 0;
        for (u64 i = 0; i < iterations; ++i) {
            total_successes += roller.roll_d6_target(10, 4);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        double tests_per_sec = iterations * 1000.0 / duration.count();

        std::cout << "Target Tests (10 dice each):" << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Time: " << duration.count() << " ms" << std::endl;
        std::cout << "  Rate: " << std::fixed << std::setprecision(2)
                  << tests_per_sec / 1e6 << " million/sec" << std::endl;
        std::cout << std::endl;
    }

    return 0;
}
