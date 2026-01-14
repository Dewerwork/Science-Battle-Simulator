#pragma once

#include "engine/match_logger.hpp"
#include "engine/game_state.hpp"
#include <vector>
#include <string>
#include <sstream>

namespace battle {

// ==============================================================================
// Structured data types for capturing match events
// ==============================================================================

struct RollData {
    std::vector<u8> rolls;
    std::vector<u8> rerolls;  // Reroll results (e.g., when 6s are rerolled for Poison/Bane)
    u8 target = 0;
    u32 successes = 0;
    u32 sixes = 0;
};

struct RuleTriggerData {
    std::string rule_name;
    std::string effect;
    i32 value = 0;  // Use signed to handle negative modifiers
    std::string phase;  // "hit", "wound", "save", "damage", "morale", "movement", etc.
};

struct AttackSequenceData {
    u32 attack_order = 0;  // Global order of this attack within the round (1-based)
    std::string weapon_name;
    bool is_melee = false;
    u8 range = 0;
    u8 distance = 0;  // Distance to enemy when attack occurred
    u8 base_attacks = 0;
    u8 ap = 0;
    std::string weapon_rules;

    u8 models_attacking = 0;
    u8 attacks_per_model = 0;
    u32 total_attacks = 0;

    // Hit phase
    RollData hit_rolls;
    i8 hit_modifier = 0;
    std::string hit_modifier_source;
    u32 normal_hits = 0;
    u32 rending_hits = 0;
    u32 total_hits = 0;

    // Defense phase
    RollData defense_rolls;
    RollData rending_defense_rolls;
    u8 total_ap = 0;
    i8 defense_modifier = 0;
    std::string defense_modifier_source;
    u32 saves = 0;
    u32 wounds = 0;

    // Regeneration
    RollData regen_rolls;
    u32 wounds_after_regen = 0;
    bool regen_bypassed = false;
    std::string regen_bypass_reason;

    // Wound allocation
    u32 wounds_allocated = 0;
    u32 models_killed = 0;
    u32 overkill_wounds = 0;

    // Rule triggers during this attack
    std::vector<RuleTriggerData> rule_triggers;
};

struct MoraleCheckData {
    std::string unit_name;
    std::string trigger_reason;
    u8 models_remaining = 0;
    u8 models_total = 0;
    u16 wounds_taken = 0;
    u16 wounds_dealt = 0;

    u8 morale_roll = 0;
    u8 morale_target = 0;
    bool morale_passed = false;

    bool had_fearless_reroll = false;
    u8 fearless_roll = 0;
    u8 fearless_target = 0;
    bool fearless_passed = false;

    bool final_passed = false;
    std::string old_status;
    std::string new_status;
    std::string result_description;
};

struct MovementData {
    bool is_unit_a = false;
    std::string move_type;
    i8 from_pos = 0;
    i8 to_pos = 0;
    i8 distance_moved = 0;
    std::string reason;
};

struct SpellCastData {
    bool is_unit_a = false;
    std::string spell_name;
    u8 spell_cost = 0;
    u8 tokens_before = 0;
    u8 tokens_after = 0;
    i8 range = 0;
    std::string target_type;  // "enemy_direct", "enemy_debuff", "self_buff"

    // Interference
    bool was_interfered = false;
    u8 interference_tokens = 0;
    i8 interference_modifier = 0;

    // Roll
    u8 roll = 0;
    u8 target_number = 0;
    i8 total_modifier = 0;
    bool success = false;

    // Effect (if successful)
    std::string effect_type;  // "direct_damage", "buff", "debuff", "morale", "movement"
    u32 hits_dealt = 0;
    u32 wounds_dealt = 0;
    u8 models_killed = 0;
    std::string buff_applied;
};

struct SpellTokenData {
    bool is_unit_a = false;
    std::string event_type;  // "granted", "spent_cast", "spent_interference"
    u8 tokens_changed = 0;
    u8 tokens_before = 0;
    u8 tokens_after = 0;
    u8 caster_value = 0;  // only for "granted"
    std::string spell_name;  // only for spending
};

struct AIDecisionData {
    bool is_unit_a = false;
    std::string ai_type;
    std::string action;
    i8 position = 0;
    i8 distance_to_enemy = 0;
    i8 distance_to_objective = 0;
    bool controls_objective = false;
    bool in_melee = false;
    bool is_shaken = false;
    bool is_fatigued = false;
    bool enemy_destroyed = false;
    u8 max_weapon_range = 0;
    u8 move_speed = 0;
    std::string reasoning;
};

struct ActivationData {
    bool is_unit_a = false;
    AIDecisionData ai_decision;
    std::vector<MovementData> movements;
    std::vector<AttackSequenceData> attack_sequences;
    std::vector<MoraleCheckData> morale_checks;
    std::vector<RuleTriggerData> rule_triggers;
    std::vector<SpellCastData> spell_casts;
    std::vector<SpellTokenData> spell_tokens;
};

struct RoundData {
    u8 round_number = 0;
    u8 initiative_roll = 0;
    bool unit_a_first = false;
    std::string initiative_reason;

