#include "core/unit.hpp"
#include "core/weapon.hpp"
#include "simulation/simulator.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>

using namespace battle;

Unit create_benchmark_unit(const char* name, int models) {
    Unit unit(name, 100);

    Weapon sword("Sword", 2, 0, 0);
    Weapon pistol("Pistol", 1, 12, 0);
    WeaponIndex sword_idx = get_weapon_pool().add(sword);
    WeaponIndex pistol_idx = get_weapon_pool().add(pistol);

    for (int i = 0; i < models; ++i) {
        Model m("Soldier", 4, 4, 1);
        m.add_weapon(sword_idx);
        m.add_weapon(pistol_idx);
        unit.add_model(m);
    }

    return unit;
}

int main() {
    std::cout << "=== Combat Benchmarks ===" << std::endl;
    std::cout << std::endl;

    // Test different unit sizes
    for (int size : {5, 10, 20}) {
        Unit attacker = create_benchmark_unit("Attacker", size);
        Unit defender = create_benchmark_unit("Defender", size);

        SimulationConfig config;
        config.iterations_per_matchup = 100000;
        config.max_rounds = 10;

        auto start = std::chrono::high_resolution_clock::now();

        MatchupSimulator sim;
        LocalStats stats;
        sim.run_batch(attacker, defender, config, config.iterations_per_matchup, stats);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        double battles_per_sec = config.iterations_per_matchup * 1000.0 / duration.count();

        std::cout << size << " vs " << size << " models:" << std::endl;
        std::cout << "  Iterations: " << config.iterations_per_matchup << std::endl;
        std::cout << "  Time: " << duration.count() << " ms" << std::endl;
        std::cout << "  Rate: " << std::fixed << std::setprecision(0)
                  << battles_per_sec << " battles/sec" << std::endl;
        std::cout << "  Attacker win rate: " << std::setprecision(1)
                  << (100.0 * stats.attacker_wins / config.iterations_per_matchup) << "%" << std::endl;
        std::cout << std::endl;
    }

    // Estimate 100B matchups
    {
        Unit attacker = create_benchmark_unit("Attacker", 10);
        Unit defender = create_benchmark_unit("Defender", 10);

        SimulationConfig config;
        config.iterations_per_matchup = 10000;

        auto start = std::chrono::high_resolution_clock::now();

        MatchupSimulator sim;
        LocalStats stats;
        sim.run_batch(attacker, defender, config, config.iterations_per_matchup, stats);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        double battles_per_sec = config.iterations_per_matchup * 1000.0 / duration.count();

        // Estimate for 100B
        double seconds_for_100b = 100'000'000'000.0 / battles_per_sec;
        double hours_for_100b = seconds_for_100b / 3600.0;
        double days_for_100b = hours_for_100b / 24.0;

        std::cout << "=== 100 Billion Estimate (single thread) ===" << std::endl;
        std::cout << "  Current rate: " << std::setprecision(0)
                  << battles_per_sec << " battles/sec" << std::endl;
        std::cout << "  Estimated time: " << std::setprecision(1);

        if (days_for_100b > 1) {
            std::cout << days_for_100b << " days" << std::endl;
        } else if (hours_for_100b > 1) {
            std::cout << hours_for_100b << " hours" << std::endl;
        } else {
            std::cout << (seconds_for_100b / 60) << " minutes" << std::endl;
        }

        // With threading
        int threads = std::thread::hardware_concurrency();
        double threaded_hours = hours_for_100b / threads;
        std::cout << "  With " << threads << " threads: " << std::setprecision(1);
        if (threaded_hours / 24 > 1) {
            std::cout << (threaded_hours / 24) << " days" << std::endl;
        } else {
            std::cout << threaded_hours << " hours" << std::endl;
        }
        std::cout << std::endl;
    }

    return 0;
}
