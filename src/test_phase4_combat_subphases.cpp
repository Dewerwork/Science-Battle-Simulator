//==============================================================================
// Phase 4 Combat Sub-Phases A/B Test
//==============================================================================
// Tests that the registry-based combat sub-phase implementations produce
// identical results to the original hardcoded implementations.

#include <iostream>
#include <cassert>
#include <cstring>

#include "core/rule_architecture_validation.hpp"
#include "core/types.hpp"
#include "core/phases.hpp"
#include "core/traits.hpp"
#include "core/rule_data.hpp"
#include "core/rule_effect.hpp"
#include "core/rule_presence.hpp"
#include "core/contexts.hpp"
#include "core/rule_registry.hpp"
#include "core/unit.hpp"
#include "core/weapon.hpp"
#include "rules/combat_rules.hpp"
#include "engine/registry_combat_resolver.hpp"
#include "engine/dice.hpp"

using namespace battle;

//==============================================================================
// Test Helpers
//==============================================================================

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "  Running " << #name << "... "; \
    test_##name(); \
    std::cout << "PASS\n"; \
} while(0)

// Helper to create a test unit with specific rules
Unit create_test_unit(const char* name, u8 quality_val, std::initializer_list<CompactRule> rules) {
    Unit unit{std::string_view(name)};
    unit.quality = quality_val;
    unit.defense = 4;
    unit.model_count = 5;
    unit.alive_count = 5;
    unit.rule_count = 0;
    unit.rule_mask = 0;

    for (const auto& rule : rules) {
        if (unit.rule_count < MAX_RULES_PER_ENTITY) {
            unit.rules[unit.rule_count++] = rule;
            unit.rule_mask |= rule_bit(rule.id);
        }
    }

    return unit;
}

// Helper to create a test weapon with specific rules
Weapon create_test_weapon(const char* name, bool ranged, std::initializer_list<CompactRule> rules) {
    Weapon weapon{std::string_view(name), u8(2), u8(ranged ? 24 : 0), u8(1)};
    weapon.rule_count = 0;
    weapon.rule_mask = 0;

    for (const auto& rule : rules) {
        if (weapon.rule_count < MAX_RULES_PER_ENTITY) {
            weapon.rules[weapon.rule_count++] = rule;
            weapon.rule_mask |= rule_bit(rule.id);
        }
    }

    return weapon;
}

//==============================================================================
// HIT_SEPARATION Tests
//==============================================================================

TEST(hit_separation_no_rules) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Rifle", true, {});

    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto new_result = resolver.apply_hit_separation(
        attacker, defender, weapon, CombatType::SHOOTING, 10, 3);
    auto old_result = compute_hit_separation_old_style(
        attacker, defender, weapon, 10, 3);

    assert(new_result.normal_hits == old_result.normal_hits);
    assert(new_result.rending_hits == old_result.rending_hits);
    assert(new_result.has_rending == old_result.has_rending);
    assert(new_result.has_rupture == old_result.has_rupture);
}

TEST(hit_separation_rending) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Rending Rifle", true, {
        CompactRule(RuleId::Rending, 0)
    });

    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto new_result = resolver.apply_hit_separation(
        attacker, defender, weapon, CombatType::SHOOTING, 10, 3);
    auto old_result = compute_hit_separation_old_style(
        attacker, defender, weapon, 10, 3);

    assert(new_result.normal_hits == old_result.normal_hits);
    assert(new_result.rending_hits == old_result.rending_hits);
    assert(new_result.has_rending == true);
    assert(new_result.rending_hits == 3);  // 3 natural sixes
    assert(new_result.normal_hits == 7);   // 10 - 3 = 7
}

TEST(hit_separation_rupture) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Rupture Rifle", true, {
        CompactRule(RuleId::Rupture, 0)
    });

    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto new_result = resolver.apply_hit_separation(
        attacker, defender, weapon, CombatType::SHOOTING, 10, 3);
    auto old_result = compute_hit_separation_old_style(
        attacker, defender, weapon, 10, 3);

    assert(new_result.has_rupture == true);
    assert(new_result.rupture_hits == 3);  // 3 natural sixes
    assert(new_result.normal_hits == 10);  // Rupture doesn't reduce normal hits
}

