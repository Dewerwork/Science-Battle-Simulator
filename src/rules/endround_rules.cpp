#include "rules/endround_rules.hpp"
#include "core/unit.hpp"
#include "simulation/sim_state.hpp"
#include "engine/game_state.hpp"
#include <unordered_map>
#include <algorithm>

namespace battle {

// ==============================================================================
// End-Round Effect Functions
// ==============================================================================

// Battleborn - 4+ to rally at round start
void effect_battleborn(EndRoundRuleContext& ctx) {
    ctx.used_battleborn = true;  // Mark that we'll attempt Battleborn rally
}

// Fearless - Reroll failed morale tests
void effect_fearless(EndRoundRuleContext& ctx) {
    ctx.reroll_available = true;
}

// MoraleBoost - +X to morale tests
void effect_morale_boost(EndRoundRuleContext& ctx) {
    // Get the rule value for MoraleBoost
    if (ctx.unit) {
        u8 boost = ctx.unit->get_rule_value(RuleId::MoraleBoost);
        ctx.morale_modifier += static_cast<i8>(boost);
    }
}

// HoldTheLine - Reroll failed morale tests
void effect_hold_the_line(EndRoundRuleContext& ctx) {
    ctx.reroll_available = true;
}

// NoRetreat - Cannot retreat, take wounds instead
void effect_no_retreat(EndRoundRuleContext& ctx) {
    ctx.cannot_retreat = true;
}

// Regeneration - 5+ to heal wounds at end of round
void effect_regeneration(EndRoundRuleContext& ctx) {
    ctx.regen_available = true;
    ctx.regen_roll_target = 5;  // 5+ regeneration
}

// Fear - Counts as extra wounds for morale (enemy sees more casualties)
void effect_fear(EndRoundRuleContext& ctx) {
    // Fear gives a morale bonus to the unit with it (enemy fears them)
    // This is tracked via the Fear rule value
    if (ctx.unit) {
        u8 fear_value = ctx.unit->get_rule_value(RuleId::Fear);
        ctx.morale_modifier += static_cast<i8>(fear_value);
    }
}

// ==============================================================================
// End-Round Rules Registry
// ==============================================================================

namespace {

std::unordered_map<u16, EndRoundRuleDefinition> g_endround_rules;
bool g_endround_rules_initialized = false;

void init_endround_rules() {
    if (g_endround_rules_initialized) return;

    // Battleborn - 4+ to rally (part of MORALE phase)
    g_endround_rules[static_cast<u16>(RuleId::Battleborn)] = {
        RuleId::Battleborn,
        EndRoundSubPhase::MORALE,  // Rally is part of morale phase
        effect_battleborn,
        {0.3f, true, false}
    };

    // Fearless - Reroll failed morale
    g_endround_rules[static_cast<u16>(RuleId::Fearless)] = {
        RuleId::Fearless,
        EndRoundSubPhase::MORALE,
        effect_fearless,
        {0.4f, true, false}
    };

    // MoraleBoost - +X to morale
    g_endround_rules[static_cast<u16>(RuleId::MoraleBoost)] = {
        RuleId::MoraleBoost,
        EndRoundSubPhase::MORALE,
        effect_morale_boost,
        {0.2f, true, false}
    };

    // HoldTheLine - Reroll morale
    g_endround_rules[static_cast<u16>(RuleId::HoldTheLine)] = {
        RuleId::HoldTheLine,
        EndRoundSubPhase::MORALE,
        effect_hold_the_line,
        {0.3f, true, false}
    };

    // NoRetreat - Can't retreat
    g_endround_rules[static_cast<u16>(RuleId::NoRetreat)] = {
        RuleId::NoRetreat,
        EndRoundSubPhase::MORALE,
        effect_no_retreat,
        {0.1f, true, false}
    };

    // Regeneration - Heal wounds
    g_endround_rules[static_cast<u16>(RuleId::Regeneration)] = {
        RuleId::Regeneration,
        EndRoundSubPhase::REGENERATION,
        effect_regeneration,
        {0.5f, false, true}
    };

    // Fear - Morale bonus
    g_endround_rules[static_cast<u16>(RuleId::Fear)] = {
        RuleId::Fear,
        EndRoundSubPhase::MORALE,
        effect_fear,
        {0.2f, true, false}
    };

    g_endround_rules_initialized = true;
}

}  // anonymous namespace

// ==============================================================================
// Public API
// ==============================================================================

void register_endround_rules() {
    init_endround_rules();
}

const EndRoundRuleDefinition* get_endround_rule(RuleId id) {
    init_endround_rules();
    auto it = g_endround_rules.find(static_cast<u16>(id));
    if (it != g_endround_rules.end()) {
        return &it->second;
    }
    return nullptr;
}

bool is_endround_rule(RuleId id) {
    return get_endround_rule(id) != nullptr;
}

// ==============================================================================
// EndRoundProcessor Implementation
// ==============================================================================

void EndRoundProcessor::process_unit_end_round(
    Unit& unit,
    UnitSimState& state,
    GameState& game,
    bool is_unit_a)
{
    init_endround_rules();

    // Build context
    EndRoundRuleContext ctx{};
    ctx.unit = &unit;
    ctx.state = &state;
    ctx.is_unit_a = is_unit_a;
    ctx.game = &game;
    ctx.current_round = game.current_round;
    ctx.morale_modifier = 0;
    ctx.auto_pass_morale = false;
    ctx.reroll_available = false;
    ctx.cannot_retreat = false;
    ctx.regen_roll_target = 5;
    ctx.regen_available = false;
    ctx.fear_modifier = 0;
    ctx.used_battleborn = false;

    // Process each end-round phase
    for (u8 phase = 0; phase < static_cast<u8>(EndRoundSubPhase::COUNT); ++phase) {
        EndRoundSubPhase current_phase = static_cast<EndRoundSubPhase>(phase);

        // Apply all rules for this phase
        for (const auto& [id, def] : g_endround_rules) {
            if (def.phase != current_phase) continue;

            // Check if unit has this rule
            if (unit.has_rule(def.rule_id)) {
                def.effect(ctx);
            }
        }
    }
}

u8 EndRoundProcessor::process_regeneration(
    Unit& unit,
    UnitSimState& state,
    u32& rng_state)
{
    if (!unit.has_rule(RuleId::Regeneration)) {
        return 0;
    }

    u8 wounds_healed = 0;

    // Check each model for wounds to regenerate
    for (u8 i = 0; i < unit.model_count; ++i) {
        if (!state.models[i].is_alive()) continue;
        if (state.models[i].wounds_taken == 0) continue;

        // Roll for each wound
        u8 wounds_on_model = state.models[i].wounds_taken;
        for (u8 w = 0; w < wounds_on_model; ++w) {
            // Simple LCG for regeneration roll
            rng_state = rng_state * 1103515245 + 12345;
            u8 roll = ((rng_state >> 16) % 6) + 1;

            if (roll >= 5) {  // 5+ regeneration
                state.models[i].wounds_taken--;
                wounds_healed++;

                // If fully healed, change state back to healthy
                if (state.models[i].wounds_taken == 0) {
                    state.models[i].state = ModelState::Healthy;
                }
            }
        }
    }

    return wounds_healed;
}

bool EndRoundProcessor::check_battleborn_rally(
    Unit& unit,
    UnitSimState& state,
    u32& rng_state)
{
    if (!unit.has_rule(RuleId::Battleborn)) {
        return false;
    }

    if (!state.is_shaken()) {
        return false;  // Not shaken, no need to rally
    }

    // Roll for Battleborn rally (4+)
    rng_state = rng_state * 1103515245 + 12345;
    u8 roll = ((rng_state >> 16) % 6) + 1;

    if (roll >= 4) {
        state.rally();
        return true;
    }

    return false;
}

i8 EndRoundProcessor::get_morale_modifier(
    const Unit& unit,
    const UnitSimState& state)
{
    i8 modifier = 0;

    // MoraleBoost adds its value
    if (unit.has_rule(RuleId::MoraleBoost)) {
        modifier += static_cast<i8>(unit.get_rule_value(RuleId::MoraleBoost));
    }

    // Fear adds its value
    if (unit.has_rule(RuleId::Fear)) {
        modifier += static_cast<i8>(unit.get_rule_value(RuleId::Fear));
    }

    // Check for modifiers from active auras/buffs using proper accessor
    const auto& modifiers = state.modifiers.get_modifiers();
    for (u8 i = 0; i < state.modifiers.modifier_count(); ++i) {
        if (modifiers[i].rule_id == RuleId::MoraleBoost) {
            modifier += static_cast<i8>(modifiers[i].value);
        }
    }

    return modifier;
}

bool EndRoundProcessor::has_morale_reroll(
    const Unit& unit,
    const UnitSimState& state)
{
    // Fearless grants reroll
    if (unit.has_rule(RuleId::Fearless)) {
        return true;
    }

    // HoldTheLine grants reroll
    if (unit.has_rule(RuleId::HoldTheLine)) {
        return true;
    }

    // Check modifiers for reroll capability
    if (state.modifiers.has_rule(RuleId::Fearless) ||
        state.modifiers.has_rule(RuleId::HoldTheLine)) {
        return true;
    }

    return false;
}

} // namespace battle
