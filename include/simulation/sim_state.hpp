#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "core/modifiers.hpp"
#include "core/spell.hpp"
#include <array>

namespace battle {

// ==============================================================================
// Lightweight Simulation State - Tracks only mutable data during combat
// ==============================================================================

// Per-model mutable state (2 bytes instead of 64+ bytes)
struct ModelSimState {
    u8 wounds_taken = 0;
    ModelState state = ModelState::Healthy;

    void reset() {
        wounds_taken = 0;
        state = ModelState::Healthy;
    }

    bool is_alive() const { return state != ModelState::Dead; }
    bool is_dead() const { return state == ModelState::Dead; }
};

// Per-unit mutable state (~70 bytes instead of ~3KB)
struct UnitSimState {
    std::array<ModelSimState, MAX_MODELS_PER_UNIT> models{};
    UnitStatus status = UnitStatus::Normal;
    u8 alive_count = 0;
    bool is_fatigued = false;
    u32 limited_weapons_used = 0;  // Bitmask of Limited weapons already used this game

    // Spell casting state (Caster(X) rule)
    u8 spell_tokens = 0;           // Current spell tokens available
    u8 caster_value = 0;           // X from Caster(X) - tokens gained per round
    u8 spells_attempted = 0;       // Bitmask of spell indices attempted this round (max 8)

    // Once-per-activation abilities
    bool breath_attack_used = false;  // Breath Attack used this activation

    // Active modifiers from auras, buffs, faction rules
    UnitModifiers modifiers;

    void init_from(const Unit& unit) {
        alive_count = unit.model_count;
        status = UnitStatus::Normal;
        is_fatigued = false;
        limited_weapons_used = 0;
        // Initialize spell casting state
        spell_tokens = 0;
        caster_value = unit.get_rule_value(RuleId::Casting);  // Caster(X) value
        spells_attempted = 0;
        modifiers.clear();
        for (u8 i = 0; i < unit.model_count; ++i) {
            models[i].wounds_taken = 0;
            models[i].state = ModelState::Healthy;
        }
    }

    void reset(u8 model_count) {
        alive_count = model_count;
        status = UnitStatus::Normal;
        is_fatigued = false;
        limited_weapons_used = 0;
        // Note: caster_value is preserved from init_from, tokens reset at round start
        spell_tokens = 0;
        spells_attempted = 0;
        modifiers.clear();
        for (u8 i = 0; i < model_count; ++i) {
            models[i].reset();
        }
    }

    // Check if a Limited weapon has been used
    bool is_limited_weapon_used(u8 weapon_idx) const {
        return (limited_weapons_used & (1u << weapon_idx)) != 0;
    }

    // Mark a Limited weapon as used
    void mark_limited_weapon_used(u8 weapon_idx) {
        limited_weapons_used |= (1u << weapon_idx);
    }

    // Spell token management
    bool is_caster() const { return caster_value > 0; }

    // Grant spell tokens at round start (capped at MAX_SPELL_TOKENS)
    void grant_spell_tokens() {
        if (caster_value > 0) {
            u8 new_tokens = spell_tokens + caster_value;
            spell_tokens = (new_tokens > MAX_SPELL_TOKENS) ? MAX_SPELL_TOKENS : new_tokens;
        }
    }

    // Check if a spell has been attempted this round
    bool is_spell_attempted(u8 spell_idx) const {
        return (spells_attempted & (1u << spell_idx)) != 0;
    }

    // Mark a spell as attempted
    void mark_spell_attempted(u8 spell_idx) {
        spells_attempted |= (1u << spell_idx);
    }

    // Spend spell tokens (returns true if successful)
    bool spend_spell_tokens(u8 amount) {
        if (spell_tokens >= amount) {
            spell_tokens -= amount;
            return true;
        }
        return false;
    }

    // Reset spell attempts at round start
    void reset_spell_attempts() {
        spells_attempted = 0;
    }

