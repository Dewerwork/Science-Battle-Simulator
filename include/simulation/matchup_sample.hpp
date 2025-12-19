#pragma once

#include "core/types.hpp"
#include "engine/game_state.hpp"
#include <cstring>

namespace battle {

// ==============================================================================
// Matchup Sample (16 bytes) - Compact record for random sampling
// Used for Tier 2 data storage: stores ~0.3% of all matchups for analysis
// ==============================================================================

#pragma pack(push, 1)
struct MatchupSample {
    // Unit IDs (6 bytes total) - supports up to 16M units
    u32 unit_a_id : 24;
    u32 unit_b_id_low : 8;      // Lower 8 bits of unit_b_id
    u16 unit_b_id_high;         // Upper 16 bits of unit_b_id

    // Outcome flags (2 bytes packed)
    // Byte 1: winner(2) | games_won_a(2) | games_won_b(2) | closeness_high(2)
    // Byte 2: closeness_low(2) | victory_type(3) | upset_flag(1) | game1_winner(2)
    u8 outcome1;
    u8 outcome2;

    // Per-game winners packed (1 byte)
    // game2_winner(2) | game3_winner(2) | variance_flag(2) | reserved(2)
    u8 game_outcomes;

    // Combat stats (5 bytes)
    u8 wounds_a_scaled;         // Total wounds by A, scaled /4 (max 1020)
    u8 wounds_b_scaled;         // Total wounds by B, scaled /4
    u8 kills_a;                 // Models killed by A
    u8 kills_b;                 // Models killed by B

    // Objective control (1 byte)
    // rounds_ctrl_a(4) | rounds_ctrl_b(4)
    u8 objective_ctrl;

    // Special rules triggered (2 bytes) - bitfield
    u16 special_rules_triggered;

    // === Accessors ===

    u32 get_unit_b_id() const {
        return static_cast<u32>(unit_b_id_low) | (static_cast<u32>(unit_b_id_high) << 8);
    }

    void set_unit_b_id(u32 id) {
        unit_b_id_low = id & 0xFF;
        unit_b_id_high = (id >> 8) & 0xFFFF;
    }

    // Winner: 0=A, 1=B, 2=Draw
    u8 winner() const { return outcome1 & 0x03; }
    void set_winner(u8 w) { outcome1 = (outcome1 & 0xFC) | (w & 0x03); }

    u8 games_won_a() const { return (outcome1 >> 2) & 0x03; }
    void set_games_won_a(u8 g) { outcome1 = (outcome1 & 0xF3) | ((g & 0x03) << 2); }

    u8 games_won_b() const { return (outcome1 >> 4) & 0x03; }
    void set_games_won_b(u8 g) { outcome1 = (outcome1 & 0xCF) | ((g & 0x03) << 4); }

    // Closeness: 0-15 scale (0=dominant, 15=razor thin)
    u8 closeness() const {
        return ((outcome1 >> 6) & 0x03) | (((outcome2 & 0x03) << 2));
    }
    void set_closeness(u8 c) {
        outcome1 = (outcome1 & 0x3F) | ((c & 0x03) << 6);
        outcome2 = (outcome2 & 0xFC) | ((c >> 2) & 0x03);
    }

    // Victory type: 0=objective, 1=tabled_a, 2=tabled_b, 3=attrition, 4=timeout
    u8 victory_type() const { return (outcome2 >> 2) & 0x07; }
    void set_victory_type(u8 v) { outcome2 = (outcome2 & 0xE3) | ((v & 0x07) << 2); }

    // Upset flag: 1 if lower-cost/ELO unit won
    bool upset_flag() const { return (outcome2 >> 5) & 0x01; }
    void set_upset_flag(bool u) { outcome2 = (outcome2 & 0xDF) | ((u ? 1 : 0) << 5); }

    // Per-game winners (0=A, 1=B, 2=Draw, 3=not played)
    u8 game1_winner() const { return (outcome2 >> 6) & 0x03; }
    void set_game1_winner(u8 w) { outcome2 = (outcome2 & 0x3F) | ((w & 0x03) << 6); }

    u8 game2_winner() const { return game_outcomes & 0x03; }
    void set_game2_winner(u8 w) { game_outcomes = (game_outcomes & 0xFC) | (w & 0x03); }

    u8 game3_winner() const { return (game_outcomes >> 2) & 0x03; }
    void set_game3_winner(u8 w) { game_outcomes = (game_outcomes & 0xF3) | ((w & 0x03) << 2); }

    // Variance flag: 0=consistent, 1=moderate, 2=high, 3=extreme
    u8 variance_flag() const { return (game_outcomes >> 4) & 0x03; }
    void set_variance_flag(u8 v) { game_outcomes = (game_outcomes & 0xCF) | ((v & 0x03) << 4); }

    // Objective control
    u8 rounds_ctrl_a() const { return objective_ctrl & 0x0F; }
    void set_rounds_ctrl_a(u8 r) { objective_ctrl = (objective_ctrl & 0xF0) | (r & 0x0F); }

