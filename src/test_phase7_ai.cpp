// ==============================================================================
// Phase 7 Tests: AI Integration
// ==============================================================================
// This file tests the rule-aware AI system added in Phase 7 of the
// Rule Registry Architecture implementation.

#include "ai/ai_hints.hpp"
#include "ai/rule_aware_ai.hpp"
#include "core/types.hpp"
#include "core/unit.hpp"
#include "core/rule_registry.hpp"
#include "engine/game_state.hpp"
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

#define ASSERT_GT(a, b) do { \
    if (!((a) > (b))) { \
        throw std::runtime_error("Expected " #a " > " #b); \
    } \
} while(0)

#define ASSERT_LT(a, b) do { \
    if (!((a) < (b))) { \
        throw std::runtime_error("Expected " #a " < " #b); \
    } \
} while(0)

// ==============================================================================
// Test Fixtures
// ==============================================================================

Unit create_melee_unit() {
    Unit unit{};
    std::strcpy(unit.name.data.data(), "Melee Unit");
    unit.name.length = 10;
    unit.model_count = 10;
    unit.alive_count = 10;
    unit.quality = 4;
    unit.defense = 4;
    unit.points_cost = 100;
    unit.max_range = 0;
    unit.ai_type = AIType::Melee;

    // Add melee-focused rules
    unit.rules[0] = CompactRule(RuleId::Fast);
    unit.rules[1] = CompactRule(RuleId::Furious);
    unit.rules[2] = CompactRule(RuleId::Impact, 3);
    unit.rule_count = 3;
    unit.rule_mask = rule_bit(RuleId::Fast) | rule_bit(RuleId::Furious) | rule_bit(RuleId::Impact);

    return unit;
}

Unit create_shooting_unit() {
    Unit unit{};
    std::strcpy(unit.name.data.data(), "Shooting Unit");
    unit.name.length = 13;
    unit.model_count = 10;
    unit.alive_count = 10;
    unit.quality = 4;
    unit.defense = 4;
    unit.points_cost = 100;
    unit.max_range = 24;
    unit.ai_type = AIType::Shooting;

    // Add shooting-focused rules
    unit.rules[0] = CompactRule(RuleId::Relentless);
    unit.rules[1] = CompactRule(RuleId::Stealth);
    unit.rules[2] = CompactRule(RuleId::GoodShot);
    unit.rule_count = 3;
    unit.rule_mask = rule_bit(RuleId::Relentless) | rule_bit(RuleId::Stealth) | rule_bit(RuleId::GoodShot);

    return unit;
}

Unit create_defensive_unit() {
    Unit unit{};
    std::strcpy(unit.name.data.data(), "Defensive Unit");
    unit.name.length = 14;
    unit.model_count = 5;
    unit.alive_count = 5;
    unit.quality = 4;
    unit.defense = 3;
    unit.points_cost = 150;
    unit.max_range = 12;
    unit.ai_type = AIType::Hybrid;

    // Add defensive rules
    unit.rules[0] = CompactRule(RuleId::Regeneration);
    unit.rules[1] = CompactRule(RuleId::Tough, 3);
    unit.rules[2] = CompactRule(RuleId::Shielded);
    unit.rule_count = 3;
    unit.rule_mask = rule_bit(RuleId::Regeneration) | rule_bit(RuleId::Tough) | rule_bit(RuleId::Shielded);

    return unit;
}

Unit create_charge_specialist_unit() {
    Unit unit{};
    std::strcpy(unit.name.data.data(), "Charge Specialist");
    unit.name.length = 17;
    unit.model_count = 5;
    unit.alive_count = 5;
    unit.quality = 3;
    unit.defense = 4;
    unit.points_cost = 200;
    unit.max_range = 0;
    unit.ai_type = AIType::Melee;

    // Add charge-focused rules
    unit.rules[0] = CompactRule(RuleId::Lance);
    unit.rules[1] = CompactRule(RuleId::Impact, 2);
    unit.rules[2] = CompactRule(RuleId::Furious);
    unit.rules[3] = CompactRule(RuleId::RapidCharge);
    unit.rule_count = 4;
    unit.rule_mask = rule_bit(RuleId::Lance) | rule_bit(RuleId::Impact) |
                     rule_bit(RuleId::Furious) | rule_bit(RuleId::RapidCharge);

    return unit;
}

// ==============================================================================
// AI Hints Structure Tests
// ==============================================================================

