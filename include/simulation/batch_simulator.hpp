#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "engine/dice.hpp"
#include "engine/game_runner.hpp"
#include "simulation/thread_pool.hpp"
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include <mutex>
#include <functional>
#include <cstring>

namespace battle {

// ==============================================================================
// Batch Configuration
// ==============================================================================

// Format options for result storage
enum class ResultFormat : u8 {
    Compact = 1,          // 8 bytes - basic win/loss only
    Extended = 2,         // 24 bytes - full game stats
    CompactExtended = 3,  // 16 bytes - compressed game stats (33% smaller than Extended)
    Aggregated = 4        // ~256 bytes per unit - comprehensive per-unit statistics
};

// Number of mutex shards for aggregated results (avoids O(n) mutex allocation)
// Using 8192 shards (~320KB) provides good parallelism while avoiding memory issues
static constexpr size_t AGGREGATED_MUTEX_SHARDS = 8192;

struct BatchConfig {
    u32 batch_size = 10000;          // Matchups per batch
    u32 checkpoint_interval = 1000000; // Save progress every N matchups
    bool enable_progress = true;
    ResultFormat format = ResultFormat::Compact;  // Output format
    std::string output_file = "results.bin";
    std::string checkpoint_file = "checkpoint.bin";

    // Convenience helpers for backwards compatibility
    bool is_extended() const { return format == ResultFormat::Extended; }
    bool is_compact_extended() const { return format == ResultFormat::CompactExtended; }
    bool has_extended_data() const { return format != ResultFormat::Compact; }

    // Returns size based on format (hardcoded to avoid forward declaration issues)
    // Note: Aggregated format size is per-unit, not per-matchup
    size_t result_size() const {
        switch (format) {
            case ResultFormat::Compact: return 8;   // sizeof(CompactMatchResult)
            case ResultFormat::Extended: return 24; // sizeof(ExtendedMatchResult)
            case ResultFormat::CompactExtended: return 16; // sizeof(CompactExtendedMatchResult)
            case ResultFormat::Aggregated: return 256; // sizeof(AggregatedUnitResult)
        }
        return 8;
    }

    bool is_aggregated() const { return format == ResultFormat::Aggregated; }
};

// ==============================================================================
// Checkpoint Data
// ==============================================================================

struct CheckpointData {
    u64 completed = 0;
    u64 total = 0;
    u32 units_a_count = 0;
    u32 units_b_count = 0;
    bool valid = false;
};

// ==============================================================================
// Compact Result for Storage (8 bytes)
// ==============================================================================

struct CompactMatchResult {
    u32 unit_a_id;        // Unit A ID
    u32 unit_b_id : 20;   // Unit B ID (20 bits for up to 1M units)
    u32 winner : 2;       // 0=A, 1=B, 2=Draw
    u32 games_a : 2;      // Games won by A (0-2)
    u32 games_b : 2;      // Games won by B (0-2)
    u32 padding : 6;      // Reserved

    CompactMatchResult() : unit_a_id(0), unit_b_id(0), winner(2),
                          games_a(0), games_b(0), padding(0) {}

    static CompactMatchResult from_match(const MatchResult& r) {
        CompactMatchResult c;
        c.unit_a_id = r.unit_a_id;
        c.unit_b_id = r.unit_b_id & 0xFFFFF;
        c.winner = static_cast<u32>(r.overall_winner);
        c.games_a = r.games_won_a;
        c.games_b = r.games_won_b;
        return c;
    }
};

static_assert(sizeof(CompactMatchResult) == 8, "CompactMatchResult must be 8 bytes");

// ==============================================================================
// Extended Result for Full Game Statistics (24 bytes)
// Stores detailed per-matchup game statistics for analysis
// ==============================================================================

struct ExtendedMatchResult {
    u32 unit_a_id;        // Unit A ID
    u32 unit_b_id;        // Unit B ID (full 32-bit for compatibility)

    // Match outcome
    u8 winner;            // 0=A, 1=B, 2=Draw
    u8 games_a;           // Games won by A (0-3)
    u8 games_b;           // Games won by B (0-3)
    u8 total_rounds;      // Total rounds played across all games

    // Combat statistics (accumulated across all games in match)
    u16 wounds_dealt_a;   // Total wounds dealt by unit A
    u16 wounds_dealt_b;   // Total wounds dealt by unit B
    u8 models_killed_a;   // Total models killed by unit A
    u8 models_killed_b;   // Total models killed by unit B

    // Objective control
    u8 rounds_holding_a;  // Rounds unit A held objective
    u8 rounds_holding_b;  // Rounds unit B held objective

    // Game ending flags (packed)
    u8 endings;           // Bits: [0-2]=game1, [3-5]=game2, [6-7]=game3 low bits
                          // Per game: 0=objective, 1=destruction_a, 2=destruction_b, 3=rout_a, 4=rout_b
    u8 endings_high;      // Bits: [0]=game3 high bit, [1-7]=reserved

    ExtendedMatchResult() : unit_a_id(0), unit_b_id(0), winner(2), games_a(0), games_b(0),
                           total_rounds(0), wounds_dealt_a(0), wounds_dealt_b(0),
                           models_killed_a(0), models_killed_b(0), rounds_holding_a(0),
                           rounds_holding_b(0), endings(0), endings_high(0) {}

    static ExtendedMatchResult from_match(const MatchResult& r) {
        ExtendedMatchResult e;
        e.unit_a_id = r.unit_a_id;
        e.unit_b_id = r.unit_b_id;
        e.winner = static_cast<u8>(r.overall_winner);
        e.games_a = r.games_won_a;
        e.games_b = r.games_won_b;
        e.wounds_dealt_a = static_cast<u16>(std::min(r.total_wounds_dealt_a, 65535u));
        e.wounds_dealt_b = static_cast<u16>(std::min(r.total_wounds_dealt_b, 65535u));
        e.models_killed_a = static_cast<u8>(std::min(static_cast<u16>(r.total_models_killed_a), static_cast<u16>(255)));
        e.models_killed_b = static_cast<u8>(std::min(static_cast<u16>(r.total_models_killed_b), static_cast<u16>(255)));
        e.rounds_holding_a = r.total_rounds_holding_a;
        e.rounds_holding_b = r.total_rounds_holding_b;
        return e;
    }

    // Convert to compact format for backwards compatibility
    CompactMatchResult to_compact() const {
        CompactMatchResult c;
        c.unit_a_id = unit_a_id;
        c.unit_b_id = unit_b_id & 0xFFFFF;
        c.winner = winner;
        c.games_a = games_a;
        c.games_b = games_b;
        return c;
    }

    // Game ending type enum
    enum class GameEnding : u8 {
        Objective = 0,
        DestructionA = 1,  // Unit A destroyed
        DestructionB = 2,  // Unit B destroyed
        RoutA = 3,         // Unit A routed
        RoutB = 4,         // Unit B routed
        Draw = 5
    };

    void set_game_ending(u8 game_index, GameEnding ending) {
        u8 val = static_cast<u8>(ending);
        if (game_index == 0) {
            endings = (endings & 0xF8) | (val & 0x07);
        } else if (game_index == 1) {
            endings = (endings & 0xC7) | ((val & 0x07) << 3);
        } else if (game_index == 2) {
            endings = (endings & 0x3F) | ((val & 0x03) << 6);
            endings_high = (endings_high & 0xFE) | ((val >> 2) & 0x01);
        }
    }

