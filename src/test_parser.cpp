#include "parser/unit_parser.hpp"
#include <iostream>
#include <iomanip>

using namespace battle;

void print_unit(const Unit& unit) {
    std::cout << "----------------------------------------\n";
    std::cout << "Name: " << unit.name.c_str() << "\n";
    std::cout << "Faction: " << unit.faction.c_str() << "\n";
    std::cout << "Models: " << static_cast<int>(unit.model_count) << "\n";
    std::cout << "Quality: " << static_cast<int>(unit.quality) << "+\n";
    std::cout << "Defense: " << static_cast<int>(unit.defense) << "+\n";
    std::cout << "Points: " << unit.points_cost << "\n";

    // Print AI type
    std::cout << "AI Type: ";
    switch (unit.ai_type) {
        case AIType::Melee: std::cout << "MELEE"; break;
        case AIType::Shooting: std::cout << "SHOOTING"; break;
        case AIType::Hybrid: std::cout << "HYBRID"; break;
    }
    std::cout << "\n";

    // Print rules
    std::cout << "Rules (" << static_cast<int>(unit.rule_count) << "): ";
    for (u8 i = 0; i < unit.rule_count; ++i) {
        const auto& rule = unit.rules[i];
        std::cout << static_cast<int>(rule.id);
        if (rule.value > 0) std::cout << "(" << static_cast<int>(rule.value) << ")";
        if (i < unit.rule_count - 1) std::cout << ", ";
    }
    std::cout << "\n";

    // Print weapons
    std::cout << "Weapons (" << static_cast<int>(unit.weapon_count) << "):\n";
    for (u8 i = 0; i < unit.weapon_count; ++i) {
        const auto& weapon = unit.weapons[i];
        std::cout << "  - " << weapon.name.c_str();
        if (weapon.range > 0) {
            std::cout << " (Range: " << static_cast<int>(weapon.range) << "\")";
        } else {
            std::cout << " (Melee)";
        }
        std::cout << " A" << static_cast<int>(weapon.attacks);
        if (weapon.ap > 0) {
            std::cout << " AP(" << static_cast<int>(weapon.ap) << ")";
        }
        if (weapon.rule_count > 0) {
            std::cout << " [";
            for (u8 j = 0; j < weapon.rule_count; ++j) {
                std::cout << static_cast<int>(weapon.rules[j].id);
                if (weapon.rules[j].value > 0) {
                    std::cout << "(" << static_cast<int>(weapon.rules[j].value) << ")";
                }
                if (j < weapon.rule_count - 1) std::cout << ", ";
            }
            std::cout << "]";
        }
        std::cout << "\n";
    }

    std::cout << "Melee Attacks: " << unit.total_melee_attacks() << "\n";
    std::cout << "Ranged Attacks: " << unit.total_ranged_attacks() << "\n";
}

int main(int argc, char* argv[]) {
    std::cout << "=== Battle Simulator Unit Parser Test ===\n\n";

    // Test with inline sample data
    std::string sample = R"(
APC [1] Q4+ D2+ | 175pts | Devout, Impact(3), Strider, Tough(6)
24" Storm Rifle (A3, AP(1))

Assault Sisters [5] Q4+ D4+ | 195pts | Devout
5x Energy Swords (A10, AP(1), Rending), 6" Fusion Pistol (A1, AP(4), Deadly(3)), 4x 12" Heavy Pistols (A4, AP(1))

Assault Walker [1] Q4+ D2+ | 350pts | Devout, Fear(2), Fearless, Piercing Assault, Regeneration, Tough(9)
Stomp (A3, AP(1)), Heavy Claw (A4, AP(1), Rending), Light Chainsaw (A1, AP(2), Deadly(3)), Heavy Fist (A4, AP(4))
)";

    std::cout << "Parsing sample units...\n\n";

    auto result = UnitParser::parse_string(sample, "Blessed Sisters");

    std::cout << "Lines processed: " << result.lines_processed << "\n";
    std::cout << "Units parsed: " << result.units_parsed << "\n";
    std::cout << "Errors: " << result.errors.size() << "\n";

    for (const auto& error : result.errors) {
        std::cout << "  ERROR: " << error << "\n";
    }

    std::cout << "\n";

    for (const auto& unit : result.units) {
        print_unit(unit);
        std::cout << "\n";
    }

    // If a file path was provided, also test that
    if (argc > 1) {
        std::cout << "\n=== Parsing file: " << argv[1] << " ===\n\n";
        auto file_result = UnitParser::parse_file(argv[1]);

        std::cout << "Lines processed: " << file_result.lines_processed << "\n";
        std::cout << "Units parsed: " << file_result.units_parsed << "\n";
        std::cout << "Errors: " << file_result.errors.size() << "\n";

        // Only print first 5 units to avoid flooding output
        int count = 0;
        for (const auto& unit : file_result.units) {
            if (count++ >= 5) {
                std::cout << "\n... and " << (file_result.units.size() - 5) << " more units\n";
                break;
            }
            print_unit(unit);
            std::cout << "\n";
        }
    }

    std::cout << "=== Test complete ===\n";
    return 0;
}
