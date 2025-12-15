#pragma once

#include "core/types.hpp"
#include "core/weapon.hpp"
#include <array>

namespace battle {

// ==============================================================================
// Model - Individual model within a unit (cache-aligned)
// ==============================================================================

struct alignas(64) Model {
    Name name;
    std::array<CompactRule, MAX_RULES_PER_ENTITY> rules{};
    std::array<WeaponRef, MAX_WEAPONS_PER_MODEL> weapons{};
    RuleMask rule_mask = 0; // Bitset for O(1) has_rule() lookup

    u8 quality     = 4;   // Quality value (2-6, roll this or higher to hit)
    u8 defense     = 4;   // Defense value (2-6, roll this or higher to save)
    u8 tough       = 1;   // Wounds required to kill (Tough(X))
    u8 wounds_taken = 0;  // Current damage taken
    u8 weapon_count = 0;  // Number of weapons
    u8 rule_count   = 0;  // Number of special rules

    ModelState state = ModelState::Healthy;
    bool is_hero     = false;

    // Constructors
    Model() = default;

    Model(std::string_view model_name, u8 qual, u8 def, u8 tgh = 1)
        : name(model_name), quality(qual), defense(def), tough(tgh) {}

    // Properties
    bool is_alive() const { return state != ModelState::Dead; }
    bool is_dead() const { return state == ModelState::Dead; }
    u8 remaining_wounds() const { return tough - wounds_taken; }

    // Combat operations
    bool apply_wound() {
        if (state == ModelState::Dead) return false;

        wounds_taken++;
        if (wounds_taken >= tough) {
            state = ModelState::Dead;
            return true; // Model died
        }
        state = ModelState::Wounded;
        return false;
    }

    void heal(u8 amount = 1) {
        if (state == ModelState::Dead) return;
        wounds_taken = (amount >= wounds_taken) ? 0 : wounds_taken - amount;
        if (wounds_taken == 0) state = ModelState::Healthy;
    }

    void reset() {
        wounds_taken = 0;
        state = ModelState::Healthy;
    }

    // Rule management
    void add_rule(RuleId id, u8 value = 0) {
        if (rule_count < MAX_RULES_PER_ENTITY) {
            rules[rule_count++] = CompactRule(id, value);
            rule_mask |= rule_bit(id);
        }
    }

    bool has_rule(RuleId id) const {
        return (rule_mask & rule_bit(id)) != 0;
    }

    u8 get_rule_value(RuleId id) const {
        for (u8 i = 0; i < rule_count; ++i) {
            if (rules[i].id == id) return rules[i].value;
        }
        return 0;
    }

    // Weapon management
    void add_weapon(WeaponIndex idx, u8 quantity = 1) {
        if (weapon_count < MAX_WEAPONS_PER_MODEL) {
            weapons[weapon_count++] = WeaponRef(idx, quantity);
        }
    }
};

} // namespace battle