    GameEnding get_game_ending(u8 game_index) const {
        u8 val = 0;
        if (game_index == 0) {
            val = endings & 0x07;
        } else if (game_index == 1) {
            val = (endings >> 3) & 0x07;
        } else if (game_index == 2) {
            val = ((endings >> 6) & 0x03) | ((endings_high & 0x01) << 2);
        }
        return static_cast<GameEnding>(val);
    }
};

static_assert(sizeof(ExtendedMatchResult) == 24, "ExtendedMatchResult must be 24 bytes");

// ==============================================================================
// Compact Extended Result (16 bytes) - 33% smaller than ExtendedMatchResult
// Trades some precision for significant space savings on large datasets
// ==============================================================================

#pragma pack(push, 1)
struct CompactExtendedMatchResult {
    u32 unit_a_id;           // Unit A ID (full 32-bit)
    u32 unit_b_id;           // Unit B ID (full 32-bit)

    // Packed outcome byte:
    // bits 0-1: winner (0=A, 1=B, 2=Draw)
    // bits 2-3: games_a (0-3)
    // bits 4-5: games_b (0-3)
    // bits 6-7: reserved
    u8 outcome;

    u8 total_rounds;         // Total rounds played (0-12 for best-of-3)

    // Wounds scaled by /4 (max representable: 1020, typical max ~500)
    u8 wounds_a_scaled;
    u8 wounds_b_scaled;

    u8 models_killed_a;      // Models killed by A (0-255)
    u8 models_killed_b;      // Models killed by B (0-255)

    // Packed objective holding:
    // bits 0-3: rounds_holding_a (0-15, max actual is 12)
    // bits 4-7: rounds_holding_b (0-15)
    u8 holding;

    u8 reserved;             // Padding for alignment

    CompactExtendedMatchResult() : unit_a_id(0), unit_b_id(0), outcome(2),
                                   total_rounds(0), wounds_a_scaled(0), wounds_b_scaled(0),
                                   models_killed_a(0), models_killed_b(0), holding(0), reserved(0) {}

    // Create from full MatchResult
    static CompactExtendedMatchResult from_match(const MatchResult& r) {
        CompactExtendedMatchResult c;
        c.unit_a_id = r.unit_a_id;
        c.unit_b_id = r.unit_b_id;

        // Pack outcome: winner(2) | games_a(2) | games_b(2) | reserved(2)
        c.outcome = (static_cast<u8>(r.overall_winner) & 0x03) |
                    ((r.games_won_a & 0x03) << 2) |
                    ((r.games_won_b & 0x03) << 4);

        c.total_rounds = 0;  // Will be set by caller if available

        // Scale wounds by /4, saturate at 255
        c.wounds_a_scaled = static_cast<u8>(std::min(r.total_wounds_dealt_a / 4, 255u));
        c.wounds_b_scaled = static_cast<u8>(std::min(r.total_wounds_dealt_b / 4, 255u));

        c.models_killed_a = static_cast<u8>(std::min(static_cast<u16>(r.total_models_killed_a), static_cast<u16>(255)));
        c.models_killed_b = static_cast<u8>(std::min(static_cast<u16>(r.total_models_killed_b), static_cast<u16>(255)));

        // Pack holding: a(4) | b(4)
        c.holding = (r.total_rounds_holding_a & 0x0F) |
                    ((r.total_rounds_holding_b & 0x0F) << 4);

        return c;
    }

    // Create from ExtendedMatchResult (for conversion)
    static CompactExtendedMatchResult from_extended(const ExtendedMatchResult& e) {
        CompactExtendedMatchResult c;
        c.unit_a_id = e.unit_a_id;
        c.unit_b_id = e.unit_b_id;
        c.outcome = (e.winner & 0x03) |
                    ((e.games_a & 0x03) << 2) |
                    ((e.games_b & 0x03) << 4);
        c.total_rounds = e.total_rounds;
        c.wounds_a_scaled = static_cast<u8>(std::min(static_cast<u32>(e.wounds_dealt_a) / 4, 255u));
        c.wounds_b_scaled = static_cast<u8>(std::min(static_cast<u32>(e.wounds_dealt_b) / 4, 255u));
        c.models_killed_a = e.models_killed_a;
        c.models_killed_b = e.models_killed_b;
        c.holding = (e.rounds_holding_a & 0x0F) | ((e.rounds_holding_b & 0x0F) << 4);
        return c;
    }

    // Accessors for packed fields
    u8 winner() const { return outcome & 0x03; }
    u8 games_a() const { return (outcome >> 2) & 0x03; }
    u8 games_b() const { return (outcome >> 4) & 0x03; }
    u8 rounds_holding_a() const { return holding & 0x0F; }
    u8 rounds_holding_b() const { return (holding >> 4) & 0x0F; }

    // Get approximate wounds (multiply by 4)
    u16 wounds_dealt_a() const { return static_cast<u16>(wounds_a_scaled) * 4; }
    u16 wounds_dealt_b() const { return static_cast<u16>(wounds_b_scaled) * 4; }

    // Convert to compact format for backwards compatibility
    CompactMatchResult to_compact() const {
        CompactMatchResult c;
        c.unit_a_id = unit_a_id;
        c.unit_b_id = unit_b_id & 0xFFFFF;
        c.winner = winner();
        c.games_a = games_a();
        c.games_b = games_b();
        return c;
    }

    // Convert to full extended format (with some precision loss)
    ExtendedMatchResult to_extended() const {
        ExtendedMatchResult e;
        e.unit_a_id = unit_a_id;
        e.unit_b_id = unit_b_id;
        e.winner = winner();
        e.games_a = games_a();
        e.games_b = games_b();
        e.total_rounds = total_rounds;
        e.wounds_dealt_a = wounds_dealt_a();
        e.wounds_dealt_b = wounds_dealt_b();
        e.models_killed_a = models_killed_a;
        e.models_killed_b = models_killed_b;
        e.rounds_holding_a = rounds_holding_a();
        e.rounds_holding_b = rounds_holding_b();
        e.endings = 0;
        e.endings_high = 0;
        return e;
    }
};
#pragma pack(pop)

static_assert(sizeof(CompactExtendedMatchResult) == 16, "CompactExtendedMatchResult must be 16 bytes");

// ==============================================================================
// Aggregated Unit Result (256 bytes) - Comprehensive per-unit statistics
// Stores aggregated stats from all matchups for a single unit
// This format trades per-matchup detail for massive file size reduction:
// 18,000 units * 256 bytes = 4.6 MB vs 333M matchups * 16 bytes = 5.3 GB
// ==============================================================================

struct AggregatedUnitResult {
    // === Unit Identification (8 bytes) ===
    u32 unit_id;                    // Unit index in the units array
    u16 points_cost;                // Unit's point cost (for reference)
    u16 total_opponents;            // Number of unique opponents faced

    // === Overall Win/Loss Statistics (24 bytes) ===
    u32 total_matchups;             // Total matchups this unit participated in
    u32 wins;                       // Matchups won (best-of-3 wins)
    u32 losses;                     // Matchups lost
    u32 draws;                      // Matchups drawn

    // Individual game wins (3 games per matchup)
    u32 games_won;                  // Individual games won across all matchups
    u32 games_lost;                 // Individual games lost across all matchups

