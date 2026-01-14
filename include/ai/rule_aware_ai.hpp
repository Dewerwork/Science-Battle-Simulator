#pragma once

#include "ai/ai_hints.hpp"
#include "core/types.hpp"
#include "core/unit.hpp"
#include "core/rule_registry.hpp"
#include "engine/game_state.hpp"
#include <algorithm>
#include <cmath>

namespace battle {

// ==============================================================================
// AI Personality - Configurable behavior modifiers
// ==============================================================================
// Personalities adjust the base AI decision weights to create different
// playstyles. Use presets or create custom personalities.

struct AIPersonality {
    // Aggression factor: -100 (very defensive) to +100 (very aggressive)
    i8 aggression = 0;

    // Risk tolerance: -100 (risk averse) to +100 (risk seeking)
    i8 risk_tolerance = 0;

    // Objective focus: -100 (ignore objective) to +100 (prioritize objective)
    i8 objective_focus = 50;

    // Charge preference: -100 (avoid charges) to +100 (always charge)
    i8 charge_preference = 0;

    // Distance preference: -100 (prefer close) to +100 (prefer far)
    i8 distance_preference = 0;

    // Default constructor - balanced personality
    constexpr AIPersonality() = default;

    // Full constructor
    constexpr AIPersonality(i8 aggr, i8 risk, i8 obj, i8 charge, i8 dist)
        : aggression(aggr)
        , risk_tolerance(risk)
        , objective_focus(obj)
        , charge_preference(charge)
        , distance_preference(dist) {}

    // Fluent builders
    constexpr AIPersonality& set_aggression(i8 v) { aggression = v; return *this; }
    constexpr AIPersonality& set_risk(i8 v) { risk_tolerance = v; return *this; }
    constexpr AIPersonality& set_objective(i8 v) { objective_focus = v; return *this; }
    constexpr AIPersonality& set_charge(i8 v) { charge_preference = v; return *this; }
    constexpr AIPersonality& set_distance(i8 v) { distance_preference = v; return *this; }
};

// ==============================================================================
// Preset Personalities
// ==============================================================================

namespace personality {

// Balanced - Default, no strong preferences
constexpr AIPersonality BALANCED{0, 0, 50, 0, 0};

// Aggressive - Charges frequently, takes risks
constexpr AIPersonality AGGRESSIVE{70, 40, 30, 60, -40};

// Defensive - Holds position, avoids risks
constexpr AIPersonality DEFENSIVE{-50, -40, 60, -40, 30};

// Berserker - Maximum aggression, ignores objectives
constexpr AIPersonality BERSERKER{100, 80, -20, 100, -80};

// Tactician - Balanced with objective focus
constexpr AIPersonality TACTICIAN{0, 0, 80, 0, 0};

// Coward - Extremely risk-averse
constexpr AIPersonality COWARD{-80, -80, 70, -70, 60};

// Gunline - Prefers ranged combat
constexpr AIPersonality GUNLINE{-30, -20, 40, -60, 70};

// Brawler - Prefers melee combat
constexpr AIPersonality BRAWLER{50, 30, 20, 70, -60};

} // namespace personality

// ==============================================================================
// Action Score - Evaluation result for a potential action
// ==============================================================================

struct ActionScore {
    ActionType action = ActionType::Idle;
    i32 score = 0;           // Higher is better
    const char* reason = ""; // Debug info

    bool operator<(const ActionScore& other) const {
        return score < other.score;
    }

    bool operator>(const ActionScore& other) const {
        return score > other.score;
    }
};

// ==============================================================================
// Rule-Aware AI Controller
// ==============================================================================
// Enhanced AI that uses rule-based hints to make better tactical decisions.
// Builds an AIProfile from unit rules and combines it with personality to
// evaluate possible actions.

class RuleAwareAI {
public:
    // Construct with registry and optional personality
    explicit RuleAwareAI(const RuleRegistry& registry,
                         AIPersonality personality = personality::BALANCED)
        : registry_(registry)
        , personality_(personality) {}

