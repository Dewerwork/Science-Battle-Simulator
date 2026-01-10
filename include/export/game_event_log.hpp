#pragma once

#include "export/export_types.hpp"
#include "core/unit.hpp"
#include "core/weapon.hpp"
#include "engine/game_state.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace battle {
namespace export_data {

// ==============================================================================
// Helper Functions for String Conversion
// ==============================================================================

inline std::string action_type_to_string(ActionType action) {
    switch (action) {
        case ActionType::Hold: return "Hold";
        case ActionType::Advance: return "Advance";
        case ActionType::Rush: return "Rush";
        case ActionType::Charge: return "Charge";
        case ActionType::Rally: return "Rally";
        case ActionType::Idle: return "Idle";
        default: return "Unknown";
    }
}

inline std::string ai_type_to_string(AIType type) {
    switch (type) {
        case AIType::Melee: return "Melee";
        case AIType::Shooting: return "Shooting";
        case AIType::Hybrid: return "Hybrid";
        default: return "Unknown";
    }
}

inline std::string unit_status_to_string(UnitStatus status) {
    switch (status) {
        case UnitStatus::Normal: return "Normal";
        case UnitStatus::Shaken: return "Shaken";
        case UnitStatus::Routed: return "Routed";
        default: return "Unknown";
    }
}

inline std::string model_state_to_string(ModelState state) {
    switch (state) {
        case ModelState::Healthy: return "Healthy";
        case ModelState::Wounded: return "Wounded";
        case ModelState::Dead: return "Dead";
        default: return "Unknown";
    }
}

inline std::string winner_to_string(GameWinner winner) {
    switch (winner) {
        case GameWinner::UnitA: return "unit_a";
        case GameWinner::UnitB: return "unit_b";
        case GameWinner::Draw: return "draw";
        default: return "unknown";
    }
}

inline std::string rule_id_to_string(RuleId id) {
    switch (id) {
        case RuleId::None: return "None";
        case RuleId::AP: return "AP";
        case RuleId::Blast: return "Blast";
        case RuleId::Deadly: return "Deadly";
        case RuleId::Lance: return "Lance";
        case RuleId::Poison: return "Poison";
        case RuleId::Precise: return "Precise";
        case RuleId::Reliable: return "Reliable";
        case RuleId::Rending: return "Rending";
        case RuleId::Bane: return "Bane";
        case RuleId::Impact: return "Impact";
        case RuleId::Indirect: return "Indirect";
        case RuleId::Sniper: return "Sniper";
        case RuleId::Lock_On: return "Lock-On";
        case RuleId::Purge: return "Purge";
        case RuleId::Regeneration: return "Regeneration";
        case RuleId::Tough: return "Tough";
        case RuleId::Protected: return "Protected";
        case RuleId::Stealth: return "Stealth";
        case RuleId::ShieldWall: return "Shield Wall";
        case RuleId::Fearless: return "Fearless";
        case RuleId::Furious: return "Furious";
        case RuleId::Hero: return "Hero";
        case RuleId::Relentless: return "Relentless";
        case RuleId::Fear: return "Fear";
        case RuleId::Counter: return "Counter";
        case RuleId::Fast: return "Fast";
        case RuleId::Flying: return "Flying";
        case RuleId::Strider: return "Strider";
        case RuleId::Scout: return "Scout";
        case RuleId::Ambush: return "Ambush";
        case RuleId::Devout: return "Devout";
        case RuleId::PiercingAssault: return "Piercing Assault";
        case RuleId::Unstoppable: return "Unstoppable";
        case RuleId::Casting: return "Casting";
        case RuleId::Slow: return "Slow";
        case RuleId::Surge: return "Surge";
        case RuleId::Thrust: return "Thrust";
        case RuleId::Takedown: return "Takedown";
        case RuleId::Limited: return "Limited";
        case RuleId::Immobile: return "Immobile";
        case RuleId::Shielded: return "Shielded";
        case RuleId::Resistance: return "Resistance";
        case RuleId::NoRetreat: return "No Retreat";
        case RuleId::MoraleBoost: return "Morale Boost";
        case RuleId::Rupture: return "Rupture";
        case RuleId::Agile: return "Agile";
        case RuleId::HitAndRun: return "Hit & Run";
        case RuleId::PointBlankSurge: return "Point-Blank Surge";
        case RuleId::Shred: return "Shred";
        case RuleId::Smash: return "Smash";
        case RuleId::Battleborn: return "Battleborn";
        case RuleId::PredatorFighter: return "Predator Fighter";
        case RuleId::RapidCharge: return "Rapid Charge";
        case RuleId::SelfDestruct: return "Self-Destruct";
        case RuleId::VersatileAttack: return "Versatile Attack";
        case RuleId::GoodShot: return "Good Shot";
        case RuleId::BadShot: return "Bad Shot";
        case RuleId::MeleeEvasion: return "Melee Evasion";
        case RuleId::MeleeShrouding: return "Melee Shrouding";
        case RuleId::RangedShrouding: return "Ranged Shrouding";
        // Category A: Simple Modifiers
        case RuleId::Evasive: return "Evasive";
        case RuleId::Steadfast: return "Steadfast";
        case RuleId::Swift: return "Swift";
        case RuleId::Ferocious: return "Ferocious";
        case RuleId::Lacerate: return "Lacerate";
        case RuleId::Mischievous: return "Mischievous";
        case RuleId::Scrapper: return "Scrapper";
        // Category B: Weapon Conditionals
        case RuleId::Bash: return "Bash";
        case RuleId::Thrash: return "Thrash";
        case RuleId::Crack: return "Crack";
        case RuleId::Destructive: return "Destructive";
        case RuleId::Fracture: return "Fracture";
        case RuleId::Break: return "Break";
        case RuleId::Slash: return "Slash";
        case RuleId::Butcher: return "Butcher";
        case RuleId::Slam: return "Slam";
        case RuleId::Quake: return "Quake";
        case RuleId::Tear: return "Tear";
        case RuleId::Scratch: return "Scratch";
        case RuleId::Puncture: return "Puncture";
        case RuleId::Shatter: return "Shatter";
        case RuleId::Demolish: return "Demolish";
        case RuleId::Impale: return "Impale";
        case RuleId::Skewer: return "Skewer";
        case RuleId::Reap: return "Reap";
        case RuleId::Disintegrate: return "Disintegrate";
        case RuleId::Decimate: return "Decimate";
        case RuleId::Fragment: return "Fragment";
        case RuleId::Wreck: return "Wreck";
        // Category C: Movement Bonuses
        case RuleId::Lustbound: return "Lustbound";
        case RuleId::Highborn: return "Highborn";
        case RuleId::Scurry: return "Scurry";
        case RuleId::Darkborn: return "Darkborn";
        case RuleId::HitAndRunShooter: return "Hit & Run Shooter";
        default: return "Unknown";
    }
}

inline std::string format_rule(const CompactRule& rule) {
    std::string name = rule_id_to_string(rule.id);
    if (rule.value > 0) {
        return name + "(" + std::to_string(rule.value) + ")";
    }
    return name;
}

inline std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// ==============================================================================
// Game Event Log - Accumulates events during game execution
// ==============================================================================

class GameEventLog {
public:
    GameEventLog() {
        export_.metadata.export_version = "1.0";
        export_.metadata.simulator_version = "1.0.0";
    }