    // Apply wound to a specific model, returns true if model died
    bool apply_wound_to_model(u8 model_idx, u8 tough) {
        ModelSimState& m = models[model_idx];
        if (m.state == ModelState::Dead) return false;

        m.wounds_taken++;
        if (m.wounds_taken >= tough) {
            m.state = ModelState::Dead;
            alive_count--;
            return true;
        }
        m.state = ModelState::Wounded;
        return false;
    }

    bool is_destroyed() const { return alive_count == 0; }
    bool is_shaken() const { return status == UnitStatus::Shaken; }
    bool is_routed() const { return status == UnitStatus::Routed; }
    bool is_out_of_action() const { return is_destroyed() || is_routed(); }

    void become_shaken() { status = UnitStatus::Shaken; }
    void rally() { if (status == UnitStatus::Shaken) status = UnitStatus::Normal; }
    void rout() { status = UnitStatus::Routed; }
    void reset_round_state() {
        is_fatigued = false;
        reset_spell_attempts();  // Spells can be attempted again each round
    }

    // Reset state at start of each activation
    void reset_activation_state() {
        breath_attack_used = false;  // Can use Breath Attack again
    }

    // Breath Attack tracking
    bool is_breath_attack_used() const { return breath_attack_used; }
    void mark_breath_attack_used() { breath_attack_used = true; }
};

// ==============================================================================
// Unit View - Combines const unit data with mutable sim state
// ==============================================================================

struct UnitView {
    const Unit* unit;      // Read-only unit data (weapons, rules, stats)
    UnitSimState* state;   // Mutable simulation state

    UnitView() : unit(nullptr), state(nullptr) {}
    UnitView(const Unit* u, UnitSimState* s) : unit(u), state(s) {}

    // Delegate read-only properties to unit
    u32 unit_id() const { return unit->unit_id; }
    u16 points_cost() const { return unit->points_cost; }
    u8 model_count() const { return unit->model_count; }
    u8 quality() const { return unit->quality; }
    u8 defense() const { return unit->defense; }
    u8 max_range() const { return unit->max_range; }
    AIType ai_type() const { return unit->ai_type; }

    const Name& name() const { return unit->name; }
    const Weapon& get_weapon(u8 idx) const { return unit->get_weapon(idx); }
    u8 weapon_count() const { return unit->weapon_count; }

    bool has_rule(RuleId id) const { return unit->has_rule(id); }
    u8 get_rule_value(RuleId id) const { return unit->get_rule_value(id); }

    // Delegate mutable state to sim state
    u8 alive_count() const { return state->alive_count; }
    bool is_destroyed() const { return state->is_destroyed(); }
    bool is_shaken() const { return state->is_shaken(); }
    bool is_routed() const { return state->is_routed(); }
    bool is_out_of_action() const { return state->is_out_of_action(); }
    bool is_fatigued() const { return state->is_fatigued; }
    void set_fatigued(bool val) { state->is_fatigued = val; }

    void become_shaken() { state->become_shaken(); }
    void rally() { state->rally(); }
    void rout() { state->rout(); }
    void reset_round_state() { state->reset_round_state(); }

    // Limited weapon tracking
    bool is_limited_weapon_used(u8 weapon_idx) const { return state->is_limited_weapon_used(weapon_idx); }
    void mark_limited_weapon_used(u8 weapon_idx) { state->mark_limited_weapon_used(weapon_idx); }

    // Spell token tracking (Caster(X) rule)
    bool is_caster() const { return state->is_caster(); }
    u8 spell_tokens() const { return state->spell_tokens; }
    u8 caster_value() const { return state->caster_value; }
    void grant_spell_tokens() { state->grant_spell_tokens(); }
    bool is_spell_attempted(u8 spell_idx) const { return state->is_spell_attempted(spell_idx); }
    void mark_spell_attempted(u8 spell_idx) { state->mark_spell_attempted(spell_idx); }
    bool spend_spell_tokens(u8 amount) { return state->spend_spell_tokens(amount); }
    void reset_spell_attempts() { state->reset_spell_attempts(); }

    // Breath Attack tracking
    bool is_breath_attack_used() const { return state->is_breath_attack_used(); }
    void mark_breath_attack_used() { state->mark_breath_attack_used(); }
    void reset_activation_state() { state->reset_activation_state(); }

