//==============================================================================
// PiercingTag Test - Verifies that ap_modifier is correctly applied
//==============================================================================

#include <iostream>
#include <cassert>

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

    for (u8 i = 0; i < model_count; ++i) {
        Model& m = unit.models[i];
        m.quality = quality_val;
        m.defense = defense;
        m.tough = tough;
        m.wounds_taken = 0;
        m.state = ModelState::Healthy;
    }

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

void test_piercing_tag_ap_bonus() {
    std::cout << "  Testing PiercingTag AP bonus... ";

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

    assert(result.attacks == 10);  // 5 models * 2 attacks
    // PiercingTag should add +1 AP: base 1 + 1 = 2
    if (result.ap_used != 2) {
        std::cerr << "FAIL: Expected ap_used=2, got " << static_cast<int>(result.ap_used) << std::endl;
        exit(1);
    }
    std::cout << "PASS\n";
}

void test_piercing_target_ap_bonus() {
    std::cout << "  Testing PiercingTarget AP bonus... ";

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

    std::cout << "attacks=" << result.attacks
              << " ap_used=" << static_cast<int>(result.ap_used) << std::endl;

    assert(result.attacks == 10);
    // PiercingTarget should add +1 AP: base 0 + 1 = 1
    if (result.ap_used != 1) {
        std::cerr << "FAIL: Expected ap_used=1, got " << static_cast<int>(result.ap_used) << std::endl;
        exit(1);
    }
    std::cout << "PASS\n";
}

void test_no_piercing_bonus_without_rule() {
    std::cout << "  Testing no AP bonus without PiercingTag... ";

    auto registry = create_default_registry();
    DiceRoller dice(12345);
    RegistryCombatResolver resolver(registry, dice);

    auto attacker = create_unit_with_models("Shooters", 4, 4, 5, 1, {});
    auto defender = create_unit_with_models("Normal Target", 4, 4, 5, 1, {});  // No PiercingTag
    auto weapon = create_weapon("Basic Gun", 2, 24, 1, {});  // Base AP 1

    auto result = resolver.resolve_combat(
        attacker, defender, weapon, CombatType::SHOOTING, 12, false);

    std::cout << "attacks=" << result.attacks
              << " ap_used=" << static_cast<int>(result.ap_used) << std::endl;

    assert(result.attacks == 10);
    // No PiercingTag, so AP should stay at base: 1
    if (result.ap_used != 1) {
        std::cerr << "FAIL: Expected ap_used=1, got " << static_cast<int>(result.ap_used) << std::endl;
        exit(1);
    }
    std::cout << "PASS\n";
}

int main() {
    std::cout << "=== PiercingTag AP Modifier Test ===\n\n";

    test_piercing_tag_ap_bonus();
    test_piercing_target_ap_bonus();
    test_no_piercing_bonus_without_rule();

    std::cout << "\n=== All PiercingTag Tests Passed ===\n";
    return 0;
}