TEST(hit_separation_rending_and_rupture) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Combo Rifle", true, {
        CompactRule(RuleId::Rending, 0),
        CompactRule(RuleId::Rupture, 0)
    });

    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto new_result = resolver.apply_hit_separation(
        attacker, defender, weapon, CombatType::SHOOTING, 10, 4);
    auto old_result = compute_hit_separation_old_style(
        attacker, defender, weapon, 10, 4);

    assert(new_result.has_rending == true);
    assert(new_result.has_rupture == true);
    assert(new_result.rending_hits == 4);
    assert(new_result.rupture_hits == 4);
    assert(new_result.normal_hits == 6);  // 10 - 4 = 6 (Rending reduces normal)
}

//==============================================================================
// HIT_BONUSES Tests
//==============================================================================

TEST(hit_bonuses_no_rules) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Rifle", true, {});

    DiceRoller dice1(12345);
    DiceRoller dice2(12345);
    RegistryCombatResolver resolver(registry, dice1);

    auto new_result = resolver.apply_hit_bonuses(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false,
        10, 3, 4, 0);
    auto old_result = compute_hit_bonuses_old_style(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false,
        10, 3, 4, 0, dice2);

    assert(new_result.bonus_hits == old_result.bonus_hits);
    assert(new_result.total_hits == old_result.total_hits);
    assert(new_result.bonus_hits == 0);
}

TEST(hit_bonuses_relentless_far) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Relentless Shooter", 4, {
        CompactRule(RuleId::Relentless, 0)
    });
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Rifle", true, {});

    DiceRoller dice1(12345);
    DiceRoller dice2(12345);
    RegistryCombatResolver resolver(registry, dice1);

    // Distance > 9 should trigger Relentless
    auto new_result = resolver.apply_hit_bonuses(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false,
        10, 3, 4, 0);
    auto old_result = compute_hit_bonuses_old_style(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false,
        10, 3, 4, 0, dice2);

    assert(new_result.bonus_hits == old_result.bonus_hits);
    assert(new_result.bonus_hits == 3);  // 3 extra hits from 3 sixes
    assert(new_result.total_hits == 13);
}

TEST(hit_bonuses_relentless_close) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Relentless Shooter", 4, {
        CompactRule(RuleId::Relentless, 0)
    });
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Rifle", true, {});

    DiceRoller dice1(12345);
    DiceRoller dice2(12345);
    RegistryCombatResolver resolver(registry, dice1);

    // Distance <= 9 should NOT trigger Relentless
    auto new_result = resolver.apply_hit_bonuses(
        attacker, defender, weapon, CombatType::SHOOTING, 8, false,
        10, 3, 4, 0);
    auto old_result = compute_hit_bonuses_old_style(
        attacker, defender, weapon, CombatType::SHOOTING, 8, false,
        10, 3, 4, 0, dice2);

    assert(new_result.bonus_hits == old_result.bonus_hits);
    assert(new_result.bonus_hits == 0);
}

TEST(hit_bonuses_surge) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Surge Rifle", true, {
        CompactRule(RuleId::Surge, 0)
    });

    DiceRoller dice1(12345);
    DiceRoller dice2(12345);
    RegistryCombatResolver resolver(registry, dice1);

    auto new_result = resolver.apply_hit_bonuses(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false,
        10, 4, 4, 0);
    auto old_result = compute_hit_bonuses_old_style(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false,
        10, 4, 4, 0, dice2);

    assert(new_result.bonus_hits == old_result.bonus_hits);
    assert(new_result.bonus_hits == 4);  // 4 extra hits from 4 sixes
    assert(new_result.total_hits == 14);
}

TEST(hit_bonuses_point_blank_surge) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Point Blank Shooter", 4, {
        CompactRule(RuleId::PointBlankSurge, 0)
    });
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Rifle", true, {});

    DiceRoller dice1(12345);
    DiceRoller dice2(12345);
    RegistryCombatResolver resolver(registry, dice1);

    // Distance <= 9 should trigger PointBlankSurge
    auto new_result = resolver.apply_hit_bonuses(
        attacker, defender, weapon, CombatType::SHOOTING, 6, false,
        10, 2, 4, 0);
    auto old_result = compute_hit_bonuses_old_style(
        attacker, defender, weapon, CombatType::SHOOTING, 6, false,
        10, 2, 4, 0, dice2);

    assert(new_result.bonus_hits == old_result.bonus_hits);
    assert(new_result.bonus_hits == 2);  // 2 extra hits from 2 sixes
}

