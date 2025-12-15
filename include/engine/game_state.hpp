#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include <array>

namespace battle {

// ==============================================================================
// Game Constants
// ==============================================================================

constexpr i8 STARTING_DISTANCE = 12;   // Units start 12" from center (24" apart)
constexpr i8 OBJECTIVE_CONTROL_RANGE = 3;  // Must be within 3" to control
constexpr u8 MAX_ROUNDS = 4;
constexpr u8 STANDARD_MOVE = 6;
constexpr u8 FAST_MOVE = 9;
constexpr u8 SLOW_MOVE = 4;
constexpr u8 RUSH_MULTIPLIER = 2;      // Rush = 2x movement
constexpr u8 CHARGE_DISTANCE = 12;

// ==============================================================================
// Action Types
// ==============================================================================

enum class ActionType : u8 {
    Hold = 0,       // Stay in place (can shoot with bonuses if Relentless)
    Advance = 1,    // Move 6" and shoot
    Rush = 2,       // Move 12", no shooting
    Charge = 3,     // Move 12" into melee
    Rally = 4,      // Remove Shaken status
    Idle = 5        // Forced idle (destroyed/routed)
};

// ==============================================================================
// Combat Results
// ==============================================================================

struct CombatResult {
    u16 wounds_dealt = 0;
    u8 models_killed = 0;
    bool target_destroyed = false;
    bool target_shaken = false;
    bool target_routed = false;
};

// ==============================================================================
// Game Statistics (for tiebreakers)
// ==============================================================================

struct GameStats {
    u16 wounds_dealt_a = 0;
    u16 wounds_dealt_b = 0;
    u8 models_killed_a = 0;
    u8 models_killed_b = 0;
    u8 rounds_holding_a = 0;
    u8 rounds_holding_b = 0;
    bool first_blood_a = false;
    bool first_blood_b = false;
    bool first_blood_set = false;

    void record_wounds(bool is_unit_a, u16 wounds, u8 models) {
        if (is_unit_a) {
            wounds_dealt_a += wounds;
            models_killed_a += models;
        } else {
            wounds_dealt_b += wounds;
            models_killed_b += models;
        }

        if (!first_blood_set && (wounds > 0 || models > 0)) {
            first_blood_set = true;
            if (is_unit_a) first_blood_a = true;
            else first_blood_b = true;
        }
    }
};

// ==============================================================================
// Game State
// ==============================================================================

struct GameState {
    // Unit copies for this game (mutable during simulation)
    Unit unit_a;
    Unit unit_b;

    // Positions: distance from center (negative = A's side, positive = B's side)
    // Units start at -12 and +12 respectively
    i8 pos_a = -STARTING_DISTANCE;
    i8 pos_b = STARTING_DISTANCE;

    // Game progress
    u8 current_round = 1;
    bool unit_a_activated = false;
    bool unit_b_activated = false;
    bool in_melee = false;

    // Statistics
    GameStats stats;

    // Initialization
    void init(const Unit& a, const Unit& b) {
        unit_a = a.copy_fresh();
        unit_b = b.copy_fresh();
        pos_a = -STARTING_DISTANCE;
        pos_b = STARTING_DISTANCE;
        current_round = 1;
        unit_a_activated = false;
        unit_b_activated = false;
        in_melee = false;
        stats = GameStats{};
    }

    // Position helpers
    i8 distance_between() const {
        return pos_b - pos_a;  // Always positive when A is left, B is right
    }

    bool unit_a_controls_objective() const {
        if (unit_a.is_out_of_action()) return false;
        if (std::abs(pos_a) > OBJECTIVE_CONTROL_RANGE) return false;
        // Check if enemy is contesting (within range and not shaken)
        if (!unit_b.is_out_of_action() &&
            std::abs(pos_b) <= OBJECTIVE_CONTROL_RANGE &&
            !unit_b.is_shaken()) {
            return false;  // Contested
        }
        return true;
    }

    bool unit_b_controls_objective() const {
        if (unit_b.is_out_of_action()) return false;
        if (std::abs(pos_b) > OBJECTIVE_CONTROL_RANGE) return false;
        // Check if enemy is contesting
        if (!unit_a.is_out_of_action() &&
            std::abs(pos_a) <= OBJECTIVE_CONTROL_RANGE &&
            !unit_a.is_shaken()) {
            return false;  // Contested
        }
        return true;
    }

    bool is_contested() const {
        if (unit_a.is_out_of_action() || unit_b.is_out_of_action()) return false;
        bool a_in_range = std::abs(pos_a) <= OBJECTIVE_CONTROL_RANGE;
        bool b_in_range = std::abs(pos_b) <= OBJECTIVE_CONTROL_RANGE;
        if (!a_in_range || !b_in_range) return false;
        // Both in range - contested unless one is shaken
        return !unit_a.is_shaken() && !unit_b.is_shaken();
    }

    // Movement helpers
    u8 get_move_speed(const Unit& unit) const {
        if (unit.has_rule(RuleId::Fast)) return FAST_MOVE;
        if (unit.has_rule(RuleId::Slow)) return SLOW_MOVE;
        return STANDARD_MOVE;
    }