    // Modifier access
    UnitModifiers& modifiers() { return state->modifiers; }
    const UnitModifiers& modifiers() const { return state->modifiers; }

    // Check if unit has a rule (including from modifiers)
    bool has_rule_with_modifiers(RuleId id) const {
        return unit->has_rule(id) || state->modifiers.has_rule(id);
    }

    // Get rule value (max of base and modifier)
    u8 get_rule_value_with_modifiers(RuleId id) const {
        u8 base_val = unit->get_rule_value(id);
        u8 mod_val = state->modifiers.get_rule_value(id);
        return std::max(base_val, mod_val);
    }

    // Get aggregated stat modifiers for current context
    StatModifiers get_stat_modifiers(bool is_melee, bool is_charging,
                                      bool is_being_charged, u8 distance) const {
        return state->modifiers.get_stat_modifiers(
            is_melee, is_charging, is_being_charged,
            is_shaken(), false, distance);
    }

    // Model access
    const Model& get_model(u8 idx) const { return unit->models[idx]; }
    ModelSimState& get_model_state(u8 idx) { return state->models[idx]; }
    const ModelSimState& get_model_state(u8 idx) const { return state->models[idx]; }

    bool model_is_alive(u8 idx) const { return state->models[idx].is_alive(); }
    u8 model_wounds_taken(u8 idx) const { return state->models[idx].wounds_taken; }
    u8 model_remaining_wounds(u8 idx) const {
        return unit->models[idx].tough - state->models[idx].wounds_taken;
    }

    // Apply wound to model
    bool apply_wound_to_model(u8 idx) {
        return state->apply_wound_to_model(idx, unit->models[idx].tough);
    }

    // Computed properties
    u16 total_wounds_remaining() const {
        u16 total = 0;
        for (u8 i = 0; i < unit->model_count; ++i) {
            if (state->models[i].is_alive()) {
                total += unit->models[i].tough - state->models[i].wounds_taken;
            }
        }
        return total;
    }

    bool is_at_half_strength() const {
        if (unit->model_count == 1) {
            u16 total_tough = unit->models[0].tough;
            return total_wounds_remaining() <= total_tough / 2;
        }
        return state->alive_count <= unit->model_count / 2;
    }

    // Get wound allocation order (same logic as Unit but uses sim state)
    // Uses insertion sort for small groups instead of std::sort for better performance
    void get_wound_allocation_order(std::array<u8, MAX_MODELS_PER_UNIT>& order, u8& count) const {
        count = 0;

        // Phase 1: Non-tough, non-hero models (no sorting needed)
        for (u8 i = 0; i < unit->model_count; ++i) {
            const Model& m = unit->models[i];
            if (state->models[i].is_alive() && m.tough == 1 && !m.is_hero) {
                order[count++] = i;
            }
        }

        // Phase 2: Tough non-hero models (most wounded first)
        // Use insertion sort for small groups (typically 0-3 elements)
        u8 tough_start = count;
        for (u8 i = 0; i < unit->model_count; ++i) {
            const Model& m = unit->models[i];
            if (state->models[i].is_alive() && m.tough > 1 && !m.is_hero) {
                // Insert in sorted order (descending by wounds_taken)
                u8 j = count;
                while (j > tough_start && state->models[order[j-1]].wounds_taken < state->models[i].wounds_taken) {
                    order[j] = order[j-1];
                    --j;
                }
                order[j] = i;
                ++count;
            }
        }

        // Phase 3: Heroes (most wounded first)
        // Use insertion sort for small groups (typically 1-2 elements)
        u8 hero_start = count;
        for (u8 i = 0; i < unit->model_count; ++i) {
            const Model& m = unit->models[i];
            if (state->models[i].is_alive() && m.is_hero) {
                // Insert in sorted order (descending by wounds_taken)
                u8 j = count;
                while (j > hero_start && state->models[order[j-1]].wounds_taken < state->models[i].wounds_taken) {
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