    u32 attack_order_counter = 0;  // Tracks global attack order within the round
    std::vector<ActivationData> activations;
    std::vector<RuleTriggerData> end_round_rules;

    // Objective control
    bool a_controls = false;
    bool b_controls = false;
    std::string control_reason;

    // End state
    u8 unit_a_models = 0;
    u8 unit_b_models = 0;
    std::string unit_a_status;
    std::string unit_b_status;
};

struct GameData {
    u8 game_number = 0;
    bool positions_swapped = false;
    i8 start_pos_a = 0;
    i8 start_pos_b = 0;

    std::vector<RuleTriggerData> deployment_rules;
    std::vector<RuleTriggerData> movement_rules;
    std::vector<RoundData> rounds;

    // Result
    std::string winner;  // "A", "B", "draw"
    u8 rounds_played = 0;
    u16 wounds_dealt_a = 0;
    u16 wounds_dealt_b = 0;
    u8 models_killed_a = 0;
    u8 models_killed_b = 0;
    bool a_destroyed = false;
    bool b_destroyed = false;
    bool a_routed = false;
    bool b_routed = false;
};

struct MatchData {
    u64 seed = 0;

    // Unit info
    std::string unit_a_name;
    std::string unit_a_faction;
    u8 unit_a_models = 0;

    std::string unit_b_name;
    std::string unit_b_faction;
    u8 unit_b_models = 0;

    std::vector<GameData> games;

    // Result
    u8 games_won_a = 0;
    u8 games_won_b = 0;
    std::string overall_winner;  // "A", "B", "draw"

    u32 total_wounds_dealt_a = 0;
    u32 total_wounds_dealt_b = 0;
    u16 total_models_killed_a = 0;
    u16 total_models_killed_b = 0;
};

// ==============================================================================
// StructuredMatchLogger - Captures all match data in queryable structures
// ==============================================================================

class StructuredMatchLogger : public MatchLogger {
public:
    StructuredMatchLogger() = default;

    // Get the captured data
    const MatchData& get_match_data() const { return match_data_; }
    MatchData& get_match_data() { return match_data_; }

    // Reset for a new match
    void reset() {
        match_data_ = MatchData{};
        current_game_ = nullptr;
        current_round_ = nullptr;
        current_activation_ = nullptr;
        current_attack_ = nullptr;
    }

    // =========================================================================
    // MATCH LIFECYCLE
    // =========================================================================

    void on_match_start(const Unit& unit_a, const Unit& unit_b, u64 seed) override {
        match_data_.seed = seed;
        match_data_.unit_a_name = unit_a.name.c_str();
        match_data_.unit_a_faction = unit_a.faction.c_str();
        match_data_.unit_a_models = unit_a.model_count;
        match_data_.unit_b_name = unit_b.name.c_str();
        match_data_.unit_b_faction = unit_b.faction.c_str();
        match_data_.unit_b_models = unit_b.model_count;
    }

    void on_match_game_start(u8 game_number, bool positions_swapped) override {
        match_data_.games.emplace_back();
        current_game_ = &match_data_.games.back();
        current_game_->game_number = game_number;
        current_game_->positions_swapped = positions_swapped;
    }

    void on_match_end(const MatchResult& result) override {
        match_data_.games_won_a = result.games_won_a;
        match_data_.games_won_b = result.games_won_b;
        match_data_.total_wounds_dealt_a = result.total_wounds_dealt_a;
        match_data_.total_wounds_dealt_b = result.total_wounds_dealt_b;
        match_data_.total_models_killed_a = result.total_models_killed_a;
        match_data_.total_models_killed_b = result.total_models_killed_b;

        switch (result.overall_winner) {
            case GameWinner::UnitA: match_data_.overall_winner = "A"; break;
            case GameWinner::UnitB: match_data_.overall_winner = "B"; break;
            case GameWinner::Draw: match_data_.overall_winner = "draw"; break;
        }
    }

