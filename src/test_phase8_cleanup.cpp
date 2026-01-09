// ==============================================================================
// Phase 8 Tests: Cleanup and Optimization
// ==============================================================================
// This file tests the Phase 8 additions:
// - CombatRuleCache for performance optimization
// - Deprecated A/B test flags
// - Overall system integration

#include "core/combat_rule_cache.hpp"
#include "core/types.hpp"
#include "core/unit.hpp"
#include "core/weapon.hpp"
#include "core/rule_registry.hpp"
#include "engine/registry_combat_resolver.hpp"
#include <iostream>
#include <cstring>
#include <cassert>

using namespace battle;

// ==============================================================================
// Test Utilities
// ==============================================================================

static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(test_func) do { \
    std::cout << "  Running " << #test_func << "..."; \
    try { \
        test_func(); \
        std::cout << " PASS\n"; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << " FAIL: " << e.what() << "\n"; \
        tests_failed++; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        throw std::runtime_error("Expected " #a " == " #b); \
    } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        throw std::runtime_error("Expected " #cond " to be true"); \
    } \
} while(0)

#define ASSERT_FALSE(cond) do { \
    if ((cond)) { \
        throw std::runtime_error("Expected " #cond " to be false"); \
    } \
} while(0)

// ==============================================================================
// Test Fixtures
// ==============================================================================

Unit create_test_attacker() {
    Unit unit{};
    std::strcpy(unit.name.data.data(), "Test Attacker");
    unit.name.length = 13;
    unit.model_count = 5;
    unit.alive_count = 5;
    unit.quality = 4;
    unit.defense = 4;
    unit.points_cost = 100;

    // Add some rules
    unit.rules[0] = CompactRule(RuleId::Furious);
    unit.rules[1] = CompactRule(RuleId::Relentless);
    unit.rules[2] = CompactRule(RuleId::GoodShot);
    unit.rule_count = 3;
    unit.rule_mask = rule_bit(RuleId::Furious) | rule_bit(RuleId::Relentless) | rule_bit(RuleId::GoodShot);

    return unit;
}

Unit create_test_defender() {
    Unit unit{};
    std::strcpy(unit.name.data.data(), "Test Defender");
    unit.name.length = 13;
    unit.model_count = 5;
    unit.alive_count = 5;
    unit.quality = 4;
    unit.defense = 3;
    unit.points_cost = 100;

    // Add defensive rules
    unit.rules[0] = CompactRule(RuleId::Regeneration);
    unit.rules[1] = CompactRule(RuleId::Stealth);
    unit.rules[2] = CompactRule(RuleId::Tough, 3);
    unit.rule_count = 3;
    unit.rule_mask = rule_bit(RuleId::Regeneration) | rule_bit(RuleId::Stealth) | rule_bit(RuleId::Tough);

    return unit;
}

Weapon create_test_weapon() {
    Weapon weapon{};
    std::strcpy(weapon.name.data.data(), "Test Weapon");
    weapon.name.length = 11;
    weapon.attacks = 2;
    weapon.ap = 1;
    weapon.range = 24;
    weapon.count = 5;

    // Add weapon rules
    weapon.rules[0] = CompactRule(RuleId::Rending);
    weapon.rules[1] = CompactRule(RuleId::Blast, 3);
    weapon.rules[2] = CompactRule(RuleId::Poison);
    weapon.rule_count = 3;
    weapon.rule_mask = rule_bit(RuleId::Rending) | rule_bit(RuleId::Blast) | rule_bit(RuleId::Poison);

    return weapon;
}

// ==============================================================================
// CombatRuleCache Tests
// ==============================================================================

void test_cache_initialization() {
    CombatRuleCache cache;
    ASSERT_FALSE(cache.is_initialized());

    Unit attacker = create_test_attacker();
    Unit defender = create_test_defender();
    Weapon weapon = create_test_weapon();

    cache.initialize(attacker, defender, weapon, CombatType::SHOOTING, false);
    ASSERT_TRUE(cache.is_initialized());
}

void test_cache_attacker_rules() {
    CombatRuleCache cache;
    Unit attacker = create_test_attacker();
    Unit defender = create_test_defender();
    Weapon weapon = create_test_weapon();

    cache.initialize(attacker, defender, weapon, CombatType::SHOOTING, false);

    // Attacker has Furious, Relentless, GoodShot
    ASSERT_TRUE(cache.has_rule(RuleId::Furious));
    ASSERT_TRUE(cache.has_rule(RuleId::Relentless));
    ASSERT_TRUE(cache.has_rule(RuleId::GoodShot));
    ASSERT_TRUE(cache.attacker_has_rule(RuleId::Furious));

    // Attacker doesn't have these
    ASSERT_FALSE(cache.attacker_has_rule(RuleId::Stealth));
    ASSERT_FALSE(cache.attacker_has_rule(RuleId::Rending));
}

void test_cache_defender_rules() {
    CombatRuleCache cache;
    Unit attacker = create_test_attacker();
    Unit defender = create_test_defender();
    Weapon weapon = create_test_weapon();

    cache.initialize(attacker, defender, weapon, CombatType::SHOOTING, false);

    // Defender has Regeneration, Stealth, Tough(3)
    ASSERT_TRUE(cache.has_rule(RuleId::Regeneration));
    ASSERT_TRUE(cache.has_rule(RuleId::Stealth));
    ASSERT_TRUE(cache.has_rule(RuleId::Tough));
    ASSERT_TRUE(cache.defender_has_rule(RuleId::Regeneration));
    ASSERT_EQ(cache.get_value(RuleId::Tough), 3);
}

void test_cache_weapon_rules() {
    CombatRuleCache cache;
    Unit attacker = create_test_attacker();
    Unit defender = create_test_defender();
    Weapon weapon = create_test_weapon();

    cache.initialize(attacker, defender, weapon, CombatType::SHOOTING, false);

    // Weapon has Rending, Blast(3), Poison
    ASSERT_TRUE(cache.has_rule(RuleId::Rending));
    ASSERT_TRUE(cache.has_rule(RuleId::Blast));
    ASSERT_TRUE(cache.has_rule(RuleId::Poison));
    ASSERT_TRUE(cache.weapon_has_rule(RuleId::Rending));
    ASSERT_EQ(cache.get_value(RuleId::Blast), 3);
}

void test_cache_trait_aggregation() {
    CombatRuleCache cache;
    Unit attacker = create_test_attacker();
    Unit defender = create_test_defender();
    Weapon weapon = create_test_weapon();

    cache.initialize(attacker, defender, weapon, CombatType::SHOOTING, false);

    // Rending bypasses regeneration
    ASSERT_TRUE(cache.bypasses_regeneration());

    // Poison forces defense reroll
    ASSERT_TRUE(cache.forces_defense_reroll());
}

void test_cache_clear() {
    CombatRuleCache cache;
    Unit attacker = create_test_attacker();
    Unit defender = create_test_defender();
    Weapon weapon = create_test_weapon();

    cache.initialize(attacker, defender, weapon, CombatType::SHOOTING, false);
    ASSERT_TRUE(cache.is_initialized());

    cache.clear();
    ASSERT_FALSE(cache.is_initialized());
    ASSERT_FALSE(cache.has_rule(RuleId::Furious));
    ASSERT_FALSE(cache.bypasses_regeneration());
}

void test_cache_combat_type() {
    CombatRuleCache cache;
    Unit attacker = create_test_attacker();
    Unit defender = create_test_defender();
    Weapon weapon = create_test_weapon();

    cache.initialize(attacker, defender, weapon, CombatType::MELEE, true);

    ASSERT_EQ(cache.combat_type(), CombatType::MELEE);
    ASSERT_TRUE(cache.is_charge());
}

void test_cache_rule_source() {
    CombatRuleCache cache;
    Unit attacker = create_test_attacker();
    Unit defender = create_test_defender();
    Weapon weapon = create_test_weapon();

    cache.initialize(attacker, defender, weapon, CombatType::SHOOTING, false);

    ASSERT_EQ(cache.get_source(RuleId::Furious), CombatRuleCache::RuleSource::ATTACKER);
    ASSERT_EQ(cache.get_source(RuleId::Stealth), CombatRuleCache::RuleSource::DEFENDER);
    ASSERT_EQ(cache.get_source(RuleId::Rending), CombatRuleCache::RuleSource::WEAPON);
    ASSERT_EQ(cache.get_source(RuleId::None), CombatRuleCache::RuleSource::NONE);
}

void test_cache_rule_counting() {
    CombatRuleCache cache;
    Unit attacker = create_test_attacker();
    Unit defender = create_test_defender();
    Weapon weapon = create_test_weapon();

    cache.initialize(attacker, defender, weapon, CombatType::SHOOTING, false);

    ASSERT_EQ(cache.attacker_rule_count(), 3);
    ASSERT_EQ(cache.defender_rule_count(), 3);
    ASSERT_EQ(cache.weapon_rule_count(), 3);
}

// ==============================================================================
// CombatRuleCache Pool Tests
// ==============================================================================

void test_cache_pool_acquire_release() {
    CombatRuleCachePool<4> pool;

    ASSERT_EQ(pool.available(), 4);

    CombatRuleCache* cache1 = pool.acquire();
    ASSERT_TRUE(cache1 != nullptr);
    ASSERT_EQ(pool.available(), 3);

    CombatRuleCache* cache2 = pool.acquire();
    ASSERT_TRUE(cache2 != nullptr);
    ASSERT_EQ(pool.available(), 2);

    pool.release(cache1);
    ASSERT_EQ(pool.available(), 3);

    pool.release(cache2);
    ASSERT_EQ(pool.available(), 4);
}

void test_cache_pool_exhaustion() {
    CombatRuleCachePool<2> pool;

    CombatRuleCache* cache1 = pool.acquire();
    CombatRuleCache* cache2 = pool.acquire();
    CombatRuleCache* cache3 = pool.acquire();  // Should return nullptr

    ASSERT_TRUE(cache1 != nullptr);
    ASSERT_TRUE(cache2 != nullptr);
    ASSERT_TRUE(cache3 == nullptr);  // Pool exhausted

    pool.release(cache1);
    cache3 = pool.acquire();  // Should work now
    ASSERT_TRUE(cache3 != nullptr);
}

void test_cache_guard() {
    CombatRuleCachePool<4> pool;

    {
        CacheGuard<4> guard(pool);
        ASSERT_TRUE(guard);
        ASSERT_EQ(pool.available(), 3);

        Unit attacker = create_test_attacker();
        Unit defender = create_test_defender();
        Weapon weapon = create_test_weapon();

        guard->initialize(attacker, defender, weapon, CombatType::SHOOTING, false);
        ASSERT_TRUE(guard->has_rule(RuleId::Furious));
    }

    // Guard released, pool should be back to full
    ASSERT_EQ(pool.available(), 4);
}

// ==============================================================================
// A/B Test Flag Tests
// ==============================================================================

void test_ab_test_flags_disabled() {
    // A/B test flags should be disabled after Phase 8
    ASSERT_FALSE(AB_TEST_HIT_MODIFIERS);
    ASSERT_FALSE(AB_TEST_HIT_SEPARATION);
    ASSERT_FALSE(AB_TEST_HIT_BONUSES);
    ASSERT_FALSE(AB_TEST_HIT_MULTIPLICATION);
}

// ==============================================================================
// Integration Tests
// ==============================================================================

void test_cache_with_registry_resolver() {
    RuleRegistry registry = create_default_registry();

    Unit attacker = create_test_attacker();
    Unit defender = create_test_defender();
    Weapon weapon = create_test_weapon();

    // Create cache
    CombatRuleCache cache;
    cache.initialize(attacker, defender, weapon, CombatType::SHOOTING, false);

    // Verify cache matches what registry would report
    ASSERT_TRUE(cache.bypasses_regeneration());  // From Rending
    ASSERT_TRUE(cache.forces_defense_reroll());  // From Poison
}

void test_cache_bane_in_melee() {
    CombatRuleCache cache;

    Unit attacker{};
    attacker.rules[0] = CompactRule(RuleId::BaneInMelee);
    attacker.rule_count = 1;
    attacker.rule_mask = rule_bit(RuleId::BaneInMelee);

    Unit defender{};
    defender.rule_count = 0;

    Weapon weapon{};
    weapon.rule_count = 0;

    // In shooting, BaneInMelee shouldn't bypass regen
    cache.initialize(attacker, defender, weapon, CombatType::SHOOTING, false);
    ASSERT_FALSE(cache.bypasses_regeneration());
    ASSERT_FALSE(cache.forces_defense_reroll());

    // In melee, BaneInMelee should bypass regen
    cache.clear();
    cache.initialize(attacker, defender, weapon, CombatType::MELEE, false);
    ASSERT_TRUE(cache.bypasses_regeneration());
    ASSERT_TRUE(cache.forces_defense_reroll());
}

void test_multiple_regen_bypass_sources() {
    CombatRuleCache cache;

    Unit attacker{};
    attacker.rules[0] = CompactRule(RuleId::Bane);
    attacker.rule_count = 1;
    attacker.rule_mask = rule_bit(RuleId::Bane);

    Unit defender{};
    defender.rule_count = 0;

    Weapon weapon{};
    weapon.rules[0] = CompactRule(RuleId::Rending);
    weapon.rules[1] = CompactRule(RuleId::Rupture);
    weapon.rule_count = 2;
    weapon.rule_mask = rule_bit(RuleId::Rending) | rule_bit(RuleId::Rupture);

    cache.initialize(attacker, defender, weapon, CombatType::SHOOTING, false);

    // All three bypass regen
    ASSERT_TRUE(cache.has_rule(RuleId::Bane));
    ASSERT_TRUE(cache.has_rule(RuleId::Rending));
    ASSERT_TRUE(cache.has_rule(RuleId::Rupture));
    ASSERT_TRUE(cache.bypasses_regeneration());
}

void test_empty_cache() {
    CombatRuleCache cache;

    Unit attacker{};
    attacker.rule_count = 0;
    attacker.rule_mask = 0;

    Unit defender{};
    defender.rule_count = 0;
    defender.rule_mask = 0;

    Weapon weapon{};
    weapon.rule_count = 0;
    weapon.rule_mask = 0;

    cache.initialize(attacker, defender, weapon, CombatType::SHOOTING, false);

    ASSERT_FALSE(cache.has_rule(RuleId::Furious));
    ASSERT_FALSE(cache.bypasses_regeneration());
    ASSERT_FALSE(cache.forces_defense_reroll());
    ASSERT_EQ(cache.attacker_rule_count(), 0);
    ASSERT_EQ(cache.defender_rule_count(), 0);
    ASSERT_EQ(cache.weapon_rule_count(), 0);
}

// ==============================================================================
// Main
// ==============================================================================

int main() {
    std::cout << "=== Phase 8: Cleanup and Optimization Tests ===\n\n";

    std::cout << "CombatRuleCache Tests:\n";
    RUN_TEST(test_cache_initialization);
    RUN_TEST(test_cache_attacker_rules);
    RUN_TEST(test_cache_defender_rules);
    RUN_TEST(test_cache_weapon_rules);
    RUN_TEST(test_cache_trait_aggregation);
    RUN_TEST(test_cache_clear);
    RUN_TEST(test_cache_combat_type);
    RUN_TEST(test_cache_rule_source);
    RUN_TEST(test_cache_rule_counting);

    std::cout << "\nCombatRuleCache Pool Tests:\n";
    RUN_TEST(test_cache_pool_acquire_release);
    RUN_TEST(test_cache_pool_exhaustion);
    RUN_TEST(test_cache_guard);

    std::cout << "\nA/B Test Flag Tests:\n";
    RUN_TEST(test_ab_test_flags_disabled);

    std::cout << "\nIntegration Tests:\n";
    RUN_TEST(test_cache_with_registry_resolver);
    RUN_TEST(test_cache_bane_in_melee);
    RUN_TEST(test_multiple_regen_bypass_sources);
    RUN_TEST(test_empty_cache);

    std::cout << "\n=== Results: " << tests_passed << " passed, "
              << tests_failed << " failed ===\n";

    if (tests_failed > 0) {
        std::cout << "\n=== SOME TESTS FAILED ===\n";
        return 1;
    }

    std::cout << "\n=== All Phase 8 Tests Passed ===\n";
    return 0;
}