void test_ai_hints_default() {
    AIHints hints{};

    ASSERT_EQ(hints.distance_pref, DistancePreference::NEUTRAL);
    ASSERT_EQ(hints.engagement_pref, EngagementPreference::NEUTRAL);
    ASSERT_EQ(hints.hint_flags, 0);
    ASSERT_EQ(hints.offensive_weight, 0);
    ASSERT_EQ(hints.defensive_weight, 0);
}

void test_ai_hints_builder() {
    AIHints hints = AIHintBuilder()
        .close_range()
        .aggressive()
        .melee_focused()
        .charge_bonus()
        .offense(30)
        .defense(10)
        .optimal_range(0, 12);

    ASSERT_EQ(hints.distance_pref, DistancePreference::CLOSE);
    ASSERT_EQ(hints.engagement_pref, EngagementPreference::AGGRESSIVE);
    ASSERT_TRUE(has_hint(hints.hint_flags, AIHintFlag::PREFERS_MELEE));
    ASSERT_TRUE(has_hint(hints.hint_flags, AIHintFlag::BENEFITS_FROM_CHARGE));
    ASSERT_EQ(hints.offensive_weight, 30);
    ASSERT_EQ(hints.defensive_weight, 10);
    ASSERT_EQ(hints.optimal_range_min, 0);
    ASSERT_EQ(hints.optimal_range_max, 12);
}

void test_ai_hints_prefers_melee() {
    AIHints melee_hints = AIHintBuilder().close_range().aggressive().melee_focused();
    ASSERT_TRUE(melee_hints.prefers_melee());
    ASSERT_TRUE(!melee_hints.prefers_shooting());
}

void test_ai_hints_prefers_shooting() {
    AIHints shooting_hints = AIHintBuilder().long_range().defensive().shooting_focused();
    ASSERT_TRUE(shooting_hints.prefers_shooting());
    ASSERT_TRUE(!shooting_hints.prefers_melee());
}

void test_ai_hints_size() {
    // Ensure AIHints stays compact
    ASSERT_TRUE(sizeof(AIHints) <= 16);
}

// ==============================================================================
// AI Profile Tests
// ==============================================================================

void test_ai_profile_aggregation() {
    AIProfile profile;

    // Add melee-focused hints
    AIHints melee1 = AIHintBuilder().close_range().aggressive().offense(20);
    AIHints melee2 = AIHintBuilder().close_range().melee_focused().offense(15);
    AIHints defense = AIHintBuilder().defensive().defense(10);

    profile.add_hints(melee1);
    profile.add_hints(melee2);
    profile.add_hints(defense);

    ASSERT_EQ(profile.total_offensive, 35);
    ASSERT_EQ(profile.total_defensive, 10);
    ASSERT_EQ(profile.close_preference_count, 2);
    ASSERT_EQ(profile.aggressive_count, 1);
    ASSERT_EQ(profile.defensive_count, 1);
    ASSERT_TRUE(profile.is_offensive());
}

void test_ai_profile_distance_preference() {
    AIProfile close_profile;
    close_profile.add_hints(AIHintBuilder().close_range());
    close_profile.add_hints(AIHintBuilder().close_range());
    close_profile.add_hints(AIHintBuilder().long_range());

    ASSERT_EQ(close_profile.get_distance_preference(), DistancePreference::CLOSE);

    AIProfile far_profile;
    far_profile.add_hints(AIHintBuilder().long_range());
    far_profile.add_hints(AIHintBuilder().long_range());
    far_profile.add_hints(AIHintBuilder().close_range());

    ASSERT_EQ(far_profile.get_distance_preference(), DistancePreference::FAR);
}

void test_ai_profile_engagement_preference() {
    AIProfile aggressive_profile;
    aggressive_profile.add_hints(AIHintBuilder().aggressive());
    aggressive_profile.add_hints(AIHintBuilder().aggressive());
    aggressive_profile.add_hints(AIHintBuilder().defensive());

    ASSERT_EQ(aggressive_profile.get_engagement_preference(), EngagementPreference::AGGRESSIVE);
}