    // =========================================================================
    // GAME LIFECYCLE
    // =========================================================================

    void on_game_start(const Unit& /*unit_a*/, const Unit& /*unit_b*/,
                       i8 pos_a, i8 pos_b) override {
        if (current_game_) {
            current_game_->start_pos_a = pos_a;
            current_game_->start_pos_b = pos_b;
        }
    }

    void on_game_end(const GameResult& result, const GameState& /*state*/) override {
        if (current_game_) {
            switch (result.winner) {
                case GameWinner::UnitA: current_game_->winner = "A"; break;
                case GameWinner::UnitB: current_game_->winner = "B"; break;
                case GameWinner::Draw: current_game_->winner = "draw"; break;
            }
            current_game_->rounds_played = result.rounds_played;
            current_game_->wounds_dealt_a = result.stats.wounds_dealt_a;
            current_game_->wounds_dealt_b = result.stats.wounds_dealt_b;
            current_game_->models_killed_a = result.stats.models_killed_a;
            current_game_->models_killed_b = result.stats.models_killed_b;
            current_game_->a_destroyed = result.a_destroyed;
            current_game_->b_destroyed = result.b_destroyed;
            current_game_->a_routed = result.a_routed;
            current_game_->b_routed = result.b_routed;
        }
        current_game_ = nullptr;
    }

    // =========================================================================
    // ROUND LIFECYCLE
    // =========================================================================

    void on_round_start(u8 round_number, const GameState& /*state*/) override {
        if (current_game_) {
            current_game_->rounds.emplace_back();
            current_round_ = &current_game_->rounds.back();
            current_round_->round_number = round_number;
        }
    }

    void on_initiative(u8 /*round_number*/, u8 roll, bool unit_a_first,
                       const char* reason) override {
        if (current_round_) {
            current_round_->initiative_roll = roll;
            current_round_->unit_a_first = unit_a_first;
            current_round_->initiative_reason = reason ? reason : "";
        }
    }

    void on_round_end(u8 /*round_number*/, const GameState& state) override {
        if (current_round_) {
            current_round_->unit_a_models = state.state_a.alive_count;
            current_round_->unit_b_models = state.state_b.alive_count;
            current_round_->unit_a_status = status_to_string(state.state_a.status);
            current_round_->unit_b_status = status_to_string(state.state_b.status);
        }
        current_round_ = nullptr;
    }

    // =========================================================================
    // OBJECTIVE CONTROL
    // =========================================================================

    void on_objective_control_check(i8 /*pos_a*/, i8 /*pos_b*/,
                                    bool /*a_in_range*/, bool /*b_in_range*/,
                                    bool /*a_shaken*/, bool /*b_shaken*/,
                                    bool /*a_ooa*/, bool /*b_ooa*/,
                                    bool a_controls, bool b_controls,
                                    const char* reason) override {
        if (current_round_) {
            current_round_->a_controls = a_controls;
            current_round_->b_controls = b_controls;
            current_round_->control_reason = reason ? reason : "";
        }
    }

    // =========================================================================
    // ACTIVATION LIFECYCLE
    // =========================================================================

    void on_activation_start(bool is_unit_a, const GameState& /*state*/) override {
        if (current_round_) {
            current_round_->activations.emplace_back();
            current_activation_ = &current_round_->activations.back();
            current_activation_->is_unit_a = is_unit_a;
        }
    }

    void on_activation_end(bool /*is_unit_a*/, const GameState& /*state*/) override {
        current_activation_ = nullptr;
    }

    // =========================================================================
    // AI DECISION
    // =========================================================================

