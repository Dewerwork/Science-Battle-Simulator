#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "engine/combat.hpp"
#include "engine/dice.hpp"
#include "simulation/statistics.hpp"
#include "simulation/sim_state.hpp"
#include "simulation/thread_pool.hpp"
#include <vector>
#include <functional>
#include <chrono>

namespace battle {

// ==============================================================================
// Simulation Configuration
// ==============================================================================

struct SimulationConfig {
    u64 iterations_per_matchup = 10000;  // Default 10K iterations
    u8  max_rounds = 10;                  // Max rounds per battle
    u32 batch_size = 1000;                // Iterations per batch
    bool attacker_charges = true;         // Who charges first
    ScenarioType scenario = ScenarioType::Charge;

    // For massive simulations
    u32 checkpoint_interval = 1000000;    // Save progress every N matchups
    bool enable_progress = true;
};

// ==============================================================================
// Progress Callback
// ==============================================================================

using ProgressCallback = std::function<void(u64 completed, u64 total, f64 rate)>;

// ==============================================================================
// Single Matchup Simulator (used by thread pool workers)
// ==============================================================================

class MatchupSimulator {
public:
    MatchupSimulator() : dice_(), resolver_(dice_) {}

    // Run single battle and return result
    struct BattleResult {
        BattleWinner winner;
        VictoryCondition condition;
        u8 rounds;
        u16 wounds_by_attacker;
        u16 wounds_by_defender;
        u8 kills_by_attacker;
        u8 kills_by_defender;
        u8 attacker_remaining;
        u8 defender_remaining;
        bool attacker_routed;
        bool defender_routed;
    };

    BattleResult run_battle(
        const Unit& attacker_template,
        const Unit& defender_template,
        const SimulationConfig& config
    );

    // Run multiple iterations and accumulate stats
    void run_batch(
        const Unit& attacker_template,
        const Unit& defender_template,
        const SimulationConfig& config,
        u32 iterations,
        LocalStats& stats
    );

private:
    DiceRoller dice_;
    CombatResolver resolver_;

    // Reusable lightweight state objects (avoid allocation per iteration)
    UnitSimState attacker_state_;
    UnitSimState defender_state_;

    // Run a single round of melee combat using lightweight views
    void run_melee_round(
        UnitView& attacker,
        UnitView& defender,
        bool attacker_charges,
        u16& attacker_wounds,
        u16& defender_wounds
    );

    // Check morale using lightweight view
    bool check_morale(UnitView& unit, bool at_half_strength, bool lost_melee);
};

// ==============================================================================
// Main Simulator - Orchestrates parallel simulation
// ==============================================================================

class Simulator {
public:
    explicit Simulator(const SimulationConfig& config = SimulationConfig())
        : config_(config) {}

    // Run simulation for a single matchup
    SimulationStatistics simulate_matchup(
        const Unit& attacker,
        const Unit& defender,
        ProgressCallback progress = nullptr
    );

    // Run simulation for multiple matchups (matrix)
    // Returns results for all pairs
    std::vector<MatchupResult> simulate_matrix(
        const std::vector<Unit>& units_a,
        const std::vector<Unit>& units_b,
        ProgressCallback progress = nullptr
    );

    // Run massive simulation (100B+ matchups)
    // Streams results to file to avoid memory issues
    void simulate_massive(
        const std::vector<Unit>& units_a,
        const std::vector<Unit>& units_b,
        const std::string& output_file,
        ProgressCallback progress = nullptr
    );

