#pragma once

#include "core/types.hpp"
#include <array>
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
        init_state(seed);
    }

    // Seed the generator
    void seed(u64 s) { init_state(s); }

    // Roll a single D6 (1-6)
    u8 roll_d6() {
        return static_cast<u8>((next() % 6) + 1);
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

        u32 successes = 0;

        // Process 8 dice at a time for better instruction-level parallelism
        while (count >= 8) {
            u64 r = next();
            // Extract 8 dice values from 64 bits (8 bits each, mod 6)
            for (int i = 0; i < 8; ++i) {
                u8 die = static_cast<u8>(((r >> (i * 8)) & 0xFF) % 6) + 1;
                if (die >= target) ++successes;
            }
            count -= 8;
        }

        // Handle remaining dice
        for (u32 i = 0; i < count; ++i) {
            if (roll_d6() >= target) ++successes;
        }

        return successes;
    }

    // Roll quality test (hits on quality+ with modifier)
    // Returns (successes, sixes_count) for Furious/Rending
    struct QualityResult {
        u32 hits;
        u32 sixes;
    };

    QualityResult roll_quality_test(u32 attacks, u8 quality, i8 modifier = 0) {
        if (attacks == 0) return {0, 0};

        // Calculate effective target (clamped 2-6)
        i8 effective = static_cast<i8>(quality) - modifier;
        effective = std::max(i8(2), std::min(i8(6), effective));

        u32 hits = 0;
        u32 sixes = 0;

        for (u32 i = 0; i < attacks; ++i) {
            u8 roll = roll_d6();
            if (roll >= effective) ++hits;
            if (roll == 6) ++sixes;
        }

        return {hits, sixes};
    }

    // Roll defense test
    // Returns wounds (failed saves)
    u32 roll_defense_test(u32 hits, u8 defense, u8 ap, i8 modifier = 0, bool reroll_sixes = false) {
        if (hits == 0) return 0;

        // AP increases the target number needed
        i8 effective = static_cast<i8>(defense) + static_cast<i8>(ap) - modifier;
        effective = std::max(i8(2), std::min(i8(6), effective));

        u32 saves = 0;

        for (u32 i = 0; i < hits; ++i) {
            u8 roll = roll_d6();

            // Poison: reroll 6s
            if (reroll_sixes && roll == 6) {
                roll = roll_d6();
            }

            if (roll >= effective) ++saves;
        }

        return hits - saves; // Failed saves = wounds
    }

    // Roll regeneration (default 5+)
    u32 roll_regeneration(u32 wounds, u8 target = 5) {
        if (wounds == 0) return 0;
        u32 saved = roll_d6_target(wounds, target);
        return wounds - saved;
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

// Roll up to 64 dice and analyze results
inline BatchDiceResult batch_roll_d6(DiceRoller& roller, u32 count, u8 target) {
    BatchDiceResult result{};
    result.count = std::min(count, u32(64));
    result.successes = 0;
    result.sixes = 0;

    for (u32 i = 0; i < result.count; ++i) {
        u8 roll = roller.roll_d6();
        result.rolls[i] = roll;
        if (roll >= target) ++result.successes;
        if (roll == 6) ++result.sixes;
    }

    return result;
}

} // namespace battle
