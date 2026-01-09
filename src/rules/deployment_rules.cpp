#include "rules/deployment_rules.hpp"
#include "core/unit.hpp"
#include <unordered_map>
#include <algorithm>

namespace battle {

// ==============================================================================
// Deployment Rules Registry
// ==============================================================================

// Ambush deployment minimum distance from enemy
constexpr i8 AMBUSH_MIN_DISTANCE = 9;

namespace {

std::unordered_map<u16, DeploymentRuleDefinition> g_deployment_rules;
bool g_deployment_rules_initialized = false;

void init_deployment_rules() {
    if (g_deployment_rules_initialized) return;

    // Scout - Forward deploy 6" closer to center (Deploy 12" ahead)
    g_deployment_rules[static_cast<u16>(RuleId::Scout)] = {
        RuleId::Scout,
        DeploymentType::Scout,
        SCOUT_FORWARD,
        {true, false, 0.3f}  // Prefers forward position
    };

    // Ambush - Set aside before deployment, deploy after round 1, 9"+ from enemy
    // Units with Ambush can't seize/contest objectives on the round they deploy
    g_deployment_rules[static_cast<u16>(RuleId::Ambush)] = {
        RuleId::Ambush,
        DeploymentType::Ambush,
        0,  // Starts in reserve (position set when deploying)
        {true, true, 0.4f}  // Delayed deployment with flexibility
    };

    // Note: DeepStrike and Infiltrate are placeholders for future expansion
    // They would require more complex game state tracking

    g_deployment_rules_initialized = true;
}

}  // anonymous namespace

// ==============================================================================
// Public API
// ==============================================================================

void register_deployment_rules() {
    init_deployment_rules();
}

const DeploymentRuleDefinition* get_deployment_rule(RuleId id) {
    init_deployment_rules();
    auto it = g_deployment_rules.find(static_cast<u16>(id));
    if (it != g_deployment_rules.end()) {
        return &it->second;
    }
    return nullptr;
}

bool is_deployment_rule(RuleId id) {
    return get_deployment_rule(id) != nullptr;
}

// ==============================================================================
// DeploymentProcessor Implementation
// ==============================================================================

i8 DeploymentProcessor::calculate_starting_position(
    const Unit& unit,
    bool is_unit_a)
{
    i8 base_position = is_unit_a ? STANDARD_DEPLOY_A : STANDARD_DEPLOY_B;

    // Ambush: Unit starts in reserve, not on the table
    // Return a special position indicating "off table"
    // The position will be set when the unit deploys from reserve
    if (unit.has_rule(RuleId::Ambush)) {
        // Use a far position to indicate reserve (will be updated on deployment)
        // Unit A: -24 (far left), Unit B: +24 (far right)
        return is_unit_a ? -24 : 24;
    }

    // Check for Scout rule - deploy 6" closer to center
    if (unit.has_rule(RuleId::Scout)) {
        // Move 6" closer to center
        if (is_unit_a) {
            base_position += SCOUT_FORWARD;  // -12 + 6 = -6
        } else {
            base_position -= SCOUT_FORWARD;  // +12 - 6 = +6
        }
    }

    return base_position;
}

DeploymentType DeploymentProcessor::get_deployment_type(const Unit& unit) {
    // Check rules in priority order
    if (unit.has_rule(RuleId::Ambush)) {
        return DeploymentType::Ambush;
    }
    if (unit.has_rule(RuleId::Scout)) {
        return DeploymentType::Scout;
    }
    // Future: Check for DeepStrike, Infiltrate

    return DeploymentType::Standard;
}

DeploymentRuleContext DeploymentProcessor::build_context(
    const Unit& unit,
    bool is_unit_a)
{
    DeploymentRuleContext ctx{};
    ctx.unit = &unit;
    ctx.is_unit_a = is_unit_a;

    // Set positions
    ctx.standard_position = is_unit_a ? STANDARD_DEPLOY_A : STANDARD_DEPLOY_B;
    ctx.forward_position = is_unit_a ?
        (STANDARD_DEPLOY_A + SCOUT_FORWARD) :
        (STANDARD_DEPLOY_B - SCOUT_FORWARD);

    // Set ambush zone
    if (is_unit_a) {
        ctx.ambush_min = AMBUSH_MIN_A;
        ctx.ambush_max = AMBUSH_MAX_A;
    } else {
        ctx.ambush_min = AMBUSH_MIN_B;
        ctx.ambush_max = AMBUSH_MAX_B;
    }

    // Check rules
    ctx.has_scout = unit.has_rule(RuleId::Scout);
    ctx.has_ambush = unit.has_rule(RuleId::Ambush);
    ctx.has_deep_strike = false;  // Future expansion
    ctx.has_infiltrate = false;   // Future expansion

    return ctx;
}

void DeploymentProcessor::apply_deployment(
    GameState& state,
    const Unit& unit_a,
    const Unit& unit_b)
{
    // Set reserve flags for Ambush units
    state.unit_a_in_reserve = unit_a.has_rule(RuleId::Ambush);
    state.unit_b_in_reserve = unit_b.has_rule(RuleId::Ambush);

    // Calculate and apply starting positions
    state.pos_a = calculate_starting_position(unit_a, true);
    state.pos_b = calculate_starting_position(unit_b, false);
}

i8 DeploymentProcessor::calculate_ambush_position(
    const Unit& unit,
    bool is_unit_a,
    i8 enemy_position)
{
    // Ambush: Deploy anywhere 9"+ from enemy
    // Choose optimal position based on unit type
    // Unit A deploys on negative side, Unit B on positive side
    i8 safe_distance = AMBUSH_MIN_DISTANCE;

    // Calculate the closest legal position (9" from enemy)
    i8 closest_legal;
    if (is_unit_a) {
        // Unit A is on negative side - deploy at most at enemy_pos - 9"
        // But also stay on negative side (or at least not past enemy)
        closest_legal = enemy_position - safe_distance;
        // If enemy is far positive, we could deploy near objective
        // Clamp to not go past center too far on enemy's side
        if (closest_legal > 0) closest_legal = 0;  // Don't deploy past center
    } else {
        // Unit B is on positive side - deploy at least at enemy_pos + 9"
        closest_legal = enemy_position + safe_distance;
        // Clamp to not go past center on enemy's side
        if (closest_legal < 0) closest_legal = 0;  // Don't deploy past center
    }

    switch (unit.ai_type) {
        case AIType::Melee:
            // Melee units want to be as close as legally possible (9" from enemy)
            return closest_legal;

        case AIType::Shooting:
            // Shooting units prefer to stay at range but close to objective
            // Deploy at a moderate distance, favoring own side
            if (is_unit_a) {
                return std::min(closest_legal, static_cast<i8>(-6));
            } else {
                return std::max(closest_legal, static_cast<i8>(6));
            }

        case AIType::Hybrid:
        default:
            // Hybrid units deploy close to enemy but with flexibility
            return closest_legal;
    }
}

} // namespace battle