void test_ai_profile_optimal_range() {
    AIProfile profile;
    profile.add_hints(AIHints{}.optimal_range(0, 24));
    profile.add_hints(AIHints{}.optimal_range(9, 36));

    // Intersection should be [9, 24]
    ASSERT_EQ(profile.optimal_min, 9);
    ASSERT_EQ(profile.optimal_max, 24);
    ASSERT_TRUE(profile.in_optimal_range(12));
    ASSERT_TRUE(!profile.in_optimal_range(6));
    ASSERT_TRUE(!profile.in_optimal_range(30));
}

// ==============================================================================
// AI Personality Tests
// ==============================================================================

void test_personality_presets() {
    // Verify presets have expected values
    ASSERT_EQ(personality::BALANCED.aggression, 0);
    ASSERT_GT(personality::AGGRESSIVE.aggression, 50);
    ASSERT_LT(personality::DEFENSIVE.aggression, 0);
    ASSERT_GT(personality::BERSERKER.aggression, 80);
    ASSERT_LT(personality::COWARD.aggression, -50);
}

void test_personality_from_ai_type() {
    AIPersonality melee_p = personality_from_ai_type(AIType::Melee);
    AIPersonality shooting_p = personality_from_ai_type(AIType::Shooting);

    // Melee should be more aggressive
    ASSERT_GT(melee_p.aggression, shooting_p.aggression);

    // Shooting should prefer more distance
    ASSERT_GT(shooting_p.distance_preference, melee_p.distance_preference);
}

// ==============================================================================
// Rule-Aware AI Tests
// ==============================================================================

void test_rule_aware_ai_construction() {
    RuleRegistry registry = create_default_registry();
    RuleAwareAI ai(registry);

    ASSERT_EQ(ai.get_personality().aggression, 0);  // Default balanced
}

void test_rule_aware_ai_build_profile_melee() {
    RuleRegistry registry = create_default_registry();
    RuleAwareAI ai(registry);

    Unit melee_unit = create_melee_unit();
    AIProfile profile = ai.build_profile(melee_unit);

    // Melee unit should have offensive profile
    ASSERT_TRUE(profile.is_offensive());
    ASSERT_TRUE(profile.should_charge());
    ASSERT_EQ(profile.get_distance_preference(), DistancePreference::CLOSE);
}

void test_rule_aware_ai_build_profile_shooting() {
    RuleRegistry registry = create_default_registry();
    RuleAwareAI ai(registry);

    Unit shooting_unit = create_shooting_unit();
    AIProfile profile = ai.build_profile(shooting_unit);

    // Shooting unit should prefer distance
    ASSERT_EQ(profile.get_distance_preference(), DistancePreference::FAR);
    ASSERT_TRUE(has_hint(profile.combined_flags, AIHintFlag::PREFERS_HOLDING));
}

void test_rule_aware_ai_build_profile_defensive() {
    RuleRegistry registry = create_default_registry();
    RuleAwareAI ai(registry);

    Unit defensive_unit = create_defensive_unit();
    AIProfile profile = ai.build_profile(defensive_unit);

    // Defensive unit should have defensive profile
    ASSERT_TRUE(profile.is_defensive());
}

void test_rule_aware_ai_decide_action_shaken() {
    RuleRegistry registry = create_default_registry();
    RuleAwareAI ai(registry);

    Unit unit_a = create_melee_unit();
    Unit unit_b = create_shooting_unit();

    GameState state;
    state.init(unit_a, unit_b);
    state.state_a.become_shaken();

    ActionType action = ai.decide_action(state, true);
    ASSERT_EQ(action, ActionType::Rally);
}

void test_rule_aware_ai_decide_action_destroyed() {
    RuleRegistry registry = create_default_registry();
    RuleAwareAI ai(registry);

    Unit unit_a = create_melee_unit();
    Unit unit_b = create_shooting_unit();

    GameState state;
    state.init(unit_a, unit_b);
    state.state_a.alive_count = 0;  // Simulate all models killed

    ActionType action = ai.decide_action(state, true);
    ASSERT_EQ(action, ActionType::Idle);
}

void test_rule_aware_ai_charge_specialist_charges() {
    RuleRegistry registry = create_default_registry();
    RuleAwareAI ai(registry, personality::AGGRESSIVE);

    Unit attacker = create_charge_specialist_unit();
    Unit defender = create_shooting_unit();

    GameState state;
    state.init(attacker, defender);

    // Position within charge range (RapidCharge gives +4")
    state.pos_a = -8;
    state.pos_b = 8;

    ActionType action = ai.decide_action(state, true);
    ASSERT_EQ(action, ActionType::Charge);
}

