#pragma once

#include "core/types.hpp"
#include <array>
#include <string_view>

namespace battle {

// ==============================================================================
// Spell System - Implements Caster(X) rule mechanics
// ==============================================================================
// Caster(X): Gets X spell tokens at start of each round (max 6).
// Spend tokens equal to spell cost to attempt casting.
// Roll 1d6, on 4+ resolve the effect on target in line of sight.
// Enemy casters within 18" can spend tokens for -1 to the roll per token.

constexpr u8 MAX_SPELL_TOKENS = 6;
constexpr u8 MAX_SPELLS_PER_FACTION = 6;
constexpr u8 SPELL_CAST_TARGET = 4;  // Need 4+ to cast

// Spell target type
enum class SpellTargetType : u8 {
    Self = 0,           // Targets the caster's own unit
    EnemyDirect = 1,    // Direct damage to enemy unit
    EnemyDebuff = 2,    // Debuff/mark enemy unit
    FriendlyBuff = 3    // Buff friendly unit (same as Self in 1v1)
};

// Spell effect type (for logging and AI decisions)
enum class SpellEffectType : u8 {
    DirectDamage = 0,   // Deals hits to target
    Buff = 1,           // Grants a beneficial effect
    Debuff = 2,         // Applies negative effect to enemy
    Movement = 3,       // Affects movement
    Morale = 4          // Forces morale test
};

// Weapon rules that can be applied to spell hits
struct SpellHitRules {
    u8 ap = 0;              // AP value
    bool has_rupture = false;
    bool has_blast = false;
    u8 blast_value = 0;
    bool has_deadly = false;
    u8 deadly_value = 0;
    bool has_bane = false;
    bool has_shred = false;
    bool has_surge = false;
    bool has_smash = false;
};

// Spell definition
struct SpellDef {
    FixedString<48> name;       // Spell name (e.g., "Psychic Blast")
    FixedString<64> faction;    // Faction this spell belongs to
    u8 cost = 1;                // Token cost (1-3)
    u8 range = 12;              // Range in inches
    u8 hits = 0;                // Number of hits dealt (for direct damage)
    SpellTargetType target_type = SpellTargetType::Self;
    SpellEffectType effect_type = SpellEffectType::Buff;
    SpellHitRules hit_rules;    // Rules applied to hits

    // For buff/debuff spells
    RuleId grants_rule = RuleId::None;  // Rule granted to target
    i8 hit_modifier = 0;        // +/- to hit rolls
    i8 defense_modifier = 0;    // +/- to defense rolls
    i8 ap_modifier = 0;         // +/- to AP
    i8 morale_modifier = 0;     // +/- to morale tests

    bool is_valid() const { return cost > 0 && cost <= 3; }
    bool is_direct_damage() const { return effect_type == SpellEffectType::DirectDamage; }
    bool targets_enemy() const {
        return target_type == SpellTargetType::EnemyDirect ||
               target_type == SpellTargetType::EnemyDebuff;
    }
    bool targets_self() const {
        return target_type == SpellTargetType::Self ||
               target_type == SpellTargetType::FriendlyBuff;
    }
};

// Result of a spell cast attempt
struct SpellCastResult {
    bool attempted = false;     // Was the spell attempted?
    bool success = false;       // Did the spell succeed (roll >= target)?
    u8 roll = 0;                // The die roll
    u8 target_number = 0;       // The target number (4 + modifiers)
    i8 modifier = 0;            // Total modifier from enemy interference
    u8 tokens_spent = 0;        // Tokens spent by caster
    u8 enemy_tokens_spent = 0;  // Tokens spent by enemy to interfere
    u8 hits_dealt = 0;          // Hits dealt (for direct damage)
    u8 wounds_dealt = 0;        // Wounds dealt after defense
    u8 models_killed = 0;       // Models killed
};

// Spell slot in a faction's spell list
struct FactionSpellSlot {
    SpellDef spell;
    bool is_defined = false;
};

// Faction spell list (6 spells per faction, 2 at each cost level)
struct FactionSpellList {
    FixedString<64> faction_name;
    std::array<FactionSpellSlot, MAX_SPELLS_PER_FACTION> spells{};
    u8 spell_count = 0;

    void add_spell(const SpellDef& spell) {
        if (spell_count < MAX_SPELLS_PER_FACTION) {
            spells[spell_count].spell = spell;
            spells[spell_count].is_defined = true;
            spell_count++;
        }
    }

    const SpellDef* get_spell(u8 index) const {
        if (index < spell_count && spells[index].is_defined) {
            return &spells[index].spell;
        }
        return nullptr;
    }

    // Get spells sorted by cost (highest first) for AI selection
    void get_spells_by_cost(std::array<u8, MAX_SPELLS_PER_FACTION>& order, u8& count) const {
        count = 0;
        for (u8 i = 0; i < spell_count; ++i) {
            if (spells[i].is_defined) {
                // Insert in sorted order (descending by cost)
                u8 j = count;
                while (j > 0 && spells[order[j-1]].spell.cost < spells[i].spell.cost) {
                    order[j] = order[j-1];
                    --j;
                }
                order[j] = i;
                ++count;
            }
        }
    }
};

} // namespace battle