    // === Combat Statistics - Totals (32 bytes) ===
    u64 total_wounds_dealt;         // Total wounds inflicted across all matchups
    u64 total_wounds_received;      // Total wounds taken across all matchups
    u32 total_models_killed;        // Total enemy models killed
    u32 total_models_lost;          // Total own models lost

    // === Combat Statistics - Averaged (16 bytes) ===
    // Pre-computed averages stored as fixed-point (multiply by 100 for display)
    u16 avg_wounds_dealt_x100;      // Average wounds dealt per matchup * 100
    u16 avg_wounds_received_x100;   // Average wounds received per matchup * 100
    u16 avg_models_killed_x100;     // Average models killed per matchup * 100
    u16 avg_models_lost_x100;       // Average models lost per matchup * 100
    u16 avg_rounds_x100;            // Average rounds per game * 100
    u16 reserved_avg;               // Reserved for future use

    // === Objective Control Statistics (16 bytes) ===
    u32 total_objective_rounds;     // Total rounds holding objectives
    u32 opponent_objective_rounds;  // Total rounds opponent held objectives
    u16 matchups_with_objective;    // Matchups where objectives were contested
    u16 matchups_won_by_objective;  // Matchups decided by objective control
    u16 matchups_lost_by_objective; // Matchups lost due to objective control
    u16 reserved_obj;               // Reserved

    // === Victory Margin Analysis (24 bytes) ===
    // Breaks down wins/losses by margin (game score difference)
    u16 decisive_wins;              // Wins 3-0 (complete dominance)
    u16 solid_wins;                 // Wins 2-0 (clear advantage)
    u16 close_wins;                 // Wins 2-1 (close fight)
    u16 close_losses;               // Losses 1-2 (close fight)
    u16 solid_losses;               // Losses 0-2 (clear disadvantage)
    u16 decisive_losses;            // Losses 0-3 (complete defeat)

    // Win streaks (longest consecutive wins/losses against different opponents)
    u16 best_win_streak;            // Longest consecutive wins
    u16 worst_loss_streak;          // Longest consecutive losses
    u16 reserved_margin[2];         // Reserved

    // === Performance by Opponent Cost Bracket (48 bytes) ===
    // Win rate against opponents in different point cost ranges
    // Brackets: 0-99, 100-199, 200-299, 300-399, 400-499, 500+
    struct CostBracketStats {
        u16 matchups;               // Matchups in this bracket
        u16 wins;                   // Wins in this bracket
        u16 avg_wound_diff_x10;     // Avg wound differential * 10 (signed, stored as u16 offset by 32768)
        u16 reserved;
    };
    CostBracketStats cost_brackets[6];  // 8 bytes * 6 = 48 bytes

    // === Efficiency Metrics (24 bytes) ===
    u16 damage_efficiency_x100;     // (wounds dealt / points) * 100
    u16 survival_efficiency_x100;   // (wounds avoided / points) * 100
    u16 kill_efficiency_x100;       // (models killed / points) * 100
    u16 objective_efficiency_x100;  // (objective rounds / points) * 100

    // Points-adjusted win rate (performance relative to cost)
    u16 underdog_wins;              // Wins vs higher cost opponents
    u16 underdog_matchups;          // Matchups vs higher cost opponents
    u16 overdog_wins;               // Wins vs lower cost opponents
    u16 overdog_matchups;           // Matchups vs lower cost opponents

    // Expected vs actual performance
    u16 expected_win_rate_x100;     // Expected win rate based on points * 100
    u16 actual_vs_expected_x100;    // Actual / Expected * 100 (>100 = overperformer)

    // === Faction Performance (32 bytes) ===
    // Store win stats against up to 8 factions (identified by hash)
    struct FactionStats {
        u16 faction_hash;           // CRC16 of faction name (0 = empty slot)
        u16 matchups;               // Matchups against this faction
        u16 wins;                   // Wins against this faction
        u16 reserved;
    };
    FactionStats faction_stats[4];  // 8 bytes * 4 = 32 bytes

    // === Reserved for Future Use (48 bytes) ===
    u8 reserved_future[48];

    // === Constructors and Methods ===
    AggregatedUnitResult() { std::memset(this, 0, sizeof(*this)); }

    // Calculate derived metrics from raw data
    void finalize() {
        if (total_matchups == 0) return;

        // Calculate averages (stored as x100 fixed point)
        avg_wounds_dealt_x100 = static_cast<u16>(std::min(
            (total_wounds_dealt * 100) / total_matchups, static_cast<u64>(65535)));
        avg_wounds_received_x100 = static_cast<u16>(std::min(
            (total_wounds_received * 100) / total_matchups, static_cast<u64>(65535)));
        avg_models_killed_x100 = static_cast<u16>(std::min(
            (static_cast<u64>(total_models_killed) * 100) / total_matchups, static_cast<u64>(65535)));
        avg_models_lost_x100 = static_cast<u16>(std::min(
            (static_cast<u64>(total_models_lost) * 100) / total_matchups, static_cast<u64>(65535)));

        // Calculate efficiency metrics
        if (points_cost > 0) {
            damage_efficiency_x100 = static_cast<u16>(std::min(
                (total_wounds_dealt * 100) / (total_matchups * points_cost), static_cast<u64>(65535)));
            kill_efficiency_x100 = static_cast<u16>(std::min(
                (static_cast<u64>(total_models_killed) * 100) / (total_matchups * points_cost), static_cast<u64>(65535)));
            objective_efficiency_x100 = static_cast<u16>(std::min(
                (static_cast<u64>(total_objective_rounds) * 100) / (total_matchups * points_cost), static_cast<u64>(65535)));

            // Survival efficiency: inverse of wounds received per point
            u64 wounds_per_point = (total_wounds_received * 100) / (total_matchups * points_cost);
            survival_efficiency_x100 = static_cast<u16>(wounds_per_point > 0 ?
                std::min(10000 / wounds_per_point, static_cast<u64>(65535)) : 65535);
        }

        // Calculate expected vs actual win rate
        // Simple model: higher cost = higher expected win rate
        // Expected = 50% adjusted by relative cost (roughly)
        u16 actual_win_pct = static_cast<u16>((static_cast<u64>(wins) * 10000) / total_matchups);
        actual_vs_expected_x100 = actual_win_pct; // Simplified - actual win rate * 100
    }

    // Accessors for display
    f64 win_rate() const {
        return total_matchups > 0 ? 100.0 * wins / total_matchups : 0.0;
    }

    f64 avg_wounds_dealt() const { return avg_wounds_dealt_x100 / 100.0; }
    f64 avg_wounds_received() const { return avg_wounds_received_x100 / 100.0; }
    f64 avg_models_killed() const { return avg_models_killed_x100 / 100.0; }
    f64 avg_models_lost() const { return avg_models_lost_x100 / 100.0; }

    f64 damage_efficiency() const { return damage_efficiency_x100 / 100.0; }
    f64 survival_efficiency() const { return survival_efficiency_x100 / 100.0; }
    f64 kill_efficiency() const { return kill_efficiency_x100 / 100.0; }
    f64 objective_efficiency() const { return objective_efficiency_x100 / 100.0; }