void test_rule_aware_ai_defensive_holds() {
    RuleRegistry registry = create_default_registry();
    RuleAwareAI ai(registry, personality::DEFENSIVE);

    Unit attacker = create_defensive_unit();
    Unit defender = create_melee_unit();

    GameState state;
    state.init(attacker, defender);

    // Position at objective with shooting range
    state.pos_a = 0;
    state.pos_b = 10;

    ActionType action = ai.decide_action(state, true);
    // Defensive AI controlling objective should hold
    ASSERT_TRUE(action == ActionType::Hold || action == ActionType::Advance);
}

void test_create_ai_for_unit() {
    RuleRegistry registry = create_default_registry();

    Unit melee_unit = create_melee_unit();
    RuleAwareAI melee_ai = create_ai_for_unit(registry, melee_unit);
    // Should get aggressive-ish personality
    ASSERT_GT(melee_ai.get_personality().aggression, -30);

    Unit shooting_unit = create_shooting_unit();
    RuleAwareAI shooting_ai = create_ai_for_unit(registry, shooting_unit);
    // Should get gunline personality
    ASSERT_GT(shooting_ai.get_personality().distance_preference, 0);
}

// ==============================================================================
// AI Hint Retrieval Tests
// ==============================================================================

void test_get_rule_hints_melee_rules() {
    RuleRegistry registry = create_default_registry();
    RuleAwareAI ai(registry);

    // Lance should have charge bonus
    AIHints lance_hints = ai.get_rule_hints(RuleId::Lance);
    ASSERT_TRUE(lance_hints.benefits_from_charge());
    ASSERT_TRUE(lance_hints.prefers_melee());

    // Impact should have charge bonus
    AIHints impact_hints = ai.get_rule_hints(RuleId::Impact);
    ASSERT_TRUE(impact_hints.benefits_from_charge());
    ASSERT_TRUE(has_hint(impact_hints.hint_flags, AIHintFlag::WANTS_FIRST_STRIKE));
}

void test_get_rule_hints_shooting_rules() {
    RuleRegistry registry = create_default_registry();
    RuleAwareAI ai(registry);

    // Relentless should prefer holding at range
    AIHints relentless_hints = ai.get_rule_hints(RuleId::Relentless);
    ASSERT_TRUE(relentless_hints.prefers_shooting());
    ASSERT_TRUE(has_hint(relentless_hints.hint_flags, AIHintFlag::PREFERS_HOLDING));
}

void test_get_rule_hints_defensive_rules() {
    RuleRegistry registry = create_default_registry();
    RuleAwareAI ai(registry);

    // Regeneration should have defensive bonus
    AIHints regen_hints = ai.get_rule_hints(RuleId::Regeneration);
    ASSERT_GT(regen_hints.defensive_weight, 0);
    ASSERT_TRUE(has_hint(regen_hints.hint_flags, AIHintFlag::DEFENSIVE_BONUS));
}

void test_get_rule_hints_movement_rules() {
    RuleRegistry registry = create_default_registry();
    RuleAwareAI ai(registry);

    // Fast should have mobile advantage
    AIHints fast_hints = ai.get_rule_hints(RuleId::Fast);
    ASSERT_TRUE(has_hint(fast_hints.hint_flags, AIHintFlag::MOBILE_ADVANTAGE));

    // RapidCharge should have charge bonus
    AIHints rapid_hints = ai.get_rule_hints(RuleId::RapidCharge);
    ASSERT_TRUE(rapid_hints.benefits_from_charge());
}

// ==============================================================================
// AI Preset Tests
// ==============================================================================

void test_ai_presets_melee_attacker() {
    AIHints hints = ai_presets::MELEE_ATTACKER;

    ASSERT_EQ(hints.distance_pref, DistancePreference::CLOSE);
    ASSERT_EQ(hints.engagement_pref, EngagementPreference::AGGRESSIVE);
    ASSERT_TRUE(has_hint(hints.hint_flags, AIHintFlag::PREFERS_MELEE));
    ASSERT_TRUE(has_hint(hints.hint_flags, AIHintFlag::BENEFITS_FROM_CHARGE));
}

void test_ai_presets_ranged_attacker() {
    AIHints hints = ai_presets::RANGED_ATTACKER;

    ASSERT_EQ(hints.distance_pref, DistancePreference::FAR);
    ASSERT_EQ(hints.engagement_pref, EngagementPreference::DEFENSIVE);
    ASSERT_TRUE(has_hint(hints.hint_flags, AIHintFlag::PREFERS_SHOOTING));
}

