//==============================================================================
// Rule Architecture Unit Tests
//==============================================================================
// Tests for Phase 1-2 of the Rule Registry Architecture implementation.

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
#include "rules/combat_rules.hpp"

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

//==============================================================================
// RuleId Tests
//==============================================================================

TEST(rule_id_enum_size) {
    // RuleId should be u16 (2 bytes)
    assert(sizeof(RuleId) == 2);
}

TEST(rule_id_primary_count) {
    // PRIMARY_COUNT should be exactly 64
    assert(static_cast<size_t>(RuleId::PRIMARY_COUNT) == 64);
}

TEST(rule_id_count_limit) {
    // COUNT should be less than 320
    assert(static_cast<size_t>(RuleId::COUNT) <= 320);
}

TEST(is_primary_rule) {
    // Check primary rule detection
    assert(is_primary_rule(RuleId::Precise));
    assert(is_primary_rule(RuleId::Stealth));
    assert(is_primary_rule(RuleId::Rending));
    assert(is_primary_rule(RuleId::Blast));

    // None is not a primary rule
    assert(!is_primary_rule(RuleId::None));

    // Extended rules are not primary
    assert(!is_primary_rule(RuleId::Devout));
    assert(!is_primary_rule(RuleId::Battleborn));
}

TEST(is_extended_rule) {
    // Check extended rule detection
    assert(is_extended_rule(RuleId::Devout));
    assert(is_extended_rule(RuleId::Battleborn));

    // Primary rules are not extended
    assert(!is_extended_rule(RuleId::Precise));
    assert(!is_extended_rule(RuleId::None));
}

//==============================================================================
// RulePresence Tests
//==============================================================================

TEST(rule_presence_primary_only) {
    RulePresence presence;

    // Add primary rules
    presence.set_rule(RuleId::Precise);
    presence.set_rule(RuleId::Rending);
    presence.set_rule(RuleId::Blast);

    // Check they're present
    assert(presence.has_rule(RuleId::Precise));
    assert(presence.has_rule(RuleId::Rending));
    assert(presence.has_rule(RuleId::Blast));

    // Check others are not
    assert(!presence.has_rule(RuleId::Stealth));
    assert(!presence.has_rule(RuleId::None));

    // No extended allocation needed
    assert(!presence.has_extended());
}

TEST(rule_presence_extended) {
    RulePresence presence;

    // Add extended rule
    presence.set_rule(RuleId::Devout);

    // Check it's present
    assert(presence.has_rule(RuleId::Devout));

    // Extended allocation should happen
    assert(presence.has_extended());
}

TEST(rule_presence_mixed) {
    RulePresence presence;

    // Add both primary and extended rules
    presence.set_rule(RuleId::Precise);
    presence.set_rule(RuleId::Devout);

    // Check both are present
    assert(presence.has_rule(RuleId::Precise));
    assert(presence.has_rule(RuleId::Devout));

    // Count should be 2
    assert(presence.count() == 2);
}

TEST(rule_presence_clear) {
    RulePresence presence;
    presence.set_rule(RuleId::Precise);
    presence.set_rule(RuleId::Devout);

    assert(presence.any());

    presence.clear();

    assert(presence.none());
    assert(!presence.has_rule(RuleId::Precise));
}

TEST(rule_presence_copy) {
    RulePresence original;
    original.set_rule(RuleId::Precise);
    original.set_rule(RuleId::Devout);

    // Copy
    RulePresence copy = original;

    // Both should have the rules
    assert(copy.has_rule(RuleId::Precise));
    assert(copy.has_rule(RuleId::Devout));

    // Modifying copy shouldn't affect original
    copy.clear_rule(RuleId::Precise);
    assert(original.has_rule(RuleId::Precise));
}

//==============================================================================
// RuleHotData Tests
//==============================================================================

TEST(rule_hot_data_size) {
    // RuleHotData must be exactly 16 bytes
    assert(sizeof(RuleHotData) == 16);
}

TEST(rule_hot_data_traits) {
    RuleHotData data;
    data.traits = static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
                  static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION);

    assert(data.has_trait(RuleTrait::MODIFIES_HIT_ROLL));
    assert(data.has_trait(RuleTrait::BYPASSES_REGENERATION));
    assert(!data.has_trait(RuleTrait::MELEE_ONLY));
}

//==============================================================================
// CombatContextCore Tests
//==============================================================================

TEST(combat_context_core_size) {
    // CombatContextCore must fit in 64 bytes (one cache line)
    assert(sizeof(CombatContextCore) <= 64);
}

