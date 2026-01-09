#pragma once

#include "core/types.hpp"

namespace battle {

// ==============================================================================
// Game Phase Definitions
// ==============================================================================
// These enums define the various phases and sub-phases of game execution.
// They are used by the rule registry to determine when rules should trigger.

// Main game phases
enum class GamePhase : u8 {
    DEPLOYMENT,     // Initial unit placement
    MOVEMENT,       // Unit movement and charges
    COMBAT,         // Shooting and melee resolution
    END_ROUND,      // Morale, regeneration, scoring
    PASSIVE         // Always-active rules (stat modifiers, auras)
};

// Combat sub-phases - ordered by execution sequence
enum class CombatSubPhase : u8 {
    PRE_ATTACK,         // Before attack rolls (Limited weapon check, VersatileAttack mode)
    HIT_MODIFIERS,      // Apply hit modifiers (+/- to hit)
    ROLL_HITS,          // Quality overrides (Reliable) and dice rolls
    HIT_SEPARATION,     // Separate special hits (Rending, Rupture)
    HIT_BONUSES,        // Extra hits from rules (Relentless, Surge, Furious, PredatorFighter)
    HIT_MULTIPLICATION, // Multiply hits (Blast)
    DEFENSE_RESOLUTION, // Defense rolls with AP modifications
    WOUND_ALLOCATION,   // Apply wounds to models (Deadly, Regeneration, Resistance)

    COUNT // Number of combat sub-phases
};

// Movement sub-phases - ordered by execution sequence
enum class MoveSubPhase : u8 {
    PRE_MOVE,           // Before movement calculation
    CALCULATE_DISTANCE, // Determine move distance (Fast, Slow)
    EXECUTE_MOVE,       // Perform the move (Flying, Strider)
    CHARGE_DECLARE,     // Declare charge targets
    CHARGE_RESOLVE,     // Resolve charge moves (Agile, RapidCharge)
    POST_MOVE,          // After movement (Hit and Run disengage)

    COUNT // Number of movement sub-phases
};

// Deployment sub-phases - ordered by execution sequence
enum class DeploySubPhase : u8 {
    SETUP,              // Standard deployment
    SCOUT_MOVE,         // Scout forward deployment
    INFILTRATE,         // Ambush placement
    RESERVES,           // Reserve arrivals

    COUNT // Number of deployment sub-phases
};

// End round sub-phases - ordered by execution sequence
enum class EndRoundSubPhase : u8 {
    MORALE,             // Morale tests and rallying
    REGENERATION,       // End-of-round healing
    SCORING,            // Objective scoring
    CLEANUP,            // State cleanup

    COUNT // Number of end round sub-phases
};

// Compile-time constants for array sizing
constexpr size_t COMBAT_SUB_PHASE_COUNT = static_cast<size_t>(CombatSubPhase::COUNT);
constexpr size_t MOVE_SUB_PHASE_COUNT = static_cast<size_t>(MoveSubPhase::COUNT);
constexpr size_t DEPLOY_SUB_PHASE_COUNT = static_cast<size_t>(DeploySubPhase::COUNT);
constexpr size_t ENDROUND_SUB_PHASE_COUNT = static_cast<size_t>(EndRoundSubPhase::COUNT);

// ==============================================================================
// Combat Type
// ==============================================================================

enum class CombatType : u8 {
    SHOOTING,   // Ranged attacks
    MELEE,      // Close combat attacks
    BOTH        // Rule applies to both combat types
};

// ==============================================================================
// Target Type - Who the rule affects
// ==============================================================================

enum class Target : u8 {
    ATTACKER,   // Rule is on the attacking unit/model
    DEFENDER,   // Rule is on the defending unit/model
    WEAPON,     // Rule is on the weapon being used
    SELF        // Rule affects only the bearer
};

// ==============================================================================
// Trigger Type - When the rule activates
// ==============================================================================

enum class Trigger : u8 {
    ALWAYS,         // Always active when conditions met
    ON_CHARGE,      // Only when charging
    ON_MODEL_DEATH, // When a model dies
    AFTER_COMBAT,   // After combat resolution
    FIRST_TURN,     // Only on first turn
    WHEN_WOUNDED,   // When taking wounds
    ON_HIT_ROLL_6,  // On unmodified 6 to hit
    ON_DEF_ROLL_1   // On unmodified 1 to defend
};

// ==============================================================================
// Rule Priority - Execution order within a phase
// ==============================================================================

enum class RulePriority : u8 {
    FIRST = 0,      // Always applies first (e.g., quality overrides)
    EARLY = 32,     // Early modifiers
    NORMAL = 64,    // Default priority
    LATE = 96,      // Late modifiers
    LAST = 128      // Always applies last (e.g., caps, floors)
};

// ==============================================================================
// Phase Execution Order Arrays
// ==============================================================================

// Combat phases in execution order
constexpr CombatSubPhase COMBAT_PHASE_ORDER[] = {
    CombatSubPhase::PRE_ATTACK,
    CombatSubPhase::HIT_MODIFIERS,
    CombatSubPhase::ROLL_HITS,
    CombatSubPhase::HIT_SEPARATION,
    CombatSubPhase::HIT_BONUSES,
    CombatSubPhase::HIT_MULTIPLICATION,
    CombatSubPhase::DEFENSE_RESOLUTION,
    CombatSubPhase::WOUND_ALLOCATION
};

// Movement phases in execution order
constexpr MoveSubPhase MOVE_PHASE_ORDER[] = {
    MoveSubPhase::PRE_MOVE,
    MoveSubPhase::CALCULATE_DISTANCE,
    MoveSubPhase::EXECUTE_MOVE,
    MoveSubPhase::CHARGE_DECLARE,
    MoveSubPhase::CHARGE_RESOLVE,
    MoveSubPhase::POST_MOVE
};

} // namespace battle