    void set_seed(u64 seed) {
        export_.metadata.seed = seed;
    }

    // ------ Game Setup ------

    void log_game_start(const Unit& unit_a, const Unit& unit_b, i8 pos_a, i8 pos_b) {
        export_.metadata.timestamp = get_current_timestamp();

        // Export unit A
        export_.setup.unit_a = export_unit(unit_a);

        // Export unit B
        export_.setup.unit_b = export_unit(unit_b);

        // Initial positions
        export_.setup.positions.unit_a_position = pos_a;
        export_.setup.positions.unit_b_position = pos_b;
        export_.setup.positions.distance_between = pos_b - pos_a;
        export_.setup.positions.objective_position = 0;
    }

    // ------ Round Events ------

    void log_round_start(u8 round_number, const GameState& state) {
        RoundEvent round;
        round.round_number = round_number;
        round.state_at_start = snapshot_game_state(state);
        current_round_ = export_.rounds.size();
        export_.rounds.push_back(std::move(round));
    }

    void log_initiative(bool is_round_one, bool unit_a_first, u8 roll_value = 0) {
        if (current_round_ >= export_.rounds.size()) return;

        auto& round = export_.rounds[current_round_];
        round.initiative.is_round_one = is_round_one;
        round.initiative.unit_a_goes_first = unit_a_first;

        if (is_round_one) {
            round.initiative.reason = "random_roll";
            DiceRollEvent roll;
            roll.context = "initiative";
            roll.target = 4;
            roll.effective_target = 4;
            DieRoll die;
            die.value = roll_value;
            die.is_success = roll_value >= 4;
            die.is_six = roll_value == 6;
            roll.dice.push_back(die);
            roll.total_successes = die.is_success ? 1 : 0;
            roll.total_sixes = die.is_six ? 1 : 0;
            round.initiative.roll = std::move(roll);
        } else {
            round.initiative.reason = "alternating";
        }

        if (unit_a_first) {
            round.activation_order = {"unit_a", "unit_b"};
        } else {
            round.activation_order = {"unit_b", "unit_a"};
        }
    }

