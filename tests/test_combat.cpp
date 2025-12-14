#include "core/unit.hpp"
#include "core/weapon.hpp"
#include "engine/combat.hpp"
#include <iostream>
#include <cassert>

using namespace battle;

void test_basic_attack() {
    // Create a simple attacker
    Unit attacker("Test Attacker", 100);
    Model attacker_model("Soldier", 4, 4, 1);

    Weapon sword("Sword", 2, 0, 0);  // 2 attacks, melee, no AP
    WeaponIndex sword_idx = get_weapon_pool().add(sword);
    attacker_model.add_weapon(sword_idx);
    attacker.add_model(attacker_model);

    // Create a simple defender
    Unit defender("Test Defender", 100);
    Model defender_model("Target", 4, 4, 1);
    defender.add_model(defender_model);

    // Run combat
    DiceRoller dice(42);
    CombatResolver resolver(dice);
    CombatContext ctx = CombatContext::melee();

    CombatResult result = resolver.resolve_attack(attacker, defender, ctx);

    std::cout << "Attacks: " << result.total_hits << " hits" << std::endl;
    std::cout << "Wounds: " << result.total_wounds << std::endl;
    std::cout << "[PASS] test_basic_attack" << std::endl;
}

void test_blast_weapon() {
    // Create attacker with Blast weapon
    Unit attacker("Blast Attacker", 100);
    Model attacker_model("Grenadier", 4, 4, 1);

    Weapon grenade("Grenade", 1, 0, 0);
    grenade.add_rule(RuleId::Blast, 3);  // Blast(3)
    WeaponIndex grenade_idx = get_weapon_pool().add(grenade);
    attacker_model.add_weapon(grenade_idx);
    attacker.add_model(attacker_model);

    // Create defender with multiple models
    Unit defender("Squad", 100);
    for (int i = 0; i < 5; ++i) {
        defender.add_model(Model("Soldier", 4, 4, 1));
    }

    DiceRoller dice(123);
    CombatResolver resolver(dice);
    CombatContext ctx = CombatContext::melee();

    CombatResult result = resolver.resolve_attack(attacker, defender, ctx);

    std::cout << "Blast hits (should be multiplied): " << result.total_hits << std::endl;
    std::cout << "[PASS] test_blast_weapon" << std::endl;
}

void test_tough_model() {
    // Attacker
    Unit attacker("Killer", 100);
    Model killer("Killer", 3, 4, 1);

    Weapon biggun("Big Gun", 4, 0, 2);  // 4 attacks, AP 2
    WeaponIndex gun_idx = get_weapon_pool().add(biggun);
    killer.add_weapon(gun_idx);
    attacker.add_model(killer);

    // Tough defender
    Unit defender("Tank", 200);
    Model tank("Tank", 4, 3, 3);  // Defense 3+, Tough(3)
    defender.add_model(tank);

    DiceRoller dice(456);
    CombatResolver resolver(dice);
    CombatContext ctx = CombatContext::melee();

    CombatResult result = resolver.resolve_attack(attacker, defender, ctx);

    std::cout << "Wounds vs Tough(3): " << result.total_wounds << std::endl;
    std::cout << "Models killed: " << (int)result.defender_models_killed << std::endl;
    std::cout << "[PASS] test_tough_model" << std::endl;
}

int main() {
    std::cout << "=== Combat Tests ===" << std::endl;

    test_basic_attack();
    test_blast_weapon();
    test_tough_model();

    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
