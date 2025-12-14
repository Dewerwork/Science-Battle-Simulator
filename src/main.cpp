/**
 * Battle Simulator - OPR Grimdark Future Combat Analysis
 *
 * A high-performance Monte Carlo simulation engine for analyzing
 * unit matchups in One Page Rules tabletop wargames.
 *
 * Designed to handle 100 billion+ matchup simulations.
 */

#include "core/types.hpp"
#include "core/unit.hpp"
#include "core/weapon.hpp"
#include "simulation/simulator.hpp"
#include "simulation/thread_pool.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <vector>

using namespace battle;

// ==============================================================================
// Progress Display
// ==============================================================================

class ProgressDisplay {
public:
    void update(u64 completed, u64 total, f64 rate) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time_).count();

        if (elapsed - last_update_ < 100 && completed < total) return;
        last_update_ = elapsed;

        f64 pct = 100.0 * completed / total;
        f64 eta_sec = (total - completed) / rate;

        std::cout << "\r[";

        // Progress bar
        int bar_width = 40;
        int filled = static_cast<int>(bar_width * pct / 100.0);
        for (int i = 0; i < bar_width; ++i) {
            std::cout << (i < filled ? '=' : (i == filled ? '>' : ' '));
        }

        std::cout << "] " << std::fixed << std::setprecision(1) << pct << "% ";
        std::cout << "(" << format_number(completed) << "/" << format_number(total) << ") ";
        std::cout << format_number(static_cast<u64>(rate)) << "/s ";
        std::cout << "ETA: " << format_time(eta_sec) << "   " << std::flush;
    }

    void finish() {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time_).count();
        std::cout << "\nCompleted in " << format_time(static_cast<f64>(elapsed)) << std::endl;
    }

private:
    std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::now();
    i64 last_update_ = 0;

    static std::string format_number(u64 n) {
        if (n >= 1'000'000'000) return std::to_string(n / 1'000'000'000) + "B";
        if (n >= 1'000'000) return std::to_string(n / 1'000'000) + "M";
        if (n >= 1'000) return std::to_string(n / 1'000) + "K";
        return std::to_string(n);
    }

    static std::string format_time(f64 seconds) {
        if (seconds < 60) return std::to_string(static_cast<int>(seconds)) + "s";
        if (seconds < 3600) {
            int m = static_cast<int>(seconds / 60);
            int s = static_cast<int>(seconds) % 60;
            return std::to_string(m) + "m " + std::to_string(s) + "s";
        }
        int h = static_cast<int>(seconds / 3600);
        int m = (static_cast<int>(seconds) % 3600) / 60;
        return std::to_string(h) + "h " + std::to_string(m) + "m";
    }
};

// ==============================================================================
// Demo Unit Creation
// ==============================================================================

Unit create_demo_assault_squad() {
    Unit unit("Assault Squad", 150);

    // Create a melee weapon
    Weapon ccw("CCW", 2, 0, 0);  // 2 attacks, melee, AP 0

    // Create a pistol
    Weapon pistol("Pistol", 1, 12, 0);  // 1 attack, 12" range, AP 0

    // Add weapons to pool
    auto& pool = get_weapon_pool();
    WeaponIndex ccw_idx = pool.add(ccw);
    WeaponIndex pistol_idx = pool.add(pistol);

    // Add 5 models
    for (int i = 0; i < 5; ++i) {
        Model m("Battle Brother", 3, 4, 1);  // Quality 3+, Defense 4+, 1 wound
        m.add_weapon(ccw_idx);
        m.add_weapon(pistol_idx);
        unit.add_model(m);
    }

    unit.add_rule(RuleId::Furious);  // Extra hits on 6s when charging

    return unit;
}

Unit create_demo_terminator_squad() {
    Unit unit("Terminator Squad", 300);

    // Storm bolter
    Weapon storm_bolter("Storm Bolter", 2, 24, 0);

    // Power fist
    Weapon power_fist("Power Fist", 2, 0, 3);  // 2 attacks, melee, AP 3
    power_fist.add_rule(RuleId::Deadly, 3);

    auto& pool = get_weapon_pool();
    WeaponIndex sb_idx = pool.add(storm_bolter);
    WeaponIndex pf_idx = pool.add(power_fist);

    // Add 5 terminators
    for (int i = 0; i < 5; ++i) {
        Model m("Terminator", 3, 2, 3);  // Quality 3+, Defense 2+, Tough(3)
        m.add_weapon(sb_idx);
        m.add_weapon(pf_idx);
        unit.add_model(m);
    }

    return unit;
}

