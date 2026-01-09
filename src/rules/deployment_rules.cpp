#include "rules/deployment_rules.hpp"
#include "core/unit.hpp"
#include <unordered_map>
#include <algorithm>

namespace battle {

// ==============================================================================
// Deployment Rules Registry
// ==============================================================================

namespace {

std::unordered_map<u16, DeploymentRuleDefinition> g_deployment_rules;
bool g_deployment_rules_initialized = false;

void init_deployment_rules() {
    if (g_deployment_rules_initialized) return;

    // Scout - Forward deploy 6" closer to center
    g_deployment_rules[static_cast<u16>(RuleId::Scout)] = {
        RuleId::Scout,
        DeploymentType::Scout,
        SCOUT_FORWARD,
        {true, false, 0.3f}  // Prefers forward position
    };

    // Ambush - Can deploy anywhere in own half
    g_deployment_rules[static_cast<u16>(RuleId::Ambush)] = {
        RuleId::Ambush,
        DeploymentType::Ambush,
        0,  // Position calculated dynamically
        {true, true, 0.2f}  // Can choose best position
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

    // Check for Scout rule
    if (unit.has_rule(RuleId::Scout)) {
        // Move 6" closer to center
        if (is_unit_a) {
            base_position += SCOUT_FORWARD;  // -12 + 6 = -6
        } else {
            base_position -= SCOUT_FORWARD;  // +12 - 6 = +6
        }
    }

    // Check for Ambush rule - deploy at optimal position
    if (unit.has_rule(RuleId::Ambush)) {
        // AI chooses optimal position based on unit type
        switch (unit.ai_type) {
            case AIType::Melee:
                // Melee units want to be as close as possible
                if (is_unit_a) {
                    base_position = AMBUSH_MAX_A;  // -3 (closest to objective)
                } else {
                    base_position = AMBUSH_MIN_B;  // +3 (closest to objective)
                }
                break;

            case AIType::Shooting:
                // Shooting units prefer to maintain range
                // Stay at standard position for maximum shooting distance
                base_position = is_unit_a ? STANDARD_DEPLOY_A : STANDARD_DEPLOY_B;
                break;

            case AIType::Hybrid:
                // Hybrid units compromise - move a bit forward
                if (is_unit_a) {
                    base_position = -6;  // Halfway forward
                } else {
                    base_position = 6;   // Halfway forward
                }
                break;
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
    // Calculate and apply starting positions
    state.pos_a = calculate_starting_position(unit_a, true);
    state.pos_b = calculate_starting_position(unit_b, false);
}

} // namespace battle
