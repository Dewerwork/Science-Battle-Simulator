#pragma once

#include "core/types.hpp"
#include <array>
#include <vector>
#include <thread>
#include <functional>
#include <algorithm>
#include <cstring>

namespace battle {

// ==============================================================================
// High-Performance Dice Roller
// Using xoshiro256++ PRNG - fast and high quality
// ==============================================================================

class DiceRoller {
public:
    // Constructor with optional seed
    explicit DiceRoller(u64 seed = 0) {
        if (seed == 0) {
            // Use a good default seed based on compile-time and runtime entropy
            seed = 0x853c49e6748fea9bULL;
        }
        seed_ = seed;
        init_state(seed);
    }

    // Seed the generator
    void seed(u64 s) { seed_ = s; init_state(s); }

    // Get the seed used for this roller (for reproducibility)
    u64 get_seed() const { return seed_; }

    // =========================================================================
    // Roll Recording (for debug mode)
    // When enabled, individual dice values are stored for logging
    // =========================================================================

    void enable_roll_recording(bool enable) {
        record_rolls_ = enable;
        if (enable) {
            recorded_rolls_.clear();
            recorded_rolls_.reserve(256);
        }
    }

    bool is_recording() const { return record_rolls_; }

    // Get and clear recorded rolls
    std::vector<u8> take_recorded_rolls() {
        std::vector<u8> result = std::move(recorded_rolls_);
        recorded_rolls_.clear();
        return result;
    }

    // Peek at recorded rolls without clearing
    const std::vector<u8>& peek_recorded_rolls() const {
        return recorded_rolls_;
    }

    // Roll a single D6 (1-6) using fast Lemire reduction
    u8 roll_d6() {
        // (byte * 6) >> 8 gives uniform 0-5, +1 for 1-6
        u8 result = static_cast<u8>(((next() & 0xFF) * 6) >> 8) + 1;
        if (record_rolls_) [[unlikely]] {
            recorded_rolls_.push_back(result);
        }
        return result;
    }

    // Roll multiple D6 into an array
    template<size_t N>
    void roll_d6_array(std::array<u8, N>& results) {
        for (size_t i = 0; i < N; ++i) {
            results[i] = roll_d6();
        }
    }

    // Roll and count successes against a target (optimized hot path)
    u32 roll_d6_target(u32 count, u8 target) {
        if (count == 0) return 0;

        // Recording path: use roll_d6() to capture each die
        if (record_rolls_) [[unlikely]] {
            u32 successes = 0;
            for (u32 i = 0; i < count; ++i) {
                successes += (roll_d6() >= target);
            }
            return successes;
        }

        u32 successes = 0;

        // Process 8 dice at a time for better instruction-level parallelism
        while (count >= 8) {
            u64 r = next();
            // Extract 8 dice values from 64 bits using fast Lemire reduction
            // (byte * 6) >> 8 gives uniform 0-5, then +1 for 1-6
            // Branchless comparison: (die >= target) evaluates to 0 or 1
            successes += (static_cast<u8>((((r      ) & 0xFF) * 6) >> 8) + 1 >= target);
            successes += (static_cast<u8>((((r >>  8) & 0xFF) * 6) >> 8) + 1 >= target);
            successes += (static_cast<u8>((((r >> 16) & 0xFF) * 6) >> 8) + 1 >= target);
            successes += (static_cast<u8>((((r >> 24) & 0xFF) * 6) >> 8) + 1 >= target);
            successes += (static_cast<u8>((((r >> 32) & 0xFF) * 6) >> 8) + 1 >= target);
            successes += (static_cast<u8>((((r >> 40) & 0xFF) * 6) >> 8) + 1 >= target);
            successes += (static_cast<u8>((((r >> 48) & 0xFF) * 6) >> 8) + 1 >= target);
            successes += (static_cast<u8>((((r >> 56) & 0xFF) * 6) >> 8) + 1 >= target);
            count -= 8;
        }

        // Handle remaining dice
        while (count-- > 0) {
            u64 r = next();
            u8 die = static_cast<u8>(((r & 0xFF) * 6) >> 8) + 1;
            successes += (die >= target);
        }

        return successes;
    }