    // Set personality
    void set_personality(const AIPersonality& p) { personality_ = p; }

    // Get current personality
    const AIPersonality& get_personality() const { return personality_; }

    // ==============================================================================
    // Main Decision Function
    // ==============================================================================

    // Decide action for a unit based on game state, rules, and personality
    ActionType decide_action(const GameState& state, bool is_unit_a) const {
        const Unit* unit = is_unit_a ? state.unit_a_ptr : state.unit_b_ptr;
        const UnitSimState& unit_state = is_unit_a ? state.state_a : state.state_b;
        const UnitSimState& enemy_state = is_unit_a ? state.state_b : state.state_a;

        // Handle forced states first
        if (unit_state.is_out_of_action()) {
            return ActionType::Idle;
        }

        if (unit_state.is_shaken()) {
            return ActionType::Rally;
        }

        // Immobile units can only Hold
        if (unit->has_rule(RuleId::Immobile)) {
            return ActionType::Hold;
        }

        // Already in melee - must hold
        if (state.in_melee) {
            return ActionType::Hold;
        }

        // Build AI profile from unit's rules
        AIProfile profile = build_profile(*unit);

        // Enemy destroyed - move to objective
        if (enemy_state.is_out_of_action()) {
            return decide_post_victory(state, is_unit_a, profile);
        }

        // Evaluate all possible actions and choose best
        ActionScore best = evaluate_all_actions(state, is_unit_a, profile);
        return best.action;
    }

    // ==============================================================================
    // Profile Building
    // ==============================================================================

    // Build an AIProfile from a unit's rules
    AIProfile build_profile(const Unit& unit) const {
        AIProfile profile;

        // Iterate through unit rules and aggregate hints
        for (u8 i = 0; i < unit.rule_count; ++i) {
            const auto& compact_rule = unit.rules[i];
            if (!compact_rule.is_valid()) continue;

            // Get AI hints for this rule (if any)
            AIHints hints = get_rule_hints(compact_rule.id);
            profile.add_hints(hints);
        }

        return profile;
    }

