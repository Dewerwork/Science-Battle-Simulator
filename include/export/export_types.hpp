#pragma once

#include "core/types.hpp"
#include "engine/game_state.hpp"
#include <vector>
#include <string>
#include <chrono>

namespace battle {
namespace export_data {

// ==============================================================================
// Verbose Dice Roll Recording
// ==============================================================================

struct DieRoll {
    u8 value;           // The die result (1-6)
    bool is_success;    // Did it meet the target?
    bool is_six;        // Was it a natural 6?
};

struct DiceRollEvent {
    std::string context;              // e.g., "to_hit", "defense", "morale", "regeneration"
    u8 target;                        // Target number needed
    i8 modifier;                      // Any modifier applied
    u8 effective_target;              // Final target after modifiers
    std::vector<DieRoll> dice;        // Individual dice results
    u32 total_successes;
    u32 total_sixes;

    // For defense rolls
    u8 ap_value = 0;
    bool reroll_sixes = false;        // Poison/Bane effect
    u32 sixes_rerolled = 0;
    u32 reroll_successes = 0;
};

// ==============================================================================
// Weapon and Attack Events
// ==============================================================================

struct WeaponInfo {
    std::string name;
    u8 range;
    u8 attacks;
    u8 ap;
    std::vector<std::string> rules;   // e.g., ["Rending", "Deadly(3)"]
    bool is_melee;
};

struct AttackSequence {
    std::string attacker_model_name;
    u8 attacker_model_index;
    WeaponInfo weapon;

    // Attack calculation
    u8 base_attacks;
    std::vector<std::pair<std::string, i8>> attack_modifiers;  // (reason, modifier)
    u8 total_attacks;

    // Hit roll
    DiceRollEvent hit_roll;
    u32 hits_before_modifiers;
    std::vector<std::pair<std::string, i32>> hit_modifiers;    // Applied after roll
    u32 hits_after_modifiers;
    u32 rending_hits;                 // Hits that get bonus AP from Rending

    // Bonus hits from special rules
    std::vector<std::pair<std::string, u32>> bonus_hits;       // (rule_name, count)

    // Defense roll
    DiceRollEvent defense_roll;
    DiceRollEvent rending_defense_roll;  // Separate roll for rending hits
    u8 base_ap;
    u8 effective_ap;
    std::vector<std::pair<std::string, i8>> ap_modifiers;      // (reason, modifier)

    // Wounds
    u32 wounds_from_normal_hits;
    u32 wounds_from_rending_hits;
    u32 total_wounds;

    // Special rule effects
    std::vector<std::string> triggered_rules;
};

// ==============================================================================
// Wound Allocation Events
// ==============================================================================

struct ModelWoundEvent {
    std::string model_name;
    u8 model_index;
    u8 wounds_allocated;

    // Regeneration
    bool has_regeneration;
    DiceRollEvent regeneration_roll;
    u32 wounds_regenerated;
    u32 wounds_after_regeneration;

    // Tough saves
    u8 tough_value;
    u8 wounds_before;
    u8 wounds_after;
    bool model_killed;
};

struct WoundAllocationSequence {
    u32 total_wounds_to_allocate;
    std::vector<std::string> allocation_order;  // Model names in order
    std::vector<ModelWoundEvent> allocations;

    u32 total_wounds_dealt;
    u32 total_wounds_regenerated;
    u32 total_models_killed;
    u32 overkill_wounds;
};

// ==============================================================================
// Combat Events (Shooting/Melee)
// ==============================================================================

struct ImpactEvent {
    u8 impact_value;
    u8 counter_reduction;
    u8 effective_impact;
    DiceRollEvent impact_roll;
    u32 impact_hits;
    DiceRollEvent impact_defense_roll;
    u32 impact_wounds;
    WoundAllocationSequence wound_allocation;
};

struct CombatEvent {
    std::string phase;                // "shooting" or "melee"
    std::string attacker_name;
    std::string defender_name;
    bool is_charging;
    bool defender_strikes_first;      // Counter rule

    // Impact (melee only)
    bool has_impact;
    ImpactEvent impact;

    // Attack sequences
    std::vector<AttackSequence> attacks;

    // Wound allocation
    WoundAllocationSequence wound_allocation;

    // Summary
    u32 total_wounds_dealt;
    u32 total_models_killed;
    bool target_destroyed;
    bool target_shaken;
    bool target_routed;
};

// ==============================================================================
// Morale Events
// ==============================================================================

struct MoraleEvent {
    std::string unit_name;
    std::string trigger_reason;       // e.g., "half_strength", "lost_melee"
    bool is_from_melee;
    u32 wounds_taken;                 // For melee comparison
    u32 wounds_dealt;                 // For melee comparison

    // Test details
    u8 quality_target;
    DiceRollEvent morale_roll;
    bool initial_pass;

    // Fearless reroll
    bool has_fearless;
    DiceRollEvent fearless_reroll;
    bool fearless_pass;

    // Result
    bool final_pass;
    std::string result;               // "passed", "shaken", "routed"
    std::string previous_status;
    std::string new_status;
};

// ==============================================================================
// AI Decision Events
// ==============================================================================

struct AIDecisionFactor {
    std::string factor;
    std::string value;
    std::string impact;
};

struct AIDecisionEvent {
    std::string unit_name;
    std::string ai_type;              // "Melee", "Shooting", "Hybrid"
    std::string current_status;
    i8 position;
    i8 distance_to_enemy;
    i8 distance_to_objective;
    bool controls_objective;
    bool enemy_destroyed;
    bool in_melee;

