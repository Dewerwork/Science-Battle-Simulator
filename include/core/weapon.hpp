#pragma once

#include "core/types.hpp"
#include <array>
#include <algorithm>

namespace battle {

// ==============================================================================
// Weapon - Compact representation (32 bytes)
// ==============================================================================

struct alignas(32) Weapon {
    Name name;                                              // 65 bytes -> padded
    std::array<CompactRule, MAX_RULES_PER_ENTITY> rules{};  // 32 bytes
    u8 attacks = 1;     // A value (number of attack dice)
    u8 range   = 0;     // Range in inches (0 = melee)
    u8 ap      = 0;     // Armor Piercing value
    u8 rule_count = 0;  // Number of active rules

    // Constructors
    Weapon() = default;

    Weapon(std::string_view weapon_name, u8 atk, u8 rng, u8 armor_pen)
        : name(weapon_name), attacks(atk), range(rng), ap(armor_pen) {}

    // Properties
    bool is_melee() const { return range == 0; }
    bool is_ranged() const { return range > 0; }

    // Rule management
    void add_rule(RuleId id, u8 value = 0) {
        if (rule_count < MAX_RULES_PER_ENTITY) {
            rules[rule_count++] = CompactRule(id, value);
        }
    }

    bool has_rule(RuleId id) const {
        for (u8 i = 0; i < rule_count; ++i) {
            if (rules[i].id == id) return true;
        }
        return false;
    }

    u8 get_rule_value(RuleId id) const {
        for (u8 i = 0; i < rule_count; ++i) {
            if (rules[i].id == id) return rules[i].value;
        }
        return 0;
    }

    const CompactRule* get_rule(RuleId id) const {
        for (u8 i = 0; i < rule_count; ++i) {
            if (rules[i].id == id) return &rules[i];
        }
        return nullptr;
    }
};

// ==============================================================================
// WeaponRef - Lightweight reference to a weapon (for models)
// ==============================================================================

struct WeaponRef {
    WeaponIndex index;  // Index into weapon pool
    u8 quantity;        // How many of this weapon (usually 1)

    WeaponRef() : index(0), quantity(0) {}
    WeaponRef(WeaponIndex idx, u8 qty = 1) : index(idx), quantity(qty) {}
};

} // namespace battle