    // Get AI hints for a specific rule
    AIHints get_rule_hints(RuleId id) const {
        // Return predefined hints based on rule ID
        // This will be populated from rule definitions after Phase 7 integration
        switch (id) {
            // Combat hit modifiers
            case RuleId::Precise:
                return AIHintBuilder().offensive_bonus().offense(15).consistent();
            case RuleId::Reliable:
                return AIHintBuilder().offensive_bonus().offense(20).consistent();
            case RuleId::Rending:
                return AIHintBuilder().offensive_bonus().offense(25).target_tough();
            case RuleId::Blast:
                return AIHintBuilder().offensive_bonus().offense(30).high_variance().target_weak();
            case RuleId::Deadly:
                return AIHintBuilder().offensive_bonus().offense(40).high_variance().target_tough();
            case RuleId::Poison:
                return AIHintBuilder().offensive_bonus().offense(20).target_tough();
            case RuleId::Bane:
                return AIHintBuilder().offensive_bonus().offense(25).target_tough();
            case RuleId::AP:
                return AIHintBuilder().offensive_bonus().offense(25).target_tough();

            // Charge-related rules
            case RuleId::Lance:
                return AIHintBuilder().close_range().aggressive().charge_bonus().offense(30);
            case RuleId::Impact:
                return AIHintBuilder().close_range().aggressive().charge_bonus().first_strike().offense(35);
            case RuleId::Furious:
                return AIHintBuilder().close_range().aggressive().charge_bonus().offense(25);
            case RuleId::Thrust:
                return AIHintBuilder().close_range().aggressive().charge_bonus().offense(20);
            case RuleId::PiercingAssault:
                return AIHintBuilder().close_range().aggressive().charge_bonus().offense(15);

            // Defensive rules
            case RuleId::Regeneration:
                return AIHintBuilder().defensive().defensive_bonus().defense(30);
            case RuleId::Tough:
                return AIHintBuilder().defensive().defensive_bonus().defense(40);
            case RuleId::Stealth:
                return AIHintBuilder().long_range().defensive().defense(20).optimal_range(9, 36);
            case RuleId::Protected:
                return AIHintBuilder().defensive().defensive_bonus().defense(15);
            case RuleId::Shielded:
                return AIHintBuilder().defensive().defensive_bonus().defense(20);
            case RuleId::ShieldWall:
                return AIHintBuilder().close_range().defensive().melee_focused().defense(25);
            case RuleId::Resistance:
                return AIHintBuilder().defensive().defensive_bonus().defense(25);

            // Shooting bonuses
            case RuleId::Relentless:
                return AIHintBuilder().long_range().shooting_focused().holding_bonus().offense(20).optimal_range(9, 36);
            case RuleId::PointBlankSurge:
                return AIHintBuilder().close_range().shooting_focused().offense(15).optimal_range(0, 9);
            case RuleId::GoodShot:
                return AIHintBuilder().shooting_focused().offense(15);
            case RuleId::Sniper:
                return AIHintBuilder().long_range().shooting_focused().offense(20);
            case RuleId::Indirect:
                return AIHintBuilder().shooting_focused().offense(15);

            // Melee bonuses
            case RuleId::PredatorFighter:
                return AIHintBuilder().close_range().aggressive().melee_focused().offense(25);
            case RuleId::Counter:
                return AIHintBuilder().defensive().avoid_charges().melee_focused().defense(20);

            // Movement rules
            case RuleId::Fast:
                return AIHintBuilder().aggressive().mobile().charge_bonus().offense(15);
            case RuleId::Slow:
                return AIHintBuilder().defensive().static_bonus().defense(5);
            case RuleId::Flying:
                return AIHintBuilder().aggressive().mobile().offense(20);
            case RuleId::Strider:
                return AIHintBuilder().mobile().offense(10);
            case RuleId::Agile:
                return AIHintBuilder().aggressive().mobile().charge_bonus().offense(15);
            case RuleId::RapidCharge:
                return AIHintBuilder().aggressive().charge_bonus().first_strike().offense(20);
            case RuleId::HitAndRun:
                return AIHintBuilder().opportunistic().mobile().avoid_charges().offense(15);
            case RuleId::Immobile:
                return AIHintBuilder().defensive().static_bonus().holding_bonus().defense(10);

            // Morale rules
            case RuleId::Fearless:
                return AIHintBuilder().aggressive().defense(20);
            case RuleId::NoRetreat:
                return AIHintBuilder().aggressive().defense(15);
            case RuleId::Fear:
                return AIHintBuilder().close_range().aggressive().melee_focused().offense(20);

            // Special rules
            case RuleId::SelfDestruct:
                return AIHintBuilder().close_range().aggressive().high_variance().offense(20);
            case RuleId::Hero:
                return AIHintBuilder().defensive().defense(10);
            case RuleId::Unstoppable:
                return AIHintBuilder().aggressive().offensive_bonus().offense(30);

            // Deployment rules
            case RuleId::Scout:
                return AIHintBuilder().aggressive().mobile().first_strike();
            case RuleId::Ambush:
                return AIHintBuilder().opportunistic().first_strike();

            default:
                return AIHints{};  // Default neutral hints
        }
    }

private:
    const RuleRegistry& registry_;
    AIPersonality personality_;

    // ==============================================================================
    // Action Evaluation
    // ==============================================================================

    ActionScore evaluate_all_actions(const GameState& state, bool is_unit_a,
                                     const AIProfile& profile) const {
        ActionScore scores[5];
        int score_count = 0;

        // Evaluate each possible action
        scores[score_count++] = evaluate_hold(state, is_unit_a, profile);
        scores[score_count++] = evaluate_advance(state, is_unit_a, profile);
        scores[score_count++] = evaluate_rush(state, is_unit_a, profile);

        if (state.can_charge(is_unit_a)) {
            scores[score_count++] = evaluate_charge(state, is_unit_a, profile);
        }

        // Find highest scoring action
        ActionScore best = scores[0];
        for (int i = 1; i < score_count; ++i) {
            if (scores[i].score > best.score) {
                best = scores[i];
            }
        }

        return best;
    }