TEST(combat_context_core_flags) {
    CombatContextCore ctx;

    // Test flag operations
    ctx.set_bypass_regen(true);
    assert(ctx.bypasses_regeneration());

    ctx.set_force_reroll(true);
    assert(ctx.forces_defense_reroll());

    ctx.set_bypass_regen(false);
    assert(!ctx.bypasses_regeneration());
    assert(ctx.forces_defense_reroll());  // Other flag still set
}

//==============================================================================
// CompactRule Tests
//==============================================================================

TEST(compact_rule_size) {
    // CompactRule is now 4 bytes (was 2)
    assert(sizeof(CompactRule) == 4);
}

TEST(compact_rule_construction) {
    CompactRule rule1;
    assert(!rule1.is_valid());
    assert(rule1.id == RuleId::None);

    CompactRule rule2(RuleId::Blast, 3);
    assert(rule2.is_valid());
    assert(rule2.id == RuleId::Blast);
    assert(rule2.value == 3);
}

//==============================================================================
// RuleRegistry Tests
//==============================================================================

TEST(rule_registry_creation) {
    RuleRegistry registry;
    // Should create without error
    assert(registry.registered_count() == 0);
}

TEST(rule_registry_default) {
    RuleRegistry registry = create_default_registry();
    // Should have some rules registered
    assert(registry.registered_count() > 0);
    assert(registry.is_registered(RuleId::Precise));
    assert(registry.is_registered(RuleId::Rending));
    assert(registry.is_registered(RuleId::Stealth));
}

TEST(rule_registry_hot_data) {
    RuleRegistry registry = create_default_registry();

    // Get hot data for Precise
    const auto& precise = registry.get_hot_data(RuleId::Precise);
    assert(precise.id == RuleId::Precise);
    assert(precise.game_phase == GamePhase::COMBAT);
    assert(precise.has_trait(RuleTrait::MODIFIES_HIT_ROLL));
}

TEST(rule_registry_alias) {
    RuleRegistry registry = create_default_registry();

    // Should be able to parse rule names
    auto precise = registry.parse_rule_name("Precise");
    assert(precise.has_value());
    assert(precise.value() == RuleId::Precise);

    auto unknown = registry.parse_rule_name("NonexistentRule");
    assert(!unknown.has_value());
}

//==============================================================================
// Trait Tests
//==============================================================================

TEST(trait_operations) {
    TraitMask mask = static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL);

    assert(has_trait(mask, RuleTrait::MODIFIES_HIT_ROLL));
    assert(!has_trait(mask, RuleTrait::MELEE_ONLY));

    // Combine traits
    mask = mask | RuleTrait::CHARGE_ONLY;
    assert(has_trait(mask, RuleTrait::MODIFIES_HIT_ROLL));
    assert(has_trait(mask, RuleTrait::CHARGE_ONLY));
}

//==============================================================================
// Main
//==============================================================================

int main() {
    std::cout << "=== Rule Architecture Unit Tests ===\n\n";

    std::cout << "RuleId Tests:\n";
    RUN_TEST(rule_id_enum_size);
    RUN_TEST(rule_id_primary_count);
    RUN_TEST(rule_id_count_limit);
    RUN_TEST(is_primary_rule);
    RUN_TEST(is_extended_rule);

    std::cout << "\nRulePresence Tests:\n";
    RUN_TEST(rule_presence_primary_only);
    RUN_TEST(rule_presence_extended);
    RUN_TEST(rule_presence_mixed);
    RUN_TEST(rule_presence_clear);
    RUN_TEST(rule_presence_copy);

    std::cout << "\nRuleHotData Tests:\n";
    RUN_TEST(rule_hot_data_size);
    RUN_TEST(rule_hot_data_traits);

    std::cout << "\nCombatContextCore Tests:\n";
    RUN_TEST(combat_context_core_size);
    RUN_TEST(combat_context_core_flags);

    std::cout << "\nCompactRule Tests:\n";
    RUN_TEST(compact_rule_size);
    RUN_TEST(compact_rule_construction);

    std::cout << "\nRuleRegistry Tests:\n";
    RUN_TEST(rule_registry_creation);
    RUN_TEST(rule_registry_default);
    RUN_TEST(rule_registry_hot_data);
    RUN_TEST(rule_registry_alias);

    std::cout << "\nTrait Tests:\n";
    RUN_TEST(trait_operations);

    std::cout << "\n=== All Tests Passed ===\n";
    return 0;
}
