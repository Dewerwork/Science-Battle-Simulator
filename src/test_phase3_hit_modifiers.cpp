//==============================================================================
// Phase 3 Hit Modifiers A/B Test
//==============================================================================
// Tests that the registry-based hit modifier implementation produces identical
// results to the original hardcoded implementation in combat_engine.hpp.

#include <iostream>
#include <cassert>
#include <cstring>

// Include validation header to verify static_asserts at compile time
#include "core/rule_architecture_validation.hpp"

// Include the new rule architecture headers
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
    Unit unit{std::string_view(name)};  // Brace initialization to avoid vexing parse
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
    Weapon weapon{std::string_view(name), u8(2), u8(ranged ? 24 : 0), u8(1)};  // Brace initialization
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
// A/B Test: Compare Old and New Hit Modifier Implementations
//==============================================================================

struct ABTestResult {
    bool passed = true;
    const char* test_name = nullptr;
    i8 old_modifier = 0;
    i8 new_modifier = 0;
    u8 old_quality = 0;
    u8 new_quality = 0;
    const char* mismatch_detail = nullptr;
};

ABTestResult run_ab_test(
    const char* test_name,
    const Unit& attacker,
    const Unit& defender,
    const Weapon& weapon,
    CombatType combat_type,
    u8 distance,
    bool is_charge,
    const RuleRegistry& registry
) {
    ABTestResult result;
    result.test_name = test_name;

    // Compute using old-style (hardcoded) implementation
    result.old_modifier = compute_hit_modifier_old_style(
        attacker, defender, weapon, combat_type, distance, is_charge);
    result.old_quality = compute_quality_old_style(attacker, weapon);

    // Create resolver and compute using new registry-based implementation
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);
    auto new_result = resolver.apply_hit_modifiers(
        attacker, defender, weapon, combat_type, distance, is_charge);

    result.new_modifier = new_result.hit_modifier;
    result.new_quality = new_result.quality_override > 0 ?
                         new_result.quality_override : attacker.quality;

    // Compare
    if (result.old_modifier != result.new_modifier) {
        result.passed = false;
        result.mismatch_detail = "hit_modifier_mismatch";
    } else if (result.old_quality != result.new_quality) {
        result.passed = false;
        result.mismatch_detail = "quality_mismatch";
    }

    return result;
}

void print_ab_result(const ABTestResult& result) {
    if (result.passed) {
        std::cout << "[PASS] " << result.test_name
                  << " (modifier=" << static_cast<int>(result.old_modifier)
                  << ", quality=" << static_cast<int>(result.old_quality) << ")\n";
    } else {
        std::cout << "[FAIL] " << result.test_name << " - " << result.mismatch_detail << "\n";
        std::cout << "  Old: modifier=" << static_cast<int>(result.old_modifier)
                  << ", quality=" << static_cast<int>(result.old_quality) << "\n";
        std::cout << "  New: modifier=" << static_cast<int>(result.new_modifier)
                  << ", quality=" << static_cast<int>(result.new_quality) << "\n";
    }
}

//==============================================================================
// Shooting Hit Modifier Tests
//==============================================================================

TEST(shooting_no_modifiers) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Rifle", true, {});

    auto result = run_ab_test("shooting_no_modifiers",
        attacker, defender, weapon, CombatType::SHOOTING, 12, false, registry);
    print_ab_result(result);
    assert(result.passed);
}

TEST(shooting_precise_weapon) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Precise Rifle", true, {
        CompactRule(RuleId::Precise, 0)
    });

    auto result = run_ab_test("shooting_precise_weapon",
        attacker, defender, weapon, CombatType::SHOOTING, 12, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 1);  // +1 from Precise
}

TEST(shooting_stealth_defender_far) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Stealth Defender", 4, {
        CompactRule(RuleId::Stealth, 0)
    });
    auto weapon = create_test_weapon("Rifle", true, {});

    // Distance > 9 should apply Stealth
    auto result = run_ab_test("shooting_stealth_defender_far",
        attacker, defender, weapon, CombatType::SHOOTING, 12, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == -1);  // -1 from Stealth
}

TEST(shooting_stealth_defender_close) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Stealth Defender", 4, {
        CompactRule(RuleId::Stealth, 0)
    });
    auto weapon = create_test_weapon("Rifle", true, {});

    // Distance <= 9 should NOT apply Stealth
    auto result = run_ab_test("shooting_stealth_defender_close",
        attacker, defender, weapon, CombatType::SHOOTING, 8, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 0);  // No modifier (Stealth doesn't apply at close range)
}

TEST(shooting_ranged_shrouding) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Shrouded Defender", 4, {
        CompactRule(RuleId::RangedShrouding, 0)
    });
    auto weapon = create_test_weapon("Rifle", true, {});

    auto result = run_ab_test("shooting_ranged_shrouding",
        attacker, defender, weapon, CombatType::SHOOTING, 12, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == -1);  // -1 from RangedShrouding
}