    void on_ai_decision(bool is_unit_a, AIType ai_type, ActionType action,
                        i8 position, i8 distance_to_enemy, i8 distance_to_objective,
                        bool controls_objective, bool in_melee, bool is_shaken,
                        bool is_fatigued, bool enemy_destroyed, u8 max_weapon_range,
                        u8 move_speed, const char* reasoning) override {
        if (current_activation_) {
            current_activation_->ai_decision.is_unit_a = is_unit_a;
            current_activation_->ai_decision.ai_type = ai_type_to_string(ai_type);
            current_activation_->ai_decision.action = action_to_string(action);
            current_activation_->ai_decision.position = position;
            current_activation_->ai_decision.distance_to_enemy = distance_to_enemy;
            current_activation_->ai_decision.distance_to_objective = distance_to_objective;
            current_activation_->ai_decision.controls_objective = controls_objective;
            current_activation_->ai_decision.in_melee = in_melee;
            current_activation_->ai_decision.is_shaken = is_shaken;
            current_activation_->ai_decision.is_fatigued = is_fatigued;
            current_activation_->ai_decision.enemy_destroyed = enemy_destroyed;
            current_activation_->ai_decision.max_weapon_range = max_weapon_range;
            current_activation_->ai_decision.move_speed = move_speed;
            current_activation_->ai_decision.reasoning = reasoning ? reasoning : "";
        }
    }

    // =========================================================================
    // MOVEMENT
    // =========================================================================

    void on_movement(bool is_unit_a, const char* move_type, i8 from_pos, i8 to_pos,
                     i8 distance_moved, const char* reason) override {
        if (current_activation_) {
            MovementData move;
            move.is_unit_a = is_unit_a;
            move.move_type = move_type ? move_type : "";
            move.from_pos = from_pos;
            move.to_pos = to_pos;
            move.distance_moved = distance_moved;
            move.reason = reason ? reason : "";
            current_activation_->movements.push_back(move);
        }
    }

    // =========================================================================
    // MELEE STATE
    // =========================================================================

    void on_melee_state_changed(bool /*now_in_melee*/, const char* /*reason*/) override {
        // Tracked implicitly through movement and combat events
    }

    // =========================================================================
    // SHOOTING COMBAT
    // =========================================================================

    void on_shooting_start(bool /*attacker_is_a*/, const char* /*attacker_name*/,
                           const char* /*defender_name*/, i8 /*distance*/,
                           u8 /*models_shooting*/) override {
        // Combat start tracked through weapon attacks
    }

    void on_shooting_end(bool /*attacker_is_a*/, u16 /*total_wounds*/,
                         u8 /*total_models_killed*/, bool /*defender_destroyed*/) override {
        // Combat end tracked through weapon attacks
    }

    // =========================================================================
    // MELEE COMBAT
    // =========================================================================

    void on_melee_start(bool /*attacker_is_a*/, const char* /*attacker_name*/,
                        const char* /*defender_name*/, bool /*is_charging*/,
                        bool /*attacker_fatigued*/) override {
        // Melee start tracked through weapon attacks
    }

    void on_melee_strike_order(bool /*defender_strikes_first*/, const char* /*reason*/) override {
        // Strike order captured implicitly
    }

    void on_melee_end(bool /*attacker_is_a*/, u16 /*attacker_wounds_dealt*/,
                      u16 /*defender_wounds_dealt*/, bool /*attacker_destroyed*/,
                      bool /*defender_destroyed*/) override {
        // Melee end tracked through weapon attacks
    }

    // =========================================================================
    // IMPACT ATTACKS
    // =========================================================================

    void on_impact_start(bool /*attacker_is_a*/, u8 base_impact_value,
                         u8 /*counter_reduction*/, u8 effective_impact) override {
        if (current_activation_) {
            current_activation_->attack_sequences.emplace_back();
            current_attack_ = &current_activation_->attack_sequences.back();
            current_attack_->weapon_name = "Impact";
            current_attack_->is_melee = true;
            current_attack_->base_attacks = base_impact_value;
            current_attack_->total_attacks = effective_impact;
        }
    }

    void on_impact_rolls(const std::vector<u8>& rolls, u8 target, u32 hits) override {
        if (current_attack_) {
            current_attack_->hit_rolls.rolls.assign(rolls.begin(), rolls.end());
            current_attack_->hit_rolls.target = target;
            current_attack_->hit_rolls.successes = hits;
            current_attack_->total_hits = hits;
        }
    }

    void on_impact_defense(const std::vector<u8>& rolls, u8 /*defense*/,
                           u8 effective_defense, u32 saves, u32 wounds) override {
        if (current_attack_) {
            current_attack_->defense_rolls.rolls.assign(rolls.begin(), rolls.end());
            current_attack_->defense_rolls.target = effective_defense;
            current_attack_->defense_rolls.successes = saves;
            current_attack_->wounds = wounds;
        }
    }

