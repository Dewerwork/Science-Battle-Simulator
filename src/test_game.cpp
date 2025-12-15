#include "parser/unit_parser.hpp"
#include "engine/game_runner.hpp"
#include <iostream>
#include <iomanip>

using namespace battle;

void print_match_result(const MatchResult& result, const Unit& a, const Unit& b) {
    std::cout << "\n=== MATCH RESULT ===\n";
    std::cout << a.name.c_str() << " vs " << b.name.c_str() << "\n";
    std::cout << "Games won: " << static_cast<int>(result.games_won_a) << " - "
              << static_cast<int>(result.games_won_b) << "\n";

    std::cout << "Winner: ";
    switch (result.overall_winner) {
        case GameWinner::UnitA: std::cout << a.name.c_str(); break;
        case GameWinner::UnitB: std::cout << b.name.c_str(); break;
        case GameWinner::Draw: std::cout << "DRAW"; break;
    }
    std::cout << "\n";

    std::cout << "\nStats:\n";
    std::cout << "  Total wounds dealt: " << result.total_wounds_dealt_a << " - "
              << result.total_wounds_dealt_b << "\n";
    std::cout << "  Total models killed: " << result.total_models_killed_a << " - "
              << result.total_models_killed_b << "\n";
    std::cout << "  Rounds holding obj: " << static_cast<int>(result.total_rounds_holding_a) << " - "
              << static_cast<int>(result.total_rounds_holding_b) << "\n";
}

int main() {
    std::cout << "=== Battle Simulator Game Engine Test ===\n\n";

    // Create sample units for testing
    std::string sample = R"(
Assault Walker [1] Q4+ D2+ | 350pts | Devout, Fear(2), Fearless, Piercing Assault, Regeneration, Tough(9)
Stomp (A3, AP(1)), Heavy Claw (A4, AP(1), Rending), Light Chainsaw (A1, AP(2), Deadly(3)), Heavy Fist (A4, AP(4))

Battle Sisters [5] Q4+ D4+ | 100pts | Devout
5x CCWs (A5), 5x 24" Rifles (A5)

APC [1] Q4+ D2+ | 175pts | Devout, Impact(3), Strider, Tough(6)
24" Storm Rifle (A3, AP(1))
)";

    auto result = UnitParser::parse_string(sample, "Blessed Sisters");
    std::cout << "Parsed " << result.units.size() << " units for testing\n";

    if (result.units.size() < 2) {
        std::cout << "Need at least 2 units to test\n";
        return 1;
    }

    // Get units
    Unit& assault_walker = result.units[0];
    Unit& battle_sisters = result.units[1];

    std::cout << "\nUnit 1: " << assault_walker.name.c_str()
              << " (" << assault_walker.points_cost << "pts)"
              << " AI: " << (assault_walker.ai_type == AIType::Melee ? "MELEE" :
                            assault_walker.ai_type == AIType::Shooting ? "SHOOTING" : "HYBRID")
              << "\n";
    std::cout << "Unit 2: " << battle_sisters.name.c_str()
              << " (" << battle_sisters.points_cost << "pts)"
              << " AI: " << (battle_sisters.ai_type == AIType::Melee ? "MELEE" :
                            battle_sisters.ai_type == AIType::Shooting ? "SHOOTING" : "HYBRID")
              << "\n";

    // Run a single match
    std::cout << "\n--- Running single match ---\n";
    DiceRoller dice(12345);  // Fixed seed for reproducibility
    GameRunner runner(dice);

    MatchResult match = runner.run_match(assault_walker, battle_sisters);
    print_match_result(match, assault_walker, battle_sisters);

    // Run many matches to get statistics
    std::cout << "\n--- Running 1000 matches for statistics ---\n";
    int a_wins = 0, b_wins = 0, draws = 0;

    for (int i = 0; i < 1000; ++i) {
        MatchResult m = runner.run_match(assault_walker, battle_sisters);
        switch (m.overall_winner) {
            case GameWinner::UnitA: a_wins++; break;
            case GameWinner::UnitB: b_wins++; break;
            case GameWinner::Draw: draws++; break;
        }
    }

    std::cout << "\nResults over 1000 matches:\n";
    std::cout << "  " << assault_walker.name.c_str() << " wins: " << a_wins
              << " (" << std::fixed << std::setprecision(1) << (a_wins / 10.0) << "%)\n";
    std::cout << "  " << battle_sisters.name.c_str() << " wins: " << b_wins
              << " (" << std::fixed << std::setprecision(1) << (b_wins / 10.0) << "%)\n";
    std::cout << "  Draws: " << draws
              << " (" << std::fixed << std::setprecision(1) << (draws / 10.0) << "%)\n";

    // Test with a mirror match
    std::cout << "\n--- Mirror match: Assault Walker vs Assault Walker ---\n";
    a_wins = b_wins = draws = 0;
    for (int i = 0; i < 1000; ++i) {
        MatchResult m = runner.run_match(assault_walker, assault_walker);
        switch (m.overall_winner) {
            case GameWinner::UnitA: a_wins++; break;
            case GameWinner::UnitB: b_wins++; break;
            case GameWinner::Draw: draws++; break;
        }
    }

    std::cout << "Results over 1000 matches:\n";
    std::cout << "  Unit A wins: " << a_wins << " (" << (a_wins / 10.0) << "%)\n";
    std::cout << "  Unit B wins: " << b_wins << " (" << (b_wins / 10.0) << "%)\n";
    std::cout << "  Draws: " << draws << " (" << (draws / 10.0) << "%)\n";

    std::cout << "\n=== Test complete ===\n";
    return 0;
}