TEST(shooting_good_shot) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Good Shooter", 4, {
        CompactRule(RuleId::GoodShot, 0)
    });
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Rifle", true, {});

    auto result = run_ab_test("shooting_good_shot",
        attacker, defender, weapon, CombatType::SHOOTING, 12, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 1);  // +1 from GoodShot
}

TEST(shooting_bad_shot) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Bad Shooter", 4, {
        CompactRule(RuleId::BadShot, 0)
    });
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Rifle", true, {});

    auto result = run_ab_test("shooting_bad_shot",
        attacker, defender, weapon, CombatType::SHOOTING, 12, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == -1);  // -1 from BadShot
}

TEST(shooting_purge_vs_tough_3) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Tough Defender", 4, {
        CompactRule(RuleId::Tough, 3)
    });
    auto weapon = create_test_weapon("Purge Rifle", true, {
        CompactRule(RuleId::Purge, 0)
    });

    auto result = run_ab_test("shooting_purge_vs_tough_3",
        attacker, defender, weapon, CombatType::SHOOTING, 12, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 1);  // +1 from Purge vs Tough(3+)
}

TEST(shooting_purge_vs_tough_2) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Weak Tough Defender", 4, {
        CompactRule(RuleId::Tough, 2)
    });
    auto weapon = create_test_weapon("Purge Rifle", true, {
        CompactRule(RuleId::Purge, 0)
    });

    auto result = run_ab_test("shooting_purge_vs_tough_2",
        attacker, defender, weapon, CombatType::SHOOTING, 12, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 0);  // No modifier (Tough < 3)
}

TEST(shooting_reliable_weapon) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Reliable Rifle", true, {
        CompactRule(RuleId::Reliable, 0)
    });

    auto result = run_ab_test("shooting_reliable_weapon",
        attacker, defender, weapon, CombatType::SHOOTING, 12, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_quality == 2);  // Quality becomes 2+ with Reliable
}

TEST(shooting_multiple_modifiers) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Good Shooter", 4, {
        CompactRule(RuleId::GoodShot, 0)
    });
    auto defender = create_test_unit("Stealth Defender", 4, {
        CompactRule(RuleId::Stealth, 0)
    });
    auto weapon = create_test_weapon("Precise Rifle", true, {
        CompactRule(RuleId::Precise, 0)
    });

    // At distance 12: Stealth -1, Precise +1, GoodShot +1 = +1 total
    auto result = run_ab_test("shooting_multiple_modifiers",
        attacker, defender, weapon, CombatType::SHOOTING, 12, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 1);  // +1 from Precise + +1 from GoodShot - 1 from Stealth
}

//==============================================================================
// Melee Hit Modifier Tests
//==============================================================================

TEST(melee_no_modifiers) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Sword", false, {});

    auto result = run_ab_test("melee_no_modifiers",
        attacker, defender, weapon, CombatType::MELEE, 0, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 0);
}

TEST(melee_precise_weapon) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Precise Sword", false, {
        CompactRule(RuleId::Precise, 0)
    });

    auto result = run_ab_test("melee_precise_weapon",
        attacker, defender, weapon, CombatType::MELEE, 0, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 1);  // +1 from Precise
}

TEST(melee_thrust_charging) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Lance", false, {
        CompactRule(RuleId::Thrust, 0)
    });

    // Charging should apply Thrust
    auto result = run_ab_test("melee_thrust_charging",
        attacker, defender, weapon, CombatType::MELEE, 0, true, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 1);  // +1 from Thrust when charging
}

TEST(melee_thrust_not_charging) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Lance", false, {
        CompactRule(RuleId::Thrust, 0)
    });

    // Not charging should NOT apply Thrust
    auto result = run_ab_test("melee_thrust_not_charging",
        attacker, defender, weapon, CombatType::MELEE, 0, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 0);  // No modifier when not charging
}

TEST(melee_evasion_defender) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Evasive Defender", 4, {
        CompactRule(RuleId::MeleeEvasion, 0)
    });
    auto weapon = create_test_weapon("Sword", false, {});

    auto result = run_ab_test("melee_evasion_defender",
        attacker, defender, weapon, CombatType::MELEE, 0, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == -1);  // -1 from MeleeEvasion
}

TEST(melee_shrouding_defender) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Shrouded Defender", 4, {
        CompactRule(RuleId::MeleeShrouding, 0)
    });
    auto weapon = create_test_weapon("Sword", false, {});

    auto result = run_ab_test("melee_shrouding_defender",
        attacker, defender, weapon, CombatType::MELEE, 0, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == -1);  // -1 from MeleeShrouding
}

TEST(melee_purge_vs_tough) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Tough Defender", 4, {
        CompactRule(RuleId::Tough, 3)
    });
    auto weapon = create_test_weapon("Purge Sword", false, {
        CompactRule(RuleId::Purge, 0)
    });

    auto result = run_ab_test("melee_purge_vs_tough",
        attacker, defender, weapon, CombatType::MELEE, 0, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 1);  // +1 from Purge vs Tough(3+)
}