    f64 underdog_win_rate() const {
        return underdog_matchups > 0 ? 100.0 * underdog_wins / underdog_matchups : 0.0;
    }

    f64 overdog_win_rate() const {
        return overdog_matchups > 0 ? 100.0 * overdog_wins / overdog_matchups : 0.0;
    }

    // Get cost bracket win rate
    f64 bracket_win_rate(size_t bracket) const {
        if (bracket >= 6 || cost_brackets[bracket].matchups == 0) return 0.0;
        return 100.0 * cost_brackets[bracket].wins / cost_brackets[bracket].matchups;
    }

    // Decode signed wound differential from unsigned storage
    i16 bracket_wound_diff(size_t bracket) const {
        if (bracket >= 6) return 0;
        return static_cast<i16>(cost_brackets[bracket].avg_wound_diff_x10) - 32768;
    }
};

static_assert(sizeof(AggregatedUnitResult) == 256, "AggregatedUnitResult must be 256 bytes");

// ==============================================================================
// Progress Callback
// ==============================================================================

// Aggregate stats from full game simulation
struct AggregateGameStats {
    std::atomic<u64> total_rounds_played{0};
    std::atomic<u64> total_games_played{0};
    std::atomic<u64> total_wounds_dealt{0};
    std::atomic<u64> total_models_killed{0};
    std::atomic<u64> total_objective_rounds{0};
    std::atomic<u64> games_ended_by_destruction{0};
    std::atomic<u64> games_ended_by_objective{0};

    void reset() {
        total_rounds_played = 0;
        total_games_played = 0;
        total_wounds_dealt = 0;
        total_models_killed = 0;
        total_objective_rounds = 0;
        games_ended_by_destruction = 0;
        games_ended_by_objective = 0;
    }

    f64 avg_rounds_per_game() const {
        u64 games = total_games_played.load();
        return games > 0 ? static_cast<f64>(total_rounds_played.load()) / games : 0.0;
    }

    f64 avg_wounds_per_game() const {
        u64 games = total_games_played.load();
        return games > 0 ? static_cast<f64>(total_wounds_dealt.load()) / games : 0.0;
    }

    f64 avg_models_killed_per_game() const {
        u64 games = total_games_played.load();
        return games > 0 ? static_cast<f64>(total_models_killed.load()) / games : 0.0;
    }

    f64 objective_game_percent() const {
        u64 games = total_games_played.load();
        return games > 0 ? 100.0 * static_cast<f64>(games_ended_by_objective.load()) / games : 0.0;
    }

    f64 destruction_game_percent() const {
        u64 games = total_games_played.load();
        return games > 0 ? 100.0 * static_cast<f64>(games_ended_by_destruction.load()) / games : 0.0;
    }
};

struct ProgressInfo {
    u64 completed;
    u64 total;
    f64 matchups_per_second;
    f64 elapsed_seconds;
    f64 estimated_remaining_seconds;
    bool resumed;  // True if this is a resumed simulation
    const AggregateGameStats* game_stats;  // Full game statistics
};

using ProgressCallback = std::function<void(const ProgressInfo&)>;

// ==============================================================================
// Batch Simulator - Parallel simulation of matchups with resume support
// ==============================================================================

class BatchSimulator {
public:
    explicit BatchSimulator(const BatchConfig& config = BatchConfig())
        : config_(config), pool_() {}

    // Get aggregate game stats (for display after simulation)
    const AggregateGameStats& game_stats() const { return game_stats_; }

    // Check if we can resume from checkpoint
    CheckpointData check_checkpoint(size_t units_a_count, size_t units_b_count) {
        CheckpointData data;

        std::ifstream in(config_.checkpoint_file, std::ios::binary);
        if (!in) return data;

        in.read(reinterpret_cast<char*>(&data.completed), sizeof(data.completed));
        in.read(reinterpret_cast<char*>(&data.total), sizeof(data.total));
        in.read(reinterpret_cast<char*>(&data.units_a_count), sizeof(data.units_a_count));
        in.read(reinterpret_cast<char*>(&data.units_b_count), sizeof(data.units_b_count));

        if (!in) return CheckpointData{};

        // Verify checkpoint matches current configuration
        u64 expected_total = static_cast<u64>(units_a_count) * units_b_count;
        if (data.total == expected_total &&
            data.units_a_count == units_a_count &&
            data.units_b_count == units_b_count &&
            data.completed < data.total) {
            data.valid = true;
        }

        return data;
    }