    // Roll quality test (hits on quality+ with modifier)
    // Returns (successes, sixes_count, fives_count) for Furious/Rending/ClanWarriorBoost
    struct QualityResult {
        u32 hits;
        u32 sixes;
        u32 fives;
    };

    // Defense test result (for Shred tracking)
    struct DefenseResult {
        u32 wounds;     // Failed saves
        u32 ones;       // Unmodified 1s rolled (for Shred)
    };

    QualityResult roll_quality_test(u32 attacks, u8 quality, i8 modifier = 0) {
        if (attacks == 0) return {0, 0, 0};

        // Calculate effective target (clamped 2-6)
        i8 effective = static_cast<i8>(quality) - modifier;
        effective = std::max(i8(2), std::min(i8(6), effective));
        u8 eff_target = static_cast<u8>(effective);

        // Recording path: use roll_d6() to capture each die
        if (record_rolls_) [[unlikely]] {
            u32 hits = 0;
            u32 sixes = 0;
            u32 fives = 0;
            for (u32 i = 0; i < attacks; ++i) {
                u8 die = roll_d6();
                hits += (die >= eff_target);
                sixes += (die == 6);
                fives += (die == 5);
            }
            return {hits, sixes, fives};
        }

        u32 hits = 0;
        u32 sixes = 0;
        u32 fives = 0;
        u32 remaining = attacks;

        // Process 8 dice at a time using Lemire's fast reduction and branchless counting
        while (remaining >= 8) {
            u64 r = next();
            // Fast Lemire reduction: (byte * 6) >> 8 gives uniform 0-5, +1 for 1-6
            u8 d0 = static_cast<u8>((((r      ) & 0xFF) * 6) >> 8) + 1;
            u8 d1 = static_cast<u8>((((r >>  8) & 0xFF) * 6) >> 8) + 1;
            u8 d2 = static_cast<u8>((((r >> 16) & 0xFF) * 6) >> 8) + 1;
            u8 d3 = static_cast<u8>((((r >> 24) & 0xFF) * 6) >> 8) + 1;
            u8 d4 = static_cast<u8>((((r >> 32) & 0xFF) * 6) >> 8) + 1;
            u8 d5 = static_cast<u8>((((r >> 40) & 0xFF) * 6) >> 8) + 1;
            u8 d6 = static_cast<u8>((((r >> 48) & 0xFF) * 6) >> 8) + 1;
            u8 d7 = static_cast<u8>((((r >> 56) & 0xFF) * 6) >> 8) + 1;
            // Branchless hit counting
            hits += (d0 >= eff_target) + (d1 >= eff_target) + (d2 >= eff_target) + (d3 >= eff_target)
                  + (d4 >= eff_target) + (d5 >= eff_target) + (d6 >= eff_target) + (d7 >= eff_target);
            // Branchless six counting
            sixes += (d0 == 6) + (d1 == 6) + (d2 == 6) + (d3 == 6)
                   + (d4 == 6) + (d5 == 6) + (d6 == 6) + (d7 == 6);
            // Branchless five counting
            fives += (d0 == 5) + (d1 == 5) + (d2 == 5) + (d3 == 5)
                   + (d4 == 5) + (d5 == 5) + (d6 == 5) + (d7 == 5);
            remaining -= 8;
        }

        // Handle remaining dice
        while (remaining-- > 0) {
            u64 r = next();
            u8 die = static_cast<u8>(((r & 0xFF) * 6) >> 8) + 1;
            hits += (die >= eff_target);
            sixes += (die == 6);
            fives += (die == 5);
        }

        return {hits, sixes, fives};
    }

