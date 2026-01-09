#pragma once

#include "core/types.hpp"
#include "core/contexts.hpp"

namespace battle {

// Forward declarations
struct Unit;
struct UnitSimState;

// ==============================================================================
// Movement Sub-Phases
// ==============================================================================

enum class MovementSubPhase : u8 {
    CALCULATE_DISTANCE,  // Calculate base movement distance
    CHARGE_DECLARE,      // Determine charge eligibility and range
    POST_MOVE,           // Effects after movement completes
    COUNT
};

// ==============================================================================
// Movement Effect Function
// ==============================================================================

// Effect function signature for movement rules
// Uses the existing MovementContext from core/contexts.hpp
using MovementEffectFn = void(*)(MovementContext&);

// ==============================================================================
// Movement Rule Definition
// ==============================================================================

struct MovementRuleDefinition {
    RuleId rule_id;
    MovementSubPhase phase;
    MovementEffectFn effect;

    // AI hints for movement
    struct AIHints {
        float charge_preference;    // How much to prefer charging
        float retreat_preference;   // How much to prefer retreating
        bool prefers_close_range;   // Prefers to close distance
        bool prefers_long_range;    // Prefers to maintain distance
    } ai_hints;
};

// ==============================================================================
// Movement Rules Registry Functions
// ==============================================================================

// Register all movement rule effects
void register_movement_rules();

// Get movement rule definition for a rule ID
const MovementRuleDefinition* get_movement_rule(RuleId id);

// Check if a rule affects movement
bool is_movement_rule(RuleId id);

// ==============================================================================
// Movement Calculator - Applies all movement rules
// ==============================================================================

class MovementCalculator {
public:
    // Calculate effective movement distance for a unit
    // Uses MovementContext::MoveType from contexts.hpp
    static u8 calculate_move_distance(
        const Unit& unit,
        UnitSimState* state,
        MovementContext::MoveType move_type);

    // Calculate effective charge range for a unit
    static u8 calculate_charge_range(
        const Unit& unit,
        UnitSimState* state);

    // Process all movement effects for a context
    static void process_movement_effects(
        MovementContext& ctx,
        MovementSubPhase phase);

    // Check if unit can fly (ignoring terrain/engagement)
    static bool can_fly(const Unit& unit);

    // Check if unit can hit-and-run (disengage after melee)
    static bool can_hit_and_run(const Unit& unit);
};

} // namespace battle
