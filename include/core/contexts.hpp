#pragma once

#include "core/types.hpp"
#include "core/phases.hpp"
#include <vector>
#include <optional>

namespace battle {

// Forward declarations
struct Unit;
struct Weapon;
class DiceRoller;
class MatchLogger;

// ==============================================================================
// Combat Context Core - Hot path data (must fit in cache line)
// ==============================================================================
// CRITICAL: This struct must be <= 64 bytes to fit in a single cache line.
// Contains only the most frequently accessed data during combat resolution.
//
// All data here is accessed multiple times per combat resolution.

struct CombatContextCore {
    // Participants (pointers for minimal size) - 24 bytes
    Unit* attacker = nullptr;           // 8 bytes
    Unit* defender = nullptr;           // 8 bytes
    Weapon* weapon = nullptr;           // 8 bytes

    // Combat parameters - 4 bytes
    CombatType combat_type = CombatType::SHOOTING;  // 1 byte
    u8 distance = 0;                    // 1 byte - Distance in inches
    bool is_charge = false;             // 1 byte
    u8 _pad1 = 0;                       // 1 byte padding

    // Hit modifiers - 4 bytes
    i8 hit_modifier = 0;                // 1 byte - Accumulated +/- to hit
    u8 quality_used = 4;                // 1 byte - Effective quality after overrides
    i8 defense_modifier = 0;            // 1 byte - Accumulated defense modifier
    i8 ap_modifier = 0;                 // 1 byte - Accumulated AP modifier

    // Hit tracking - 16 bytes
    u32 attacks = 0;                    // 4 bytes - Total attacks made
    u32 hits = 0;                       // 4 bytes - Hits after dice rolls
    u32 natural_sixes = 0;              // 4 bytes - Unmodified 6s rolled
    u32 natural_fives = 0;              // 4 bytes - Unmodified 5s rolled

    // Wound tracking - 8 bytes
    u32 wounds_to_allocate = 0;         // 4 bytes - Wounds pending allocation
    u32 wounds_allocated = 0;           // 4 bytes - Wounds actually dealt

    // Aggregated trait flags - 4 bytes
    // These are aggregated at combat start for fast checking
    u8 trait_flags = 0;                 // 1 byte - Combat-relevant traits
    u8 models_killed = 0;               // 1 byte - Models killed this attack
    u8 models_attacking = 0;            // 1 byte - Models participating in attack
    u8 _pad2 = 0;                       // 1 byte padding

    // Trait flag bits (for trait_flags field)
    static constexpr u8 BYPASS_REGEN = 0x01;    // Bypasses regeneration
    static constexpr u8 BYPASS_RESIST = 0x02;   // Bypasses resistance
    static constexpr u8 FORCE_REROLL = 0x04;    // Forces defense reroll (Poison/Bane)
    static constexpr u8 HAS_TAKEDOWN = 0x08;    // Takedown active
    static constexpr u8 IS_FATIGUED = 0x10;     // Attacker is fatigued/shaken

    // Padding to exactly 64 bytes - 4 bytes
    u8 _reserved[4] = {};

    // ==============================================================================
    // Trait flag helpers
    // ==============================================================================

    bool bypasses_regeneration() const { return trait_flags & BYPASS_REGEN; }
    bool bypasses_resistance() const { return trait_flags & BYPASS_RESIST; }
    bool forces_defense_reroll() const { return trait_flags & FORCE_REROLL; }
    bool has_takedown() const { return trait_flags & HAS_TAKEDOWN; }
    bool is_fatigued() const { return trait_flags & IS_FATIGUED; }

    void set_bypass_regen(bool v) { if (v) trait_flags |= BYPASS_REGEN; else trait_flags &= ~BYPASS_REGEN; }
    void set_bypass_resist(bool v) { if (v) trait_flags |= BYPASS_RESIST; else trait_flags &= ~BYPASS_RESIST; }
    void set_force_reroll(bool v) { if (v) trait_flags |= FORCE_REROLL; else trait_flags &= ~FORCE_REROLL; }
    void set_has_takedown(bool v) { if (v) trait_flags |= HAS_TAKEDOWN; else trait_flags &= ~HAS_TAKEDOWN; }
    void set_is_fatigued(bool v) { if (v) trait_flags |= IS_FATIGUED; else trait_flags &= ~IS_FATIGUED; }
};

// Compile-time size validation
static_assert(sizeof(CombatContextCore) <= 64, "CombatContextCore must fit in 64 bytes");

// ==============================================================================
// Combat Context Extended - Cold path data (allocated on demand)
// ==============================================================================
// Contains data only needed when specific rules are active.
// Lazily allocated to avoid overhead when not needed.

struct CombatContextExtended {
    // Separated hit types (for Rending, Rupture)
    u32 rending_hits = 0;               // Hits with AP+4 (from 6s to hit)
    u32 rupture_hits = 0;               // Hits that bypass regen and deal +1 wound
    u32 bonus_hits = 0;                 // Extra hits from Relentless, Surge, etc.