    void on_impact_end(u32 wounds_dealt, u8 models_killed) override {
        if (current_attack_) {
            current_attack_->wounds_allocated = wounds_dealt;
            current_attack_->models_killed = models_killed;
        }
        current_attack_ = nullptr;
    }

    // =========================================================================
    // WEAPON ATTACKS
    // =========================================================================

    void on_weapon_attack_start(const char* weapon_name, bool is_melee, u8 range, u8 distance,
                                u8 base_attacks, u8 ap, const char* weapon_rules) override {
        if (current_activation_) {
            current_activation_->attack_sequences.emplace_back();
            current_attack_ = &current_activation_->attack_sequences.back();

            // Set global attack order within the round
            if (current_round_) {
                current_round_->attack_order_counter++;
                current_attack_->attack_order = current_round_->attack_order_counter;
            }

            current_attack_->weapon_name = weapon_name ? weapon_name : "";
            current_attack_->is_melee = is_melee;
            current_attack_->range = range;
            current_attack_->distance = distance;
            current_attack_->base_attacks = base_attacks;
            current_attack_->ap = ap;
            current_attack_->weapon_rules = weapon_rules ? weapon_rules : "";
        }
    }

    void on_attack_count(u8 models_attacking, u8 attacks_per_model, u32 total_attacks) override {
        if (current_attack_) {
            current_attack_->models_attacking = models_attacking;
            current_attack_->attacks_per_model = attacks_per_model;
            current_attack_->total_attacks = total_attacks;
        }
    }

    void on_weapon_attack_end(const char* /*weapon_name*/, u32 total_wounds,
                              u8 models_killed) override {
        if (current_attack_) {
            current_attack_->wounds_allocated = total_wounds;
            current_attack_->models_killed = models_killed;
        }
        current_attack_ = nullptr;
    }

    // =========================================================================
    // HIT ROLLS
    // =========================================================================

    void on_hit_modifier(const char* source, i8 modifier, const char* reason) override {
        if (current_attack_) {
            current_attack_->hit_modifier += modifier;
            if (!current_attack_->hit_modifier_source.empty()) {
                current_attack_->hit_modifier_source += ", ";
            }
            current_attack_->hit_modifier_source += source;
            current_attack_->hit_modifier_source += ": ";
            current_attack_->hit_modifier_source += reason;
        }
    }

    void on_hit_rolls(u8 /*base_quality*/, i8 /*total_modifier*/, u8 effective_target,
                      const std::vector<u8>& rolls, u32 hits, u32 sixes) override {
        if (current_attack_) {
            current_attack_->hit_rolls.rolls.assign(rolls.begin(), rolls.end());
            current_attack_->hit_rolls.target = effective_target;
            current_attack_->hit_rolls.successes = hits;
            current_attack_->hit_rolls.sixes = sixes;
        }
    }

    // =========================================================================
    // HIT MODIFIERS
    // =========================================================================

    void on_rule_triggered(const char* rule_name, const char* effect, u32 value) override {
        RuleTriggerData trigger;
        trigger.rule_name = rule_name ? rule_name : "";
        trigger.effect = effect ? effect : "";
        trigger.value = static_cast<i32>(value);

        // Determine phase based on context
        if (current_attack_) {
            trigger.phase = "combat";
            current_attack_->rule_triggers.push_back(trigger);
        } else if (current_activation_) {
            trigger.phase = "activation";
            current_activation_->rule_triggers.push_back(trigger);
        } else if (current_round_) {
            trigger.phase = "round";
            current_round_->end_round_rules.push_back(trigger);
        }
    }

    void on_hits_after_modifiers(u32 normal_hits, u32 rending_hits, u32 total_hits) override {
        if (current_attack_) {
            current_attack_->normal_hits = normal_hits;
            current_attack_->rending_hits = rending_hits;
            current_attack_->total_hits = total_hits;
        }
    }

    // =========================================================================
    // DEFENSE ROLLS
    // =========================================================================

    void on_defense_modifier(const char* source, i8 modifier, const char* reason) override {
        if (current_attack_) {
            current_attack_->defense_modifier += modifier;
            if (!current_attack_->defense_modifier_source.empty()) {
                current_attack_->defense_modifier_source += ", ";
            }
            current_attack_->defense_modifier_source += source;
            current_attack_->defense_modifier_source += ": ";
            current_attack_->defense_modifier_source += reason;
        }
    }