    void log_round_end(const GameState& state) {
        if (current_round_ >= export_.rounds.size()) return;

        auto& round = export_.rounds[current_round_];
        round.state_at_end = snapshot_game_state(state);

        // Objective control
        round.objective_control.unit_a_distance = std::abs(state.pos_a);
        round.objective_control.unit_b_distance = std::abs(state.pos_b);
        round.objective_control.unit_a_in_range = std::abs(state.pos_a) <= OBJECTIVE_CONTROL_RANGE;
        round.objective_control.unit_b_in_range = std::abs(state.pos_b) <= OBJECTIVE_CONTROL_RANGE;
        round.objective_control.unit_a_shaken = state.state_a.is_shaken();
        round.objective_control.unit_b_shaken = state.state_b.is_shaken();

        if (state.unit_a_controls_objective()) {
            round.objective_control.holder = "unit_a";
            round.objective_control.reason = "Unit A controls objective";
        } else if (state.unit_b_controls_objective()) {
            round.objective_control.holder = "unit_b";
            round.objective_control.reason = "Unit B controls objective";
        } else if (state.is_contested()) {
            round.objective_control.holder = "contested";
            round.objective_control.reason = "Both units in range, neither shaken";
        } else {
            round.objective_control.holder = "none";
            round.objective_control.reason = "No unit in control range";
        }
    }

    // ------ Activation Events ------

    void begin_activation(bool is_unit_a, const Unit& unit, ActionType action) {
        current_activation_ = ActivationEvent{};
        current_activation_.is_unit_a = is_unit_a;
        current_activation_.unit_name = std::string(unit.name.view());
        current_activation_.action_type = action_type_to_string(action);
    }

    void log_ai_decision(const AIDecisionEvent& decision) {
        current_activation_.ai_decision = decision;
    }

    void log_movement(const std::string& type, i8 from, i8 to, const std::string& reason = "") {
        current_activation_.has_movement = true;
        current_activation_.movement.unit_name = current_activation_.unit_name;
        current_activation_.movement.movement_type = type;
        current_activation_.movement.from_position = from;
        current_activation_.movement.to_position = to;
        current_activation_.movement.distance_moved = std::abs(to - from);
        current_activation_.movement.reason = reason;
    }

