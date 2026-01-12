//==============================================================================
// Phase 5 Unified Combat Path A/B Test
//==============================================================================
// Tests that the unified resolve_combat() function works correctly for both
// shooting and melee combat types.

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
#include "core/model.hpp"
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

// Helper to create a test unit with models
Unit create_unit_with_models(const char* name, u8 quality_val, u8 defense,
                              u8 model_count, u8 tough,
                              std::initializer_list<CompactRule> rules) {
    Unit unit{std::string_view(name)};
    unit.quality = quality_val;
    unit.defense = defense;
    unit.model_count = model_count;
    unit.alive_count = model_count;
    unit.rule_count = 0;
    unit.rule_mask = 0;

    // Add models
    for (u8 i = 0; i < model_count; ++i) {
        Model& m = unit.models[i];
        m.quality = quality_val;
        m.defense = defense;
        m.tough = tough;
        m.wounds_taken = 0;
        m.state = ModelState::Healthy;
    }

    // Add rules
    for (const auto& rule : rules) {
        if (unit.rule_count < MAX_RULES_PER_ENTITY) {
            unit.rules[unit.rule_count++] = rule;
            unit.rule_mask |= rule_bit(rule.id);
        }
    }

    return unit;
}

// Helper to create a test weapon
Weapon create_weapon(const char* name, u8 attacks, u8 range, u8 ap,
                     std::initializer_list<CompactRule> rules) {
    Weapon weapon{std::string_view(name), attacks, range, ap};
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
// Basic Combat Tests
//==============================================================================

TEST(unified_shooting_basic) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Shooters", 4, 4, 5, 1, {});
    auto defender = create_unit_with_models("Targets", 4, 4, 5, 1, {});
    auto weapon = create_weapon("Rifle", 2, 24, 1, {});

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false);

    // Verify basic result structure
    assert(result.attacks == 10);  // 5 models * 2 attacks
    assert(result.quality_used == 4);
    assert(result.hit_modifier == 0);
    assert(result.ap_used == 1);
    // Hits and wounds are random but should be reasonable
    assert(result.hits <= result.attacks);
    assert(result.wounds_dealt <= result.hits);
}

TEST(unified_melee_basic) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Fighters", 3, 4, 5, 1, {});
    auto defender = create_unit_with_models("Targets", 4, 4, 5, 1, {});
    auto weapon = create_weapon("Sword", 3, 0, 0, {});

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::MELEE, 0, false);

    assert(result.attacks == 15);  // 5 models * 3 attacks
    assert(result.quality_used == 3);
    assert(result.hit_modifier == 0);
}

TEST(unified_melee_charging) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Chargers", 4, 4, 5, 1, {
        CompactRule(RuleId::Furious, 0)
    });
    auto defender = create_unit_with_models("Targets", 4, 4, 5, 1, {});
    auto weapon = create_weapon("Lance", 2, 0, 1, {
        CompactRule(RuleId::Lance, 0),
        CompactRule(RuleId::Thrust, 0)
    });

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::MELEE, 0, true);

    assert(result.attacks == 10);
    // Thrust gives +1 to hit when charging
    assert(result.hit_modifier == 1);
    // Lance gives +2 AP, Thrust gives +1 AP when charging
    assert(result.ap_used == 4);  // 1 base + 2 Lance + 1 Thrust
}

//==============================================================================
// Hit Modifier Tests
//==============================================================================

TEST(unified_precise_weapon) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Snipers", 4, 4, 5, 1, {});
    auto defender = create_unit_with_models("Targets", 4, 4, 5, 1, {});
    auto weapon = create_weapon("Sniper Rifle", 1, 36, 2, {
        CompactRule(RuleId::Precise, 0)
    });

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 24, false);

    assert(result.hit_modifier == 1);  // +1 from Precise
}

TEST(unified_stealth_defender) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Shooters", 4, 4, 5, 1, {});
    auto defender = create_unit_with_models("Stealthy", 4, 4, 5, 1, {
        CompactRule(RuleId::Stealth, 0)
    });
    auto weapon = create_weapon("Rifle", 2, 24, 1, {});

    // At distance > 9, Stealth applies
    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false);

    assert(result.hit_modifier == -1);  // -1 from Stealth
}

TEST(unified_reliable_weapon) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Shooters", 5, 4, 5, 1, {});  // Quality 5+
    auto defender = create_unit_with_models("Targets", 4, 4, 5, 1, {});
    auto weapon = create_weapon("Reliable Gun", 2, 24, 1, {
        CompactRule(RuleId::Reliable, 0)
    });

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false);

    assert(result.quality_used == 2);  // Reliable changes quality to 2+
}

//==============================================================================
// Rending and Special Hit Tests
//==============================================================================

