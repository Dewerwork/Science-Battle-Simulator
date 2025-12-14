#pragma once

#include "core/types.hpp"
#include <atomic>
#include <array>
#include <cmath>

namespace battle {

// ==============================================================================
// Atomic Statistics Accumulator
// Thread-safe accumulation for parallel simulation
// ==============================================================================

struct AtomicStats {
    std::atomic<u64> attacker_wins{0};
    std::atomic<u64> defender_wins{0};
    std::atomic<u64> draws{0};

    std::atomic<u64> total_rounds{0};
    std::atomic<u64> total_wounds_by_attacker{0};
    std::atomic<u64> total_wounds_by_defender{0};
    std::atomic<u64> total_kills_by_attacker{0};
    std::atomic<u64> total_kills_by_defender{0};

    std::atomic<u64> attacker_routs{0};
    std::atomic<u64> defender_routs{0};

    std::atomic<u64> attacker_remaining_total{0};
    std::atomic<u64> defender_remaining_total{0};

    // Victory conditions
    std::array<std::atomic<u64>, 10> victory_conditions{};

    void reset() {
        attacker_wins = 0;
        defender_wins = 0;
        draws = 0;
        total_rounds = 0;
        total_wounds_by_attacker = 0;
        total_wounds_by_defender = 0;
        total_kills_by_attacker = 0;
        total_kills_by_defender = 0;
        attacker_routs = 0;
        defender_routs = 0;
        attacker_remaining_total = 0;
        defender_remaining_total = 0;
        for (auto& vc : victory_conditions) vc = 0;
    }
};

// ==============================================================================
// Per-Thread Statistics (No atomics needed)
// Merged at the end for efficiency
// ==============================================================================

struct LocalStats {
    u64 attacker_wins = 0;
    u64 defender_wins = 0;
    u64 draws = 0;

    u64 total_rounds = 0;
    u64 total_wounds_by_attacker = 0;
    u64 total_wounds_by_defender = 0;
    u64 total_kills_by_attacker = 0;
    u64 total_kills_by_defender = 0;

    u64 attacker_routs = 0;
    u64 defender_routs = 0;

    u64 attacker_remaining_total = 0;
    u64 defender_remaining_total = 0;

    std::array<u64, 10> victory_conditions{};

    void merge_into(AtomicStats& target) const {
        target.attacker_wins += attacker_wins;
        target.defender_wins += defender_wins;
        target.draws += draws;
        target.total_rounds += total_rounds;
        target.total_wounds_by_attacker += total_wounds_by_attacker;
        target.total_wounds_by_defender += total_wounds_by_defender;
        target.total_kills_by_attacker += total_kills_by_attacker;
        target.total_kills_by_defender += total_kills_by_defender;
        target.attacker_routs += attacker_routs;
        target.defender_routs += defender_routs;
        target.attacker_remaining_total += attacker_remaining_total;
        target.defender_remaining_total += defender_remaining_total;
        for (size_t i = 0; i < victory_conditions.size(); ++i) {
            target.victory_conditions[i] += victory_conditions[i];
        }
    }

    void reset() {
        attacker_wins = 0;
        defender_wins = 0;
        draws = 0;
        total_rounds = 0;
        total_wounds_by_attacker = 0;
        total_wounds_by_defender = 0;
        total_kills_by_attacker = 0;
        total_kills_by_defender = 0;
        attacker_routs = 0;
        defender_routs = 0;
        attacker_remaining_total = 0;
        defender_remaining_total = 0;
        victory_conditions.fill(0);
    }
};

// ==============================================================================
// Final Statistics Result (Computed from AtomicStats)
// ==============================================================================

struct SimulationStatistics {
    // Identifiers
    u32 attacker_id;
    u32 defender_id;
    u64 iterations;

    // Win rates (0.0 - 1.0)
    f64 attacker_win_rate;
    f64 defender_win_rate;
    f64 draw_rate;

    // Averages
    f64 avg_rounds;
    f64 avg_wounds_by_attacker;
    f64 avg_wounds_by_defender;
    f64 avg_kills_by_attacker;
    f64 avg_kills_by_defender;
    f64 avg_attacker_remaining;
    f64 avg_defender_remaining;

    // Rout rates
    f64 attacker_rout_rate;
    f64 defender_rout_rate;

    // Victory condition percentages
    std::array<f64, 10> victory_condition_rates{};

    // Computed from AtomicStats
    static SimulationStatistics compute(const AtomicStats& stats, u64 iterations,
                                        u32 atk_id = 0, u32 def_id = 0) {
        SimulationStatistics result{};
        result.attacker_id = atk_id;
        result.defender_id = def_id;
        result.iterations = iterations;

        if (iterations == 0) return result;

        f64 inv_iter = 1.0 / static_cast<f64>(iterations);

        result.attacker_win_rate = stats.attacker_wins.load() * inv_iter;
        result.defender_win_rate = stats.defender_wins.load() * inv_iter;
        result.draw_rate = stats.draws.load() * inv_iter;

        result.avg_rounds = stats.total_rounds.load() * inv_iter;
        result.avg_wounds_by_attacker = stats.total_wounds_by_attacker.load() * inv_iter;
        result.avg_wounds_by_defender = stats.total_wounds_by_defender.load() * inv_iter;
        result.avg_kills_by_attacker = stats.total_kills_by_attacker.load() * inv_iter;
        result.avg_kills_by_defender = stats.total_kills_by_defender.load() * inv_iter;
        result.avg_attacker_remaining = stats.attacker_remaining_total.load() * inv_iter;
        result.avg_defender_remaining = stats.defender_remaining_total.load() * inv_iter;

        result.attacker_rout_rate = stats.attacker_routs.load() * inv_iter;
        result.defender_rout_rate = stats.defender_routs.load() * inv_iter;

        for (size_t i = 0; i < 10; ++i) {
            result.victory_condition_rates[i] = stats.victory_conditions[i].load() * inv_iter;
        }

        return result;
    }
};

// ==============================================================================
// Matchup Result - Compact storage for 100B matchups
// ==============================================================================

struct alignas(8) MatchupResult {
    u16 attacker_id;
    u16 defender_id;
    u16 attacker_win_pct;  // Win percentage * 100 (0-10000 for 0.00-100.00%)
    u16 defender_win_pct;

    MatchupResult() : attacker_id(0), defender_id(0),
                      attacker_win_pct(0), defender_win_pct(0) {}

    MatchupResult(u16 atk, u16 def, f64 atk_rate, f64 def_rate)
        : attacker_id(atk), defender_id(def),
          attacker_win_pct(static_cast<u16>(atk_rate * 10000)),
          defender_win_pct(static_cast<u16>(def_rate * 10000)) {}

    f64 get_attacker_win_rate() const { return attacker_win_pct / 10000.0; }
    f64 get_defender_win_rate() const { return defender_win_pct / 10000.0; }
    f64 get_draw_rate() const {
        return 1.0 - get_attacker_win_rate() - get_defender_win_rate();
    }
};

static_assert(sizeof(MatchupResult) == 8, "MatchupResult must be 8 bytes for cache efficiency");

} // namespace battle