    void log_combat(const CombatEvent& combat) {
        current_activation_.has_combat = true;
        current_activation_.combat = combat;
    }

    void log_counter_attack(const CombatEvent& combat) {
        current_activation_.has_counter_attack = true;
        current_activation_.counter_attack = combat;
    }

    void log_morale_check(const MoraleEvent& morale) {
        current_activation_.has_morale_check = true;
        current_activation_.morale = morale;
    }

    void end_activation(const GameState& state) {
        current_activation_.state_after = snapshot_game_state(state);

        if (current_round_ < export_.rounds.size()) {
            export_.rounds[current_round_].activations.push_back(std::move(current_activation_));
        }
        current_activation_ = ActivationEvent{};
    }

    // ------ Game End ------

    void log_game_end(const GameResult& result, const GameState& state) {
        export_.result.winner = winner_to_string(result.winner);
        export_.result.rounds_played = result.rounds_played;

        // Determine victory type
        if (result.a_destroyed && result.b_destroyed) {
            export_.result.victory_type = "mutual_destruction";
            export_.result.ending_condition = "Both units destroyed";
        } else if (result.a_destroyed) {
            export_.result.victory_type = "destruction";
            export_.result.ending_condition = "Unit A destroyed";
        } else if (result.b_destroyed) {
            export_.result.victory_type = "destruction";
            export_.result.ending_condition = "Unit B destroyed";
        } else if (result.a_routed) {
            export_.result.victory_type = "rout";
            export_.result.ending_condition = "Unit A routed";
        } else if (result.b_routed) {
            export_.result.victory_type = "rout";
            export_.result.ending_condition = "Unit B routed";
        } else {
            export_.result.victory_type = "objective_control";
            export_.result.ending_condition = "Max rounds reached";
        }

        // Unit A stats
        export_.result.unit_a_stats.total_wounds_dealt = result.stats.wounds_dealt_a;
        export_.result.unit_a_stats.total_models_killed = result.stats.models_killed_a;
        export_.result.unit_a_stats.rounds_holding_objective = result.stats.rounds_holding_a;
        export_.result.unit_a_stats.first_blood = result.stats.first_blood_a;
        export_.result.unit_a_stats.models_remaining = state.state_a.alive_count;
        export_.result.unit_a_stats.final_status = unit_status_to_string(state.state_a.status);

        // Unit B stats
        export_.result.unit_b_stats.total_wounds_dealt = result.stats.wounds_dealt_b;
        export_.result.unit_b_stats.total_models_killed = result.stats.models_killed_b;
        export_.result.unit_b_stats.rounds_holding_objective = result.stats.rounds_holding_b;
        export_.result.unit_b_stats.first_blood = result.stats.first_blood_b;
        export_.result.unit_b_stats.models_remaining = state.state_b.alive_count;
        export_.result.unit_b_stats.final_status = unit_status_to_string(state.state_b.status);
    }

    // ------ Access ------

    const GameExport& get_export() const { return export_; }
    GameExport& get_export() { return export_; }

private:
    GameExport export_;
    size_t current_round_ = 0;
    ActivationEvent current_activation_;

    // Helper to export a unit's static data
    UnitExport export_unit(const Unit& unit) {
        UnitExport exp;
        exp.name = std::string(unit.name.view());
        exp.faction = std::string(unit.faction.view());
        exp.unit_id = unit.unit_id;
        exp.points_cost = unit.points_cost;
        exp.ai_type = ai_type_to_string(unit.ai_type);
        exp.quality = unit.quality;
        exp.defense = unit.defense;
        exp.model_count = unit.model_count;
        exp.max_range = unit.max_range;

        // Unit rules
        for (u8 i = 0; i < MAX_RULES_PER_ENTITY; ++i) {
            if (unit.rules[i].is_valid()) {
                exp.unit_rules.push_back(format_rule(unit.rules[i]));
            }
        }

        // Models
        for (u8 i = 0; i < unit.model_count; ++i) {
            exp.models.push_back(export_model(unit.models[i], unit));
        }

        return exp;
    }