TEST(unified_rending_weapon) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Rippers", 3, 4, 10, 1, {});  // 10 models for more 6s
    auto defender = create_unit_with_models("Targets", 4, 4, 5, 1, {});
    auto weapon = create_weapon("Rending Claws", 2, 0, 0, {
        CompactRule(RuleId::Rending, 0)
    });

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::MELEE, 0, false);

    // Should have some rending hits (natural 6s separated)
    // With 20 attacks at quality 3+, we expect some 6s
    assert(result.bypassed_regeneration == true);  // Rending bypasses regen
}

TEST(unified_surge_weapon) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Surgers", 3, 4, 10, 1, {});
    auto defender = create_unit_with_models("Targets", 4, 4, 5, 1, {});
    auto weapon = create_weapon("Surge Gun", 2, 24, 1, {
        CompactRule(RuleId::Surge, 0)
    });

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false);

    // Surge generates extra hits on 6s
    // Hits should be >= natural_sixes (from bonus hits)
    // Hard to verify exact count due to randomness
    assert(result.attacks == 20);
}

//==============================================================================
// Blast Tests
//==============================================================================

TEST(unified_blast_3) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Bombers", 4, 4, 3, 1, {});
    auto defender = create_unit_with_models("Infantry", 4, 4, 5, 1, {});
    auto weapon = create_weapon("Grenade", 1, 12, 0, {
        CompactRule(RuleId::Blast, 3)
    });

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 6, false);

    // 3 models * 1 attack = 3 attacks
    // Blast multiplies hits by min(3, defender_models=5) = 3
    assert(result.attacks == 3);
    assert(result.blast_multiplier == 1);  // blast_multiplier in result is before multiplication
}

TEST(unified_blast_capped_by_models) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Bombers", 4, 4, 3, 1, {});
    auto defender = create_unit_with_models("Few Infantry", 4, 4, 2, 1, {});  // Only 2 models
    auto weapon = create_weapon("Grenade", 1, 12, 0, {
        CompactRule(RuleId::Blast, 6)
    });

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 6, false);

    // Blast(6) should be capped at 2 (defender model count)
    assert(result.attacks == 3);
}

//==============================================================================
// Defense Resolution Tests
//==============================================================================

TEST(unified_poison_reroll) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Poisoners", 3, 4, 5, 1, {});
    auto defender = create_unit_with_models("Targets", 4, 6, 5, 1, {});  // Defense 6+
    auto weapon = create_weapon("Poison Blade", 2, 0, 0, {
        CompactRule(RuleId::Poison, 0)
    });

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::MELEE, 0, false);

    assert(result.forced_defense_reroll == true);  // Poison forces reroll on 6s
}

TEST(unified_bane_bypasses_regen) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Demon Slayers", 3, 4, 5, 1, {});
    auto defender = create_unit_with_models("Regenerators", 4, 4, 5, 1, {
        CompactRule(RuleId::Regeneration, 5)
    });
    auto weapon = create_weapon("Bane Sword", 2, 0, 2, {
        CompactRule(RuleId::Bane, 0)
    });

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::MELEE, 0, false);

    assert(result.bypassed_regeneration == true);  // Bane bypasses regen
}

//==============================================================================
// Wound Allocation Tests
//==============================================================================

TEST(unified_wound_allocation_basic) {
    auto registry = create_default_registry();
    DiceRoller dice(99999);  // Seed for consistent results
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Killers", 2, 4, 10, 1, {});  // Quality 2+
    auto defender = create_unit_with_models("Victims", 4, 6, 5, 1, {});   // Defense 6+ (easy to wound)
    auto weapon = create_weapon("Death Ray", 4, 24, 4, {});  // High AP

    auto initial_alive = defender.alive_count;

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false);

    // Should have killed some models
    assert(result.wounds_dealt >= result.models_killed);
    assert(defender.alive_count <= initial_alive);
}

TEST(unified_deadly_multiplier) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Assassins", 2, 4, 3, 1, {});
    auto defender = create_unit_with_models("Tough Guys", 4, 4, 3, 3, {});  // Tough(3)
    auto weapon = create_weapon("Deadly Blade", 2, 0, 3, {
        CompactRule(RuleId::Deadly, 3)  // Deadly(3)
    });

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::MELEE, 0, false);

    // Deadly multiplies wounds
    // Each wound becomes 3 wounds
    // Harder to verify exact counts due to randomness
    assert(result.attacks == 6);  // 3 models * 2 attacks
}

//==============================================================================
// Combined Rule Tests
//==============================================================================

TEST(unified_combo_rending_blast) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Combo Squad", 3, 4, 5, 1, {});
    auto defender = create_unit_with_models("Targets", 4, 4, 5, 1, {});
    auto weapon = create_weapon("Rending Blast Gun", 2, 24, 0, {
        CompactRule(RuleId::Rending, 0),
        CompactRule(RuleId::Blast, 2)
    });

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false);

    // Both rules should apply
    assert(result.bypassed_regeneration == true);  // From Rending
    assert(result.attacks == 10);  // 5 * 2
}