    void on_defense_rolls(u8 /*base_defense*/, u8 total_ap, u8 effective_target,
                          bool /*reroll_sixes*/, const std::vector<u8>& rolls,
                          u32 saves, u32 wounds, u32 /*sixes_rerolled*/,
                          const std::vector<u8>& rerolls, u32 /*reroll_saves*/) override {
        if (current_attack_) {
            current_attack_->defense_rolls.rolls.assign(rolls.begin(), rolls.end());
            current_attack_->defense_rolls.rerolls.assign(rerolls.begin(), rerolls.end());
            current_attack_->defense_rolls.target = effective_target;
            current_attack_->defense_rolls.successes = saves;
            current_attack_->total_ap = total_ap;
            current_attack_->saves = saves;
            current_attack_->wounds = wounds;
        }
    }

    void on_defense_rolls_rending(u8 /*base_defense*/, u8 /*rending_ap*/,
                                  u8 effective_target, const std::vector<u8>& rolls,
                                  u32 saves, u32 wounds) override {
        if (current_attack_) {
            current_attack_->rending_defense_rolls.rolls.assign(rolls.begin(), rolls.end());
            current_attack_->rending_defense_rolls.target = effective_target;
            current_attack_->rending_defense_rolls.successes = saves;
            current_attack_->wounds += wounds;
        }
    }

    // =========================================================================
    // REGENERATION
    // =========================================================================

    void on_regeneration(u32 /*wounds_before*/, u8 target, const std::vector<u8>& rolls,
                         u32 wounds_saved, u32 wounds_after, bool was_bypassed,
                         const char* bypass_reason) override {
        if (current_attack_) {
            current_attack_->regen_rolls.rolls.assign(rolls.begin(), rolls.end());
            current_attack_->regen_rolls.target = target;
            current_attack_->regen_rolls.successes = wounds_saved;
            current_attack_->wounds_after_regen = wounds_after;
            current_attack_->regen_bypassed = was_bypassed;
            current_attack_->regen_bypass_reason = bypass_reason ? bypass_reason : "";
        }
    }

    // =========================================================================
    // WOUND ALLOCATION
    // =========================================================================

    void on_wound_allocation_start(u32 /*wounds_to_allocate*/,
                                   const char* /*allocation_order*/) override {
        // Tracked through wound_allocated events
    }

    void on_wound_allocated(u8 /*model_index*/, const char* /*model_name*/,
                            u8 /*model_tough*/, u8 /*wounds_before*/,
                            u8 /*wounds_applied*/, u8 /*wounds_after*/,
                            bool /*model_killed*/) override {
        // Individual wound allocations - could be tracked if needed
    }

    void on_deadly_wound(u8 /*model_index*/, const char* /*model_name*/,
                         u8 /*deadly_value*/, u8 /*wounds_applied*/,
                         u8 /*wounds_wasted*/, bool /*model_killed*/) override {
        // Deadly wounds - could be tracked if needed
    }

    void on_wound_allocation_end(u32 total_wounds_dealt, u8 total_models_killed,
                                 u32 overkill_wounds) override {
        if (current_attack_) {
            current_attack_->wounds_allocated = total_wounds_dealt;
            current_attack_->models_killed = total_models_killed;
            current_attack_->overkill_wounds = overkill_wounds;
        }
    }

    // =========================================================================
    // MORALE
    // =========================================================================

    void on_morale_check_start(bool is_unit_a, const char* unit_name,
                               const char* trigger_reason, u8 models_remaining,
                               u8 models_total, u16 wounds_taken, u16 wounds_dealt) override {
        if (current_activation_) {
            current_activation_->morale_checks.emplace_back();
            current_morale_ = &current_activation_->morale_checks.back();
            current_morale_->unit_name = unit_name ? unit_name : (is_unit_a ? "Unit A" : "Unit B");
            current_morale_->trigger_reason = trigger_reason ? trigger_reason : "";
            current_morale_->models_remaining = models_remaining;
            current_morale_->models_total = models_total;
            current_morale_->wounds_taken = wounds_taken;
            current_morale_->wounds_dealt = wounds_dealt;
        }
    }

    void on_morale_roll(u8 roll, u8 quality_target, bool passed) override {
        if (current_morale_) {
            current_morale_->morale_roll = roll;
            current_morale_->morale_target = quality_target;
            current_morale_->morale_passed = passed;
        }
    }