    ModelExport export_model(const Model& model, const Unit& unit) {
        ModelExport exp;
        exp.name = std::string(model.name.view());
        exp.quality = model.quality;
        exp.defense = model.defense;
        exp.tough = model.tough;
        exp.is_hero = model.is_hero;

        // Model rules
        for (u8 i = 0; i < model.rule_count; ++i) {
            if (model.rules[i].is_valid()) {
                exp.rules.push_back(format_rule(model.rules[i]));
            }
        }

        // Weapons - WeaponRef stores index into unit's weapon array
        for (u8 i = 0; i < model.weapon_count; ++i) {
            const WeaponRef& ref = model.weapons[i];
            if (ref.quantity > 0) {
                exp.weapons.push_back(export_weapon(unit.get_weapon(ref.index)));
            }
        }

        return exp;
    }

    WeaponExport export_weapon(const Weapon& w) {
        WeaponExport exp;
        exp.name = std::string(w.name.view());
        exp.range = w.range;
        exp.attacks = w.attacks;
        exp.ap = w.ap;

        for (u8 i = 0; i < w.rule_count; ++i) {
            if (w.rules[i].is_valid()) {
                exp.rules.push_back(format_rule(w.rules[i]));
            }
        }

        return exp;
    }

    GameStateSnapshot snapshot_game_state(const GameState& state) {
        GameStateSnapshot snap;

        snap.unit_a = snapshot_unit(*state.unit_a_ptr, state.state_a, state.pos_a);
        snap.unit_b = snapshot_unit(*state.unit_b_ptr, state.state_b, state.pos_b);
        snap.in_melee = state.in_melee;
        snap.distance_between = state.distance_between();

        if (state.unit_a_controls_objective()) {
            snap.objective_holder = "unit_a";
        } else if (state.unit_b_controls_objective()) {
            snap.objective_holder = "unit_b";
        } else if (state.is_contested()) {
            snap.objective_holder = "contested";
        } else {
            snap.objective_holder = "none";
        }

        return snap;
    }

    UnitSnapshot snapshot_unit(const Unit& unit, const UnitSimState& state, i8 position) {
        UnitSnapshot snap;
        snap.name = std::string(unit.name.view());
        snap.faction = std::string(unit.faction.view());
        snap.unit_id = unit.unit_id;
        snap.points_cost = unit.points_cost;
        snap.ai_type = ai_type_to_string(unit.ai_type);
        snap.quality = unit.quality;
        snap.defense = unit.defense;
        snap.status = unit_status_to_string(state.status);
        snap.is_fatigued = state.is_fatigued;
        snap.position = position;
        snap.models_alive = state.alive_count;
        snap.models_total = unit.model_count;

        // Calculate wounds remaining
        u16 wounds_remaining = 0;
        for (u8 i = 0; i < unit.model_count; ++i) {
            if (state.models[i].is_alive()) {
                wounds_remaining += unit.models[i].tough - state.models[i].wounds_taken;
            }

            ModelSnapshot model_snap;
            model_snap.name = std::string(unit.models[i].name.view());
            model_snap.index = i;
            model_snap.wounds_taken = state.models[i].wounds_taken;
            model_snap.wounds_remaining = state.models[i].is_alive() ?
                (unit.models[i].tough - state.models[i].wounds_taken) : 0;
            model_snap.state = model_state_to_string(state.models[i].state);
            model_snap.is_hero = unit.models[i].is_hero;
            snap.models.push_back(std::move(model_snap));
        }
        snap.wounds_remaining = wounds_remaining;

        return snap;
    }
};

} // namespace export_data
} // namespace battle
