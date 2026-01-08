#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "core/weapon.hpp"
#include "core/faction_rules.hpp"
#include "engine/dice.hpp"
#include "engine/faction_combat.hpp"
#include "simulation/sim_state.hpp"
#include <array>

namespace battle {

// ==============================================================================
// Combat Result Structures
// ==============================================================================

struct AttackResult {
    u16 attacks_made;
    u16 hits;
    u16 hits_after_modifiers;  // After Blast, Furious, etc.
    u16 wounds_dealt;
    u8  sixes_rolled;          // For Furious, Rending tracking
    bool has_bane;             // Bypasses regeneration

    AttackResult() : attacks_made(0), hits(0), hits_after_modifiers(0),
                     wounds_dealt(0), sixes_rolled(0), has_bane(false) {}
};

struct WoundAllocationResult {
    u16 wounds_allocated;
    u8  models_killed;
    u16 wounds_regenerated;
    u16 overkill_wounds;

    WoundAllocationResult() : wounds_allocated(0), models_killed(0),
                              wounds_regenerated(0), overkill_wounds(0) {}
};

struct CombatResult {
    u16 total_hits;
    u16 total_wounds;
    WoundAllocationResult wound_allocation;
    u8  defender_models_killed;
    u8  defender_models_remaining;
    u8  attacker_models_remaining;

    CombatResult() : total_hits(0), total_wounds(0), defender_models_killed(0),
                     defender_models_remaining(0), attacker_models_remaining(0) {}
};

// ==============================================================================
// Combat Context - All parameters for a single attack resolution
// ==============================================================================

struct CombatContext {
    CombatPhase phase;
    bool is_charging;
    bool in_cover;
    bool attacker_shaken;
    bool defender_shaken;
    bool attacker_fatigued;

    CombatContext()
        : phase(CombatPhase::Melee), is_charging(false), in_cover(false),
          attacker_shaken(false), defender_shaken(false), attacker_fatigued(false) {}

    static CombatContext shooting(bool cover = false) {
        CombatContext ctx;
        ctx.phase = CombatPhase::Shooting;
        ctx.in_cover = cover;
        return ctx;
    }

    static CombatContext charge() {
        CombatContext ctx;
        ctx.phase = CombatPhase::Melee;
        ctx.is_charging = true;
        return ctx;
    }

    static CombatContext melee() {
        CombatContext ctx;
        ctx.phase = CombatPhase::Melee;
        ctx.is_charging = false;
        return ctx;
    }
};

// ==============================================================================
// Weapon Pool - Shared storage for all weapons
// Weapons are referenced by index for cache efficiency
// ==============================================================================

class WeaponPool {
public:
    static constexpr size_t MAX_WEAPONS = 1024;

    WeaponIndex add(const Weapon& weapon) {
        if (count_ >= MAX_WEAPONS) return 0;
        weapons_[count_] = weapon;
        return count_++;
    }

    const Weapon& get(WeaponIndex index) const {
        return weapons_[index];
    }

    Weapon& get(WeaponIndex index) {
        return weapons_[index];
    }

    WeaponIndex count() const { return count_; }

    void clear() { count_ = 0; }

private:
    std::array<Weapon, MAX_WEAPONS> weapons_;
    WeaponIndex count_ = 0;
};

// Global weapon pool (thread-safe read, single-thread write during setup)
inline WeaponPool& get_weapon_pool() {
    static WeaponPool pool;
    return pool;
}

// ==============================================================================
// Combat Resolver - Core combat logic
// ==============================================================================

class CombatResolver {
public:
    explicit CombatResolver(DiceRoller& dice) : dice_(dice) {}

    // Main entry point: resolve an attack from attacker to defender
    CombatResult resolve_attack(
        Unit& attacker,
        Unit& defender,
        const CombatContext& ctx
    );

    // Lightweight version using UnitView (avoids copying units)
    CombatResult resolve_attack(
        UnitView& attacker,
        UnitView& defender,
        const CombatContext& ctx
    );

private:
    DiceRoller& dice_;
    const WeaponPool& weapons_ = get_weapon_pool();

    // Resolve a single weapon's attacks
    AttackResult resolve_weapon_attack(
        const Model& model,
        const Weapon& weapon,
        Unit& defender,
        const Unit& attacker_unit,
        const CombatContext& ctx
    );

    // Calculate effective AP including modifiers
    u8 calculate_ap(const Weapon& weapon, const Unit& unit, bool is_charging);