// ==============================================================================
// Main Entry Point
// ==============================================================================

void print_banner() {
    std::cout << R"(
  ____        _   _   _        ____  _
 | __ )  __ _| |_| |_| | ___  / ___|(_)_ __ ___
 |  _ \ / _` | __| __| |/ _ \ \___ \| | '_ ` _ \
 | |_) | (_| | |_| |_| |  __/  ___) | | | | | | |
 |____/ \__,_|\__|\__|_|\___| |____/|_|_| |_| |_|

 OPR Grimdark Future Combat Simulator v1.0
 Optimized for 100 billion matchups
)" << std::endl;
}

void print_system_info() {
    std::cout << "System Configuration:" << std::endl;
    std::cout << "  Threads: " << std::thread::hardware_concurrency() << std::endl;
    std::cout << "  Matchup Result Size: " << sizeof(MatchupResult) << " bytes" << std::endl;
    std::cout << "  Unit Size: " << sizeof(Unit) << " bytes" << std::endl;
    std::cout << "  Model Size: " << sizeof(Model) << " bytes" << std::endl;
    std::cout << std::endl;
}

void run_demo_simulation() {
    std::cout << "Running demo simulation..." << std::endl;
    std::cout << std::endl;

    // Create demo units
    Unit assault = create_demo_assault_squad();
    Unit terminators = create_demo_terminator_squad();

    std::cout << "Matchup: " << assault.name.c_str() << " vs " << terminators.name.c_str() << std::endl;
    std::cout << "  " << assault.name.c_str() << ": " << (int)assault.model_count << " models, "
              << assault.points_cost << " pts" << std::endl;
    std::cout << "  " << terminators.name.c_str() << ": " << (int)terminators.model_count << " models, "
              << terminators.points_cost << " pts" << std::endl;
    std::cout << std::endl;

    // Configure simulation
    SimulationConfig config;
    config.iterations_per_matchup = 100000;  // 100K iterations for demo
    config.max_rounds = 10;

    Simulator sim(config);
    ProgressDisplay progress;

    auto start = std::chrono::high_resolution_clock::now();

    // Run simulation with progress callback
    auto stats = sim.simulate_matchup(assault, terminators,
        [&progress](u64 completed, u64 total, f64 rate) {
            progress.update(completed, total, rate);
        }
    );

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    progress.finish();
    std::cout << std::endl;

    // Print results
    std::cout << "Results (" << stats.iterations << " iterations):" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Attacker Win Rate: " << (stats.attacker_win_rate * 100) << "%" << std::endl;
    std::cout << "  Defender Win Rate: " << (stats.defender_win_rate * 100) << "%" << std::endl;
    std::cout << "  Draw Rate: " << (stats.draw_rate * 100) << "%" << std::endl;
    std::cout << std::endl;
    std::cout << "  Avg Rounds: " << stats.avg_rounds << std::endl;
    std::cout << "  Avg Kills by Attacker: " << stats.avg_kills_by_attacker << std::endl;
    std::cout << "  Avg Kills by Defender: " << stats.avg_kills_by_defender << std::endl;
    std::cout << std::endl;
    std::cout << "  Attacker Rout Rate: " << (stats.attacker_rout_rate * 100) << "%" << std::endl;
    std::cout << "  Defender Rout Rate: " << (stats.defender_rout_rate * 100) << "%" << std::endl;
    std::cout << std::endl;

    f64 iterations_per_sec = stats.iterations * 1000.0 / duration.count();
    std::cout << "Performance: " << std::fixed << std::setprecision(0)
              << iterations_per_sec << " battles/second" << std::endl;

    // Estimate time for 100B matchups
    f64 time_for_100b = 100'000'000'000.0 / iterations_per_sec;
    std::cout << "Estimated time for 100B iterations: ";
    if (time_for_100b < 3600) {
        std::cout << static_cast<int>(time_for_100b / 60) << " minutes" << std::endl;
    } else if (time_for_100b < 86400) {
        std::cout << static_cast<int>(time_for_100b / 3600) << " hours" << std::endl;
    } else {
        std::cout << static_cast<int>(time_for_100b / 86400) << " days" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    print_banner();
    print_system_info();

    // TODO: Parse command line arguments
    // TODO: Load units from file
    // TODO: Configure simulation parameters

    // For now, run demo
    run_demo_simulation();

    return 0;
}
