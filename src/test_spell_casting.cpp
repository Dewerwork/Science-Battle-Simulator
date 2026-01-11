#include "core/types.hpp"
#include "core/unit.hpp"
#include "core/rule_registry.hpp"
#include "engine/dice.hpp"
#include "engine/game_runner.hpp"
#include "export/text_match_logger.hpp"
#include <iostream>
#include <sstream>

using namespace battle;

// Create the Chosen Change Champion unit
Unit create_chosen_change_champion() {
    Unit unit("Chosen Change Champion", 155);
    unit.faction = Name("Change Disciples");
    unit.quality = 3;
    unit.defense = 3;

    // Add model with Tough(3) and Hero
    Model hero;
    hero.name = Name("Champion");
    hero.quality = 3;
    hero.defense = 3;
    hero.tough = 3;
    hero.is_hero = true;
    unit.add_model(hero);

    // Add unit rules
    unit.add_rule(RuleId::Ambush);
    unit.add_rule(RuleId::Casting, 3);  // Caster(3)
    unit.add_rule(RuleId::Flying);
    unit.add_rule(RuleId::Hero);
    unit.add_rule(RuleId::Tough, 3);
    unit.add_rule(RuleId::VersatileAttack);
    unit.add_rule(RuleId::Fear, 1);

    // Add weapons
    // CCW (A2, melee)
    Weapon ccw;
    ccw.name = Name("CCW");
    ccw.attacks = 2;
    ccw.range = 0;  // Melee
    ccw.ap = 0;
    unit.add_weapon(ccw);

    // Champion Heavy Pistol (12", A2, AP(1))
    Weapon pistol;
    pistol.name = Name("Champion Heavy Pistol");
    pistol.attacks = 2;
    pistol.range = 12;
    pistol.ap = 1;
    unit.add_weapon(pistol);

    unit.compute_ai_type();
    return unit;
}

// Create the Flamer Beast unit
Unit create_flamer_beast() {
    Unit unit("Flamer Beast", 185);
    unit.faction = Name("Alien Hives");
    unit.quality = 4;
    unit.defense = 3;

    // Add model with Tough(6)
    Model beast;
    beast.name = Name("Flamer Beast");
    beast.quality = 4;
    beast.defense = 3;
    beast.tough = 6;
    beast.is_hero = false;
    unit.add_model(beast);

    // Add unit rules
    unit.add_rule(RuleId::Fear, 1);
    unit.add_rule(RuleId::Tough, 6);

    // Add weapons
    // Spit Flames (18", A2, AP(1), Blast(3), Reliable)
    Weapon flames;
    flames.name = Name("Spit Flames");
    flames.attacks = 2;
    flames.range = 18;
    flames.ap = 1;
    flames.add_rule(RuleId::Blast, 3);
    flames.add_rule(RuleId::Reliable);
    unit.add_weapon(flames);

    // Heavy Razor Claws (A3, AP(1))
    Weapon claws;
    claws.name = Name("Heavy Razor Claws");
    claws.attacks = 3;
    claws.range = 0;  // Melee
    claws.ap = 1;
    unit.add_weapon(claws);

    // Stomp (A2, AP(1))
    Weapon stomp;
    stomp.name = Name("Stomp");
    stomp.attacks = 2;
    stomp.range = 0;  // Melee
    stomp.ap = 1;
    unit.add_weapon(stomp);

    unit.compute_ai_type();
    return unit;
}

int main() {
    std::cout << "=== Spell Casting Test ===" << std::endl;
    std::cout << "Chosen Change Champion (Caster(3)) vs Flamer Beast" << std::endl;
    std::cout << std::endl;

    // Create units
    Unit champion = create_chosen_change_champion();
    Unit beast = create_flamer_beast();

    // Set up dice roller with a fixed seed for reproducibility
    DiceRoller dice(12345);

    // Create logger
    TextMatchLogger logger(std::cout);

    // Create game runner
    GameRunner runner(dice, &logger);

    // Run a single game
    std::cout << "Running game..." << std::endl;
    std::cout << std::endl;

    GameResult result = runner.run_game(champion, beast);

    std::cout << std::endl;
    std::cout << "=== Game Complete ===" << std::endl;
    std::cout << "Winner: ";
    switch (result.winner) {
        case GameWinner::UnitA: std::cout << "Chosen Change Champion"; break;
        case GameWinner::UnitB: std::cout << "Flamer Beast"; break;
        case GameWinner::Draw: std::cout << "Draw"; break;
    }
    std::cout << std::endl;

    return 0;
}