    // Configuration access
    SimulationConfig& config() { return config_; }
    const SimulationConfig& config() const { return config_; }

private:
    SimulationConfig config_;
};

// ==============================================================================
// Inline Implementations
// ==============================================================================

inline MatchupSimulator::BattleResult MatchupSimulator::run_battle(
    const Unit& attacker_template,
    const Unit& defender_template,
    const SimulationConfig& config
) {
    // Reset lightweight state objects (no allocation, just reset values)
    attacker_state_.reset(attacker_template.model_count);
    defender_state_.reset(defender_template.model_count);

    // Create views combining const unit data with mutable state
    UnitView attacker(&attacker_template, &attacker_state_);
    UnitView defender(&defender_template, &defender_state_);

    BattleResult result{};
    result.attacker_remaining = attacker.alive_count();
    result.defender_remaining = defender.alive_count();

    u8 round = 0;
    bool attacker_charges = config.attacker_charges;

    while (round < config.max_rounds) {
        ++round;

        // Check if battle is over
        if (attacker.is_out_of_action() || defender.is_out_of_action()) break;

        u16 atk_wounds = 0, def_wounds = 0;
        run_melee_round(attacker, defender, attacker_charges && round == 1, atk_wounds, def_wounds);

        result.wounds_by_attacker += atk_wounds;
        result.wounds_by_defender += def_wounds;

        // End of round morale
        if (!attacker.is_out_of_action() && !defender.is_out_of_action()) {
            if (atk_wounds > def_wounds) {
                // Defender lost - test morale
                if (check_morale(defender, defender.is_at_half_strength(), true)) {
                    result.defender_routed = true;
                }
            } else if (def_wounds > atk_wounds) {
                // Attacker lost
                if (check_morale(attacker, attacker.is_at_half_strength(), true)) {
                    result.attacker_routed = true;
                }
            }
        }

        // Reset round state
        attacker.reset_round_state();
        defender.reset_round_state();
    }

    result.rounds = round;
    result.attacker_remaining = attacker.alive_count();
    result.defender_remaining = defender.alive_count();
    result.kills_by_attacker = defender_template.model_count - defender.alive_count();
    result.kills_by_defender = attacker_template.model_count - attacker.alive_count();

    // Determine winner
    bool attacker_out = attacker.is_out_of_action();
    bool defender_out = defender.is_out_of_action();

    if (attacker_out && defender_out) {
        result.winner = BattleWinner::Draw;
        result.condition = VictoryCondition::MutualDestruction;
    } else if (defender_out) {
        result.winner = BattleWinner::Attacker;
        result.condition = result.defender_routed ?
            VictoryCondition::AttackerRoutedEnemy : VictoryCondition::AttackerDestroyedEnemy;
    } else if (attacker_out) {
        result.winner = BattleWinner::Defender;
        result.condition = result.attacker_routed ?
            VictoryCondition::DefenderRoutedEnemy : VictoryCondition::DefenderDestroyedEnemy;
    } else {
        // Compare remaining
        f64 atk_pct = static_cast<f64>(attacker.alive_count()) / attacker_template.model_count;
        f64 def_pct = static_cast<f64>(defender.alive_count()) / defender_template.model_count;

        if (atk_pct > def_pct) {
            result.winner = BattleWinner::Attacker;
            result.condition = VictoryCondition::MaxRoundsAttackerAhead;
        } else if (def_pct > atk_pct) {
            result.winner = BattleWinner::Defender;
            result.condition = VictoryCondition::MaxRoundsDefenderAhead;
        } else {
            result.winner = BattleWinner::Draw;
            result.condition = VictoryCondition::MaxRoundsDraw;
        }
    }

    return result;
}

inline void MatchupSimulator::run_batch(
    const Unit& attacker_template,
    const Unit& defender_template,
    const SimulationConfig& config,
    u32 iterations,
    LocalStats& stats
) {
    for (u32 i = 0; i < iterations; ++i) {
        BattleResult result = run_battle(attacker_template, defender_template, config);

        // Accumulate stats
        switch (result.winner) {
            case BattleWinner::Attacker: ++stats.attacker_wins; break;
            case BattleWinner::Defender: ++stats.defender_wins; break;
            case BattleWinner::Draw:     ++stats.draws; break;
        }

        stats.total_rounds += result.rounds;
        stats.total_wounds_by_attacker += result.wounds_by_attacker;
        stats.total_wounds_by_defender += result.wounds_by_defender;
        stats.total_kills_by_attacker += result.kills_by_attacker;
        stats.total_kills_by_defender += result.kills_by_defender;
        stats.attacker_remaining_total += result.attacker_remaining;
        stats.defender_remaining_total += result.defender_remaining;

        if (result.attacker_routed) ++stats.attacker_routs;
        if (result.defender_routed) ++stats.defender_routs;

        ++stats.victory_conditions[static_cast<size_t>(result.condition)];
    }
}

inline void MatchupSimulator::run_melee_round(
    UnitView& attacker,
    UnitView& defender,
    bool attacker_charges,
    u16& attacker_wounds,
    u16& defender_wounds
) {
    CombatContext ctx;
    ctx.phase = CombatPhase::Melee;

    // Attacker strikes
    ctx.is_charging = attacker_charges;
    ctx.attacker_shaken = attacker.is_shaken();
    ctx.defender_shaken = defender.is_shaken();
    ctx.attacker_fatigued = attacker.is_fatigued();

    CombatResult atk_result = resolver_.resolve_attack(attacker, defender, ctx);
    attacker_wounds = atk_result.total_wounds;
    attacker.set_fatigued(true);

    // Defender strikes back (if alive)
    if (!defender.is_out_of_action()) {
        ctx.is_charging = false;
        ctx.attacker_shaken = defender.is_shaken();
        ctx.defender_shaken = attacker.is_shaken();
        ctx.attacker_fatigued = defender.is_fatigued() || defender.is_shaken();

        CombatResult def_result = resolver_.resolve_attack(defender, attacker, ctx);
        defender_wounds = def_result.total_wounds;
        defender.set_fatigued(true);
    }
}

inline bool MatchupSimulator::check_morale(UnitView& unit, bool at_half_strength, bool lost_melee) {
    // Already shaken - fail automatically
    if (unit.is_shaken()) {
        if (at_half_strength) {
            unit.rout();
            return true;
        }
        // Stay shaken
        return false;
    }

    // Roll morale (quality test)
    u8 roll = dice_.roll_d6();
    bool passed = roll >= unit.quality();

    // Fearless reroll
    if (!passed && unit.has_rule(RuleId::Fearless)) {
        u8 reroll = dice_.roll_d6();
        if (reroll >= 4) passed = true;
    }

    if (passed) return false;

    // Failed
    if (at_half_strength && lost_melee) {
        unit.rout();
        return true;
    }

    unit.become_shaken();
    return false;
}

} // namespace battle