    // Check if unit can charge the enemy
    bool can_charge(bool is_unit_a) const {
        if (in_melee) return false;  // Already in melee
        i8 dist = distance_between();
        return dist <= CHARGE_DISTANCE;
    }

    // Check if unit can shoot (has ranged weapons in range)
    bool can_shoot(bool is_unit_a, const Unit& shooter) const {
        if (in_melee) return false;  // Can't shoot in melee
        i8 dist = distance_between();
        return shooter.max_range >= static_cast<u8>(dist);
    }

    // Game state checks
    bool is_game_over() const {
        // Game ends early if both units are out of action
        if (unit_a.is_out_of_action() && unit_b.is_out_of_action()) return true;
        // Or if we've completed all rounds
        return current_round > MAX_ROUNDS;
    }

    bool round_complete() const {
        return unit_a_activated && unit_b_activated;
    }

    void next_round() {
        current_round++;
        unit_a_activated = false;
        unit_b_activated = false;
        unit_a.reset_round_state();
        unit_b.reset_round_state();
    }

    void update_objective_control() {
        if (unit_a_controls_objective()) stats.rounds_holding_a++;
        if (unit_b_controls_objective()) stats.rounds_holding_b++;
    }
};

// ==============================================================================
// Game Result
// ==============================================================================

enum class GameWinner : u8 {
    UnitA = 0,
    UnitB = 1,
    Draw = 2
};

struct GameResult {
    GameWinner winner = GameWinner::Draw;
    GameStats stats;
    u8 rounds_played = 0;
    bool a_destroyed = false;
    bool b_destroyed = false;
    bool a_routed = false;
    bool b_routed = false;

    // Determine winner based on objective control at end of round 4
    static GameResult determine(const GameState& state) {
        GameResult result;
        result.stats = state.stats;
        result.rounds_played = state.current_round > MAX_ROUNDS ? MAX_ROUNDS : state.current_round;
        result.a_destroyed = state.unit_a.is_destroyed();
        result.b_destroyed = state.unit_b.is_destroyed();
        result.a_routed = state.unit_a.is_routed();
        result.b_routed = state.unit_b.is_routed();

        // Check objective control
        bool a_controls = state.unit_a_controls_objective();
        bool b_controls = state.unit_b_controls_objective();

        if (a_controls && !b_controls) {
            result.winner = GameWinner::UnitA;
        } else if (b_controls && !a_controls) {
            result.winner = GameWinner::UnitB;
        } else {
            // Draw or contested - use tiebreakers
            result.winner = GameWinner::Draw;
        }

        return result;
    }
};

// ==============================================================================
// Match Result (Best of 3)
// ==============================================================================

struct MatchResult {
    u32 unit_a_id = 0;
    u32 unit_b_id = 0;
    u8 games_won_a = 0;
    u8 games_won_b = 0;
    GameWinner overall_winner = GameWinner::Draw;

    // Aggregated stats across all games
    u32 total_wounds_dealt_a = 0;
    u32 total_wounds_dealt_b = 0;
    u16 total_models_killed_a = 0;
    u16 total_models_killed_b = 0;
    u8 total_rounds_holding_a = 0;
    u8 total_rounds_holding_b = 0;

    void add_game(const GameResult& game) {
        if (game.winner == GameWinner::UnitA) games_won_a++;
        else if (game.winner == GameWinner::UnitB) games_won_b++;

        total_wounds_dealt_a += game.stats.wounds_dealt_a;
        total_wounds_dealt_b += game.stats.wounds_dealt_b;
        total_models_killed_a += game.stats.models_killed_a;
        total_models_killed_b += game.stats.models_killed_b;
        total_rounds_holding_a += game.stats.rounds_holding_a;
        total_rounds_holding_b += game.stats.rounds_holding_b;
    }

    void determine_winner() {
        if (games_won_a > games_won_b) {
            overall_winner = GameWinner::UnitA;
        } else if (games_won_b > games_won_a) {
            overall_winner = GameWinner::UnitB;
        } else {
            // Tied on wins - use tiebreakers
            // 1. Total wounds dealt
            if (total_wounds_dealt_a > total_wounds_dealt_b) {
                overall_winner = GameWinner::UnitA;
            } else if (total_wounds_dealt_b > total_wounds_dealt_a) {
                overall_winner = GameWinner::UnitB;
            }
            // 2. Models killed
            else if (total_models_killed_a > total_models_killed_b) {
                overall_winner = GameWinner::UnitA;
            } else if (total_models_killed_b > total_models_killed_a) {
                overall_winner = GameWinner::UnitB;
            }
            // 3. Rounds holding objective
            else if (total_rounds_holding_a > total_rounds_holding_b) {
                overall_winner = GameWinner::UnitA;
            } else if (total_rounds_holding_b > total_rounds_holding_a) {
                overall_winner = GameWinner::UnitB;
            }
            // 4. True draw
            else {
                overall_winner = GameWinner::Draw;
            }
        }
    }
};

} // namespace battle
