#pragma once

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace battle {

// ==============================================================================
// Fundamental Types - Optimized for cache efficiency
// ==============================================================================

// Use fixed-width integers for predictable memory layout
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;

// Index types for referencing into arrays
using UnitIndex   = u16;
using ModelIndex  = u16;
using WeaponIndex = u16;
using RuleIndex   = u16;

// Maximum counts (compile-time constants)
constexpr size_t MAX_MODELS_PER_UNIT   = 32;
constexpr size_t MAX_WEAPONS_PER_MODEL = 8;
constexpr size_t MAX_RULES_PER_ENTITY  = 16;
constexpr size_t MAX_UNITS_PER_ARMY    = 64;
constexpr size_t MAX_NAME_LENGTH       = 64;

// ==============================================================================
// Enumerations
// ==============================================================================

enum class ModelState : u8 {
    Healthy = 0,
    Wounded = 1,
    Dead    = 2
};

enum class UnitStatus : u8 {
    Normal  = 0,
    Shaken  = 1,
    Routed  = 2
};

enum class CombatPhase : u8 {
    Shooting = 0,
    Melee    = 1
};

enum class ScenarioType : u8 {
    ShootingOnly    = 0,
    MutualShooting  = 1,
    Charge          = 2,
    ReceiveCharge   = 3,
    ShootThenCharge = 4,
    Approach1Turn   = 5,
    Approach2Turns  = 6,
    FullEngagement  = 7,
    FightingRetreat = 8,

    COUNT // Number of scenarios
};

enum class BattleWinner : u8 {
    Attacker = 0,
    Defender = 1,
    Draw     = 2
};

enum class VictoryCondition : u8 {
    AttackerDestroyedEnemy = 0,
    DefenderDestroyedEnemy = 1,
    AttackerRoutedEnemy    = 2,
    DefenderRoutedEnemy    = 3,
    AttackerRouted         = 4,
    DefenderRouted         = 5,
    MaxRoundsAttackerAhead = 6,
    MaxRoundsDefenderAhead = 7,
    MaxRoundsDraw          = 8,
    MutualDestruction      = 9
};

// ==============================================================================
// Special Rule Identifiers (Compact representation)
// ==============================================================================

// Rules are identified by enum for fast comparison and switch statements
enum class RuleId : u8 {
    None = 0,

    // Weapon rules (affect attacks)
    AP,             // AP(X) - Armor Piercing
    Blast,          // Blast(X) - Multiply hits
    Deadly,         // Deadly(X) - Multiply wounds
    Lance,          // Lance - +2 AP on charge
    Poison,         // Poison - Reroll defense 6s
    Precise,        // Precise - +1 to hit
    Reliable,       // Reliable - Quality 2+
    Rending,        // Rending - 6s to hit get AP(4)
    Bane,           // Bane - Bypass regeneration
    Impact,         // Impact(X) - Extra attacks on charge
    Indirect,       // Indirect - Ignore cover
    Sniper,         // Sniper - Pick target model
    Lock_On,        // Lock-On - +1 to hit vs vehicles
    Purge,          // Purge - +1 to hit vs Tough(3+)

    // Defense rules
    Regeneration,   // Regeneration - 5+ to ignore wound
    Tough,          // Tough(X) - Multiple wounds to kill
    Protected,      // Protected - 6+ to reduce AP
    Stealth,        // Stealth - -1 to be hit from >9"
    ShieldWall,     // Shield Wall - +1 Defense in melee

    // Unit rules
    Fearless,       // Fearless - Reroll failed morale
    Furious,        // Furious - Extra hits on 6s when charging
    Hero,           // Hero - Takes wounds last
    Relentless,     // Relentless - Extra hits on 6s shooting >9"
    Fear,           // Fear(X) - Counts as +X wounds in melee
    Counter,        // Counter - Strikes first when charged
    Fast,           // Fast - 9" move instead of 6"
    Flying,         // Flying - Can fly over terrain/units
    Strider,        // Strider - Ignore difficult terrain
    Scout,          // Scout - Deploy 12" ahead
    Ambush,         // Ambush - Can deploy anywhere >9" from enemy
    Devout,         // Devout - Faction rule
    PiercingAssault,// Piercing Assault - AP(1) on melee in charge
    Unstoppable,    // Unstoppable - Ignore regen and negative modifiers
    Casting,        // Casting(X) - Can cast X spells
    Slow,           // Slow - 4" move instead of 6"
    Surge,          // Surge - 6s to hit deal 1 extra hit
    Thrust,         // Thrust - +1 to hit and AP(+1) when charging
    Takedown,       // Takedown - Pick target model, resolve as unit of 1
    Limited,        // Limited - Weapon may only be used once per game

    COUNT // Number of rules
};

// Rule presence bitset - fast O(1) lookup for has_rule()
// Each bit corresponds to a RuleId (fits in 64 bits since COUNT < 64)
using RuleMask = u64;

inline constexpr RuleMask rule_bit(RuleId id) {
    return (id == RuleId::None) ? 0 : (RuleMask(1) << static_cast<u8>(id));
}

static_assert(static_cast<int>(RuleId::COUNT) <= 64, "RuleMask requires COUNT <= 64");

// AI behavior type for Solo Play rules
enum class AIType : u8 {
    Melee = 0,      // No ranged weapons - charges aggressively
    Shooting = 1,   // Ranged > melee - maintains distance
    Hybrid = 2      // Melee >= ranged - opportunistic
};

// ==============================================================================
// Compact Rule Representation
// ==============================================================================

// A special rule with its value, packed into 2 bytes
struct alignas(2) CompactRule {
    RuleId id    : 8;  // Rule identifier
    u8     value : 8;  // Rule value (e.g., 3 for Blast(3), 0 if no value)

    constexpr CompactRule() : id(RuleId::None), value(0) {}
    constexpr CompactRule(RuleId r) : id(r), value(0) {}
    constexpr CompactRule(RuleId r, u8 v) : id(r), value(v) {}

    constexpr bool is_valid() const { return id != RuleId::None; }
    constexpr bool operator==(const CompactRule& other) const {
        return id == other.id && value == other.value;
    }
};

static_assert(sizeof(CompactRule) == 2, "CompactRule must be 2 bytes");

// ==============================================================================
// Fixed-Size String (avoids heap allocation)
// ==============================================================================

template<size_t N>
struct FixedString {
    std::array<char, N> data{};
    u8 length = 0;

    FixedString() = default;

    FixedString(std::string_view sv) {
        length = static_cast<u8>(std::min(sv.size(), N - 1));
        std::copy_n(sv.begin(), length, data.begin());
        data[length] = '\0';
    }

    std::string_view view() const { return {data.data(), length}; }
    const char* c_str() const { return data.data(); }
    bool empty() const { return length == 0; }
};

using Name = FixedString<MAX_NAME_LENGTH>;

} // namespace battle