    void on_fearless_roll(u8 roll, u8 target, bool passed) override {
        if (current_morale_) {
            current_morale_->had_fearless_reroll = true;
            current_morale_->fearless_roll = roll;
            current_morale_->fearless_target = target;
            current_morale_->fearless_passed = passed;
        }
    }

    void on_morale_check_end(bool final_passed, UnitStatus old_status,
                             UnitStatus new_status, const char* result_description) override {
        if (current_morale_) {
            current_morale_->final_passed = final_passed;
            current_morale_->old_status = status_to_string(old_status);
            current_morale_->new_status = status_to_string(new_status);
            current_morale_->result_description = result_description ? result_description : "";
        }
        current_morale_ = nullptr;
    }

    // =========================================================================
    // STATUS CHANGES
    // =========================================================================

    void on_status_changed(bool /*is_unit_a*/, UnitStatus /*old_status*/,
                           UnitStatus /*new_status*/, const char* /*reason*/) override {
        // Status tracked through morale and round end
    }

    // =========================================================================
    // FATIGUE
    // =========================================================================

    void on_fatigue_changed(bool /*is_unit_a*/, bool /*now_fatigued*/,
                            const char* /*reason*/) override {
        // Fatigue tracked implicitly
    }

    // =========================================================================
    // DEPLOYMENT RULES
    // =========================================================================

    void on_deployment_rule(bool is_unit_a, const char* rule_name, i8 old_position,
                            i8 new_position, const char* reason) override {
        if (current_game_) {
            RuleTriggerData trigger;
            trigger.rule_name = rule_name ? rule_name : "";
            trigger.effect = reason ? reason : "";
            trigger.value = static_cast<i32>(new_position - old_position);
            trigger.phase = is_unit_a ? "deployment_a" : "deployment_b";
            current_game_->deployment_rules.push_back(trigger);
        }
    }

    // =========================================================================
    // MOVEMENT RULES
    // =========================================================================

    void on_movement_rule_applied(bool is_unit_a, const char* rule_name, i8 modifier,
                                  const char* effect) override {
        if (current_game_) {
            RuleTriggerData trigger;
            trigger.rule_name = rule_name ? rule_name : "";
            trigger.effect = effect ? effect : "";
            trigger.value = static_cast<i32>(modifier);
            trigger.phase = is_unit_a ? "movement_rule_a" : "movement_rule_b";
            current_game_->movement_rules.push_back(trigger);
        }
    }

    // =========================================================================
    // END-ROUND RULES
    // =========================================================================

    void on_end_round_rule(bool is_unit_a, const char* rule_name, const char* effect,
                           u32 value) override {
        if (current_round_) {
            RuleTriggerData trigger;
            trigger.rule_name = rule_name ? rule_name : "";
            trigger.effect = effect ? effect : "";
            trigger.value = static_cast<i32>(value);
            trigger.phase = is_unit_a ? "end_round_a" : "end_round_b";
            current_round_->end_round_rules.push_back(trigger);
        }
    }

    // =========================================================================
    // SPELL CASTING
    // =========================================================================

    void on_spell_tokens_granted(bool is_unit_a, u8 tokens_gained,
                                 u8 tokens_total, u8 caster_value) override {
        if (current_activation_) {
            SpellTokenData token_data;
            token_data.is_unit_a = is_unit_a;
            token_data.event_type = "granted";
            token_data.tokens_changed = tokens_gained;
            token_data.tokens_before = tokens_total - tokens_gained;
            token_data.tokens_after = tokens_total;
            token_data.caster_value = caster_value;
            current_activation_->spell_tokens.push_back(token_data);
        }
    }

    void on_spell_cast_attempt(bool is_unit_a, const char* spell_name,
                               u8 spell_cost, u8 tokens_remaining,
                               i8 range, const char* target_type) override {
        if (current_activation_) {
            // Record token spending
            SpellTokenData token_data;
            token_data.is_unit_a = is_unit_a;
            token_data.event_type = "spent_cast";
            token_data.tokens_changed = spell_cost;
            token_data.tokens_before = tokens_remaining + spell_cost;
            token_data.tokens_after = tokens_remaining;
            token_data.spell_name = spell_name ? spell_name : "";
            current_activation_->spell_tokens.push_back(token_data);

            // Start new spell cast record
            current_activation_->spell_casts.emplace_back();
            current_spell_ = &current_activation_->spell_casts.back();
            current_spell_->is_unit_a = is_unit_a;
            current_spell_->spell_name = spell_name ? spell_name : "";
            current_spell_->spell_cost = spell_cost;
            current_spell_->tokens_before = tokens_remaining + spell_cost;
            current_spell_->tokens_after = tokens_remaining;
            current_spell_->range = range;
            current_spell_->target_type = target_type ? target_type : "";
        }
    }