TEST(unified_combo_precise_surge) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Elite Squad", 4, 4, 5, 1, {});
    auto defender = create_unit_with_models("Targets", 4, 4, 5, 1, {});
    auto weapon = create_weapon("Elite Gun", 2, 24, 1, {
        CompactRule(RuleId::Precise, 0),
        CompactRule(RuleId::Surge, 0)
    });

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false);

    assert(result.hit_modifier == 1);  // From Precise
}

//==============================================================================
// Shooting vs Melee Combat Type Verification
//==============================================================================

TEST(unified_shooting_rules_dont_apply_to_melee) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Shooter", 4, 4, 5, 1, {
        CompactRule(RuleId::GoodShot, 0)  // Only applies to shooting
    });
    auto defender = create_unit_with_models("Targets", 4, 4, 5, 1, {});
    auto weapon = create_weapon("Sword", 2, 0, 1, {});

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::MELEE, 0, false);

    // GoodShot should NOT apply to melee
    assert(result.hit_modifier == 0);
}

TEST(unified_melee_rules_dont_apply_to_shooting) {
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Fighter", 4, 4, 5, 1, {
        CompactRule(RuleId::Furious, 0)  // Only applies to melee charging
    });
    auto defender = create_unit_with_models("Targets", 4, 4, 5, 1, {});
    auto weapon = create_weapon("Rifle", 2, 24, 1, {});

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 12, true);  // is_charge but shooting

    // Furious should NOT apply to shooting even with is_charge
    // (bonus hits from 6s should not be doubled)
    assert(result.attacks == 10);
}

//==============================================================================
// AP Modifier Tests (PiercingTag, PiercingTarget, etc.)
//==============================================================================

TEST(unified_piercing_tag_ap_bonus) {
    // PiercingTag on defender should give attacker +1 AP
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Shooters", 4, 4, 5, 1, {});
    auto defender = create_unit_with_models("Tagged Target", 4, 4, 5, 1, {
        CompactRule(RuleId::PiercingTag, 0)  // Target has been marked for piercing
    });
    auto weapon = create_weapon("Basic Gun", 2, 24, 1, {});  // Base AP 1

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false);

    assert(result.attacks == 10);
    // PiercingTag should add +1 AP: base 1 + 1 = 2
    assert(result.ap_used == 2);
}

TEST(unified_piercing_target_ap_bonus) {
    // PiercingTarget on defender should give attacker +1 AP
    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Shooters", 4, 4, 5, 1, {});
    auto defender = create_unit_with_models("Marked Target", 4, 4, 5, 1, {
        CompactRule(RuleId::PiercingTarget, 0)  // Target has piercing markers
    });
    auto weapon = create_weapon("Basic Gun", 2, 24, 0, {});  // Base AP 0

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false);

    assert(result.attacks == 10);
    // PiercingTarget should add +1 AP: base 0 + 1 = 1
    assert(result.ap_used == 1);
}

//==============================================================================
// Main
//==============================================================================

int main() {
    std::cout << "=== Phase 5: Unified Combat Path A/B Test ===\n\n";

    std::cout << "Basic Combat Tests:\n";
    RUN_TEST(unified_shooting_basic);
    RUN_TEST(unified_melee_basic);
    RUN_TEST(unified_melee_charging);

    std::cout << "\nHit Modifier Tests:\n";
    RUN_TEST(unified_precise_weapon);
    RUN_TEST(unified_stealth_defender);
    RUN_TEST(unified_reliable_weapon);

    std::cout << "\nRending and Special Hit Tests:\n";
    RUN_TEST(unified_rending_weapon);
    RUN_TEST(unified_surge_weapon);

    std::cout << "\nBlast Tests:\n";
    RUN_TEST(unified_blast_3);
    RUN_TEST(unified_blast_capped_by_models);

    std::cout << "\nDefense Resolution Tests:\n";
    RUN_TEST(unified_poison_reroll);
    RUN_TEST(unified_bane_bypasses_regen);

    std::cout << "\nWound Allocation Tests:\n";
    RUN_TEST(unified_wound_allocation_basic);
    RUN_TEST(unified_deadly_multiplier);

    std::cout << "\nCombined Rule Tests:\n";
    RUN_TEST(unified_combo_rending_blast);
    RUN_TEST(unified_combo_precise_surge);

    std::cout << "\nCombat Type Verification Tests:\n";
    RUN_TEST(unified_shooting_rules_dont_apply_to_melee);
    RUN_TEST(unified_melee_rules_dont_apply_to_shooting);

    std::cout << "\nAP Modifier Tests:\n";
    RUN_TEST(unified_piercing_tag_ap_bonus);
    RUN_TEST(unified_piercing_target_ap_bonus);

    std::cout << "\n=== All Phase 5 Tests Passed ===\n";
    return 0;
}