    // Quality override
    std::optional<u8> quality_override;  // From Reliable

    // Multipliers
    u8 hit_multiplier = 1;              // From Blast
    u8 wound_multiplier = 1;            // From Deadly

    // Takedown targeting
    i8 takedown_target_idx = -1;        // Specific model index, -1 = normal allocation

    // VersatileAttack state
    bool versatile_ap_chosen = false;   // True = AP+1, False = +1 hit

    // Shred tracking
    u32 defense_ones_rolled = 0;        // 1s on defense for Shred bonus wounds

    // Self-destruct tracking
    u32 self_destruct_hits = 0;         // Queued hits from dying models
    u8 self_destruct_value = 0;         // Value of SelfDestruct(X) rule

    // Additional rule state flags
    bool protected_active = false;       // Protected - 6+ to reduce AP
    bool resistance_active = false;      // Resistance - 6+ to ignore wound
    bool shred_active = false;           // Shred - extra wound on defense 1s
    bool takedown_active = false;        // Takedown - targeting active
    bool limited_weapon = false;         // Limited - one use weapon
    bool counter_active = false;         // Counter - strike first
    bool sniper_active = false;          // Sniper - pick target
    bool ignores_cover = false;          // Indirect - ignore cover
    bool predator_fighter_active = false; // PredatorFighter - recursive attacks
    bool hero_present = false;           // Hero - takes wounds last
    bool ignores_negative_hit_mods = false; // Unstoppable - ignore negative modifiers
    bool moved_this_activation = false;  // Did attacker move before shooting (for Indirect)
    bool knightborn_active = false;      // Knightborn - 6+ ignore wounds (4+ vs spells)
    bool plaguebound_active = false;     // Plaguebound - 6+ ignore wounds

    // Category F: Combat Choice & Retaliation
    u8 versatile_roll = 0;               // D6 roll for Unpredictable rules (1-3 AP, 4-6 hit)
    u32 retaliate_hits = 0;              // Accumulated hits from Retaliate
    u32 deathstrike_hits = 0;            // Accumulated hits from Deathstrike

    // Category G: Enhanced Combat Modifiers & Boost Rules
    bool ferocious_boost_active = false;   // Ferocious Boost - extra hits on 5-6
    bool warbound_boost_active = false;    // Warbound Boost - extra wound on 1-2
    bool plaguebound_boost_active = false; // Plaguebound Boost - 5-6 ignore wounds
    bool melee_slayer_active = false;      // Melee Slayer - AP+2 vs Tough(3+)

    // Rule values
    u8 tough_value = 0;                  // Tough(X) value
    u8 impact_attacks = 0;               // Impact(X) bonus attacks
    u8 smash_bonus_blast = 0;            // Smash bonus blast vs heavy armor
    u8 crack_ap_bonus = 0;               // Crack/Destructive AP bonus on 6s

    // Logging support
    MatchLogger* logger = nullptr;
    DiceRoller* dice = nullptr;

    // Detailed tracking for debugging/logging
    struct RuleApplication {
        RuleId rule;
        u8 value;
        const char* effect_description;
    };
    std::vector<RuleApplication> applied_rules;

    // Reset for reuse
    void reset() {
        rending_hits = 0;
        rupture_hits = 0;
        bonus_hits = 0;
        quality_override.reset();
        hit_multiplier = 1;
        wound_multiplier = 1;
        takedown_target_idx = -1;
        versatile_ap_chosen = false;
        defense_ones_rolled = 0;
        self_destruct_hits = 0;
        self_destruct_value = 0;
        protected_active = false;
        resistance_active = false;
        shred_active = false;
        takedown_active = false;
        limited_weapon = false;
        counter_active = false;
        sniper_active = false;
        ignores_cover = false;
        predator_fighter_active = false;
        hero_present = false;
        ignores_negative_hit_mods = false;
        moved_this_activation = false;
        knightborn_active = false;
        plaguebound_active = false;
        versatile_roll = 0;
        retaliate_hits = 0;
        deathstrike_hits = 0;
        ferocious_boost_active = false;
        warbound_boost_active = false;
        plaguebound_boost_active = false;
        melee_slayer_active = false;
        tough_value = 0;
        impact_attacks = 0;
        smash_bonus_blast = 0;
        crack_ap_bonus = 0;
        applied_rules.clear();
        // Note: logger and dice are not reset - they're set per-engine
    }
};

// ==============================================================================
// Full Combat Context - Combines core and extended
// ==============================================================================

struct CombatContext {
    CombatContextCore core;
    CombatContextExtended* extended = nullptr;

