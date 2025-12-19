#pragma once

#include "core/types.hpp"
#include "engine/game_state.hpp"
#include <cstring>
#include <array>
#include <limits>

namespace battle {

// ==============================================================================
// Showcase Replay - Full replay data for one "best" match per unit
// Used for Tier 3 data storage: ~300 bytes per unit for narrative content
// ==============================================================================

// Selection strategy for choosing which match to save as showcase
enum class ShowcaseStrategy : u8 {
    BiggestUpset = 0,       // Largest ELO/points differential win
    ClosestWin = 1,         // Tightest margin victory
    HighestEloDefeated = 2, // Beat the strongest opponent by ELO
    MostDramatic = 3        // Combination: close + upset
};

// ==============================================================================
// Per-Round Snapshot (~20 bytes)
// ==============================================================================

#pragma pack(push, 1)
struct RoundSnapshot {
    // Unit A state (5 bytes)
    u8 models_remaining_a;      // Models alive
    u8 wounds_on_leader_a;      // Wounds on multi-wound model
    u8 morale_state_a;          // 0=steady, 1=shaken, 2=routed, 3=destroyed
    i8 position_a;              // Distance from center (-12 to +12)
    u8 action_a;                // Action taken this round

    // Unit B state (5 bytes)
    u8 models_remaining_b;
    u8 wounds_on_leader_b;
    u8 morale_state_b;
    i8 position_b;
    u8 action_b;

    // Round events (6 bytes)
    u8 wounds_dealt_a;          // Wounds dealt by A this round
    u8 wounds_dealt_b;          // Wounds dealt by B this round
    u8 kills_a;                 // Models killed by A this round
    u8 kills_b;                 // Models killed by B this round
    u8 objective_holder;        // 0=none, 1=A, 2=B, 3=contested
    u8 critical_events;         // Bitfield: charge, rout_check, regen, etc.

    // Special rule triggers (2 bytes)
    u16 special_triggers;       // Bitfield for rules that activated

    // Reserved (2 bytes)
    u16 reserved;

    RoundSnapshot() { std::memset(this, 0, sizeof(*this)); }
};
#pragma pack(pop)

static_assert(sizeof(RoundSnapshot) == 20, "RoundSnapshot must be 20 bytes");

// Critical event flags for RoundSnapshot::critical_events
namespace CriticalEvents {
    constexpr u8 CHARGE_A = 0x01;           // Unit A charged
    constexpr u8 CHARGE_B = 0x02;           // Unit B charged
    constexpr u8 ROUT_CHECK_A = 0x04;       // Unit A took rout check
    constexpr u8 ROUT_CHECK_B = 0x08;       // Unit B took rout check
    constexpr u8 REGENERATION = 0x10;       // Regeneration activated
    constexpr u8 FEARLESS_SAVE = 0x20;      // Fearless prevented rout
    constexpr u8 TOUGH_SAVE = 0x40;         // Tough prevented kill
    constexpr u8 COUNTER_STRIKE = 0x80;     // Counter rule activated
}

// ==============================================================================
// Per-Game Replay (~88 bytes)
// ==============================================================================

#pragma pack(push, 1)
struct GameReplay {
    // Game outcome (4 bytes)
    u8 winner;                  // 0=A, 1=B, 2=Draw
    u8 rounds_played;           // 1-4
    u8 ending_type;             // 0=objective, 1=tabled_a, 2=tabled_b, 3=rout_a, 4=rout_b
    u8 reserved;

    // Per-round snapshots (80 bytes)
    RoundSnapshot rounds[4];    // MAX_ROUNDS = 4

    GameReplay() : winner(0), rounds_played(0), ending_type(0), reserved(0), rounds{} {}

    // Factory method from GameResult and state history
    static GameReplay from_game(
        const GameResult& result,
        const RoundSnapshot* round_snapshots,
        u8 num_rounds
    ) {
        GameReplay replay;
        replay.winner = static_cast<u8>(result.winner);
        replay.rounds_played = num_rounds;

        // Determine ending type
        if (result.a_destroyed) {
            replay.ending_type = 1;
        } else if (result.b_destroyed) {
            replay.ending_type = 2;
        } else if (result.a_routed) {
            replay.ending_type = 3;
        } else if (result.b_routed) {
            replay.ending_type = 4;
        } else {
            replay.ending_type = 0;  // Objective/timeout
        }

        // Copy round snapshots
        for (u8 r = 0; r < num_rounds && r < 4; ++r) {
            replay.rounds[r] = round_snapshots[r];
        }

        return replay;
    }
};
#pragma pack(pop)

static_assert(sizeof(GameReplay) == 84, "GameReplay must be 84 bytes");

// ==============================================================================
// Showcase Replay - Complete replay data for one unit's "best" match
// ==============================================================================

#pragma pack(push, 1)
struct ShowcaseReplay {
    // Header (24 bytes)
    u32 unit_id;                // The unit this showcase is for
    u32 opponent_id;            // The opponent unit ID
    u16 unit_points;            // Unit's point cost
    u16 opponent_points;        // Opponent's point cost
    u16 unit_elo;               // Unit's ELO rating (if computed)
    u16 opponent_elo;           // Opponent's ELO rating
    i16 elo_differential;       // opponent_elo - unit_elo (positive = upset)
    u8 selection_reason;        // ShowcaseStrategy value
    u8 match_winner;            // 0=unit won, 1=unit lost (shouldn't happen), 2=draw
    u8 games_won_unit;          // Games won by the showcase unit
    u8 games_won_opponent;      // Games won by opponent
    u8 games_played;            // Total games in match (2 or 3)
    u8 reserved_header;