    u8 rounds_ctrl_b() const { return (objective_ctrl >> 4) & 0x0F; }
    void set_rounds_ctrl_b(u8 r) { objective_ctrl = (objective_ctrl & 0x0F) | ((r & 0x0F) << 4); }

    // Get approximate wounds (multiply by 4)
    u16 wounds_dealt_a() const { return static_cast<u16>(wounds_a_scaled) * 4; }
    u16 wounds_dealt_b() const { return static_cast<u16>(wounds_b_scaled) * 4; }

    // === Factory Method ===

    static MatchupSample from_match(
        const MatchResult& result,
        u32 a_idx,
        u32 b_idx,
        u16 points_a,
        u16 points_b,
        const u8 game_winners[3],  // Per-game winners
        u8 games_played
    ) {
        MatchupSample sample;
        std::memset(&sample, 0, sizeof(sample));

        // Set IDs
        sample.unit_a_id = a_idx & 0xFFFFFF;
        sample.set_unit_b_id(b_idx);

        // Set outcome
        sample.set_winner(static_cast<u8>(result.overall_winner));
        sample.set_games_won_a(result.games_won_a);
        sample.set_games_won_b(result.games_won_b);

        // Calculate closeness (0-15)
        // Based on game score difference and wound differential
        u8 game_diff = (result.games_won_a > result.games_won_b) ?
                       (result.games_won_a - result.games_won_b) :
                       (result.games_won_b - result.games_won_a);
        i32 wound_diff = static_cast<i32>(result.total_wounds_dealt_a) -
                         static_cast<i32>(result.total_wounds_dealt_b);
        u32 wound_ratio = (result.total_wounds_dealt_a + result.total_wounds_dealt_b > 0) ?
            (std::abs(wound_diff) * 15) / (result.total_wounds_dealt_a + result.total_wounds_dealt_b + 1) : 0;

        u8 closeness = 15;  // Start at maximum closeness
        if (game_diff == 2) closeness -= 8;  // Dominant game score
        else if (game_diff == 1) closeness -= 3;
        closeness -= std::min(static_cast<u8>(wound_ratio), static_cast<u8>(7));
        sample.set_closeness(closeness);

        // Victory type
        u8 victory_type = 3;  // Default: attrition
        if (result.total_rounds_holding_a > result.total_rounds_holding_b ||
            result.total_rounds_holding_b > result.total_rounds_holding_a) {
            victory_type = 0;  // Objective
        }
        sample.set_victory_type(victory_type);

        // Upset flag (lower points unit won)
        bool is_upset = (result.overall_winner == GameWinner::UnitA && points_a < points_b) ||
                        (result.overall_winner == GameWinner::UnitB && points_b < points_a);
        sample.set_upset_flag(is_upset);

        // Per-game winners
        sample.set_game1_winner(games_played >= 1 ? game_winners[0] : 3);
        sample.set_game2_winner(games_played >= 2 ? game_winners[1] : 3);
        sample.set_game3_winner(games_played >= 3 ? game_winners[2] : 3);

        // Variance flag based on game-to-game consistency
        // If all games had same winner, low variance
        // If games were split, higher variance
        u8 variance = 0;
        if (games_played >= 2) {
            if (game_winners[0] != game_winners[1]) variance++;
            if (games_played >= 3 && game_winners[1] != game_winners[2]) variance++;
        }
        sample.set_variance_flag(variance);

        // Combat stats (scaled)
        sample.wounds_a_scaled = static_cast<u8>(std::min(result.total_wounds_dealt_a / 4, 255u));
        sample.wounds_b_scaled = static_cast<u8>(std::min(result.total_wounds_dealt_b / 4, 255u));
        sample.kills_a = static_cast<u8>(std::min(static_cast<u16>(result.total_models_killed_a), u16(255)));
        sample.kills_b = static_cast<u8>(std::min(static_cast<u16>(result.total_models_killed_b), u16(255)));

        // Objective control
        sample.set_rounds_ctrl_a(std::min(result.total_rounds_holding_a, u8(15)));
        sample.set_rounds_ctrl_b(std::min(result.total_rounds_holding_b, u8(15)));

        // Special rules - placeholder (would be populated from game data)
        sample.special_rules_triggered = 0;

        return sample;
    }
};
#pragma pack(pop)

static_assert(sizeof(MatchupSample) == 16, "MatchupSample must be 16 bytes");

// ==============================================================================
// Sample File Header
// ==============================================================================

struct SampleFileHeader {
    u32 magic;           // 0x534D504C ("SMPL")
    u32 version;         // 1
    f64 sample_rate;     // e.g., 0.003 for 0.3%
    u64 total_matchups;  // Total matchups in simulation
    u64 sampled_count;   // Number of samples in this file

    static constexpr u32 MAGIC = 0x534D504C;  // "SMPL"
    static constexpr u32 VERSION = 1;

    SampleFileHeader() : magic(MAGIC), version(VERSION), sample_rate(0.0),
                         total_matchups(0), sampled_count(0) {}
};

static_assert(sizeof(SampleFileHeader) == 32, "SampleFileHeader must be 32 bytes");

} // namespace battle
