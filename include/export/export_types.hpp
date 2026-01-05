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
    u8 value = 0;           // The die result (1-6)
    bool is_success = false;    // Did it meet the target?
    bool is_six = false;        // Was it a natural 6?
};

struct DiceRollEvent {
    std::string context;              // e.g., "to_hit", "defense", "morale", "regeneration"
    u8 target = 0;                    // Target number needed
    i8 modifier = 0;                  // Any modifier applied
    u8 effective_target = 0;          // Final target after modifiers
    std::vector<DieRoll> dice;        // Individual dice results
    u32 total_successes = 0;
    u32 total_sixes = 0;

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
    u8 range = 0;
    u8 attacks = 0;
    u8 ap = 0;
    std::vector<std::string> rules;   // e.g., ["Rending", "Deadly(3)"]
    bool is_melee = true;
};

struct AttackSequence {
    std::string attacker_model_name;
    u8 attacker_model_index = 0;
    WeaponInfo weapon;

    // Attack calculation
    u8 base_attacks = 0;
    std::vector<std::pair<std::string, i8>> attack_modifiers;  // (reason, modifier)
    u8 total_attacks = 0;

    // Hit roll
    DiceRollEvent hit_roll;
    u32 hits_before_modifiers = 0;
    std::vector<std::pair<std::string, i32>> hit_modifiers;    // Applied after roll
    u32 hits_after_modifiers = 0;
    u32 rending_hits = 0;                 // Hits that get bonus AP from Rending

    // Bonus hits from special rules
    std::vector<std::pair<std::string, u32>> bonus_hits;       // (rule_name, count)

    // Defense roll
    DiceRollEvent defense_roll;
    DiceRollEvent rending_defense_roll;  // Separate roll for rending hits
    u8 base_ap = 0;
    u8 effective_ap = 0;
    std::vector<std::pair<std::string, i8>> ap_modifiers;      // (reason, modifier)

    // Wounds
    u32 wounds_from_normal_hits = 0;
    u32 wounds_from_rending_hits = 0;
    u32 total_wounds = 0;

    // Special rule effects
    std::vector<std::string> triggered_rules;
};

// ==============================================================================
// Wound Allocation Events
// ==============================================================================

struct ModelWoundEvent {
    std::string model_name;
    u8 model_index = 0;
    u8 wounds_allocated = 0;

    // Regeneration
    bool has_regeneration = false;
    DiceRollEvent regeneration_roll;
    u32 wounds_regenerated = 0;
    u32 wounds_after_regeneration = 0;

    // Tough saves
    u8 tough_value = 0;
    u8 wounds_before = 0;
    u8 wounds_after = 0;
    bool model_killed = false;
};

struct WoundAllocationSequence {
    u32 total_wounds_to_allocate = 0;
    std::vector<std::string> allocation_order;  // Model names in order
    std::vector<ModelWoundEvent> allocations;

    u32 total_wounds_dealt = 0;
    u32 total_wounds_regenerated = 0;
    u32 total_models_killed = 0;
    u32 overkill_wounds = 0;
};

// ==============================================================================
// Combat Events (Shooting/Melee)
// ==============================================================================

struct ImpactEvent {
    u8 impact_value = 0;
    u8 counter_reduction = 0;
    u8 effective_impact = 0;
    DiceRollEvent impact_roll;
    u32 impact_hits = 0;
    DiceRollEvent impact_defense_roll;
    u32 impact_wounds = 0;
    WoundAllocationSequence wound_allocation;
};

struct CombatEvent {
    std::string phase;                // "shooting" or "melee"
    std::string attacker_name;
    std::string defender_name;
    bool is_charging = false;
    bool defender_strikes_first = false;      // Counter rule

    // Impact (melee only)
    bool has_impact = false;
    ImpactEvent impact;

    // Attack sequences
    std::vector<AttackSequence> attacks;

    // Wound allocation
    WoundAllocationSequence wound_allocation;

    // Summary
    u32 total_wounds_dealt = 0;
    u32 total_models_killed = 0;
    bool target_destroyed = false;
    bool target_shaken = false;
    bool target_routed = false;
};

// ==============================================================================
// Morale Events
// ==============================================================================

struct MoraleEvent {
    std::string unit_name;
    std::string trigger_reason;       // e.g., "half_strength", "lost_melee"
    bool is_from_melee = false;
    u32 wounds_taken = 0;                 // For melee comparison
    u32 wounds_dealt = 0;                 // For melee comparison

