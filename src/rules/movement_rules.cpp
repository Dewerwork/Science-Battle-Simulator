#include "rules/movement_rules.hpp"
#include "core/unit.hpp"
#include "simulation/sim_state.hpp"
#include <unordered_map>
#include <algorithm>

namespace battle {

// ==============================================================================
// Movement Constants
// ==============================================================================

constexpr u8 LOCAL_STANDARD_MOVE = 6;
constexpr i8 FAST_ADVANCE_BONUS = 2;     // Fast: +2" when using Advance
constexpr i8 FAST_RUSH_CHARGE_BONUS = 4; // Fast: +4" when using Rush/Charge
constexpr i8 SLOW_PENALTY = -2;          // Slow: -2" (4" total)
constexpr i8 AGILE_ADVANCE_BONUS = 1;    // Agile: +1" advance
constexpr i8 AGILE_CHARGE_BONUS = 2;     // Agile: +2" charge
constexpr i8 RAPID_CHARGE_BONUS = 4;     // RapidCharge: +4" charge
constexpr i8 DARKBORN_CHARGE_BONUS = 3;  // Darkborn: +3" charge

constexpr u8 BASE_CHARGE_RANGE = 12;

// ==============================================================================
// Movement Effect Functions
// ==============================================================================

// Note: These use the existing MovementContext from core/contexts.hpp

// Fast - +2" Advance, +4" Rush/Charge
void effect_fast(MovementContext& ctx) {
    if (ctx.move_type == MovementContext::MoveType::ADVANCE) {
        ctx.distance_modifier += FAST_ADVANCE_BONUS;
    } else if (ctx.move_type == MovementContext::MoveType::RUSH ||
               ctx.move_type == MovementContext::MoveType::CHARGE) {
        ctx.distance_modifier += FAST_RUSH_CHARGE_BONUS;
    }
}

// Slow - Decreases base movement by 2"
void effect_slow(MovementContext& ctx) {
    ctx.distance_modifier += SLOW_PENALTY;  // Note: penalty is negative
}

// Flying - Can fly over terrain and units
void effect_flying(MovementContext& ctx) {
    ctx.terrain_flags |= MovementContext::FLY_OVER;
    ctx.terrain_flags |= MovementContext::IGNORE_DIFFICULT;
    ctx.terrain_flags |= MovementContext::IGNORE_DANGEROUS;
    ctx.ignores_engagement = true;
}

// Strider - Ignores difficult terrain
void effect_strider(MovementContext& ctx) {
    ctx.terrain_flags |= MovementContext::IGNORE_DIFFICULT;
}

// Agile - +1" advance, +2" charge
void effect_agile(MovementContext& ctx) {
    if (ctx.move_type == MovementContext::MoveType::ADVANCE) {
        ctx.distance_modifier += AGILE_ADVANCE_BONUS;
    } else if (ctx.move_type == MovementContext::MoveType::CHARGE) {
        ctx.charge_bonus += AGILE_CHARGE_BONUS;
    }
}

// RapidCharge - +4" charge range
void effect_rapid_charge(MovementContext& ctx) {
    if (ctx.move_type == MovementContext::MoveType::CHARGE) {
        ctx.charge_bonus += RAPID_CHARGE_BONUS;
    }
}

// HitAndRun - Can disengage from melee
void effect_hit_and_run(MovementContext& ctx) {
    ctx.hit_and_run_pending = true;
}

// Teleport - Once per activation, place model within 6" of position before moving
// This is bonus movement that happens before the normal action (advance/rush/charge)
// Teleport 6" + Advance 6" = 12", Teleport 6" + Rush 12" = 18", Teleport 6" + Charge 12" = 18"
void effect_teleport(MovementContext& ctx) {
    if (ctx.move_type == MovementContext::MoveType::CHARGE) {
        ctx.charge_bonus += 6;  // +6" to charge range
    } else if (ctx.move_type == MovementContext::MoveType::RUSH) {
        // For rush, add +3 before doubling so result is +6 after: (6+3)*2 = 18
        ctx.distance_modifier += 3;
    } else {
        ctx.distance_modifier += 6;  // +6" to advance
    }
}

// Wolfborn - When activated, place models within 3" of their position before moving
// NOTE: This is now handled as a pre-movement step in game_runner.hpp, not as a movement bonus.
// The effect function is kept empty but registered for rule detection purposes.
void effect_wolfborn(MovementContext& /*ctx*/) {
    // Pre-movement placement is handled in game_runner.hpp before the action switch
    // This provides a separate logged movement step as per rule specification
}

// Darkborn - +3" range when shooting, +3" when charging
// NOTE: Range bonus is handled in UnitView::effective_max_range(), charge bonus here
void effect_darkborn(MovementContext& ctx) {
    if (ctx.move_type == MovementContext::MoveType::CHARGE) {
        ctx.charge_bonus += DARKBORN_CHARGE_BONUS;
    }
}

// ==============================================================================
// Movement Rules Registry
// ==============================================================================

namespace {

std::unordered_map<u16, MovementRuleDefinition> g_movement_rules;
bool g_movement_rules_initialized = false;

void init_movement_rules() {
    if (g_movement_rules_initialized) return;

    // Fast - +2" Advance, +4" Rush/Charge
    g_movement_rules[static_cast<u16>(RuleId::Fast)] = {
        RuleId::Fast,
        MovementSubPhase::CALCULATE_DISTANCE,
        effect_fast,
        {0.6f, 0.3f, true, false}  // Prefers closing distance
    };

    // Slow - -2" movement
    g_movement_rules[static_cast<u16>(RuleId::Slow)] = {
        RuleId::Slow,
        MovementSubPhase::CALCULATE_DISTANCE,
        effect_slow,
        {0.3f, 0.4f, false, true}  // Less mobile
    };

    // Flying - Can fly over terrain/units
    g_movement_rules[static_cast<u16>(RuleId::Flying)] = {
        RuleId::Flying,
        MovementSubPhase::CALCULATE_DISTANCE,
        effect_flying,
        {0.7f, 0.5f, false, false}  // Very mobile
    };

    // Strider - Ignores difficult terrain
    g_movement_rules[static_cast<u16>(RuleId::Strider)] = {
        RuleId::Strider,
        MovementSubPhase::CALCULATE_DISTANCE,
        effect_strider,
        {0.5f, 0.5f, false, false}  // Neutral
    };

    // Agile - +1" advance, +2" charge
    g_movement_rules[static_cast<u16>(RuleId::Agile)] = {
        RuleId::Agile,
        MovementSubPhase::CHARGE_DECLARE,
        effect_agile,
        {0.6f, 0.4f, true, false}  // Slightly prefers close
    };

    // RapidCharge - +4" charge range
    g_movement_rules[static_cast<u16>(RuleId::RapidCharge)] = {
        RuleId::RapidCharge,
        MovementSubPhase::CHARGE_DECLARE,
        effect_rapid_charge,
        {0.8f, 0.2f, true, false}  // Strong charge preference
    };

    // HitAndRun - Can disengage after melee
    g_movement_rules[static_cast<u16>(RuleId::HitAndRun)] = {
        RuleId::HitAndRun,
        MovementSubPhase::POST_MOVE,
        effect_hit_and_run,
        {0.5f, 0.7f, false, true}  // Prefers hit-and-run tactics
    };

    // Teleport - +6" bonus movement before normal action
    g_movement_rules[static_cast<u16>(RuleId::Teleport)] = {
        RuleId::Teleport,
        MovementSubPhase::CALCULATE_DISTANCE,
        effect_teleport,
        {0.8f, 0.2f, true, false}  // Strong mobility preference
    };

    // Wolfborn - +3" bonus movement before normal action
    g_movement_rules[static_cast<u16>(RuleId::Wolfborn)] = {
        RuleId::Wolfborn,
        MovementSubPhase::CALCULATE_DISTANCE,
        effect_wolfborn,
        {0.7f, 0.3f, true, false}  // Good mobility preference
    };

    // Darkborn - +3" range when shooting, +3" when charging
    g_movement_rules[static_cast<u16>(RuleId::Darkborn)] = {
        RuleId::Darkborn,
        MovementSubPhase::CHARGE_DECLARE,
        effect_darkborn,
        {0.6f, 0.4f, true, false}  // Balanced mobility preference
    };

    g_movement_rules_initialized = true;
}

}  // anonymous namespace

// ==============================================================================
// Public API
// ==============================================================================

void register_movement_rules() {
    init_movement_rules();
}

const MovementRuleDefinition* get_movement_rule(RuleId id) {
    init_movement_rules();
    auto it = g_movement_rules.find(static_cast<u16>(id));
    if (it != g_movement_rules.end()) {
        return &it->second;
    }
    return nullptr;
}

bool is_movement_rule(RuleId id) {
    return get_movement_rule(id) != nullptr;
}

// ==============================================================================
// MovementCalculator Implementation
// ==============================================================================

u8 MovementCalculator::calculate_move_distance(
    const Unit& unit,
    UnitSimState* /*state*/,
    MovementContext::MoveType move_type)
{
    // Build context using the existing MovementContext structure
    MovementContext ctx{};
    ctx.unit = const_cast<Unit*>(&unit);  // Context uses non-const, but we won't modify
    ctx.move_type = move_type;
    ctx.base_distance = LOCAL_STANDARD_MOVE;
    ctx.distance_modifier = 0;
    ctx.terrain_flags = 0;
    ctx.charge_bonus = 0;
    ctx.ignores_engagement = false;
    ctx.can_advance_and_charge = false;
    ctx.hit_and_run_pending = false;

    // Apply all movement rules in CALCULATE_DISTANCE phase
    process_movement_effects(ctx, MovementSubPhase::CALCULATE_DISTANCE);

    // If it's an advance move and Agile applies, also run CHARGE_DECLARE for advance bonus
    if (move_type == MovementContext::MoveType::ADVANCE) {
        process_movement_effects(ctx, MovementSubPhase::CHARGE_DECLARE);
    }

    // Calculate total (clamp to minimum of 1)
    i16 total = static_cast<i16>(ctx.base_distance) + ctx.distance_modifier;
    if (total < 1) total = 1;

    // Apply rush multiplier
    if (move_type == MovementContext::MoveType::RUSH) {
        total *= 2;
    }

    ctx.final_distance = static_cast<u8>(total);
    return ctx.final_distance;
}

u8 MovementCalculator::calculate_charge_range(
    const Unit& unit,
    UnitSimState* /*state*/)
{
    // Build context
    MovementContext ctx{};
    ctx.unit = const_cast<Unit*>(&unit);
    ctx.move_type = MovementContext::MoveType::CHARGE;
    ctx.base_distance = LOCAL_STANDARD_MOVE;
    ctx.distance_modifier = 0;
    ctx.terrain_flags = 0;
    ctx.charge_bonus = 0;
    ctx.ignores_engagement = false;
    ctx.can_advance_and_charge = false;
    ctx.hit_and_run_pending = false;

    // Apply CALCULATE_DISTANCE phase effects (Fast/Slow affect charge)
    process_movement_effects(ctx, MovementSubPhase::CALCULATE_DISTANCE);

    // Apply CHARGE_DECLARE phase effects (Agile, RapidCharge)
    process_movement_effects(ctx, MovementSubPhase::CHARGE_DECLARE);

    // Calculate charge range:
    // Base charge range (12") + movement modifier + charge bonus
    i16 charge_range = BASE_CHARGE_RANGE + ctx.distance_modifier + ctx.charge_bonus;

    // Clamp to minimum of 1
    if (charge_range < 1) charge_range = 1;

    return static_cast<u8>(charge_range);
}

void MovementCalculator::process_movement_effects(
    MovementContext& ctx,
    MovementSubPhase phase)
{
    init_movement_rules();

    // Check each movement rule
    for (const auto& [id, def] : g_movement_rules) {
        if (def.phase != phase) continue;

        // Check if unit has this rule
        if (ctx.unit && ctx.unit->has_rule(def.rule_id)) {
            def.effect(ctx);
        }
    }
}

bool MovementCalculator::can_fly(const Unit& unit) {
    return unit.has_rule(RuleId::Flying);
}

bool MovementCalculator::can_hit_and_run(const Unit& unit) {
    return unit.has_rule(RuleId::HitAndRun);
}

} // namespace battle
