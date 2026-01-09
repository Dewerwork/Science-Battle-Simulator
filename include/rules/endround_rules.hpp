#pragma once

#include "core/types.hpp"
#include "core/phases.hpp"  // For EndRoundSubPhase

namespace battle {

// Forward declarations
struct Unit;
struct UnitSimState;
struct GameState;

// ==============================================================================
// End-Round Rule Context (for rule effect processing)
// ==============================================================================
// This is separate from EndRoundContext in contexts.hpp which is for
// batch processing. This context is for individual rule effects.

struct EndRoundRuleContext {
    // Unit info
    Unit* unit;
    UnitSimState* state;
    bool is_unit_a;

    // Game state reference
    GameState* game;
    u8 current_round;

    // Morale state
    i8 morale_modifier;      // Accumulated morale bonus/penalty
    bool auto_pass_morale;   // Automatically pass morale test
    bool reroll_available;   // Can reroll failed morale test
    bool cannot_retreat;     // NoRetreat rule active

    // Regeneration
    u8 regen_roll_target;    // Roll needed to regenerate (e.g., 5 = 5+)
    bool regen_available;    // Can attempt regeneration

    // Fear effects
    i8 fear_modifier;        // Enemy fear penalty to morale

    // Flags
    bool used_battleborn;    // Already attempted Battleborn rally
};

// ==============================================================================
// End-Round Effect Function
// ==============================================================================

using EndRoundEffectFn = void(*)(EndRoundRuleContext&);

// ==============================================================================
// End-Round Rule Definition
// ==============================================================================

struct EndRoundRuleDefinition {
    RuleId rule_id;
    EndRoundSubPhase phase;  // From core/phases.hpp
    EndRoundEffectFn effect;

    // AI hints
    struct AIHints {
        float survival_bonus;    // How much this improves survival
        bool affects_morale;     // Affects morale tests
        bool affects_wounds;     // Affects wound recovery
    } ai_hints;
};

// ==============================================================================
// End-Round Rules Registry Functions
// ==============================================================================

// Register all end-round rule effects
void register_endround_rules();

// Get end-round rule definition for a rule ID
const EndRoundRuleDefinition* get_endround_rule(RuleId id);

// Check if a rule affects end-round
bool is_endround_rule(RuleId id);

// ==============================================================================
// End-Round Processor
// ==============================================================================

class EndRoundProcessor {
public:
    // Process all end-round effects for a unit
    static void process_unit_end_round(
        Unit& unit,
        UnitSimState& state,
        GameState& game,
        bool is_unit_a);

    // Process regeneration for a unit
    static u8 process_regeneration(
        Unit& unit,
        UnitSimState& state,
        u32& rng_state);

    // Check Battleborn rally (4+ to remove shaken)
    static bool check_battleborn_rally(
        Unit& unit,
        UnitSimState& state,
        u32& rng_state);

    // Get morale modifier from all applicable rules
    static i8 get_morale_modifier(
        const Unit& unit,
        const UnitSimState& state);

    // Check if unit has reroll for morale
    static bool has_morale_reroll(
        const Unit& unit,
        const UnitSimState& state);
};

} // namespace battle