TEST(melee_multiple_modifiers) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Evasive Shrouded", 4, {
        CompactRule(RuleId::MeleeEvasion, 0),
        CompactRule(RuleId::MeleeShrouding, 0)
    });
    auto weapon = create_test_weapon("Precise Lance", false, {
        CompactRule(RuleId::Precise, 0),
        CompactRule(RuleId::Thrust, 0)
    });

    // Charging: Precise +1, Thrust +1, MeleeEvasion -1, MeleeShrouding -1 = 0
    auto result = run_ab_test("melee_multiple_modifiers",
        attacker, defender, weapon, CombatType::MELEE, 0, true, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 0);  // +1 + +1 - 1 - 1 = 0
}

//==============================================================================
// Cross-Combat Type Tests (verify rules don't apply incorrectly)
//==============================================================================

TEST(shooting_melee_evasion_not_applied) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Evasive Defender", 4, {
        CompactRule(RuleId::MeleeEvasion, 0)
    });
    auto weapon = create_test_weapon("Rifle", true, {});

    // MeleeEvasion should NOT apply to shooting
    auto result = run_ab_test("shooting_melee_evasion_not_applied",
        attacker, defender, weapon, CombatType::SHOOTING, 12, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 0);  // No modifier (MeleeEvasion only in melee)
}

TEST(melee_stealth_not_applied) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Stealth Defender", 4, {
        CompactRule(RuleId::Stealth, 0)
    });
    auto weapon = create_test_weapon("Sword", false, {});

    // Stealth should NOT apply to melee
    auto result = run_ab_test("melee_stealth_not_applied",
        attacker, defender, weapon, CombatType::MELEE, 0, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 0);  // No modifier (Stealth only for ranged)
}

TEST(melee_ranged_shrouding_not_applied) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Shrouded Defender", 4, {
        CompactRule(RuleId::RangedShrouding, 0)
    });
    auto weapon = create_test_weapon("Sword", false, {});

    // RangedShrouding should NOT apply to melee
    auto result = run_ab_test("melee_ranged_shrouding_not_applied",
        attacker, defender, weapon, CombatType::MELEE, 0, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 0);  // No modifier
}

TEST(melee_good_shot_not_applied) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Good Shooter", 4, {
        CompactRule(RuleId::GoodShot, 0)
    });
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Sword", false, {});

    // GoodShot should NOT apply to melee
    auto result = run_ab_test("melee_good_shot_not_applied",
        attacker, defender, weapon, CombatType::MELEE, 0, false, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 0);  // No modifier
}

TEST(shooting_thrust_not_applied) {
    auto registry = create_default_registry();
    auto attacker = create_test_unit("Attacker", 4, {});
    auto defender = create_test_unit("Defender", 4, {});
    auto weapon = create_test_weapon("Rifle", true, {
        CompactRule(RuleId::Thrust, 0)
    });

    // Thrust should NOT apply to shooting (even if weapon has it somehow)
    auto result = run_ab_test("shooting_thrust_not_applied",
        attacker, defender, weapon, CombatType::SHOOTING, 12, true, registry);
    print_ab_result(result);
    assert(result.passed);
    assert(result.old_modifier == 0);  // No modifier
}

//==============================================================================
// Main
//==============================================================================

int main() {
    std::cout << "=== Phase 3: Hit Modifiers A/B Test ===\n\n";

    std::cout << "Shooting Hit Modifier Tests:\n";
    RUN_TEST(shooting_no_modifiers);
    RUN_TEST(shooting_precise_weapon);
    RUN_TEST(shooting_stealth_defender_far);
    RUN_TEST(shooting_stealth_defender_close);
    RUN_TEST(shooting_ranged_shrouding);
    RUN_TEST(shooting_good_shot);
    RUN_TEST(shooting_bad_shot);
    RUN_TEST(shooting_purge_vs_tough_3);
    RUN_TEST(shooting_purge_vs_tough_2);
    RUN_TEST(shooting_reliable_weapon);
    RUN_TEST(shooting_multiple_modifiers);

    std::cout << "\nMelee Hit Modifier Tests:\n";
    RUN_TEST(melee_no_modifiers);
    RUN_TEST(melee_precise_weapon);
    RUN_TEST(melee_thrust_charging);
    RUN_TEST(melee_thrust_not_charging);
    RUN_TEST(melee_evasion_defender);
    RUN_TEST(melee_shrouding_defender);
    RUN_TEST(melee_purge_vs_tough);
    RUN_TEST(melee_multiple_modifiers);

    std::cout << "\nCross-Combat Type Tests:\n";
    RUN_TEST(shooting_melee_evasion_not_applied);
    RUN_TEST(melee_stealth_not_applied);
    RUN_TEST(melee_ranged_shrouding_not_applied);
    RUN_TEST(melee_good_shot_not_applied);
    RUN_TEST(shooting_thrust_not_applied);

    std::cout << "\n=== All Phase 3 Tests Passed ===\n";
    return 0;
}
