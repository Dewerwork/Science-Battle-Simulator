#pragma once

#include "core/types.hpp"
#include "engine/game_state.hpp"

namespace battle {

// Forward declarations
struct Unit;
struct UnitSimState;

// ==============================================================================
// Deployment Rules - Handle pre-game deployment based on rules
// ==============================================================================

// Deployment rule types
enum class DeploymentType : u8 {
    Standard,      // Normal deployment at edge
    Scout,         // Forward deploy (closer to center)
    Ambush,        // Flexible deploy (anywhere on own half)
    DeepStrike,    // Reserve deployment (appears later)
    Infiltrate     // Can deploy in enemy territory
};

// ==============================================================================
// Deployment Rule Context (for rule processing)
// ==============================================================================
// This is separate from DeploymentContext in contexts.hpp

struct DeploymentRuleContext {
    const Unit* unit;
    bool is_unit_a;

    // Deployment zone
    i8 standard_position;     // Default starting position
    i8 forward_position;      // Scout forward position
    i8 ambush_min;            // Minimum position for ambush
    i8 ambush_max;            // Maximum position for ambush

    // Flags
    bool has_scout;
    bool has_ambush;
    bool has_deep_strike;
    bool has_infiltrate;
};

// ==============================================================================
// Deployment Rule Definition
// ==============================================================================

struct DeploymentRuleDefinition {
    RuleId rule_id;
    DeploymentType type;
    i8 position_modifier;     // How much closer to center (for Scout)

    // AI hints
    struct AIHints {
        bool prefers_forward;    // Wants to start closer to enemy
        bool prefers_objective;  // Wants to start near objective
        float aggression_bonus;  // How much this encourages aggressive play
    } ai_hints;
};

// ==============================================================================
// Deployment Rules Registry Functions
// ==============================================================================

// Register all deployment rule effects
void register_deployment_rules();

// Get deployment rule definition for a rule ID
const DeploymentRuleDefinition* get_deployment_rule(RuleId id);

// Check if a rule affects deployment
bool is_deployment_rule(RuleId id);

// ==============================================================================
// Deployment Processor
// ==============================================================================

class DeploymentProcessor {
public:
    // Calculate starting position for a unit
    static i8 calculate_starting_position(
        const Unit& unit,
        bool is_unit_a);

    // Get deployment type for a unit
    static DeploymentType get_deployment_type(const Unit& unit);

    // Build deployment context for a unit
    static DeploymentRuleContext build_context(
        const Unit& unit,
        bool is_unit_a);

    // Apply deployment rules to game state
    static void apply_deployment(
        GameState& state,
        const Unit& unit_a,
        const Unit& unit_b);
};

// ==============================================================================
// Constants
// ==============================================================================

// Standard deployment positions
constexpr i8 STANDARD_DEPLOY_A = -STARTING_DISTANCE;  // Unit A starts at -12
constexpr i8 STANDARD_DEPLOY_B = STARTING_DISTANCE;   // Unit B starts at +12

// Scout forward deployment (6" closer to center)
constexpr i8 SCOUT_FORWARD = 6;

// Ambush zone (own half of the table)
constexpr i8 AMBUSH_MIN_A = -STARTING_DISTANCE;
constexpr i8 AMBUSH_MAX_A = -3;  // Can't deploy on objective
constexpr i8 AMBUSH_MIN_B = 3;   // Can't deploy on objective
constexpr i8 AMBUSH_MAX_B = STARTING_DISTANCE;

} // namespace battle