    // Test details
    u8 quality_target = 0;
    DiceRollEvent morale_roll;
    bool initial_pass = false;

    // Fearless reroll
    bool has_fearless = false;
    DiceRollEvent fearless_reroll;
    bool fearless_pass = false;

    // Result
    bool final_pass = false;
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
    i8 position = 0;
    i8 distance_to_enemy = 0;
    i8 distance_to_objective = 0;
    bool controls_objective = false;
    bool enemy_destroyed = false;
    bool in_melee = false;

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
    i8 from_position = 0;
    i8 to_position = 0;
    i8 distance_moved = 0;
    std::string reason;
};

// ==============================================================================
// Unit State Snapshot
// ==============================================================================

struct ModelSnapshot {
    std::string name;
    u8 index = 0;
    u8 wounds_taken = 0;
    u8 wounds_remaining = 0;
    std::string state;                // "Healthy", "Wounded", "Dead"
    bool is_hero = false;
};

struct UnitSnapshot {
    std::string name;
    std::string faction;
    u32 unit_id = 0;
    u16 points_cost = 0;
    std::string ai_type;
    u8 quality = 0;
    u8 defense = 0;
    std::string status;               // "Normal", "Shaken", "Routed"
    bool is_fatigued = false;
    i8 position = 0;
    u8 models_alive = 0;
    u8 models_total = 0;
    u16 wounds_remaining = 0;
    std::vector<ModelSnapshot> models;
};

struct GameStateSnapshot {
    UnitSnapshot unit_a;
    UnitSnapshot unit_b;
    bool in_melee = false;
    i8 distance_between = 0;
    std::string objective_holder;     // "none", "unit_a", "unit_b", "contested"
};

// ==============================================================================
// Activation Event
// ==============================================================================

struct ActivationEvent {
    std::string unit_name;
    bool is_unit_a = true;
    std::string action_type;          // "Hold", "Advance", "Rush", "Charge", "Rally", "Idle"

    // AI decision
    AIDecisionEvent ai_decision;

    // Movement (if any)
    bool has_movement = false;
    MovementEvent movement;

    // Combat (if any)
    bool has_combat = false;
    CombatEvent combat;

    // Counter-attack (melee only)
    bool has_counter_attack = false;
    CombatEvent counter_attack;

    // Morale (if any)
    bool has_morale_check = false;
    MoraleEvent morale;

    // State after activation
    GameStateSnapshot state_after;
};

// ==============================================================================
// Round Event
// ==============================================================================

struct InitiativeRoll {
    bool is_round_one = true;
    DiceRollEvent roll;               // Only for round 1
    bool unit_a_goes_first = true;
    std::string reason;               // "random_roll" or "alternating"
};

struct ObjectiveControlEvent {
    i8 unit_a_distance = 0;
    i8 unit_b_distance = 0;
    bool unit_a_in_range = false;
    bool unit_b_in_range = false;
    bool unit_a_shaken = false;
    bool unit_b_shaken = false;
    std::string holder;               // "none", "unit_a", "unit_b", "contested"
    std::string reason;
};

struct RoundEvent {
    u8 round_number = 0;
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
    u8 range = 0;
    u8 attacks = 0;
    u8 ap = 0;
    std::vector<std::string> rules;
};

struct ModelExport {
    std::string name;
    u8 quality = 0;
    u8 defense = 0;
    u8 tough = 0;
    bool is_hero = false;
    std::vector<std::string> rules;
    std::vector<WeaponExport> weapons;
};

struct UnitExport {
    std::string name;
    std::string faction;
    u32 unit_id = 0;
    u16 points_cost = 0;
    std::string ai_type;
    u8 quality = 0;
    u8 defense = 0;
    u8 model_count = 0;
    u8 max_range = 0;
    std::vector<std::string> unit_rules;
    std::vector<ModelExport> models;
};

struct InitialPositions {
    i8 unit_a_position = 0;
    i8 unit_b_position = 0;
    i8 distance_between = 0;
    i8 objective_position = 0;
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
    u16 total_wounds_dealt = 0;
    u8 total_models_killed = 0;
    u8 rounds_holding_objective = 0;
    bool first_blood = false;
    u8 models_remaining = 0;
    std::string final_status;
};

struct GameResultExport {
    std::string winner;               // "unit_a", "unit_b", "draw"
    std::string victory_type;         // "objective_control", "destruction", "rout"
    u8 rounds_played = 0;
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