    // Simulate all matchups with optional resume
    void simulate_all(
        const std::vector<Unit>& units_a,
        const std::vector<Unit>& units_b,
        ProgressCallback progress = nullptr,
        bool try_resume = false
    ) {
        // Handle aggregated format separately (no resume support for aggregated)
        if (config_.format == ResultFormat::Aggregated) {
            simulate_all_aggregated(units_a, units_b, progress);
            return;
        }

        u64 total_matchups = static_cast<u64>(units_a.size()) * units_b.size();
        u64 resume_from = 0;
        bool resumed = false;

        // Determine result size based on format
        const size_t result_size = config_.result_size();

        // Reset game stats for this simulation
        game_stats_.reset();

        // Check for resume
        if (try_resume) {
            CheckpointData checkpoint = check_checkpoint(units_a.size(), units_b.size());
            if (checkpoint.valid) {
                // Verify output file exists and has correct size
                std::ifstream check_out(config_.output_file, std::ios::binary | std::ios::ate);
                if (check_out) {
                    size_t file_size = check_out.tellg();
                    size_t expected_size = 16 + checkpoint.completed * result_size;
                    if (file_size >= expected_size) {
                        resume_from = checkpoint.completed;
                        resumed = true;
                    }
                }
            }
        }

        // Use a large write buffer (4MB) to reduce syscall frequency
        // This prevents progressive slowdown caused by OS-level I/O throttling
        static constexpr size_t WRITE_BUFFER_SIZE = 4 * 1024 * 1024;  // 4MB
        std::vector<char> write_buffer(WRITE_BUFFER_SIZE);

        // Open output file (append if resuming, truncate if starting fresh)
        std::ofstream out;
        out.rdbuf()->pubsetbuf(write_buffer.data(), write_buffer.size());

        if (resumed) {
            out.open(config_.output_file, std::ios::binary | std::ios::in | std::ios::out);
            if (out) {
                // Seek to position after existing results
                out.seekp(16 + resume_from * result_size);
            }
        } else {
            out.open(config_.output_file, std::ios::binary | std::ios::trunc);
            if (out) {
                write_header(out, units_a.size(), units_b.size());
            }
        }

        if (!out) {
            throw std::runtime_error("Cannot open output file: " + config_.output_file);
        }

        // Track progress
        std::atomic<u64> completed{resume_from};
        auto start_time = std::chrono::high_resolution_clock::now();

        // Create batches
        std::vector<std::pair<u32, u32>> matchups;
        matchups.reserve(config_.batch_size);

        std::mutex output_mutex;  // Kept for API compatibility with process_batch signature

        // Buffers for different formats (only one will be used)
        std::vector<CompactMatchResult> results_buffer;
        std::vector<ExtendedMatchResult> extended_results_buffer;
        std::vector<CompactExtendedMatchResult> compact_extended_results_buffer;
        switch (config_.format) {
            case ResultFormat::Compact:
                results_buffer.reserve(config_.batch_size + 16);
                break;
            case ResultFormat::Extended:
                extended_results_buffer.reserve(config_.batch_size + 16);
                break;
            case ResultFormat::CompactExtended:
                compact_extended_results_buffer.reserve(config_.batch_size + 16);
                break;
        }

        // Calculate starting position if resuming
        u32 start_i = static_cast<u32>(resume_from / units_b.size());
        u32 start_j = static_cast<u32>(resume_from % units_b.size());

        // Process all matchups
        u64 current_index = resume_from;
        for (u32 i = start_i; i < units_a.size(); ++i) {
            u32 j_start = (i == start_i) ? start_j : 0;
            for (u32 j = j_start; j < units_b.size(); ++j) {
                matchups.emplace_back(i, j);

                // Process batch when full
                if (matchups.size() >= config_.batch_size) {
                    switch (config_.format) {
                        case ResultFormat::Compact:
                            process_batch(units_a, units_b, matchups, results_buffer, output_mutex);
                            {
                                std::lock_guard<std::mutex> lock(output_mutex);
                                out.write(reinterpret_cast<const char*>(results_buffer.data()),
                                         results_buffer.size() * sizeof(CompactMatchResult));
                                completed += results_buffer.size();
                                results_buffer.clear();
                            }
                            break;
                        case ResultFormat::Extended:
                            process_batch_extended(units_a, units_b, matchups, extended_results_buffer, output_mutex);
                            {
                                std::lock_guard<std::mutex> lock(output_mutex);
                                out.write(reinterpret_cast<const char*>(extended_results_buffer.data()),
                                         extended_results_buffer.size() * sizeof(ExtendedMatchResult));
                                completed += extended_results_buffer.size();
                                extended_results_buffer.clear();
                            }
                            break;
                        case ResultFormat::CompactExtended:
                            process_batch_compact_extended(units_a, units_b, matchups, compact_extended_results_buffer, output_mutex);
                            {
                                std::lock_guard<std::mutex> lock(output_mutex);
                                out.write(reinterpret_cast<const char*>(compact_extended_results_buffer.data()),
                                         compact_extended_results_buffer.size() * sizeof(CompactExtendedMatchResult));
                                completed += compact_extended_results_buffer.size();
                                compact_extended_results_buffer.clear();
                            }
                            break;
                    }

                    matchups.clear();

                    // Report progress
                    if (progress && config_.enable_progress) {
                        auto now = std::chrono::high_resolution_clock::now();
                        f64 elapsed = std::chrono::duration<f64>(now - start_time).count();
                        u64 done = completed.load();
                        u64 done_this_session = done - resume_from;
                        f64 rate = done_this_session / elapsed;
                        f64 remaining = (total_matchups - done) / rate;

                        progress({done, total_matchups, rate, elapsed, remaining, resumed, &game_stats_});
                    }

                    // Checkpoint - also flush here to ensure data safety
                    u64 current_completed = completed.load();
                    if (current_completed % config_.checkpoint_interval == 0) {
                        out.flush();  // Only flush at checkpoints
                        write_checkpoint(current_completed, total_matchups,
                                        units_a.size(), units_b.size());
                    }
                }
            }
        }

        // Process remaining matchups
        if (!matchups.empty()) {
            switch (config_.format) {
                case ResultFormat::Compact:
                    process_batch(units_a, units_b, matchups, results_buffer, output_mutex);
                    out.write(reinterpret_cast<const char*>(results_buffer.data()),
                             results_buffer.size() * sizeof(CompactMatchResult));
                    completed += results_buffer.size();
                    break;
                case ResultFormat::Extended:
                    process_batch_extended(units_a, units_b, matchups, extended_results_buffer, output_mutex);
                    out.write(reinterpret_cast<const char*>(extended_results_buffer.data()),
                             extended_results_buffer.size() * sizeof(ExtendedMatchResult));
                    completed += extended_results_buffer.size();
                    break;
                case ResultFormat::CompactExtended:
                    process_batch_compact_extended(units_a, units_b, matchups, compact_extended_results_buffer, output_mutex);
                    out.write(reinterpret_cast<const char*>(compact_extended_results_buffer.data()),
                             compact_extended_results_buffer.size() * sizeof(CompactExtendedMatchResult));
                    completed += compact_extended_results_buffer.size();
                    break;
            }
            out.flush();
        }

        // Final checkpoint (mark as complete)
        write_checkpoint(completed.load(), total_matchups, units_a.size(), units_b.size());

        // Final progress report
        if (progress) {
            auto now = std::chrono::high_resolution_clock::now();
            f64 elapsed = std::chrono::duration<f64>(now - start_time).count();
            u64 done = completed.load();
            u64 done_this_session = done - resume_from;
            f64 rate = done_this_session / elapsed;
            progress({done, total_matchups, rate, elapsed, 0.0, resumed, &game_stats_});
        }
    }

    // Legacy method for backwards compatibility
    u64 resume_from_checkpoint() {
        std::ifstream in(config_.checkpoint_file, std::ios::binary);
        if (!in) return 0;

        u64 completed;
        in.read(reinterpret_cast<char*>(&completed), sizeof(completed));
        return in ? completed : 0;
    }

    // Get thread count
    size_t thread_count() const { return pool_.thread_count(); }

    // Simulate all matchups with aggregated output (per-unit statistics)
    void simulate_all_aggregated(
        const std::vector<Unit>& units_a,
        const std::vector<Unit>& units_b,
        ProgressCallback progress = nullptr
    ) {
        const size_t num_units = units_a.size();
        const u64 total_matchups = static_cast<u64>(num_units) * units_b.size();

        // Reset game stats
        game_stats_.reset();

        // Initialize aggregated results for all units
        std::vector<AggregatedUnitResult> aggregated_results;
        aggregated_results.resize(num_units);

        // Use sharded mutexes instead of per-unit mutexes to avoid O(n) memory overhead
        // With 5M+ units, per-unit mutexes would require ~200MB just for mutex objects
        std::vector<std::mutex> unit_mutexes(AGGREGATED_MUTEX_SHARDS);

        // Initialize each result with unit info
        for (size_t i = 0; i < num_units; ++i) {
            aggregated_results[i].unit_id = static_cast<u32>(i);
            aggregated_results[i].points_cost = units_a[i].points_cost;
            aggregated_results[i].total_opponents = static_cast<u16>(units_b.size());
        }

        // Track progress
        std::atomic<u64> completed{0};
        auto start_time = std::chrono::high_resolution_clock::now();

        // Create batches
        std::vector<std::pair<u32, u32>> matchups;
        matchups.reserve(config_.batch_size);

        // Process all matchups
        for (u32 i = 0; i < num_units; ++i) {
            for (u32 j = 0; j < units_b.size(); ++j) {
                matchups.emplace_back(i, j);

                // Process batch when full
                if (matchups.size() >= config_.batch_size) {
                    process_batch_aggregated(units_a, units_b, matchups,
                                            aggregated_results, unit_mutexes);
                    completed += matchups.size();
                    matchups.clear();

                    // Report progress
                    if (progress && config_.enable_progress) {
                        auto now = std::chrono::high_resolution_clock::now();
                        f64 elapsed = std::chrono::duration<f64>(now - start_time).count();
                        u64 done = completed.load();
                        f64 rate = done / elapsed;
                        f64 remaining = (total_matchups - done) / rate;

                        progress({done, total_matchups, rate, elapsed, remaining, false, &game_stats_});
                    }
                }
            }
        }

        // Process remaining matchups
        if (!matchups.empty()) {
            process_batch_aggregated(units_a, units_b, matchups,
                                    aggregated_results, unit_mutexes);
            completed += matchups.size();
        }

        // Finalize all results (compute averages and derived metrics)
        for (auto& result : aggregated_results) {
            result.finalize();
        }

        // Write output file
        std::ofstream out(config_.output_file, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("Cannot open output file: " + config_.output_file);
        }

        // Write header (version 4 = aggregated)
        u32 magic = 0x42415453;  // "SABS"
        u32 version = 4;         // Aggregated format
        u32 unit_count = static_cast<u32>(num_units);
        u32 opponent_count = static_cast<u32>(units_b.size());

        out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        out.write(reinterpret_cast<const char*>(&unit_count), sizeof(unit_count));
        out.write(reinterpret_cast<const char*>(&opponent_count), sizeof(opponent_count));

        // Write all aggregated results
        out.write(reinterpret_cast<const char*>(aggregated_results.data()),
                  aggregated_results.size() * sizeof(AggregatedUnitResult));

        out.flush();

        // Final progress report
        if (progress) {
            auto now = std::chrono::high_resolution_clock::now();
            f64 elapsed = std::chrono::duration<f64>(now - start_time).count();
            u64 done = completed.load();
            f64 rate = done / elapsed;
            progress({done, total_matchups, rate, elapsed, 0.0, false, &game_stats_});
        }
    }

private:
    BatchConfig config_;
    ThreadPool pool_;
    AggregateGameStats game_stats_;