    // Roll defense test
    // Returns wounds (failed saves)
    u32 roll_defense_test(u32 hits, u8 defense, u8 ap, i8 modifier = 0, bool reroll_sixes = false) {
        if (hits == 0) return 0;

        // AP increases the target number needed
        i8 effective = static_cast<i8>(defense) + static_cast<i8>(ap) - modifier;
        effective = std::max(i8(2), std::min(i8(6), effective));
        u8 eff_target = static_cast<u8>(effective);

        // Recording path: use roll_d6() to capture each die
        if (record_rolls_) [[unlikely]] {
            u32 saves = 0;
            u32 sixes_to_reroll = 0;
            for (u32 i = 0; i < hits; ++i) {
                u8 die = roll_d6();
                if (reroll_sixes && die == 6) {
                    sixes_to_reroll++;
                } else {
                    saves += (die >= eff_target);
                }
            }
            // Reroll sixes (these also get recorded via roll_d6)
            for (u32 i = 0; i < sixes_to_reroll; ++i) {
                saves += (roll_d6() >= eff_target);
            }
            return hits - saves;
        }

        u32 saves = 0;
        u32 remaining = hits;

        // Fast path: no poison rerolls needed (most common case)
        if (!reroll_sixes) {
            // Process 8 dice at a time using Lemire's fast reduction
            while (remaining >= 8) {
                u64 r = next();
                u8 d0 = static_cast<u8>((((r      ) & 0xFF) * 6) >> 8) + 1;
                u8 d1 = static_cast<u8>((((r >>  8) & 0xFF) * 6) >> 8) + 1;
                u8 d2 = static_cast<u8>((((r >> 16) & 0xFF) * 6) >> 8) + 1;
                u8 d3 = static_cast<u8>((((r >> 24) & 0xFF) * 6) >> 8) + 1;
                u8 d4 = static_cast<u8>((((r >> 32) & 0xFF) * 6) >> 8) + 1;
                u8 d5 = static_cast<u8>((((r >> 40) & 0xFF) * 6) >> 8) + 1;
                u8 d6 = static_cast<u8>((((r >> 48) & 0xFF) * 6) >> 8) + 1;
                u8 d7 = static_cast<u8>((((r >> 56) & 0xFF) * 6) >> 8) + 1;
                // Branchless save counting
                saves += (d0 >= eff_target) + (d1 >= eff_target) + (d2 >= eff_target) + (d3 >= eff_target)
                       + (d4 >= eff_target) + (d5 >= eff_target) + (d6 >= eff_target) + (d7 >= eff_target);
                remaining -= 8;
            }
            // Handle remaining
            while (remaining-- > 0) {
                u64 r = next();
                u8 die = static_cast<u8>(((r & 0xFF) * 6) >> 8) + 1;
                saves += (die >= eff_target);
            }
        } else {
            // Poison path: must handle reroll of 6s (less common)
            u32 sixes_to_reroll = 0;

            // Process 8 dice at a time, track 6s
            while (remaining >= 8) {
                u64 r = next();
                u8 d0 = static_cast<u8>((((r      ) & 0xFF) * 6) >> 8) + 1;
                u8 d1 = static_cast<u8>((((r >>  8) & 0xFF) * 6) >> 8) + 1;
                u8 d2 = static_cast<u8>((((r >> 16) & 0xFF) * 6) >> 8) + 1;
                u8 d3 = static_cast<u8>((((r >> 24) & 0xFF) * 6) >> 8) + 1;
                u8 d4 = static_cast<u8>((((r >> 32) & 0xFF) * 6) >> 8) + 1;
                u8 d5 = static_cast<u8>((((r >> 40) & 0xFF) * 6) >> 8) + 1;
                u8 d6 = static_cast<u8>((((r >> 48) & 0xFF) * 6) >> 8) + 1;
                u8 d7 = static_cast<u8>((((r >> 56) & 0xFF) * 6) >> 8) + 1;
                // Count sixes separately for reroll
                sixes_to_reroll += (d0 == 6) + (d1 == 6) + (d2 == 6) + (d3 == 6)
                                 + (d4 == 6) + (d5 == 6) + (d6 == 6) + (d7 == 6);
                // Count non-six saves (branchless: save if >= target AND != 6)
                saves += (d0 >= eff_target && d0 != 6) + (d1 >= eff_target && d1 != 6)
                       + (d2 >= eff_target && d2 != 6) + (d3 >= eff_target && d3 != 6)
                       + (d4 >= eff_target && d4 != 6) + (d5 >= eff_target && d5 != 6)
                       + (d6 >= eff_target && d6 != 6) + (d7 >= eff_target && d7 != 6);
                remaining -= 8;
            }
            // Handle remaining
            while (remaining-- > 0) {
                u64 r = next();
                u8 die = static_cast<u8>(((r & 0xFF) * 6) >> 8) + 1;
                sixes_to_reroll += (die == 6);
                saves += (die >= eff_target && die != 6);
            }

            // Reroll the 6s
            saves += roll_d6_target(sixes_to_reroll, eff_target);
        }

        return hits - saves; // Failed saves = wounds
    }

