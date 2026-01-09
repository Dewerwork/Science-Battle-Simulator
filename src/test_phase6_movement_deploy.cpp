// ==============================================================================
// Phase 6 Tests: Movement, Deployment, and End Round Rules
// ==============================================================================
// This file tests the movement, deployment, and end-round rule definitions
// added in Phase 6 of the Rule Registry Architecture implementation.

#include "core/types.hpp"
#include "core/phases.hpp"
#include "core/traits.hpp"
#include "core/contexts.hpp"
#include "core/rule_registry.hpp"
#include "core/rule_effect.hpp"
#include "core/unit.hpp"
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

// ==============================================================================
// Movement Rule Tests
// ==============================================================================

void test_fast_rule() {
    RuleRegistry registry = create_default_registry();

    // Verify Fast is registered
    ASSERT_TRUE(registry.is_registered(RuleId::Fast));

    // Verify hot data
    const auto& hot = registry.get_hot_data(RuleId::Fast);
    ASSERT_EQ(hot.id, RuleId::Fast);
    ASSERT_EQ(hot.game_phase, GamePhase::MOVEMENT);
    ASSERT_EQ(hot.move_sub_phase(), MoveSubPhase::CALCULATE_DISTANCE);

    // Test effect application
    MovementContext ctx;
    ctx.base_distance = 6;
    ctx.distance_modifier = 0;

    const auto& effects = registry.get_effects(RuleId::Fast);
    apply_movement_effect(effects, MoveSubPhase::CALCULATE_DISTANCE, ctx, 0);

    ASSERT_EQ(ctx.distance_modifier, 3);  // Fast adds +3"
}

void test_slow_rule() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::Slow));

    MovementContext ctx;
    ctx.base_distance = 6;
    ctx.distance_modifier = 0;

    const auto& effects = registry.get_effects(RuleId::Slow);
    apply_movement_effect(effects, MoveSubPhase::CALCULATE_DISTANCE, ctx, 0);

    ASSERT_EQ(ctx.distance_modifier, -2);  // Slow subtracts -2"
}

void test_flying_rule() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::Flying));

    MovementContext ctx;
    ctx.terrain_flags = 0;
    ctx.ignores_engagement = false;

    const auto& effects = registry.get_effects(RuleId::Flying);
    apply_movement_effect(effects, MoveSubPhase::EXECUTE_MOVE, ctx, 0);

    // Flying should ignore all terrain and engagement
    ASSERT_TRUE(ctx.terrain_flags & MovementContext::IGNORE_DIFFICULT);
    ASSERT_TRUE(ctx.terrain_flags & MovementContext::IGNORE_DANGEROUS);
    ASSERT_TRUE(ctx.terrain_flags & MovementContext::FLY_OVER);
    ASSERT_TRUE(ctx.ignores_engagement);
}

void test_strider_rule() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::Strider));

    MovementContext ctx;
    ctx.terrain_flags = 0;

    const auto& effects = registry.get_effects(RuleId::Strider);
    apply_movement_effect(effects, MoveSubPhase::EXECUTE_MOVE, ctx, 0);

    // Strider should ignore difficult terrain only
    ASSERT_TRUE(ctx.terrain_flags & MovementContext::IGNORE_DIFFICULT);
    ASSERT_TRUE((ctx.terrain_flags & MovementContext::FLY_OVER) == 0);  // Should NOT fly
}

void test_agile_rule_advance() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::Agile));

    MovementContext ctx;
    ctx.move_type = MovementContext::MoveType::ADVANCE;
    ctx.distance_modifier = 0;

    const auto& effects = registry.get_effects(RuleId::Agile);
    apply_movement_effect(effects, MoveSubPhase::CALCULATE_DISTANCE, ctx, 0);

    ASSERT_EQ(ctx.distance_modifier, 1);  // +1" on advance
}

void test_agile_rule_charge() {
    RuleRegistry registry = create_default_registry();

    MovementContext ctx;
    ctx.move_type = MovementContext::MoveType::CHARGE;
    ctx.distance_modifier = 0;

    const auto& effects = registry.get_effects(RuleId::Agile);
    apply_movement_effect(effects, MoveSubPhase::CALCULATE_DISTANCE, ctx, 0);

    ASSERT_EQ(ctx.distance_modifier, 2);  // +2" on charge
}

void test_rapid_charge_rule() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::RapidCharge));

    MovementContext ctx;
    ctx.charge_bonus = 0;

    const auto& effects = registry.get_effects(RuleId::RapidCharge);
    apply_movement_effect(effects, MoveSubPhase::CHARGE_RESOLVE, ctx, 0);

    ASSERT_EQ(ctx.charge_bonus, 4);  // +4" charge bonus
}

void test_hit_and_run_rule() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::HitAndRun));

    MovementContext ctx;
    ctx.hit_and_run_pending = false;

    const auto& effects = registry.get_effects(RuleId::HitAndRun);
    apply_movement_effect(effects, MoveSubPhase::POST_MOVE, ctx, 0);

    ASSERT_TRUE(ctx.hit_and_run_pending);
}