    std::vector<AIDecisionFactor> factors_considered;
    std::string decision;             // Final action chosen
    std::string reasoning;            // Human-readable explanation
};

// ==============================================================================
// Movement Events
// ==============================================================================

struct MovementEvent {
    std::string unit_name;
    std::string movement_type;        // "advance", "rush", "charge", "consolidation"
    i8 from_position;
    i8 to_position;
    i8 distance_moved;
    std::string reason;
};

// ==============================================================================
// Unit State Snapshot
// ==============================================================================

struct ModelSnapshot {
    std::string name;
    u8 index;
    u8 wounds_taken;
    u8 wounds_remaining;
    std::string state;                // "Healthy", "Wounded", "Dead"
    bool is_hero;
};

struct UnitSnapshot {
    std::string name;
    std::string faction;
    u32 unit_id;
    u16 points_cost;
    std::string ai_type;
    u8 quality;
    u8 defense;
    std::string status;               // "Normal", "Shaken", "Routed"
    bool is_fatigued;
    i8 position;
    u8 models_alive;
    u8 models_total;
    u16 wounds_remaining;
    std::vector<ModelSnapshot> models;
};

struct GameStateSnapshot {
    UnitSnapshot unit_a;
    UnitSnapshot unit_b;
    bool in_melee;
    i8 distance_between;
    std::string objective_holder;     // "none", "unit_a", "unit_b", "contested"
};

// ==============================================================================
// Activation Event
// ==============================================================================

struct ActivationEvent {
    std::string unit_name;
    bool is_unit_a;
    std::string action_type;          // "Hold", "Advance", "Rush", "Charge", "Rally", "Idle"

    // AI decision
    AIDecisionEvent ai_decision;

    // Movement (if any)
    bool has_movement;
    MovementEvent movement;

    // Combat (if any)
    bool has_combat;
    CombatEvent combat;

    // Counter-attack (melee only)
    bool has_counter_attack;
    CombatEvent counter_attack;

    // Morale (if any)
    bool has_morale_check;
    MoraleEvent morale;

    // State after activation
    GameStateSnapshot state_after;
};

// ==============================================================================
// Round Event
// ==============================================================================

struct InitiativeRoll {
    bool is_round_one;
    DiceRollEvent roll;               // Only for round 1
    bool unit_a_goes_first;
    std::string reason;               // "random_roll" or "alternating"
};

struct ObjectiveControlEvent {
    i8 unit_a_distance;
    i8 unit_b_distance;
    bool unit_a_in_range;
    bool unit_b_in_range;
    bool unit_a_shaken;
    bool unit_b_shaken;
    std::string holder;               // "none", "unit_a", "unit_b", "contested"
    std::string reason;
};

struct RoundEvent {
    u8 round_number;
    GameStateSnapshot state_at_start;

    // Initiative
    InitiativeRoll initiative;
    std::vector<std::string> activation_order;  // ["unit_a", "unit_b"] or reversed

    // Activations
    std::vector<ActivationEvent> activations;

    // End of round
    ObjectiveControlEvent objective_control;
    GameStateSnapshot state_at_end;
};

// ==============================================================================
// Game Setup
// ==============================================================================

struct WeaponExport {
    std::string name;
    u8 range;
    u8 attacks;
    u8 ap;
    std::vector<std::string> rules;
};

struct ModelExport {
    std::string name;
    u8 quality;
    u8 defense;
    u8 tough;
    bool is_hero;
    std::vector<std::string> rules;
    std::vector<WeaponExport> weapons;
};

struct UnitExport {
    std::string name;
    std::string faction;
    u32 unit_id;
    u16 points_cost;
    std::string ai_type;
    u8 quality;
    u8 defense;
    u8 model_count;
    u8 max_range;
    std::vector<std::string> unit_rules;
    std::vector<ModelExport> models;
};

struct InitialPositions {
    i8 unit_a_position;
    i8 unit_b_position;
    i8 distance_between;
    i8 objective_position;
};

struct GameSetup {
    UnitExport unit_a;
    UnitExport unit_b;
    InitialPositions positions;
};

// ==============================================================================
// Game Result
// ==============================================================================

struct UnitFinalStats {
    u16 total_wounds_dealt;
    u8 total_models_killed;
    u8 rounds_holding_objective;
    bool first_blood;
    u8 models_remaining;
    std::string final_status;
};

struct GameResultExport {
    std::string winner;               // "unit_a", "unit_b", "draw"
    std::string victory_type;         // "objective_control", "destruction", "rout"
    u8 rounds_played;
    std::string ending_condition;     // "max_rounds", "both_destroyed", etc.

    UnitFinalStats unit_a_stats;
    UnitFinalStats unit_b_stats;
};

// ==============================================================================
// Complete Game Export
// ==============================================================================

struct ExportMetadata {
    std::string export_version;
    std::string timestamp;
    std::string simulator_version;
    u64 seed;
};

struct GameExport {
    ExportMetadata metadata;
    GameSetup setup;
    std::vector<RoundEvent> rounds;
    GameResultExport result;
};

} // namespace export_data
} // namespace battle