    ActionScore evaluate_hold(const GameState& state, bool is_unit_a,
                              const AIProfile& profile) const {
        ActionScore result{ActionType::Hold, 0, "hold"};

        // Base score for holding
        result.score = 100;

        // Can we shoot from here?
        if (state.can_shoot(is_unit_a)) {
            result.score += 50;

            // Relentless bonus for holding
            if (has_hint(profile.combined_flags, AIHintFlag::PREFERS_HOLDING)) {
                result.score += 30;
            }

            // Long range preference bonus
            if (profile.get_distance_preference() == DistancePreference::FAR) {
                result.score += 20;
            }
        } else {
            // Can't shoot, holding is less valuable
            result.score -= 50;
        }

        // Objective control bonus
        bool controls = is_unit_a ? state.unit_a_controls_objective() : state.unit_b_controls_objective();
        if (controls) {
            result.score += personality_.objective_focus;
        }

        // Defensive profile bonus
        if (profile.is_defensive()) {
            result.score += profile.total_defensive / 3;
        }

        // Personality adjustments
        result.score -= personality_.aggression / 2;

        return result;
    }

    ActionScore evaluate_advance(const GameState& state, bool is_unit_a,
                                 const AIProfile& profile) const {
        ActionScore result{ActionType::Advance, 0, "advance"};
        const Unit* unit = is_unit_a ? state.unit_a_ptr : state.unit_b_ptr;
        i8 my_pos = is_unit_a ? state.pos_a : state.pos_b;
        u8 move_speed = state.get_move_speed(*unit);

        // Base score for advancing
        result.score = 100;

        // Calculate new position after advance
        i8 new_pos = my_pos + (is_unit_a ? move_speed : -static_cast<i8>(move_speed));
        i8 new_dist = is_unit_a ? (state.pos_b - new_pos) : (new_pos - state.pos_a);

        // Can still shoot after advancing?
        if (unit->max_range >= static_cast<u8>(std::abs(new_dist))) {
            result.score += 40;
        }

        // Moving toward objective bonus
        bool controls = is_unit_a ? state.unit_a_controls_objective() : state.unit_b_controls_objective();
        if (!controls && std::abs(new_pos) < std::abs(my_pos)) {
            result.score += personality_.objective_focus / 2;
        }

        // Aggressive profile bonus
        if (profile.is_offensive()) {
            result.score += profile.total_offensive / 4;
        }

        // Getting closer to optimal range
        if (profile.in_optimal_range(static_cast<u8>(std::abs(new_dist)))) {
            result.score += 25;
        }

        // Personality adjustments
        result.score += personality_.aggression / 3;

        return result;
    }

    ActionScore evaluate_rush(const GameState& state, bool is_unit_a,
                              const AIProfile& profile) const {
        ActionScore result{ActionType::Rush, 0, "rush"};
        const Unit* unit = is_unit_a ? state.unit_a_ptr : state.unit_b_ptr;
        i8 my_pos = is_unit_a ? state.pos_a : state.pos_b;
        u8 move_speed = state.get_move_speed(*unit);
        u8 rush_dist = move_speed * RUSH_MULTIPLIER;

        // Base score for rushing
        result.score = 80;  // Lower base - gives up shooting

        // Calculate new position after rush
        i8 new_pos = my_pos + (is_unit_a ? rush_dist : -static_cast<i8>(rush_dist));

        // Strong bonus if we can charge next turn
        i8 new_dist = is_unit_a ? (state.pos_b - new_pos) : (new_pos - state.pos_a);
        u8 charge_dist = state.get_charge_distance(*unit);
        if (static_cast<u8>(std::abs(new_dist)) <= charge_dist) {
            result.score += 50;

            // Extra bonus if we have charge-related rules
            if (has_hint(profile.combined_flags, AIHintFlag::BENEFITS_FROM_CHARGE)) {
                result.score += 40;
            }
        }

        // Rush to objective bonus
        bool controls = is_unit_a ? state.unit_a_controls_objective() : state.unit_b_controls_objective();
        if (!controls) {
            result.score += personality_.objective_focus / 2;
        }

        // Mobile advantage bonus
        if (has_hint(profile.combined_flags, AIHintFlag::MOBILE_ADVANTAGE)) {
            result.score += 20;
        }

        // Close range preference bonus
        if (profile.get_distance_preference() == DistancePreference::CLOSE) {
            result.score += 25;
        }

        // Personality adjustments
        result.score += personality_.aggression / 2;
        result.score += personality_.charge_preference / 3;

        return result;
    }