TEST(hit_bonuses_furious_charging) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Furious Fighter", 4, {
        CompactRule(RuleId::Furious, 0)
    });
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Sword", false, {});

    DiceRoller dice1(12345);
    DiceRoller dice2(12345);
    RegistryCombatResolver resolver(registry, dice1);

    // Charging in melee should trigger Furious
    auto new_result = resolver.apply_hit_bonuses(
        attacker, defender, weapon, CombatType::MELEE, 0, true,
        10, 3, 4, 0);
    auto old_result = compute_hit_bonuses_old_style(
        attacker, defender, weapon, CombatType::MELEE, 0, true,
        10, 3, 4, 0, dice2);

    assert(new_result.bonus_hits == old_result.bonus_hits);
    assert(new_result.bonus_hits == 3);
}

TEST(hit_bonuses_furious_not_charging) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Furious Fighter", 4, {
        CompactRule(RuleId::Furious, 0)
    });
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Sword", false, {});

    DiceRoller dice1(12345);
    DiceRoller dice2(12345);
    RegistryCombatResolver resolver(registry, dice1);

    // Not charging should NOT trigger Furious
    auto new_result = resolver.apply_hit_bonuses(
        attacker, defender, weapon, CombatType::MELEE, 0, false,
        10, 3, 4, 0);
    auto old_result = compute_hit_bonuses_old_style(
        attacker, defender, weapon, CombatType::MELEE, 0, false,
        10, 3, 4, 0, dice2);

    assert(new_result.bonus_hits == old_result.bonus_hits);
    assert(new_result.bonus_hits == 0);
}

TEST(hit_bonuses_multiple_rules) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Multi-Rule Shooter", 4, {
        CompactRule(RuleId::Relentless, 0),
        CompactRule(RuleId::PointBlankSurge, 0)  // Won't apply at long range
    });
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Surge Rifle", true, {
        CompactRule(RuleId::Surge, 0)
    });

    DiceRoller dice1(12345);
    DiceRoller dice2(12345);
    RegistryCombatResolver resolver(registry, dice1);

    // At distance 12: Relentless + Surge should both apply
    auto new_result = resolver.apply_hit_bonuses(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false,
        10, 3, 4, 0);
    auto old_result = compute_hit_bonuses_old_style(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false,
        10, 3, 4, 0, dice2);

    assert(new_result.bonus_hits == old_result.bonus_hits);
    assert(new_result.bonus_hits == 6);  // 3 from Relentless + 3 from Surge
}

//==============================================================================
// HIT_MULTIPLICATION Tests
//==============================================================================

TEST(hit_multiplication_no_blast) {
    auto registry = create_default_registry();
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Rifle", true, {});

    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto new_result = resolver.apply_hit_multiplication(
        defender, weapon, 10, 2, 1, false);
    auto old_result = compute_hit_multiplication_old_style(
        defender, weapon, 10, 2, 1, false);

    assert(new_result.total_hits == old_result.total_hits);
    assert(new_result.multiplier_used == 1);
    assert(new_result.total_hits == 10);
}

TEST(hit_multiplication_blast_3) {
    auto registry = create_default_registry();
    auto defender = create_test_unit("5 Model Defender", 4, {});
    auto weapon = create_test_weapon("Blast 3 Rifle", true, {
        CompactRule(RuleId::Blast, 3)
    });

    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto new_result = resolver.apply_hit_multiplication(
        defender, weapon, 5, 1, 0, false);
    auto old_result = compute_hit_multiplication_old_style(
        defender, weapon, 5, 1, 0, false);

    assert(new_result.total_hits == old_result.total_hits);
    assert(new_result.multiplier_used == 3);
    assert(new_result.total_hits == 15);     // 5 * 3
    assert(new_result.rending_hits == 3);    // 1 * 3
}

TEST(hit_multiplication_blast_capped_by_models) {
    auto registry = create_default_registry();
    auto defender = create_test_unit("3 Model Defender", 4, {});
    defender.alive_count = 3;  // Only 3 models alive
    auto weapon = create_test_weapon("Blast 6 Rifle", true, {
        CompactRule(RuleId::Blast, 6)
    });

    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto new_result = resolver.apply_hit_multiplication(
        defender, weapon, 4, 0, 0, false);
    auto old_result = compute_hit_multiplication_old_style(
        defender, weapon, 4, 0, 0, false);

    assert(new_result.total_hits == old_result.total_hits);
    assert(new_result.multiplier_used == 3);  // Capped at 3 models
    assert(new_result.total_hits == 12);      // 4 * 3
}