    void on_spell_interference(bool interferer_is_a, u8 tokens_spent,
                               i8 modifier_applied) override {
        if (current_spell_) {
            current_spell_->was_interfered = true;
            current_spell_->interference_tokens = tokens_spent;
            current_spell_->interference_modifier = modifier_applied;
        }
        if (current_activation_) {
            // Record token spending by interferer
            SpellTokenData token_data;
            token_data.is_unit_a = interferer_is_a;
            token_data.event_type = "spent_interference";
            token_data.tokens_changed = tokens_spent;
            // Note: we don't have exact before/after counts here
            current_activation_->spell_tokens.push_back(token_data);
        }
    }

    void on_spell_roll(u8 roll, u8 target_number, i8 modifier,
                       bool success) override {
        if (current_spell_) {
            current_spell_->roll = roll;
            current_spell_->target_number = target_number;
            current_spell_->total_modifier = modifier;
            current_spell_->success = success;
        }
    }

    void on_spell_effect(const char* spell_name, const char* effect_type,
                         u32 hits_dealt, u32 wounds_dealt, u8 models_killed,
                         const char* buff_applied) override {
        if (current_spell_) {
            current_spell_->effect_type = effect_type ? effect_type : "";
            current_spell_->hits_dealt = hits_dealt;
            current_spell_->wounds_dealt = wounds_dealt;
            current_spell_->models_killed = models_killed;
            current_spell_->buff_applied = buff_applied ? buff_applied : "";
        }

        if (current_activation_) {
            // Also record as rule trigger for backward compatibility
            RuleTriggerData trigger;
            trigger.rule_name = std::string("Spell: ") + (spell_name ? spell_name : "");
            trigger.effect = effect_type ? effect_type : "";
            trigger.value = wounds_dealt;
            trigger.phase = "spell";
            current_activation_->rule_triggers.push_back(trigger);

            // Also record as an attack sequence if it dealt damage
            if (hits_dealt > 0 || wounds_dealt > 0 || models_killed > 0) {
                current_activation_->attack_sequences.emplace_back();
                auto& atk = current_activation_->attack_sequences.back();
                atk.weapon_name = std::string("Spell: ") + (spell_name ? spell_name : "");
                atk.is_melee = false;
                atk.total_hits = hits_dealt;
                atk.wounds = wounds_dealt;
                atk.wounds_allocated = wounds_dealt;
                atk.models_killed = models_killed;
            }
        }
        current_spell_ = nullptr;  // Spell cast complete
    }

    void on_spell_phase_end(bool /*is_unit_a*/, u8 /*spells_cast*/,
                            u8 /*spells_succeeded*/, u8 /*tokens_remaining*/) override {
        // Spell phase end - ensure current_spell_ is cleared
        current_spell_ = nullptr;
    }

private:
    MatchData match_data_;
    GameData* current_game_ = nullptr;
    RoundData* current_round_ = nullptr;
    ActivationData* current_activation_ = nullptr;
    AttackSequenceData* current_attack_ = nullptr;
    MoraleCheckData* current_morale_ = nullptr;
    SpellCastData* current_spell_ = nullptr;

    static std::string status_to_string(UnitStatus status) {
        switch (status) {
            case UnitStatus::Normal: return "normal";
            case UnitStatus::Shaken: return "shaken";
            case UnitStatus::Routed: return "routed";
            default: return "unknown";
        }
    }

    static std::string ai_type_to_string(AIType type) {
        switch (type) {
            case AIType::Melee: return "melee";
            case AIType::Shooting: return "shooting";
            case AIType::Hybrid: return "hybrid";
            default: return "unknown";
        }
    }

    static std::string action_to_string(ActionType action) {
        switch (action) {
            case ActionType::Hold: return "hold";
            case ActionType::Advance: return "advance";
            case ActionType::Rush: return "rush";
            case ActionType::Charge: return "charge";
            case ActionType::Rally: return "rally";
            case ActionType::Idle: return "idle";
            default: return "unknown";
        }
    }
};

} // namespace battle