    void write_header(std::ofstream& out, size_t units_a_count, size_t units_b_count) {
        u32 magic = 0x42415453;  // "SABS" = Science Battle Sim
        u32 version = static_cast<u32>(config_.format);  // 1=compact, 2=extended, 3=compact_extended
        u32 a_count = static_cast<u32>(units_a_count);
        u32 b_count = static_cast<u32>(units_b_count);

        out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        out.write(reinterpret_cast<const char*>(&a_count), sizeof(a_count));
        out.write(reinterpret_cast<const char*>(&b_count), sizeof(b_count));
    }

    void write_checkpoint(u64 completed, u64 total, size_t units_a, size_t units_b) {
        std::ofstream out(config_.checkpoint_file, std::ios::binary);
        if (out) {
            u32 a_count = static_cast<u32>(units_a);
            u32 b_count = static_cast<u32>(units_b);
            out.write(reinterpret_cast<const char*>(&completed), sizeof(completed));
            out.write(reinterpret_cast<const char*>(&total), sizeof(total));
            out.write(reinterpret_cast<const char*>(&a_count), sizeof(a_count));
            out.write(reinterpret_cast<const char*>(&b_count), sizeof(b_count));
        }
    }

    void process_batch(
        const std::vector<Unit>& units_a,
        const std::vector<Unit>& units_b,
        const std::vector<std::pair<u32, u32>>& matchups,
        std::vector<CompactMatchResult>& results,
        std::mutex& /* unused - kept for API compatibility */
    ) {
        const size_t batch_size = matchups.size();
        const size_t num_threads = pool_.thread_count();
        const size_t chunk_size = (batch_size + num_threads - 1) / num_threads;

        // Pre-allocate results array - threads write directly to their slots
        results.resize(batch_size);

        // Atomic counter for completion tracking (no futures needed)
        std::atomic<size_t> threads_done{0};

        for (size_t t = 0; t < num_threads; ++t) {
            size_t start = t * chunk_size;
            size_t end = std::min(start + chunk_size, batch_size);

            if (start >= end) {
                ++threads_done;  // Empty chunk, count as done
                continue;
            }

            // Fire-and-forget task (no packaged_task allocation)
            pool_.submit_detached([&, start, end, t]() {
                // Use thread_local to reuse GameRunner across batches
                thread_local DiceRoller dice(
                    std::hash<std::thread::id>{}(std::this_thread::get_id()) * 2654435761ULL +
                    static_cast<u64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
                );
                thread_local GameRunner runner(dice);

                // Thread-local accumulators to reduce atomic contention
                u64 local_games = 0;
                u64 local_wounds = 0;
                u64 local_models_killed = 0;
                u64 local_obj_rounds = 0;
                u64 local_objective_games = 0;

                // Write directly to pre-allocated result slots (no vector allocation)
                for (size_t i = start; i < end; ++i) {
                    auto [a_idx, b_idx] = matchups[i];
                    MatchResult mr = runner.run_match(units_a[a_idx], units_b[b_idx]);
                    results[i] = CompactMatchResult::from_match(mr);

                    // Accumulate full game stats
                    // run_match runs 3 games (best of 3), so we get stats from all 3
                    local_games += 3;  // Best-of-3 match
                    local_wounds += mr.total_wounds_dealt_a + mr.total_wounds_dealt_b;
                    local_models_killed += mr.total_models_killed_a + mr.total_models_killed_b;
                    local_obj_rounds += mr.total_rounds_holding_a + mr.total_rounds_holding_b;

                    // Track game endings - we can infer from match results
                    // If objective rounds are significant, it was likely an objective game
                    if (mr.total_rounds_holding_a > 0 || mr.total_rounds_holding_b > 0) {
                        local_objective_games += 3;  // Approximate - objective was contested
                    }
                }

                // Update global stats atomically (batched to reduce contention)
                game_stats_.total_games_played.fetch_add(local_games, std::memory_order_relaxed);
                game_stats_.total_wounds_dealt.fetch_add(local_wounds, std::memory_order_relaxed);
                game_stats_.total_models_killed.fetch_add(local_models_killed, std::memory_order_relaxed);
                game_stats_.total_objective_rounds.fetch_add(local_obj_rounds, std::memory_order_relaxed);
                game_stats_.games_ended_by_objective.fetch_add(local_objective_games, std::memory_order_relaxed);

                ++threads_done;
            });
        }

        // Wait for all threads to complete (simple spin-wait with yield)
        while (threads_done.load(std::memory_order_acquire) < num_threads) {
            std::this_thread::yield();
        }
    }