// ==============================================================================
// Deployment Rule Tests
// ==============================================================================

void test_scout_rule() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::Scout));

    // Verify hot data
    const auto& hot = registry.get_hot_data(RuleId::Scout);
    ASSERT_EQ(hot.id, RuleId::Scout);
    ASSERT_EQ(hot.game_phase, GamePhase::DEPLOYMENT);

    DeploymentContext ctx;
    ctx.zone = DeploymentContext::DeployZone::STANDARD;
    ctx.forward_distance = 0;

    const auto& effects = registry.get_effects(RuleId::Scout);
    auto effect = effects.deploy_effects[static_cast<size_t>(DeploySubPhase::SCOUT_MOVE)];
    if (effect) {
        effect(ctx, 0);
    }

    ASSERT_EQ(ctx.zone, DeploymentContext::DeployZone::FORWARD);
    ASSERT_EQ(ctx.forward_distance, 12);
    ASSERT_EQ(ctx.timing, DeploymentContext::DeployTiming::SCOUT_MOVE);
}

void test_ambush_rule() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::Ambush));

    DeploymentContext ctx;
    ctx.zone = DeploymentContext::DeployZone::STANDARD;
    ctx.min_enemy_distance = 0;

    const auto& effects = registry.get_effects(RuleId::Ambush);
    auto effect = effects.deploy_effects[static_cast<size_t>(DeploySubPhase::INFILTRATE)];
    if (effect) {
        effect(ctx, 0);
    }

    ASSERT_EQ(ctx.zone, DeploymentContext::DeployZone::ANYWHERE);
    ASSERT_EQ(ctx.min_enemy_distance, 9);
}

// ==============================================================================
// End Round Rule Tests
// ==============================================================================

void test_fearless_rule() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::Fearless));

    // Verify hot data
    const auto& hot = registry.get_hot_data(RuleId::Fearless);
    ASSERT_EQ(hot.id, RuleId::Fearless);
    ASSERT_EQ(hot.game_phase, GamePhase::END_ROUND);
    ASSERT_TRUE(has_trait(hot.traits, RuleTrait::AFFECTS_MORALE));
}

void test_no_retreat_rule() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::NoRetreat));

    const auto& hot = registry.get_hot_data(RuleId::NoRetreat);
    ASSERT_EQ(hot.game_phase, GamePhase::END_ROUND);
    ASSERT_EQ(hot.priority, RulePriority::EARLY);  // Processes before other morale
    ASSERT_TRUE(has_trait(hot.traits, RuleTrait::AFFECTS_MORALE));
}

void test_morale_boost_rule() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::MoraleBoost));

    const auto& hot = registry.get_hot_data(RuleId::MoraleBoost);
    ASSERT_EQ(hot.game_phase, GamePhase::END_ROUND);
    ASSERT_TRUE(has_trait(hot.traits, RuleTrait::AFFECTS_MORALE));
}

void test_hold_the_line_rule() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::HoldTheLine));

    const auto& hot = registry.get_hot_data(RuleId::HoldTheLine);
    ASSERT_EQ(hot.game_phase, GamePhase::END_ROUND);
    ASSERT_TRUE(has_trait(hot.traits, RuleTrait::AFFECTS_MORALE));
}

void test_regeneration_endround_rule() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::Regeneration));

    const auto& hot = registry.get_hot_data(RuleId::Regeneration);
    ASSERT_EQ(hot.game_phase, GamePhase::END_ROUND);
    ASSERT_EQ(hot.endround_sub_phase(), EndRoundSubPhase::REGENERATION);
    ASSERT_TRUE(has_trait(hot.traits, RuleTrait::HEALS_WOUNDS));
}

void test_battleborn_rule() {
    RuleRegistry registry = create_default_registry();

    ASSERT_TRUE(registry.is_registered(RuleId::Battleborn));

    const auto& hot = registry.get_hot_data(RuleId::Battleborn);
    ASSERT_EQ(hot.game_phase, GamePhase::END_ROUND);
    ASSERT_EQ(hot.priority, RulePriority::FIRST);  // Processes very early
    ASSERT_TRUE(has_trait(hot.traits, RuleTrait::AFFECTS_MORALE));
}

// ==============================================================================
// Alias Parsing Tests
// ==============================================================================

void test_movement_aliases() {
    RuleRegistry registry = create_default_registry();

    ASSERT_EQ(registry.parse_rule_name("Fast"), RuleId::Fast);
    ASSERT_EQ(registry.parse_rule_name("fast"), RuleId::Fast);
    ASSERT_EQ(registry.parse_rule_name("Slow"), RuleId::Slow);
    ASSERT_EQ(registry.parse_rule_name("Flying"), RuleId::Flying);
    ASSERT_EQ(registry.parse_rule_name("Strider"), RuleId::Strider);
    ASSERT_EQ(registry.parse_rule_name("Agile"), RuleId::Agile);
    ASSERT_EQ(registry.parse_rule_name("RapidCharge"), RuleId::RapidCharge);
    ASSERT_EQ(registry.parse_rule_name("HitAndRun"), RuleId::HitAndRun);
}