    // Match summary stats (8 bytes)
    u16 total_wounds_dealt;     // By the unit
    u16 total_wounds_received;  // By the unit
    u8 total_kills;             // By the unit
    u8 total_deaths;            // Of the unit's models
    u8 objective_rounds;        // Rounds unit held objective
    u8 reserved_stats;

    // Full game replays (252 bytes for 3 games)
    GameReplay games[3];

    ShowcaseReplay() : unit_id(0), opponent_id(0), unit_points(0), opponent_points(0),
                       unit_elo(0), opponent_elo(0), elo_differential(0),
                       selection_reason(0), match_winner(0), games_won_unit(0),
                       games_won_opponent(0), games_played(0), reserved_header(0),
                       total_wounds_dealt(0), total_wounds_received(0),
                       total_kills(0), total_deaths(0), objective_rounds(0),
                       reserved_stats(0), games{} {}

    // Calculate showcase score based on strategy
    i32 score(ShowcaseStrategy strategy) const {
        // Only wins should be considered (match_winner == 0 means unit won)
        if (match_winner != 0) return std::numeric_limits<i32>::min();

        switch (strategy) {
            case ShowcaseStrategy::BiggestUpset:
                // Maximize ELO differential for wins
                return static_cast<i32>(elo_differential) * 100;

            case ShowcaseStrategy::ClosestWin:
                // Minimize game differential, prefer more games played
                {
                    i32 game_diff = std::abs(games_won_unit - games_won_opponent);
                    i32 wound_diff = std::abs(static_cast<i32>(total_wounds_dealt) -
                                              static_cast<i32>(total_wounds_received));
                    return (3 - game_diff) * 1000 + games_played * 100 - wound_diff;
                }

            case ShowcaseStrategy::HighestEloDefeated:
                // Maximize opponent ELO
                return static_cast<i32>(opponent_elo) * 100;

            case ShowcaseStrategy::MostDramatic:
                // Combination: close game + upset
                {
                    i32 game_diff = std::abs(games_won_unit - games_won_opponent);
                    i32 closeness_score = (3 - game_diff) * 500 + games_played * 100;
                    i32 upset_score = elo_differential * 50;
                    return closeness_score + upset_score;
                }
        }
        return 0;
    }

    // Check if this replay is better than another for the given strategy
    bool is_better_than(const ShowcaseReplay& other, ShowcaseStrategy strategy) const {
        return score(strategy) > other.score(strategy);
    }
};
#pragma pack(pop)

static_assert(sizeof(ShowcaseReplay) == 284, "ShowcaseReplay should be ~284 bytes");

// ==============================================================================
// Showcase File Header and Index
// ==============================================================================

struct ShowcaseFileHeader {
    u32 magic;              // 0x53484F57 ("SHOW")
    u32 version;            // 1
    u32 unit_count;         // Number of units with showcases
    u8 strategy;            // ShowcaseStrategy used
    u8 reserved[19];        // Padding to 32 bytes

    static constexpr u32 MAGIC = 0x53484F57;  // "SHOW"
    static constexpr u32 VERSION = 1;

    ShowcaseFileHeader() : magic(MAGIC), version(VERSION), unit_count(0), strategy(0) {
        std::memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(ShowcaseFileHeader) == 32, "ShowcaseFileHeader must be 32 bytes");

// Index entry for random access (8 bytes per unit)
struct ShowcaseIndexEntry {
    u32 unit_id;
    u32 offset;             // Byte offset into data section

    ShowcaseIndexEntry() : unit_id(0), offset(0) {}
    ShowcaseIndexEntry(u32 id, u32 off) : unit_id(id), offset(off) {}
};

static_assert(sizeof(ShowcaseIndexEntry) == 8, "ShowcaseIndexEntry must be 8 bytes");

// ==============================================================================
// Showcase Candidate - Used during simulation to track best showcase per unit
// ==============================================================================

struct ShowcaseCandidate {
    u32 opponent_id;
    u16 opponent_points;
    i16 elo_differential;
    i32 cached_score;           // Cached score for quick comparison
    bool has_replay;            // Whether we have replay data

    // The actual replay data (only stored if this is the current best)
    ShowcaseReplay replay;

    ShowcaseCandidate()
        : opponent_id(0), opponent_points(0), elo_differential(0),
          cached_score(std::numeric_limits<i32>::min()), has_replay(false) {}

    // Update score cache
    void update_score(ShowcaseStrategy strategy) {
        if (has_replay) {
            cached_score = replay.score(strategy);
        }
    }

    // Check if a new result would be better
    bool should_replace(i16 new_elo_diff, u8 new_winner, ShowcaseStrategy strategy) const {
        if (!has_replay) return true;  // Always replace if we have nothing

        // Quick reject: if unit didn't win, don't consider
        if (new_winner != 0) return false;

        // Estimate new score based on strategy
        i32 estimated_score = 0;
        switch (strategy) {
            case ShowcaseStrategy::BiggestUpset:
                estimated_score = static_cast<i32>(new_elo_diff) * 100;
                break;
            case ShowcaseStrategy::HighestEloDefeated:
                // Can't estimate without opponent ELO
                return true;  // Always capture to be safe
            default:
                // For other strategies, we need full data to compare
                return true;
        }

        return estimated_score > cached_score;
    }
};

} // namespace battle