    // Get extended context, allocating if needed
    CombatContextExtended& ext() {
        if (!extended) {
            extended = new CombatContextExtended();
        }
        return *extended;
    }

    // Check if extended context exists
    bool has_extended() const { return extended != nullptr; }

    // Safe access to extended fields (returns default if not allocated)
    u32 rending_hits() const { return extended ? extended->rending_hits : 0; }
    u32 rupture_hits() const { return extended ? extended->rupture_hits : 0; }
    u32 bonus_hits() const { return extended ? extended->bonus_hits : 0; }

    // Destructor
    ~CombatContext() {
        delete extended;
    }

    // Move constructor
    CombatContext(CombatContext&& other) noexcept
        : core(other.core), extended(other.extended) {
        other.extended = nullptr;
    }

    // Move assignment
    CombatContext& operator=(CombatContext&& other) noexcept {
        if (this != &other) {
            core = other.core;
            delete extended;
            extended = other.extended;
            other.extended = nullptr;
        }
        return *this;
    }

    // Default constructor
    CombatContext() = default;

    // No copying (to avoid double-free of extended)
    CombatContext(const CombatContext&) = delete;
    CombatContext& operator=(const CombatContext&) = delete;
};

// ==============================================================================
// Movement Context
// ==============================================================================

struct MovementContext {
    Unit* unit = nullptr;

    // Move type
    enum class MoveType : u8 {
        ADVANCE,
        RUSH,
        CHARGE,
        RETREAT
    };
    MoveType move_type = MoveType::ADVANCE;

    // Distance calculation
    u8 base_distance = 6;               // Base movement (usually 6")
    i8 distance_modifier = 0;           // Accumulated modifiers
    u8 final_distance = 6;              // Computed final distance

    // Terrain handling
    u8 terrain_flags = 0;               // Which terrain types ignored
    static constexpr u8 IGNORE_DIFFICULT = 0x01;
    static constexpr u8 IGNORE_DANGEROUS = 0x02;
    static constexpr u8 IGNORE_IMPASSABLE = 0x04;
    static constexpr u8 FLY_OVER = 0x08;

    // Charge modifiers
    i8 charge_bonus = 0;                // From Agile, RapidCharge

    // State flags
    bool ignores_engagement = false;    // Can leave melee without penalty
    bool can_advance_and_charge = false;
    bool hit_and_run_pending = false;   // Post-combat move available
    bool swift_active = false;          // Swift - ignore Slow rule

    // Hit and Run distance
    u8 hit_and_run_distance = 0;        // Distance to move after combat
};

// ==============================================================================
// Deployment Context
// ==============================================================================

struct DeploymentContext {
    Unit* unit = nullptr;

    // Deployment zone
    enum class DeployZone : u8 {
        STANDARD,           // Normal deployment zone
        FORWARD,            // Scout deployment
        ANYWHERE            // Ambush deployment
    };
    DeployZone zone = DeployZone::STANDARD;

    // Deployment timing
    enum class DeployTiming : u8 {
        NORMAL,             // Standard deployment phase
        SCOUT_MOVE,         // After deployment, before first turn
        RESERVES            // Arrives during game
    };
    DeployTiming timing = DeployTiming::NORMAL;

    // Forward deployment distance (Scout)
    u8 forward_distance = 0;

    // Minimum enemy distance (Ambush)
    u8 min_enemy_distance = 0;
};

// ==============================================================================
// End Round Context
// ==============================================================================

struct EndRoundContext {
    u32 round_number = 0;

    // Units to process
    std::vector<Unit*> units;

    // Morale modifiers per unit
    struct MoraleState {
        Unit* unit;
        i8 modifier;
        bool reroll_available;
        bool auto_pass;
    };
    std::vector<MoraleState> morale_states;

    // Regeneration tracking
    struct HealState {
        Unit* unit;
        u8 models_healed;
        u8 wounds_healed;
    };
    std::vector<HealState> heal_states;
};

// ==============================================================================
// Context Pool - Avoid per-combat allocation
// ==============================================================================
// Pool allocator for extended contexts to avoid heap allocation during combat.

class CombatContextPool {
public:
    CombatContextExtended* acquire() {
        if (!free_list_.empty()) {
            auto* ctx = free_list_.back();
            free_list_.pop_back();
            ctx->reset();
            return ctx;
        }
        pool_.emplace_back();
        return &pool_.back();
    }

    void release(CombatContextExtended* ctx) {
        if (ctx) {
            free_list_.push_back(ctx);
        }
    }

    // Clear all (for end of simulation)
    void clear() {
        pool_.clear();
        free_list_.clear();
    }

private:
    std::vector<CombatContextExtended> pool_;
    std::vector<CombatContextExtended*> free_list_;
};

} // namespace battle