TEST(hit_multiplication_blast_with_takedown) {
    auto registry = create_default_registry();
    auto defender = create_test_unit("5 Model Defender", 4, {});
    auto weapon = create_test_weapon("Blast 3 Rifle", true, {
        CompactRule(RuleId::Blast, 3)
    });

    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    // Takedown should cap multiplier at 1
    auto new_result = resolver.apply_hit_multiplication(
        defender, weapon, 5, 1, 1, true);  // has_takedown = true
    auto old_result = compute_hit_multiplication_old_style(
        defender, weapon, 5, 1, 1, true);

    assert(new_result.total_hits == old_result.total_hits);
    assert(new_result.multiplier_used == 1);  // Capped at 1 for Takedown
    assert(new_result.total_hits == 5);       // No multiplication
}

TEST(hit_multiplication_blast_all_hit_types) {
    auto registry = create_default_registry();
    auto defender = create_test_unit("5 Model Defender", 4, {});
    auto weapon = create_test_weapon("Blast 2 Rifle", true, {
        CompactRule(RuleId::Blast, 2)
    });

    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto new_result = resolver.apply_hit_multiplication(
        defender, weapon, 8, 2, 3, false);
    auto old_result = compute_hit_multiplication_old_style(
        defender, weapon, 8, 2, 3, false);

    assert(new_result.total_hits == old_result.total_hits);
    assert(new_result.rending_hits == old_result.rending_hits);
    assert(new_result.rupture_hits == old_result.rupture_hits);
    assert(new_result.multiplier_used == 2);
    assert(new_result.total_hits == 16);      // 8 * 2
    assert(new_result.rending_hits == 4);     // 2 * 2
    assert(new_result.rupture_hits == 6);     // 3 * 2
}

//==============================================================================
// Cross-Phase Integration Tests
//==============================================================================

TEST(full_hit_pipeline_rending_blast) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 3, {});
    auto defender = create_test_unit("5 Model Defender", 4, {});
    auto weapon = create_test_weapon("Rending Blast Rifle", true, {
        CompactRule(RuleId::Rending, 0),
        CompactRule(RuleId::Blast, 3)
    });

    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    // Simulate 10 hits with 4 natural sixes
    u32 total_hits = 10;
    u32 natural_sixes = 4;

    // Step 1: Hit Separation
    auto sep_result = resolver.apply_hit_separation(
        attacker, defender, weapon, CombatType::SHOOTING, total_hits, natural_sixes);

    assert(sep_result.rending_hits == 4);
    assert(sep_result.normal_hits == 6);

    // Step 2: Hit Multiplication
    auto mult_result = resolver.apply_hit_multiplication(
        defender, weapon, sep_result.normal_hits + sep_result.rending_hits,
        sep_result.rending_hits, sep_result.rupture_hits, false);

    assert(mult_result.multiplier_used == 3);
    assert(mult_result.total_hits == 30);     // 10 * 3
    assert(mult_result.rending_hits == 12);   // 4 * 3
}

//==============================================================================
// Main
//==============================================================================

int main() {
    std::cout << "=== Phase 4: Combat Sub-Phases A/B Test ===\n\n";

    std::cout << "HIT_SEPARATION Tests:\n";
    RUN_TEST(hit_separation_no_rules);
    RUN_TEST(hit_separation_rending);
    RUN_TEST(hit_separation_rupture);
    RUN_TEST(hit_separation_rending_and_rupture);

    std::cout << "\nHIT_BONUSES Tests:\n";
    RUN_TEST(hit_bonuses_no_rules);
    RUN_TEST(hit_bonuses_relentless_far);
    RUN_TEST(hit_bonuses_relentless_close);
    RUN_TEST(hit_bonuses_surge);
    RUN_TEST(hit_bonuses_point_blank_surge);
    RUN_TEST(hit_bonuses_furious_charging);
    RUN_TEST(hit_bonuses_furious_not_charging);
    RUN_TEST(hit_bonuses_multiple_rules);

    std::cout << "\nHIT_MULTIPLICATION Tests:\n";
    RUN_TEST(hit_multiplication_no_blast);
    RUN_TEST(hit_multiplication_blast_3);
    RUN_TEST(hit_multiplication_blast_capped_by_models);
    RUN_TEST(hit_multiplication_blast_with_takedown);
    RUN_TEST(hit_multiplication_blast_all_hit_types);

    std::cout << "\nIntegration Tests:\n";
    RUN_TEST(full_hit_pipeline_rending_blast);

    std::cout << "\n=== All Phase 4 Tests Passed ===\n";
    return 0;
}