    // Apply hit modifiers (Blast, Furious)
    u16 apply_hit_modifiers(
        u16 hits,
        u8 sixes,
        const Weapon& weapon,
        const Unit& attacker,
        const Unit& defender,
        bool is_charging
    );

    // Apply wound modifiers (Deadly)
    u16 apply_wound_modifiers(u16 wounds, const Weapon& weapon);

    // Allocate wounds to defender unit
    WoundAllocationResult allocate_wounds(
        Unit& defender,
        u16 wounds,
        bool allow_regeneration
    );

    // UnitView-based methods for lightweight simulation
    AttackResult resolve_weapon_attack_view(
        const Model& model,
        const Weapon& weapon,
        UnitView& defender,
        const Unit& attacker_unit,
        const CombatContext& ctx
    );

    u16 apply_hit_modifiers_view(
        u16 hits,
        u8 sixes,
        const Weapon& weapon,
        const Unit& attacker,
        const UnitView& defender,
        bool is_charging
    );

    WoundAllocationResult allocate_wounds_view(
        UnitView& defender,
        u16 wounds,
        bool allow_regeneration
    );
};

// ==============================================================================
// Inline Implementations (Hot Path)
// ==============================================================================

inline CombatResult CombatResolver::resolve_attack(
    Unit& attacker,
    Unit& defender,
    const CombatContext& ctx
) {
    CombatResult result;
    bool any_bane = false;
    u16 total_wounds = 0;

    // Iterate through alive models
    for (u8 i = 0; i < attacker.model_count; ++i) {
        Model& model = attacker.models[i];
        if (!model.is_alive()) continue;

        // Get weapons for this phase
        for (u8 w = 0; w < model.weapon_count; ++w) {
            const Weapon& weapon = weapons_.get(model.weapons[w].index);

            // Check if weapon is appropriate for phase
            bool use_weapon = (ctx.phase == CombatPhase::Melee && weapon.is_melee()) ||
                              (ctx.phase == CombatPhase::Shooting && weapon.is_ranged());
            if (!use_weapon) continue;

            AttackResult attack = resolve_weapon_attack(model, weapon, defender, attacker, ctx);
            result.total_hits += attack.hits_after_modifiers;
            total_wounds += attack.wounds_dealt;

            if (attack.has_bane && attack.wounds_dealt > 0) {
                any_bane = true;
            }
        }
    }

    result.total_wounds = total_wounds;

    // Allocate wounds
    result.wound_allocation = allocate_wounds(defender, total_wounds, !any_bane);
    result.defender_models_killed = result.wound_allocation.models_killed;
    result.defender_models_remaining = defender.alive_count;
    result.attacker_models_remaining = attacker.alive_count;

    return result;
}

inline AttackResult CombatResolver::resolve_weapon_attack(
    const Model& model,
    const Weapon& weapon,
    Unit& defender,
    const Unit& attacker_unit,
    const CombatContext& ctx
) {
    AttackResult result;
    result.attacks_made = weapon.attacks;

    // Get faction combat modifiers
    auto& faction_applicator = get_faction_applicator();
    FactionCombatModifiers attack_mods = faction_applicator.calculate_attack_modifiers(
        attacker_unit, defender, ctx.phase, ctx.is_charging
    );
    FactionCombatModifiers defense_mods = faction_applicator.calculate_defense_modifiers(
        defender, attacker_unit, ctx.phase
    );

    // Handle fatigued (only hits on 6s)
    if (ctx.attacker_fatigued && ctx.phase == CombatPhase::Melee) {
        auto [hits, sixes] = dice_.roll_quality_test(weapon.attacks, 6, 0);
        result.hits = sixes;  // Only 6s hit when fatigued
        result.sixes_rolled = sixes;
    } else {
        // Calculate quality modifier including faction modifiers
        i8 quality_mod = ctx.attacker_shaken ? -1 : 0;
        quality_mod += attack_mods.hit_modifier;
        u8 effective_quality = model.quality;

        // Check for Reliable (Quality 2+)
        if (weapon.has_rule(RuleId::Reliable)) {
            u8 val = weapon.get_rule_value(RuleId::Reliable);
            effective_quality = val > 0 ? val : 2;
        }

        // Check for Precise (+1 to hit)
        if (weapon.has_rule(RuleId::Precise)) {
            quality_mod += 1;
        }

        // Check for GoodShot (+1 to hit when shooting)
        if (attacker_unit.has_rule(RuleId::GoodShot) && ctx.phase == CombatPhase::Shooting) {
            quality_mod += 1;
        }

        // Check for BadShot (-1 to hit when shooting)
        if (attacker_unit.has_rule(RuleId::BadShot) && ctx.phase == CombatPhase::Shooting) {
            quality_mod -= 1;
        }

        // Apply extra attacks from faction rules
        u8 total_attacks = weapon.attacks + attack_mods.extra_attacks;

        auto [hits, sixes] = dice_.roll_quality_test(total_attacks, effective_quality, quality_mod);
        result.hits = hits;
        result.sixes_rolled = sixes;

        // Apply Predator Fighter (6s in melee generate extra attacks)
        if (ctx.phase == CombatPhase::Melee &&
            (attacker_unit.has_rule(RuleId::PredatorFighter) ||
             attack_mods.has_granted_rule(RuleId::PredatorFighter))) {
            u8 extra_hits = apply_predator_fighter(dice_, sixes, attacker_unit, weapon.attacks, model.quality);
            result.hits += extra_hits;
        }
    }

    // Apply hit modifiers
    result.hits_after_modifiers = apply_hit_modifiers(
        result.hits, result.sixes_rolled, weapon, attacker_unit, defender, ctx.is_charging
    );

    // Add extra hits from faction rules
    result.hits_after_modifiers += attack_mods.extra_hits;

    // Calculate AP including faction modifiers
    u8 effective_ap = calculate_ap(weapon, attacker_unit, ctx.is_charging);
    effective_ap += static_cast<u8>(std::max(0, static_cast<int>(attack_mods.ap_modifier)));

    // Get defender stats
    if (defender.alive_count == 0) {
        result.wounds_dealt = 0;
        return result;
    }

    u8 defense = defender.get_base_defense();
    i8 defense_mod = 0;
    if (ctx.in_cover) defense_mod += 1;
    if (ctx.defender_shaken) defense_mod -= 1;

    // Apply Shielded (+1 defense vs non-spell hits)
    defense_mod += apply_shielded(defender);

    // Apply faction defense modifiers
    defense_mod += defense_mods.defense_modifier;

    // Apply MeleeEvasion/MeleeShrouding (-1 to be hit in melee)
    if (ctx.phase == CombatPhase::Melee) {
        if (defender.has_rule(RuleId::MeleeEvasion) ||
            defender.has_rule(RuleId::MeleeShrouding)) {
            // This affects attacker's hit roll, but we simulate it as defense bonus
            defense_mod += 1;
        }
    }

    // Apply RangedShrouding (-1 to be hit when shooting)
    if (ctx.phase == CombatPhase::Shooting &&
        defender.has_rule(RuleId::RangedShrouding)) {
        defense_mod += 1;
    }

    // Check for Poison
    bool has_poison = weapon.has_rule(RuleId::Poison);

    // Roll defense
    u16 wounds = dice_.roll_defense_test(
        result.hits_after_modifiers, defense, effective_ap, defense_mod, has_poison
    );

    // Apply wound modifiers
    result.wounds_dealt = apply_wound_modifiers(wounds, weapon);

    // Apply Rupture extra wounds (extra wound on unmodified 6 to hit)
    if (attacker_unit.has_rule(RuleId::Rupture) || weapon.has_rule(RuleId::Rupture)) {
        result.wounds_dealt += apply_rupture_extra_wounds(result.sixes_rolled, attacker_unit);
    }

    // Apply extra wounds from faction rules
    result.wounds_dealt += attack_mods.extra_wounds;

    // Apply Resistance (6+ to ignore wounds)
    result.wounds_dealt = apply_resistance(dice_, result.wounds_dealt, defender);

    // Check for Bane (bypasses regeneration)
    result.has_bane = weapon.has_rule(RuleId::Bane) ||
                      attacker_unit.has_rule(RuleId::Unstoppable) ||
                      attack_mods.ignores_regeneration ||
                      attacker_unit.has_rule(RuleId::Rupture) ||
                      weapon.has_rule(RuleId::Rupture);

    return result;
}

inline u8 CombatResolver::calculate_ap(const Weapon& weapon, const Unit& unit, bool is_charging) {
    u8 ap = weapon.ap;

    // Lance: +2 AP when charging
    if (is_charging && weapon.has_rule(RuleId::Lance)) {
        u8 val = weapon.get_rule_value(RuleId::Lance);
        ap += (val > 0) ? val : 2;
    }

    return ap;
}

inline u16 CombatResolver::apply_hit_modifiers(
    u16 hits,
    u8 sixes,
    const Weapon& weapon,
    const Unit& attacker,
    const Unit& defender,
    bool is_charging
) {
    u16 modified = hits;

    // Furious: extra hits on 6s when charging
    if (is_charging) {
        if (attacker.has_rule(RuleId::Furious)) {
            modified += sixes;
        }
    }

    // Blast(X): multiply hits (capped at defender model count)
    if (weapon.has_rule(RuleId::Blast)) {
        u8 blast_val = weapon.get_rule_value(RuleId::Blast);
        u8 multiplier = std::min(blast_val, defender.model_count);
        modified *= multiplier;
    }

    return modified;
}

inline u16 CombatResolver::apply_wound_modifiers(u16 wounds, const Weapon& weapon) {
    if (wounds == 0) return 0;

    // Deadly(X): multiply wounds
    if (weapon.has_rule(RuleId::Deadly)) {
        u8 deadly_val = weapon.get_rule_value(RuleId::Deadly);
        wounds *= deadly_val;
    }

    return wounds;
}

inline WoundAllocationResult CombatResolver::allocate_wounds(
    Unit& defender,
    u16 wounds,
    bool allow_regeneration
) {
    WoundAllocationResult result;
    if (wounds == 0) return result;

    u16 remaining = wounds;
    std::array<u8, MAX_MODELS_PER_UNIT> order;
    u8 order_count;

    while (remaining > 0) {
        defender.get_wound_allocation_order(order, order_count);
        if (order_count == 0) break;

        Model& target = defender.models[order[0]];

        // Check Regeneration (5+)
        if (allow_regeneration && (target.has_rule(RuleId::Regeneration) ||
                                   defender.has_rule(RuleId::Regeneration))) {
            u8 regen_target = 5;
            if (target.has_rule(RuleId::Regeneration)) {
                u8 val = target.get_rule_value(RuleId::Regeneration);
                if (val > 0) regen_target = val;
            }

            // Roll for each wound
            u16 wounds_to_try = std::min(remaining, static_cast<u16>(target.remaining_wounds()));
            remaining = dice_.roll_regeneration(wounds_to_try, regen_target);
            result.wounds_regenerated += wounds_to_try - remaining;
        }

        // Apply wounds to model
        while (remaining > 0 && target.is_alive()) {
            bool died = target.apply_wound();
            result.wounds_allocated++;
            remaining--;

            if (died) {
                result.models_killed++;
                defender.update_alive_count();
                break;
            }
        }
    }

    result.overkill_wounds = remaining;
    return result;
}

// ==============================================================================
// UnitView-based implementations (lightweight simulation path)
// ==============================================================================

inline CombatResult CombatResolver::resolve_attack(
    UnitView& attacker,
    UnitView& defender,
    const CombatContext& ctx
) {
    CombatResult result;
    bool any_bane = false;
    u16 total_wounds = 0;

    // Iterate through alive models
    for (u8 i = 0; i < attacker.model_count(); ++i) {
        if (!attacker.model_is_alive(i)) continue;
        const Model& model = attacker.get_model(i);

        // Get weapons for this phase
        for (u8 w = 0; w < model.weapon_count; ++w) {
            const Weapon& weapon = weapons_.get(model.weapons[w].index);

            // Check if weapon is appropriate for phase
            bool use_weapon = (ctx.phase == CombatPhase::Melee && weapon.is_melee()) ||
                              (ctx.phase == CombatPhase::Shooting && weapon.is_ranged());
            if (!use_weapon) continue;

            AttackResult attack = resolve_weapon_attack_view(model, weapon, defender, *attacker.unit, ctx);
            result.total_hits += attack.hits_after_modifiers;
            total_wounds += attack.wounds_dealt;

            if (attack.has_bane && attack.wounds_dealt > 0) {
                any_bane = true;
            }
        }
    }

    result.total_wounds = total_wounds;

    // Allocate wounds
    result.wound_allocation = allocate_wounds_view(defender, total_wounds, !any_bane);
    result.defender_models_killed = result.wound_allocation.models_killed;
    result.defender_models_remaining = defender.alive_count();
    result.attacker_models_remaining = attacker.alive_count();

    return result;
}

inline AttackResult CombatResolver::resolve_weapon_attack_view(
    const Model& model,
    const Weapon& weapon,
    UnitView& defender,
    const Unit& attacker_unit,
    const CombatContext& ctx
) {
    AttackResult result;
    result.attacks_made = weapon.attacks;

    // Handle fatigued (only hits on 6s)
    if (ctx.attacker_fatigued && ctx.phase == CombatPhase::Melee) {
        auto [hits, sixes] = dice_.roll_quality_test(weapon.attacks, 6, 0);
        result.hits = sixes;
        result.sixes_rolled = sixes;
    } else {
        i8 quality_mod = ctx.attacker_shaken ? -1 : 0;
        u8 effective_quality = model.quality;

        if (weapon.has_rule(RuleId::Reliable)) {
            u8 val = weapon.get_rule_value(RuleId::Reliable);
            effective_quality = val > 0 ? val : 2;
        }

        if (weapon.has_rule(RuleId::Precise)) {
            quality_mod += 1;
        }

        auto [hits, sixes] = dice_.roll_quality_test(weapon.attacks, effective_quality, quality_mod);
        result.hits = hits;
        result.sixes_rolled = sixes;
    }

    result.hits_after_modifiers = apply_hit_modifiers_view(
        result.hits, result.sixes_rolled, weapon, attacker_unit, defender, ctx.is_charging
    );

    u8 effective_ap = calculate_ap(weapon, attacker_unit, ctx.is_charging);

    if (defender.alive_count() == 0) {
        result.wounds_dealt = 0;
        return result;
    }

    u8 defense = defender.defense();
    i8 defense_mod = 0;
    if (ctx.in_cover) defense_mod += 1;
    if (ctx.defender_shaken) defense_mod -= 1;

    bool has_poison = weapon.has_rule(RuleId::Poison);

    u16 wounds = dice_.roll_defense_test(
        result.hits_after_modifiers, defense, effective_ap, defense_mod, has_poison
    );

    result.wounds_dealt = apply_wound_modifiers(wounds, weapon);
    result.has_bane = weapon.has_rule(RuleId::Bane);

    return result;
}

inline u16 CombatResolver::apply_hit_modifiers_view(
    u16 hits,
    u8 sixes,
    const Weapon& weapon,
    const Unit& attacker,
    const UnitView& defender,
    bool is_charging
) {
    u16 modified = hits;

    if (is_charging) {
        if (attacker.has_rule(RuleId::Furious)) {
            modified += sixes;
        }
    }

    if (weapon.has_rule(RuleId::Blast)) {
        u8 blast_val = weapon.get_rule_value(RuleId::Blast);
        u8 multiplier = std::min(blast_val, defender.model_count());
        modified *= multiplier;
    }

    return modified;
}

inline WoundAllocationResult CombatResolver::allocate_wounds_view(
    UnitView& defender,
    u16 wounds,
    bool allow_regeneration
) {
    WoundAllocationResult result;
    if (wounds == 0) return result;

    u16 remaining = wounds;
    std::array<u8, MAX_MODELS_PER_UNIT> order;
    u8 order_count;

    while (remaining > 0) {
        defender.get_wound_allocation_order(order, order_count);
        if (order_count == 0) break;

        u8 target_idx = order[0];
        const Model& target_model = defender.get_model(target_idx);

        // Check Regeneration (5+)
        if (allow_regeneration && (target_model.has_rule(RuleId::Regeneration) ||
                                   defender.has_rule(RuleId::Regeneration))) {
            u8 regen_target = 5;
            if (target_model.has_rule(RuleId::Regeneration)) {
                u8 val = target_model.get_rule_value(RuleId::Regeneration);
                if (val > 0) regen_target = val;
            }

            u16 wounds_to_try = std::min(remaining, static_cast<u16>(defender.model_remaining_wounds(target_idx)));
            remaining = dice_.roll_regeneration(wounds_to_try, regen_target);
            result.wounds_regenerated += wounds_to_try - remaining;
        }

        // Apply wounds to model
        while (remaining > 0 && defender.model_is_alive(target_idx)) {
            bool died = defender.apply_wound_to_model(target_idx);
            result.wounds_allocated++;
            remaining--;

            if (died) {
                result.models_killed++;
                break;
            }
        }
    }

    result.overkill_wounds = remaining;
    return result;
}

} // namespace battle