    void process_batch_extended(
        const std::vector<Unit>& units_a,
        const std::vector<Unit>& units_b,
        const std::vector<std::pair<u32, u32>>& matchups,
        std::vector<ExtendedMatchResult>& results,
        std::mutex& /* unused - kept for API compatibility */
    ) {
        const size_t batch_size = matchups.size();
        const size_t num_threads = pool_.thread_count();
        const size_t chunk_size = (batch_size + num_threads - 1) / num_threads;

        // Pre-allocate results array - threads write directly to their slots
        results.resize(batch_size);

        // Atomic counter for completion tracking (no futures needed)
        std::atomic<size_t> threads_done{0};

        for (size_t t = 0; t < num_threads; ++t) {
            size_t start = t * chunk_size;
            size_t end = std::min(start + chunk_size, batch_size);

            if (start >= end) {
                ++threads_done;  // Empty chunk, count as done
                continue;
            }

            // Fire-and-forget task (no packaged_task allocation)
            pool_.submit_detached([&, start, end, t]() {
                // Use thread_local to reuse GameRunner across batches
                thread_local DiceRoller dice(
                    std::hash<std::thread::id>{}(std::this_thread::get_id()) * 2654435761ULL +
                    static_cast<u64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
                );
                thread_local GameRunner runner(dice);

                // Thread-local accumulators to reduce atomic contention
                u64 local_games = 0;
                u64 local_wounds = 0;
                u64 local_models_killed = 0;
                u64 local_obj_rounds = 0;
                u64 local_objective_games = 0;

                // Write directly to pre-allocated result slots (no vector allocation)
                for (size_t i = start; i < end; ++i) {
                    auto [a_idx, b_idx] = matchups[i];
                    MatchResult mr = runner.run_match(units_a[a_idx], units_b[b_idx]);
                    results[i] = ExtendedMatchResult::from_match(mr);

                    // Accumulate full game stats
                    // run_match runs 3 games (best of 3), so we get stats from all 3
                    local_games += 3;  // Best-of-3 match
                    local_wounds += mr.total_wounds_dealt_a + mr.total_wounds_dealt_b;
                    local_models_killed += mr.total_models_killed_a + mr.total_models_killed_b;
                    local_obj_rounds += mr.total_rounds_holding_a + mr.total_rounds_holding_b;

                    // Track game endings - we can infer from match results
                    // If objective rounds are significant, it was likely an objective game
                    if (mr.total_rounds_holding_a > 0 || mr.total_rounds_holding_b > 0) {
                        local_objective_games += 3;  // Approximate - objective was contested
                    }
                }

                // Update global stats atomically (batched to reduce contention)
                game_stats_.total_games_played.fetch_add(local_games, std::memory_order_relaxed);
                game_stats_.total_wounds_dealt.fetch_add(local_wounds, std::memory_order_relaxed);
                game_stats_.total_models_killed.fetch_add(local_models_killed, std::memory_order_relaxed);
                game_stats_.total_objective_rounds.fetch_add(local_obj_rounds, std::memory_order_relaxed);
                game_stats_.games_ended_by_objective.fetch_add(local_objective_games, std::memory_order_relaxed);

                ++threads_done;
            });
        }

        // Wait for all threads to complete (simple spin-wait with yield)
        while (threads_done.load(std::memory_order_acquire) < num_threads) {
            std::this_thread::yield();
        }
    }

    void process_batch_compact_extended(
        const std::vector<Unit>& units_a,
        const std::vector<Unit>& units_b,
        const std::vector<std::pair<u32, u32>>& matchups,
        std::vector<CompactExtendedMatchResult>& results,
        std::mutex& /* unused - kept for API compatibility */
    ) {
        const size_t batch_size = matchups.size();
        const size_t num_threads = pool_.thread_count();
        const size_t chunk_size = (batch_size + num_threads - 1) / num_threads;

        // Pre-allocate results array - threads write directly to their slots
        results.resize(batch_size);

        // Atomic counter for completion tracking (no futures needed)
        std::atomic<size_t> threads_done{0};

        for (size_t t = 0; t < num_threads; ++t) {
            size_t start = t * chunk_size;
            size_t end = std::min(start + chunk_size, batch_size);

            if (start >= end) {
                ++threads_done;  // Empty chunk, count as done
                continue;
            }

            // Fire-and-forget task (no packaged_task allocation)
            pool_.submit_detached([&, start, end, t]() {
                // Use thread_local to reuse GameRunner across batches
                thread_local DiceRoller dice(
                    std::hash<std::thread::id>{}(std::this_thread::get_id()) * 2654435761ULL +
                    static_cast<u64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
                );
                thread_local GameRunner runner(dice);

                // Thread-local accumulators to reduce atomic contention
                u64 local_games = 0;
                u64 local_wounds = 0;
                u64 local_models_killed = 0;
                u64 local_obj_rounds = 0;
                u64 local_objective_games = 0;

                // Write directly to pre-allocated result slots (no vector allocation)
                for (size_t i = start; i < end; ++i) {
                    auto [a_idx, b_idx] = matchups[i];
                    MatchResult mr = runner.run_match(units_a[a_idx], units_b[b_idx]);
                    results[i] = CompactExtendedMatchResult::from_match(mr);

                    // Accumulate full game stats
                    // run_match runs 3 games (best of 3), so we get stats from all 3
                    local_games += 3;  // Best-of-3 match
                    local_wounds += mr.total_wounds_dealt_a + mr.total_wounds_dealt_b;
                    local_models_killed += mr.total_models_killed_a + mr.total_models_killed_b;
                    local_obj_rounds += mr.total_rounds_holding_a + mr.total_rounds_holding_b;

                    // Track game endings - we can infer from match results
                    // If objective rounds are significant, it was likely an objective game
                    if (mr.total_rounds_holding_a > 0 || mr.total_rounds_holding_b > 0) {
                        local_objective_games += 3;  // Approximate - objective was contested
                    }
                }

                // Update global stats atomically (batched to reduce contention)
                game_stats_.total_games_played.fetch_add(local_games, std::memory_order_relaxed);
                game_stats_.total_wounds_dealt.fetch_add(local_wounds, std::memory_order_relaxed);
                game_stats_.total_models_killed.fetch_add(local_models_killed, std::memory_order_relaxed);
                game_stats_.total_objective_rounds.fetch_add(local_obj_rounds, std::memory_order_relaxed);
                game_stats_.games_ended_by_objective.fetch_add(local_objective_games, std::memory_order_relaxed);

                ++threads_done;
            });
        }

        // Wait for all threads to complete (simple spin-wait with yield)
        while (threads_done.load(std::memory_order_acquire) < num_threads) {
            std::this_thread::yield();
        }
    }