void test_ai_presets_defensive() {
    AIHints hints = ai_presets::DEFENSIVE;

    ASSERT_EQ(hints.engagement_pref, EngagementPreference::DEFENSIVE);
    ASSERT_TRUE(has_hint(hints.hint_flags, AIHintFlag::DEFENSIVE_BONUS));
    ASSERT_GT(hints.defensive_weight, 0);
}

// ==============================================================================
// Integration Tests
// ==============================================================================

void test_full_game_melee_vs_shooting() {
    RuleRegistry registry = create_default_registry();

    Unit melee_unit = create_melee_unit();
    Unit shooting_unit = create_shooting_unit();

    RuleAwareAI melee_ai = create_ai_for_unit(registry, melee_unit);
    RuleAwareAI shooting_ai = create_ai_for_unit(registry, shooting_unit);

    GameState state;
    state.init(melee_unit, shooting_unit);

    // Simulate a few turns
    for (int turn = 0; turn < 3; ++turn) {
        if (!state.state_a.is_out_of_action()) {
            ActionType melee_action = melee_ai.decide_action(state, true);
            // Melee unit should generally be aggressive
            ASSERT_TRUE(melee_action != ActionType::Idle);
        }

        if (!state.state_b.is_out_of_action()) {
            ActionType shooting_action = shooting_ai.decide_action(state, false);
            // Shooting unit should generally prefer distance
            ASSERT_TRUE(shooting_action != ActionType::Idle);
        }

        // Move units closer for next iteration
        state.pos_a += 3;
    }
}

// ==============================================================================
// Main
// ==============================================================================

int main() {
    std::cout << "=== Phase 7: AI Integration Tests ===\n\n";

    std::cout << "AI Hints Structure Tests:\n";
    RUN_TEST(test_ai_hints_default);
    RUN_TEST(test_ai_hints_builder);
    RUN_TEST(test_ai_hints_prefers_melee);
    RUN_TEST(test_ai_hints_prefers_shooting);
    RUN_TEST(test_ai_hints_size);

    std::cout << "\nAI Profile Tests:\n";
    RUN_TEST(test_ai_profile_aggregation);
    RUN_TEST(test_ai_profile_distance_preference);
    RUN_TEST(test_ai_profile_engagement_preference);
    RUN_TEST(test_ai_profile_optimal_range);

    std::cout << "\nAI Personality Tests:\n";
    RUN_TEST(test_personality_presets);
    RUN_TEST(test_personality_from_ai_type);

    std::cout << "\nRule-Aware AI Tests:\n";
    RUN_TEST(test_rule_aware_ai_construction);
    RUN_TEST(test_rule_aware_ai_build_profile_melee);
    RUN_TEST(test_rule_aware_ai_build_profile_shooting);
    RUN_TEST(test_rule_aware_ai_build_profile_defensive);
    RUN_TEST(test_rule_aware_ai_decide_action_shaken);
    RUN_TEST(test_rule_aware_ai_decide_action_destroyed);
    RUN_TEST(test_rule_aware_ai_charge_specialist_charges);
    RUN_TEST(test_rule_aware_ai_defensive_holds);
    RUN_TEST(test_create_ai_for_unit);

    std::cout << "\nAI Hint Retrieval Tests:\n";
    RUN_TEST(test_get_rule_hints_melee_rules);
    RUN_TEST(test_get_rule_hints_shooting_rules);
    RUN_TEST(test_get_rule_hints_defensive_rules);
    RUN_TEST(test_get_rule_hints_movement_rules);

    std::cout << "\nAI Preset Tests:\n";
    RUN_TEST(test_ai_presets_melee_attacker);
    RUN_TEST(test_ai_presets_ranged_attacker);
    RUN_TEST(test_ai_presets_defensive);

    std::cout << "\nIntegration Tests:\n";
    RUN_TEST(test_full_game_melee_vs_shooting);

    std::cout << "\n=== Results: " << tests_passed << " passed, "
              << tests_failed << " failed ===\n";

    if (tests_failed > 0) {
        std::cout << "\n=== SOME TESTS FAILED ===\n";
        return 1;
    }

    std::cout << "\n=== All Phase 7 Tests Passed ===\n";
    return 0;
}