    // Roll defense test with ones tracking (for Shred)
    // Returns wounds (failed saves) and count of unmodified 1s
    DefenseResult roll_defense_test_with_ones(u32 hits, u8 defense, u8 ap, i8 modifier = 0, bool reroll_sixes = false) {
        if (hits == 0) return {0, 0};

        // AP increases the target number needed
        i8 effective = static_cast<i8>(defense) + static_cast<i8>(ap) - modifier;
        effective = std::max(i8(2), std::min(i8(6), effective));
        u8 eff_target = static_cast<u8>(effective);

        u32 saves = 0;
        u32 ones = 0;
        u32 remaining = hits;

        // Recording path
        if (record_rolls_) [[unlikely]] {
            u32 sixes_to_reroll = 0;
            for (u32 i = 0; i < hits; ++i) {
                u8 die = roll_d6();
                ones += (die == 1);
                if (reroll_sixes && die == 6) {
                    sixes_to_reroll++;
                } else {
                    saves += (die >= eff_target);
                }
            }
            // Reroll sixes (these also get recorded)
            for (u32 i = 0; i < sixes_to_reroll; ++i) {
                u8 die = roll_d6();
                ones += (die == 1);
                saves += (die >= eff_target);
            }
            return {hits - saves, ones};
        }

        // Fast path: no poison rerolls
        if (!reroll_sixes) {
            while (remaining >= 8) {
                u64 r = next();
                u8 d0 = static_cast<u8>((((r      ) & 0xFF) * 6) >> 8) + 1;
                u8 d1 = static_cast<u8>((((r >>  8) & 0xFF) * 6) >> 8) + 1;
                u8 d2 = static_cast<u8>((((r >> 16) & 0xFF) * 6) >> 8) + 1;
                u8 d3 = static_cast<u8>((((r >> 24) & 0xFF) * 6) >> 8) + 1;
                u8 d4 = static_cast<u8>((((r >> 32) & 0xFF) * 6) >> 8) + 1;
                u8 d5 = static_cast<u8>((((r >> 40) & 0xFF) * 6) >> 8) + 1;
                u8 d6 = static_cast<u8>((((r >> 48) & 0xFF) * 6) >> 8) + 1;
                u8 d7 = static_cast<u8>((((r >> 56) & 0xFF) * 6) >> 8) + 1;
                saves += (d0 >= eff_target) + (d1 >= eff_target) + (d2 >= eff_target) + (d3 >= eff_target)
                       + (d4 >= eff_target) + (d5 >= eff_target) + (d6 >= eff_target) + (d7 >= eff_target);
                ones += (d0 == 1) + (d1 == 1) + (d2 == 1) + (d3 == 1)
                      + (d4 == 1) + (d5 == 1) + (d6 == 1) + (d7 == 1);
                remaining -= 8;
            }
            while (remaining-- > 0) {
                u64 r = next();
                u8 die = static_cast<u8>(((r & 0xFF) * 6) >> 8) + 1;
                saves += (die >= eff_target);
                ones += (die == 1);
            }
        } else {
            // Poison path with reroll tracking
            u32 sixes_to_reroll = 0;
            while (remaining >= 8) {
                u64 r = next();
                u8 d0 = static_cast<u8>((((r      ) & 0xFF) * 6) >> 8) + 1;
                u8 d1 = static_cast<u8>((((r >>  8) & 0xFF) * 6) >> 8) + 1;
                u8 d2 = static_cast<u8>((((r >> 16) & 0xFF) * 6) >> 8) + 1;
                u8 d3 = static_cast<u8>((((r >> 24) & 0xFF) * 6) >> 8) + 1;
                u8 d4 = static_cast<u8>((((r >> 32) & 0xFF) * 6) >> 8) + 1;
                u8 d5 = static_cast<u8>((((r >> 40) & 0xFF) * 6) >> 8) + 1;
                u8 d6 = static_cast<u8>((((r >> 48) & 0xFF) * 6) >> 8) + 1;
                u8 d7 = static_cast<u8>((((r >> 56) & 0xFF) * 6) >> 8) + 1;
                sixes_to_reroll += (d0 == 6) + (d1 == 6) + (d2 == 6) + (d3 == 6)
                                 + (d4 == 6) + (d5 == 6) + (d6 == 6) + (d7 == 6);
                saves += (d0 >= eff_target && d0 != 6) + (d1 >= eff_target && d1 != 6)
                       + (d2 >= eff_target && d2 != 6) + (d3 >= eff_target && d3 != 6)
                       + (d4 >= eff_target && d4 != 6) + (d5 >= eff_target && d5 != 6)
                       + (d6 >= eff_target && d6 != 6) + (d7 >= eff_target && d7 != 6);
                ones += (d0 == 1) + (d1 == 1) + (d2 == 1) + (d3 == 1)
                      + (d4 == 1) + (d5 == 1) + (d6 == 1) + (d7 == 1);
                remaining -= 8;
            }
            while (remaining-- > 0) {
                u64 r = next();
                u8 die = static_cast<u8>(((r & 0xFF) * 6) >> 8) + 1;
                sixes_to_reroll += (die == 6);
                saves += (die >= eff_target && die != 6);
                ones += (die == 1);
            }
            // Reroll sixes - count ones from rerolls too
            for (u32 i = 0; i < sixes_to_reroll; ++i) {
                u64 r = next();
                u8 die = static_cast<u8>(((r & 0xFF) * 6) >> 8) + 1;
                saves += (die >= eff_target);
                ones += (die == 1);
            }
        }

        return {hits - saves, ones};
    }