    // Helper function to compute CRC16 hash for faction names
    static u16 crc16_hash(std::string_view str) {
        u16 crc = 0xFFFF;
        for (char c : str) {
            crc ^= static_cast<u16>(c) << 8;
            for (int i = 0; i < 8; ++i) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ 0x1021;
                } else {
                    crc <<= 1;
                }
            }
        }
        return crc == 0 ? 1 : crc;  // Ensure non-zero for valid entries
    }

    void process_batch_aggregated(
        const std::vector<Unit>& units_a,
        const std::vector<Unit>& units_b,
        const std::vector<std::pair<u32, u32>>& matchups,
        std::vector<AggregatedUnitResult>& aggregated,
        std::vector<std::mutex>& unit_mutexes
    ) {
        const size_t batch_size = matchups.size();
        const size_t num_threads = pool_.thread_count();
        const size_t chunk_size = (batch_size + num_threads - 1) / num_threads;

        std::atomic<size_t> threads_done{0};

        for (size_t t = 0; t < num_threads; ++t) {
            size_t start = t * chunk_size;
            size_t end = std::min(start + chunk_size, batch_size);

            if (start >= end) {
                ++threads_done;
                continue;
            }

            pool_.submit_detached([&, start, end, t]() {
                thread_local DiceRoller dice(
                    std::hash<std::thread::id>{}(std::this_thread::get_id()) * 2654435761ULL +
                    static_cast<u64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
                );
                thread_local GameRunner runner(dice);

                // Thread-local accumulators for global stats
                u64 local_games = 0;
                u64 local_wounds = 0;
                u64 local_models_killed = 0;
                u64 local_obj_rounds = 0;
                u64 local_objective_games = 0;

                for (size_t i = start; i < end; ++i) {
                    auto [a_idx, b_idx] = matchups[i];

                    MatchResult mr = runner.run_match(units_a[a_idx], units_b[b_idx]);

                        // Get opponent info for categorization
                        const Unit& unit_a = units_a[a_idx];
                        const Unit& unit_b = units_b[b_idx];

                        // Update global game stats
                        local_games += 3;
                        local_wounds += mr.total_wounds_dealt_a + mr.total_wounds_dealt_b;
                        local_models_killed += mr.total_models_killed_a + mr.total_models_killed_b;
                        local_obj_rounds += mr.total_rounds_holding_a + mr.total_rounds_holding_b;
                        if (mr.total_rounds_holding_a > 0 || mr.total_rounds_holding_b > 0) {
                            local_objective_games += 3;
                        }

                        // Update unit A's aggregated stats
                        {
                            std::lock_guard<std::mutex> lock(unit_mutexes[a_idx % AGGREGATED_MUTEX_SHARDS]);
                            AggregatedUnitResult& ar = aggregated[a_idx];

                        ar.total_matchups++;

                        // Win/loss tracking
                        if (mr.overall_winner == GameWinner::UnitA) {
                            ar.wins++;
                            // Victory margin
                            if (mr.games_won_b == 0) {
                                if (mr.games_won_a >= 3) ar.decisive_wins++;
                                else ar.solid_wins++;
                            } else {
                                ar.close_wins++;
                            }
                        } else if (mr.overall_winner == GameWinner::UnitB) {
                            ar.losses++;
                            // Loss margin
                            if (mr.games_won_a == 0) {
                                if (mr.games_won_b >= 3) ar.decisive_losses++;
                                else ar.solid_losses++;
                            } else {
                                ar.close_losses++;
                            }
                        } else {
                            ar.draws++;
                        }

                        // Game counts
                        ar.games_won += mr.games_won_a;
                        ar.games_lost += mr.games_won_b;

                        // Combat stats
                        ar.total_wounds_dealt += mr.total_wounds_dealt_a;
                        ar.total_wounds_received += mr.total_wounds_dealt_b;
                        ar.total_models_killed += mr.total_models_killed_a;
                        ar.total_models_lost += mr.total_models_killed_b;

                        // Objective stats
                        ar.total_objective_rounds += mr.total_rounds_holding_a;
                        ar.opponent_objective_rounds += mr.total_rounds_holding_b;
                        if (mr.total_rounds_holding_a > 0 || mr.total_rounds_holding_b > 0) {
                            ar.matchups_with_objective++;
                        }

                        // Cost bracket tracking (opponent's cost)
                        size_t bracket = std::min(static_cast<size_t>(unit_b.points_cost / 100), size_t(5));
                        ar.cost_brackets[bracket].matchups++;
                        if (mr.overall_winner == GameWinner::UnitA) {
                            ar.cost_brackets[bracket].wins++;
                        }
                        // Store wound differential (offset by 32768 for signed storage)
                        i32 wound_diff = static_cast<i32>(mr.total_wounds_dealt_a) -
                                        static_cast<i32>(mr.total_wounds_dealt_b);
                        // Running average stored as x10 offset
                        i32 current_avg = static_cast<i32>(ar.cost_brackets[bracket].avg_wound_diff_x10) - 32768;
                        i32 new_avg = current_avg + (wound_diff * 10 - current_avg) /
                                     static_cast<i32>(ar.cost_brackets[bracket].matchups);
                        ar.cost_brackets[bracket].avg_wound_diff_x10 = static_cast<u16>(new_avg + 32768);

                        // Underdog/overdog tracking
                        if (unit_b.points_cost > unit_a.points_cost) {
                            ar.underdog_matchups++;
                            if (mr.overall_winner == GameWinner::UnitA) {
                                ar.underdog_wins++;
                            }
                        } else if (unit_b.points_cost < unit_a.points_cost) {
                            ar.overdog_matchups++;
                            if (mr.overall_winner == GameWinner::UnitA) {
                                ar.overdog_wins++;
                            }
                        }

                        // Faction stats (find or add slot)
                        u16 faction_hash = crc16_hash(unit_b.faction.view());
                        bool found = false;
                        for (size_t f = 0; f < 4; ++f) {
                            if (ar.faction_stats[f].faction_hash == faction_hash) {
                                ar.faction_stats[f].matchups++;
                                if (mr.overall_winner == GameWinner::UnitA) {
                                    ar.faction_stats[f].wins++;
                                }
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            // Find empty slot or replace least-used
                            size_t min_slot = 0;
                            u16 min_matchups = ar.faction_stats[0].matchups;
                            for (size_t f = 0; f < 4; ++f) {
                                if (ar.faction_stats[f].faction_hash == 0) {
                                    min_slot = f;
                                    break;
                                }
                                if (ar.faction_stats[f].matchups < min_matchups) {
                                    min_matchups = ar.faction_stats[f].matchups;
                                    min_slot = f;
                                }
                            }
                            ar.faction_stats[min_slot].faction_hash = faction_hash;
                            ar.faction_stats[min_slot].matchups = 1;
                            ar.faction_stats[min_slot].wins = (mr.overall_winner == GameWinner::UnitA) ? 1 : 0;
                        }
                        }  // end lock_guard scope
                }

                // Update global stats
                game_stats_.total_games_played.fetch_add(local_games, std::memory_order_relaxed);
                game_stats_.total_wounds_dealt.fetch_add(local_wounds, std::memory_order_relaxed);
                game_stats_.total_models_killed.fetch_add(local_models_killed, std::memory_order_relaxed);
                game_stats_.total_objective_rounds.fetch_add(local_obj_rounds, std::memory_order_relaxed);
                game_stats_.games_ended_by_objective.fetch_add(local_objective_games, std::memory_order_relaxed);

                threads_done.fetch_add(1, std::memory_order_release);
            });
        }

        while (threads_done.load(std::memory_order_acquire) < num_threads) {
            std::this_thread::yield();
        }
    }
};

// ==============================================================================
// Benchmark Helper
// ==============================================================================

inline void benchmark_simulation(const std::vector<Unit>& units, size_t num_matchups = 10000) {
    ThreadPool pool;
    std::atomic<u64> completed{0};

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::future<void>> futures;
    size_t matchups_per_thread = num_matchups / pool.thread_count();

    for (size_t t = 0; t < pool.thread_count(); ++t) {
        futures.push_back(pool.submit([&, matchups_per_thread]() {
            DiceRoller dice;
            GameRunner runner(dice);

            for (size_t i = 0; i < matchups_per_thread; ++i) {
                size_t a = i % units.size();
                size_t b = (i + 1) % units.size();
                runner.run_match(units[a], units[b]);
                ++completed;
            }
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    f64 elapsed = std::chrono::duration<f64>(end - start).count();
    f64 rate = completed.load() / elapsed;

    std::cout << "Benchmark Results:\n";
    std::cout << "  Threads: " << pool.thread_count() << "\n";
    std::cout << "  Matchups: " << completed.load() << "\n";
    std::cout << "  Time: " << elapsed << " seconds\n";
    std::cout << "  Rate: " << rate << " matchups/second\n";
    std::cout << "  Estimated for 1T matchups: " << (1e12 / rate / 86400) << " days\n";
}

} // namespace battle
