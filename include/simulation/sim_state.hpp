#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
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

    void init_from(const Unit& unit) {
        alive_count = unit.model_count;
        status = UnitStatus::Normal;
        is_fatigued = false;
        for (u8 i = 0; i < unit.model_count; ++i) {
            models[i].wounds_taken = 0;
            models[i].state = ModelState::Healthy;
        }
    }

    void reset(u8 model_count) {
        alive_count = model_count;
        status = UnitStatus::Normal;
        is_fatigued = false;
        for (u8 i = 0; i < model_count; ++i) {
            models[i].reset();
        }
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
    void reset_round_state() { is_fatigued = false; }
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
    void get_wound_allocation_order(std::array<u8, MAX_MODELS_PER_UNIT>& order, u8& count) const {
        count = 0;

        // Phase 1: Non-tough, non-hero models
        for (u8 i = 0; i < unit->model_count; ++i) {
            const Model& m = unit->models[i];
            if (state->models[i].is_alive() && m.tough == 1 && !m.is_hero) {
                order[count++] = i;
            }
        }

        // Phase 2: Tough non-hero models (most wounded first)
        u8 tough_start = count;
        for (u8 i = 0; i < unit->model_count; ++i) {
            const Model& m = unit->models[i];
            if (state->models[i].is_alive() && m.tough > 1 && !m.is_hero) {
                order[count++] = i;
            }
        }
        if (count > tough_start + 1) {
            std::sort(order.begin() + tough_start, order.begin() + count,
                [this](u8 a, u8 b) {
                    return state->models[a].wounds_taken > state->models[b].wounds_taken;
                });
        }

        // Phase 3: Heroes (most wounded first)
        u8 hero_start = count;
        for (u8 i = 0; i < unit->model_count; ++i) {
            const Model& m = unit->models[i];
            if (state->models[i].is_alive() && m.is_hero) {
                order[count++] = i;
            }
        }
        if (count > hero_start + 1) {
            std::sort(order.begin() + hero_start, order.begin() + count,
                [this](u8 a, u8 b) {
                    return state->models[a].wounds_taken > state->models[b].wounds_taken;
                });
        }
    }
};

} // namespace battle