    // Roll regeneration (default 5+)
    u32 roll_regeneration(u32 wounds, u8 target = 5) {
        if (wounds == 0) return 0;
        u32 saved = roll_d6_target(wounds, target);
        return wounds - saved;
    }

    // Roll Impact attacks (hits on 2+)
    u32 roll_impact(u32 count) {
        if (count == 0) return 0;
        return roll_d6_target(count, 2);
    }

    // Generate raw 64-bit value (for custom use)
    u64 next() {
        const u64 result = rotl(state[0] + state[3], 23) + state[0];

        const u64 t = state[1] << 17;

        state[2] ^= state[0];
        state[3] ^= state[1];
        state[1] ^= state[2];
        state[0] ^= state[3];

        state[2] ^= t;
        state[3] = rotl(state[3], 45);

        return result;
    }

private:
    std::array<u64, 4> state;
    u64 seed_ = 0;
    bool record_rolls_ = false;
    std::vector<u8> recorded_rolls_;

    void init_state(u64 seed) {
        // Use splitmix64 to initialize state from seed
        u64 z = seed;
        for (int i = 0; i < 4; ++i) {
            z += 0x9e3779b97f4a7c15ULL;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            state[i] = z ^ (z >> 31);
        }
    }

    static u64 rotl(u64 x, int k) {
        return (x << k) | (x >> (64 - k));
    }
};

// ==============================================================================
// Thread-Local Dice Roller
// Each thread gets its own seeded instance
// ==============================================================================

inline DiceRoller& get_thread_dice() {
    thread_local DiceRoller roller(
        std::hash<std::thread::id>{}(std::this_thread::get_id()) ^
        static_cast<u64>(reinterpret_cast<uintptr_t>(&roller))
    );
    return roller;
}

// ==============================================================================
// Batch Dice Operations (for SIMD optimization in critical paths)
// ==============================================================================

struct BatchDiceResult {
    std::array<u8, 64> rolls;
    u32 count;
    u32 successes;
    u32 sixes;
};

// Roll up to 64 dice and analyze results (optimized with branchless counting)
inline BatchDiceResult batch_roll_d6(DiceRoller& roller, u32 count, u8 target) {
    BatchDiceResult result{};
    result.count = std::min(count, u32(64));
    result.successes = 0;
    result.sixes = 0;

    for (u32 i = 0; i < result.count; ++i) {
        u8 roll = roller.roll_d6();
        result.rolls[i] = roll;
        result.successes += (roll >= target);
        result.sixes += (roll == 6);
    }

    return result;
}

} // namespace battle