    ActionScore evaluate_charge(const GameState& state, bool is_unit_a,
                                const AIProfile& profile) const {
        ActionScore result{ActionType::Charge, 0, "charge"};

        // Base score for charging
        result.score = 120;  // Higher base - decisive action

        // Charge bonus rules
        if (has_hint(profile.combined_flags, AIHintFlag::BENEFITS_FROM_CHARGE)) {
            result.score += 60;
        }

        // Melee preference bonus
        if (has_hint(profile.combined_flags, AIHintFlag::PREFERS_MELEE)) {
            result.score += 40;
        }

        // First strike bonus
        if (has_hint(profile.combined_flags, AIHintFlag::WANTS_FIRST_STRIKE)) {
            result.score += 30;
        }

        // Offensive profile bonus
        result.score += profile.total_offensive / 2;

        // Aggressive engagement preference
        if (profile.get_engagement_preference() == EngagementPreference::AGGRESSIVE) {
            result.score += 35;
        }

        // Close range preference bonus
        if (profile.get_distance_preference() == DistancePreference::CLOSE) {
            result.score += 25;
        }

        // Penalty if enemy has Counter
        const Unit* enemy = is_unit_a ? state.unit_b_ptr : state.unit_a_ptr;
        if (enemy && enemy->has_rule(RuleId::Counter)) {
            result.score -= 40;

            // Extra penalty if we're risk-averse
            if (personality_.risk_tolerance < 0) {
                result.score -= 20;
            }
        }

        // Personality adjustments
        result.score += personality_.aggression;
        result.score += personality_.charge_preference;
        result.score += personality_.risk_tolerance / 2;
        result.score -= personality_.distance_preference;

        return result;
    }

    // ==============================================================================
    // Post-Victory Decision
    // ==============================================================================

    ActionType decide_post_victory(const GameState& state, bool is_unit_a,
                                   [[maybe_unused]] const AIProfile& profile) const {
        i8 my_pos = is_unit_a ? state.pos_a : state.pos_b;

        // Already at objective?
        if (std::abs(my_pos) <= OBJECTIVE_CONTROL_RANGE) {
            return ActionType::Hold;
        }

        // Rush to objective
        return ActionType::Rush;
    }
};

// ==============================================================================
// Utility Functions
// ==============================================================================

// Create a rule-aware AI with personality based on unit's rules
inline RuleAwareAI create_ai_for_unit(const RuleRegistry& registry, const Unit& unit) {
    RuleAwareAI ai(registry);

    // Build profile and select appropriate personality
    AIProfile profile = ai.build_profile(unit);

    if (profile.aggressive_count > profile.defensive_count * 2) {
        ai.set_personality(personality::AGGRESSIVE);
    } else if (profile.defensive_count > profile.aggressive_count * 2) {
        ai.set_personality(personality::DEFENSIVE);
    } else if (profile.get_distance_preference() == DistancePreference::FAR) {
        ai.set_personality(personality::GUNLINE);
    } else if (profile.get_distance_preference() == DistancePreference::CLOSE) {
        ai.set_personality(personality::BRAWLER);
    }

    return ai;
}

// Get personality based on AIType (for backwards compatibility)
inline AIPersonality personality_from_ai_type(AIType type) {
    switch (type) {
        case AIType::Melee:
            return personality::BRAWLER;
        case AIType::Shooting:
            return personality::GUNLINE;
        case AIType::Hybrid:
        default:
            return personality::BALANCED;
    }
}

} // namespace battle