void test_deployment_aliases() {
    RuleRegistry registry = create_default_registry();

    ASSERT_EQ(registry.parse_rule_name("Scout"), RuleId::Scout);
    ASSERT_EQ(registry.parse_rule_name("scout"), RuleId::Scout);
    ASSERT_EQ(registry.parse_rule_name("Ambush"), RuleId::Ambush);
    ASSERT_EQ(registry.parse_rule_name("ambush"), RuleId::Ambush);
}

void test_endround_aliases() {
    RuleRegistry registry = create_default_registry();

    ASSERT_EQ(registry.parse_rule_name("Fearless"), RuleId::Fearless);
    ASSERT_EQ(registry.parse_rule_name("fearless"), RuleId::Fearless);
    ASSERT_EQ(registry.parse_rule_name("NoRetreat"), RuleId::NoRetreat);
    ASSERT_EQ(registry.parse_rule_name("MoraleBoost"), RuleId::MoraleBoost);
    ASSERT_EQ(registry.parse_rule_name("HoldTheLine"), RuleId::HoldTheLine);
    ASSERT_EQ(registry.parse_rule_name("Regeneration"), RuleId::Regeneration);
    ASSERT_EQ(registry.parse_rule_name("Battleborn"), RuleId::Battleborn);
}

// ==============================================================================
// Registry Registration Count Test
// ==============================================================================

void test_phase6_rules_registered() {
    RuleRegistry registry = create_default_registry();

    // Count how many Phase 6 rules are registered
    int movement_count = 0;
    int deploy_count = 0;
    int endround_count = 0;

    // Movement rules
    if (registry.is_registered(RuleId::Fast)) movement_count++;
    if (registry.is_registered(RuleId::Slow)) movement_count++;
    if (registry.is_registered(RuleId::Flying)) movement_count++;
    if (registry.is_registered(RuleId::Strider)) movement_count++;
    if (registry.is_registered(RuleId::Agile)) movement_count++;
    if (registry.is_registered(RuleId::RapidCharge)) movement_count++;
    if (registry.is_registered(RuleId::HitAndRun)) movement_count++;

    // Deployment rules
    if (registry.is_registered(RuleId::Scout)) deploy_count++;
    if (registry.is_registered(RuleId::Ambush)) deploy_count++;

    // End round rules
    if (registry.is_registered(RuleId::Fearless)) endround_count++;
    if (registry.is_registered(RuleId::NoRetreat)) endround_count++;
    if (registry.is_registered(RuleId::MoraleBoost)) endround_count++;
    if (registry.is_registered(RuleId::HoldTheLine)) endround_count++;
    if (registry.is_registered(RuleId::Regeneration)) endround_count++;
    if (registry.is_registered(RuleId::Battleborn)) endround_count++;

    ASSERT_EQ(movement_count, 7);
    ASSERT_EQ(deploy_count, 2);
    ASSERT_EQ(endround_count, 6);

    std::cout << " [Movement=" << movement_count
              << ", Deploy=" << deploy_count
              << ", EndRound=" << endround_count << "]";
}

// ==============================================================================
// Main
// ==============================================================================

int main() {
    std::cout << "=== Phase 6: Movement, Deployment, and End Round Tests ===\n\n";

    std::cout << "Movement Rule Tests:\n";
    RUN_TEST(test_fast_rule);
    RUN_TEST(test_slow_rule);
    RUN_TEST(test_flying_rule);
    RUN_TEST(test_strider_rule);
    RUN_TEST(test_agile_rule_advance);
    RUN_TEST(test_agile_rule_charge);
    RUN_TEST(test_rapid_charge_rule);
    RUN_TEST(test_hit_and_run_rule);

    std::cout << "\nDeployment Rule Tests:\n";
    RUN_TEST(test_scout_rule);
    RUN_TEST(test_ambush_rule);

    std::cout << "\nEnd Round Rule Tests:\n";
    RUN_TEST(test_fearless_rule);
    RUN_TEST(test_no_retreat_rule);
    RUN_TEST(test_morale_boost_rule);
    RUN_TEST(test_hold_the_line_rule);
    RUN_TEST(test_regeneration_endround_rule);
    RUN_TEST(test_battleborn_rule);

    std::cout << "\nAlias Parsing Tests:\n";
    RUN_TEST(test_movement_aliases);
    RUN_TEST(test_deployment_aliases);
    RUN_TEST(test_endround_aliases);

    std::cout << "\nRegistration Tests:\n";
    RUN_TEST(test_phase6_rules_registered);

    std::cout << "\n=== Results: " << tests_passed << " passed, "
              << tests_failed << " failed ===\n";

    if (tests_failed > 0) {
        std::cout << "\n=== SOME TESTS FAILED ===\n";
        return 1;
    }

    std::cout << "\n=== All Phase 6 Tests Passed ===\n";
    return 0;
}
