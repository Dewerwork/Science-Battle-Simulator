#include "simulation/simulator.hpp"
#include <iostream>
#include <cassert>

using namespace battle;

Unit create_test_unit(const char* name, int models, int quality, int defense) {
    Unit unit(name, 100);

    Weapon sword("Sword", 2, 0, 0);
    WeaponIndex sword_idx = get_weapon_pool().add(sword);

    for (int i = 0; i < models; ++i) {
        Model m("Soldier", quality, defense, 1);
        m.add_weapon(sword_idx);
        unit.add_model(m);
    }

    return unit;
}

void test_single_battle() {
    Unit attacker = create_test_unit("Attacker", 5, 4, 4);
    Unit defender = create_test_unit("Defender", 5, 4, 4);

    SimulationConfig config;
    config.iterations_per_matchup = 1;
    config.max_rounds = 10;

    MatchupSimulator sim;
    auto result = sim.run_battle(attacker, defender, config);

    std::cout << "Winner: " << (result.winner == BattleWinner::Attacker ? "Attacker" :
                                result.winner == BattleWinner::Defender ? "Defender" : "Draw") << std::endl;
    std::cout << "Rounds: " << (int)result.rounds << std::endl;
    std::cout << "[PASS] test_single_battle" << std::endl;
}

void test_batch_simulation() {
    Unit attacker = create_test_unit("Attacker", 5, 4, 4);
    Unit defender = create_test_unit("Defender", 5, 4, 4);

    SimulationConfig config;
    config.iterations_per_matchup = 1000;

    MatchupSimulator sim;
    LocalStats stats;
    sim.run_batch(attacker, defender, config, 1000, stats);

    double win_rate = stats.attacker_wins / 1000.0;
    std::cout << "Attacker win rate (1000 iterations): " << (win_rate * 100) << "%" << std::endl;

    // With equal units, expect roughly 50% win rate
    assert(win_rate > 0.3 && win_rate < 0.7);
    std::cout << "[PASS] test_batch_simulation" << std::endl;
}

void test_asymmetric_matchup() {
    Unit elite = create_test_unit("Elite", 5, 3, 3);  // Better quality and defense
    Unit basic = create_test_unit("Basic", 5, 4, 5);  // Worse quality and defense

    SimulationConfig config;
    config.iterations_per_matchup = 1000;

    MatchupSimulator sim;
    LocalStats stats;
    sim.run_batch(elite, basic, config, 1000, stats);

    double elite_win_rate = stats.attacker_wins / 1000.0;
    std::cout << "Elite vs Basic win rate: " << (elite_win_rate * 100) << "%" << std::endl;

    // Elite should win more often
    assert(elite_win_rate > 0.5);
    std::cout << "[PASS] test_asymmetric_matchup" << std::endl;
}

int main() {
    std::cout << "=== Scenario Tests ===" << std::endl;

    test_single_battle();
    test_batch_simulation();
    test_asymmetric_matchup();

    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
