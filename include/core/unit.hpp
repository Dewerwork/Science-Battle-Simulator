#pragma once

#include "core/types.hpp"
#include "core/model.hpp"
#include <array>
#include <algorithm>
#include <span>

namespace battle {

// ==============================================================================
// Unit - A group of models that act together
// ==============================================================================

struct Unit {
    Name name;
    Name faction;
    std::array<Model, MAX_MODELS_PER_UNIT> models{};
    std::array<CompactRule, MAX_RULES_PER_ENTITY> rules{};
    std::array<Weapon, MAX_WEAPONS_PER_MODEL * 2> weapons{};  // Weapon storage

    u32 unit_id      = 0;    // Unique identifier for this loadout
    u16 points_cost  = 0;    // Points value
    u8  model_count  = 0;    // Total models in unit
    u8  rule_count   = 0;    // Number of unit-wide rules
    u8  weapon_count = 0;    // Total weapons stored
    u8  quality      = 4;    // Base quality (Q value)
    u8  defense      = 4;    // Base defense (D value)

    // Computed fields
    AIType ai_type     = AIType::Hybrid;
    u8  max_range      = 0;    // Longest weapon range

    // Combat state (mutable during simulation)
    UnitStatus status    = UnitStatus::Normal;
    bool is_fatigued     = false;
    u8   alive_count     = 0;  // Cached count of alive models

    // Constructors
    Unit() = default;

    Unit(std::string_view unit_name, u16 points = 0)
        : name(unit_name), points_cost(points) {}

    // Model management
    Model& add_model(const Model& model) {
        if (model_count < MAX_MODELS_PER_UNIT) {
            models[model_count] = model;
            alive_count++;
            return models[model_count++];
        }
        return models[0]; // Return first model if full (shouldn't happen)
    }

    // Unit-wide rule management
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

    // Properties
    bool is_destroyed() const { return alive_count == 0; }
    bool is_shaken() const { return status == UnitStatus::Shaken; }
    bool is_routed() const { return status == UnitStatus::Routed; }
    bool is_out_of_action() const { return is_destroyed() || is_routed(); }

    // Computed properties
    u8 get_base_quality() const {
        if (model_count == 0) return 4;
        return models[0].quality; // Assume uniform quality
    }

    u8 get_base_defense() const {
        if (model_count == 0) return 4;
        return models[0].defense; // Assume uniform defense
    }

    u16 total_wounds() const {
        u16 total = 0;
        for (u8 i = 0; i < model_count; ++i) {
            total += models[i].tough;
        }
        return total;
    }

    u16 total_wounds_remaining() const {
        u16 total = 0;
        for (u8 i = 0; i < model_count; ++i) {
            if (models[i].is_alive()) {
                total += models[i].remaining_wounds();
            }
        }
        return total;
    }

    bool is_at_half_strength() const {
        if (model_count == 1) {
            // Single model - check tough value
            return total_wounds_remaining() <= total_wounds() / 2;
        }
        // Multi-model - check model count
        return alive_count <= model_count / 2;
    }

    // Get alive models in wound allocation order
    // Order: non-tough non-heroes -> tough non-heroes (most wounded first) -> heroes
    void get_wound_allocation_order(std::array<u8, MAX_MODELS_PER_UNIT>& order, u8& count) const {
        count = 0;

        // Phase 1: Non-tough, non-hero models
        for (u8 i = 0; i < model_count; ++i) {
            const Model& m = models[i];
            if (m.is_alive() && m.tough == 1 && !m.is_hero) {
                order[count++] = i;
            }
        }

        // Phase 2: Tough non-hero models (sorted by wounds_taken descending)
        u8 tough_start = count;
        for (u8 i = 0; i < model_count; ++i) {
            const Model& m = models[i];
            if (m.is_alive() && m.tough > 1 && !m.is_hero) {
                order[count++] = i;
            }
        }
        // Sort tough models by wounds_taken (most wounded first)
        if (count > tough_start + 1) {
            std::sort(order.begin() + tough_start, order.begin() + count,
                [this](u8 a, u8 b) {
                    return models[a].wounds_taken > models[b].wounds_taken;
                });
        }

        // Phase 3: Heroes (sorted by wounds_taken descending)
        u8 hero_start = count;
        for (u8 i = 0; i < model_count; ++i) {
            const Model& m = models[i];
            if (m.is_alive() && m.is_hero) {
                order[count++] = i;
            }
        }
        // Sort heroes by wounds_taken
        if (count > hero_start + 1) {
            std::sort(order.begin() + hero_start, order.begin() + count,
                [this](u8 a, u8 b) {
                    return models[a].wounds_taken > models[b].wounds_taken;
                });
        }
    }

    // State management
    void update_alive_count() {
        alive_count = 0;
        for (u8 i = 0; i < model_count; ++i) {
            if (models[i].is_alive()) alive_count++;
        }
    }

    void become_shaken() {
        status = UnitStatus::Shaken;
    }

    void rally() {
        if (status == UnitStatus::Shaken) {
            status = UnitStatus::Normal;
        }
    }

    void rout() {
        status = UnitStatus::Routed;
    }

    void reset_round_state() {
        is_fatigued = false;
    }

    void reset() {
        status = UnitStatus::Normal;
        is_fatigued = false;
        alive_count = model_count;
        for (u8 i = 0; i < model_count; ++i) {
            models[i].reset();
        }
    }

    // Create a fresh copy for simulation
    Unit copy_fresh() const {
        Unit copy = *this;
        copy.reset();
        return copy;
    }

    // Weapon management at unit level
    u8 add_weapon(const Weapon& weapon) {
        if (weapon_count < weapons.size()) {
            weapons[weapon_count] = weapon;
            if (weapon.range > max_range) {
                max_range = weapon.range;
            }
            return weapon_count++;
        }
        return 0;
    }

    const Weapon& get_weapon(u8 index) const {
        return weapons[index];
    }

    // Compute total attacks for AI classification
    u16 total_melee_attacks() const {
        u16 total = 0;
        for (u8 i = 0; i < weapon_count; ++i) {
            if (weapons[i].is_melee()) {
                total += weapons[i].attacks;
            }
        }
        return total;
    }

    u16 total_ranged_attacks() const {
        u16 total = 0;
        for (u8 i = 0; i < weapon_count; ++i) {
            if (weapons[i].is_ranged()) {
                total += weapons[i].attacks;
            }
        }
        return total;
    }

    // Compute and set AI type based on weapons
    void compute_ai_type() {
        u16 melee = total_melee_attacks();
        u16 ranged = total_ranged_attacks();

        if (ranged == 0) {
            ai_type = AIType::Melee;
        } else if (melee >= ranged) {
            ai_type = AIType::Hybrid;
        } else {
            ai_type = AIType::Shooting;
        }
    }
};

} // namespace battle
