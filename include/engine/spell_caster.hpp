#pragma once

#include "core/types.hpp"
#include "core/spell.hpp"
#include "core/spell_registry.hpp"
#include "core/modifiers.hpp"
#include "simulation/sim_state.hpp"
#include "engine/dice.hpp"
#include "engine/match_logger.hpp"
#include <algorithm>

namespace battle {

// ==============================================================================
// Spell Caster - Handles spell casting phase logic
// ==============================================================================
// Implements Caster(X) rule:
// - Gets X spell tokens at round start (max 6)
// - Spend tokens = spell cost to cast (before attacking)
// - Roll 1d6, on 4+ resolve effect
// - Enemy casters can spend tokens for -1 to roll per token
// - Spells must be from caster's faction

class SpellCaster {
public:
    explicit SpellCaster(DiceRoller& dice, MatchLogger* logger = nullptr)
        : dice_(dice), logger_(logger) {
        // Initialize spell registry
        SpellRegistry::instance().initialize();
    }

    // Grant spell tokens at round start (even for units in reserve)
    void grant_round_tokens(UnitSimState& state, bool is_unit_a) {
        if (!state.is_caster()) return;

        u8 old_tokens = state.spell_tokens;
        state.grant_spell_tokens();
        u8 new_tokens = state.spell_tokens;

        if (logger_ && new_tokens > old_tokens) {
            logger_->on_spell_tokens_granted(
                is_unit_a,
                new_tokens - old_tokens,
                new_tokens,
                state.caster_value);
        }
    }

    // Process spell phase for a unit (after movement, before attack)
    // Returns number of spells successfully cast
    u8 process_spell_phase(
        UnitView& caster,
        UnitView& enemy,
        i8 distance,
        bool is_caster_unit_a
    ) {
        if (!caster.is_caster()) return 0;
        if (caster.spell_tokens() == 0) return 0;
        if (caster.is_out_of_action()) return 0;

        // Get faction spell list
        const FactionSpellList* spell_list = SpellRegistry::instance().get_faction_spells(
            caster.unit->faction.view());
        if (!spell_list || spell_list->spell_count == 0) return 0;

        u8 spells_cast = 0;
        u8 spells_succeeded = 0;

        // AI spell selection loop: keep casting until out of tokens or no valid spells
        while (caster.spell_tokens() > 0) {
            // Select best spell using AI priority
            i8 selected_spell = select_spell_ai(caster, *spell_list, distance);
            if (selected_spell < 0) break;  // No valid spell found

            const SpellDef* spell = spell_list->get_spell(static_cast<u8>(selected_spell));
            if (!spell) break;

            // Attempt to cast the spell
            SpellCastResult result = attempt_cast(
                caster, enemy, *spell, static_cast<u8>(selected_spell),
                distance, is_caster_unit_a);

            if (result.attempted) {
                spells_cast++;
                if (result.success) {
                    spells_succeeded++;
                }
            }
        }

        if (logger_ && spells_cast > 0) {
            logger_->on_spell_phase_end(
                is_caster_unit_a,
                spells_cast,
                spells_succeeded,
                caster.spell_tokens());
        }

        return spells_succeeded;
    }

private:
    DiceRoller& dice_;
    MatchLogger* logger_;

    // AI spell selection priority:
    // 1. Direct damage in range -> highest cost first
    // 2. Non-direct enemy targeting in range -> highest cost first
    // 3. Self-targeting -> highest cost first
    i8 select_spell_ai(
        const UnitView& caster,
        const FactionSpellList& spell_list,
        i8 distance
    ) {
        u8 tokens = caster.spell_tokens();
        i8 best_direct = -1;
        i8 best_enemy = -1;
        i8 best_self = -1;
        u8 best_direct_cost = 0;
        u8 best_enemy_cost = 0;
        u8 best_self_cost = 0;

        for (u8 i = 0; i < spell_list.spell_count; ++i) {
            const SpellDef* spell = spell_list.get_spell(i);
            if (!spell) continue;

            // Skip if already attempted
            if (caster.is_spell_attempted(i)) continue;

            // Skip if not enough tokens
            if (spell->cost > tokens) continue;

            // Check range for enemy-targeting spells
            bool in_range = (spell->range >= static_cast<u8>(distance));

            if (spell->is_direct_damage() && in_range) {
                // Priority 1: Direct damage
                if (spell->cost > best_direct_cost) {
                    best_direct = i;
                    best_direct_cost = spell->cost;
                }
            } else if (spell->targets_enemy() && in_range) {
                // Priority 2: Non-direct enemy targeting
                if (spell->cost > best_enemy_cost) {
                    best_enemy = i;
                    best_enemy_cost = spell->cost;
                }
            } else if (spell->targets_self()) {
                // Priority 3: Self targeting (no range check needed)
                if (spell->cost > best_self_cost) {
                    best_self = i;
                    best_self_cost = spell->cost;
                }
            }
        }

        // Return highest priority spell found
        if (best_direct >= 0) return best_direct;
        if (best_enemy >= 0) return best_enemy;
        if (best_self >= 0) return best_self;
        return -1;  // No valid spell
    }

    // Attempt to cast a spell
    SpellCastResult attempt_cast(
        UnitView& caster,
        UnitView& enemy,
        const SpellDef& spell,
        u8 spell_index,
        i8 distance,
        bool is_caster_unit_a
    ) {
        (void)distance;  // May be used for range checks in future

        SpellCastResult result;

        // Check if spell was already attempted
        if (caster.is_spell_attempted(spell_index)) {
            return result;
        }

        // Check if enough tokens
        if (caster.spell_tokens() < spell.cost) {
            return result;
        }

        // Mark spell as attempted and spend tokens
        caster.mark_spell_attempted(spell_index);
        caster.spend_spell_tokens(spell.cost);
        result.attempted = true;
        result.tokens_spent = spell.cost;

        if (logger_) {
            const char* target_type = spell.is_direct_damage() ? "enemy_direct" :
                                      spell.targets_enemy() ? "enemy_debuff" : "self_buff";
            logger_->on_spell_cast_attempt(
                is_caster_unit_a,
                spell.name.c_str(),
                spell.cost,
                caster.spell_tokens(),
                static_cast<i8>(spell.range),
                target_type);
        }

        // Enemy interference: AI spends 1 token if available
        i8 modifier = 0;
        if (enemy.is_caster() && enemy.spell_tokens() > 0 && !enemy.is_out_of_action()) {
            // Enemy spends 1 token for -1 to the roll
            enemy.spend_spell_tokens(1);
            modifier = -1;
            result.enemy_tokens_spent = 1;

            if (logger_) {
                logger_->on_spell_interference(!is_caster_unit_a, 1, -1);
            }
        }

        // Roll to cast (need 4+ with modifier)
        u8 roll = dice_.roll_d6();
        u8 target = static_cast<u8>(std::max(2, SPELL_CAST_TARGET - modifier));
        result.roll = roll;
        result.target_number = target;
        result.modifier = modifier;
        result.success = (roll >= target);

        if (logger_) {
            logger_->on_spell_roll(roll, target, modifier, result.success);
        }

        // Apply spell effect if successful
        if (result.success) {
            apply_spell_effect(caster, enemy, spell, result, is_caster_unit_a);
        }

        return result;
    }

    // Apply the spell's effect
    void apply_spell_effect(
        UnitView& caster,
        UnitView& enemy,
        const SpellDef& spell,
        SpellCastResult& result,
        bool is_caster_unit_a
    ) {
        (void)is_caster_unit_a;  // May be used for side-specific effects in future

        const char* effect_type = "none";
        const char* buff_applied = "";

        switch (spell.effect_type) {
            case SpellEffectType::DirectDamage: {
                effect_type = "direct_damage";
                // Apply hits to enemy
                if (spell.hits > 0 && !enemy.is_out_of_action()) {
                    result.hits_dealt = spell.hits;

                    // Roll defense for the hits
                    u8 ap = spell.hit_rules.ap;
                    u8 defense = enemy.defense();

                    u32 wounds = dice_.roll_defense_test(
                        spell.hits,
                        defense,
                        ap,
                        0,  // No reroll sixes for spells
                        false);

                    result.wounds_dealt = static_cast<u8>(wounds);

                    // Apply wounds
                    if (wounds > 0) {
                        apply_spell_wounds(enemy, wounds, result, spell);
                    }
                }
                break;
            }

            case SpellEffectType::Buff: {
                effect_type = "buff";
                // Apply buff to caster's unit
                if (spell.grants_rule != RuleId::None) {
                    ActiveModifier mod(spell.grants_rule, 1);
                    mod.source = ModifierSource::BUFF;
                    mod.duration = ModifierDuration::UNTIL_END_OF_ROUND;
                    caster.modifiers().add_modifier(mod);
                    buff_applied = "rule_granted";
                }
                if (spell.hit_modifier > 0) {
                    // Use Precise for +1 to hit
                    ActiveModifier mod(RuleId::Precise, 1);
                    mod.source = ModifierSource::BUFF;
                    mod.duration = ModifierDuration::UNTIL_END_OF_ROUND;
                    caster.modifiers().add_modifier(mod);
                    buff_applied = "hit_modifier";
                }
                if (spell.defense_modifier > 0) {
                    // Use Shielded for +1 to defense
                    ActiveModifier mod(RuleId::Shielded, 1);
                    mod.source = ModifierSource::BUFF;
                    mod.duration = ModifierDuration::UNTIL_END_OF_ROUND;
                    caster.modifiers().add_modifier(mod);
                    buff_applied = "defense_modifier";
                }
                break;
            }

            case SpellEffectType::Debuff: {
                effect_type = "debuff";
                // Apply debuff to enemy
                if (spell.hit_modifier < 0) {
                    // Use BadShot for -1 to hit (applied to enemy)
                    ActiveModifier mod(RuleId::BadShot, 1);
                    mod.source = ModifierSource::DEBUFF;
                    mod.duration = ModifierDuration::UNTIL_END_OF_ROUND;
                    enemy.modifiers().add_modifier(mod);
                    buff_applied = "enemy_hit_debuff";
                }
                // Note: defense_modifier debuffs are not yet implemented
                // They would require adding special rules to the combat resolver
                if (spell.defense_modifier < 0) {
                    buff_applied = "defense_debuff_pending";
                }
                break;
            }

            case SpellEffectType::Morale: {
                effect_type = "morale";
                // Force morale test on enemy
                if (!enemy.is_out_of_action() && !enemy.is_shaken()) {
                    u8 roll = dice_.roll_d6();
                    if (roll < enemy.quality()) {
                        enemy.become_shaken();
                        buff_applied = "enemy_shaken";
                    }
                }
                break;
            }

            case SpellEffectType::Movement:
                effect_type = "movement";
                // Movement effects are handled during movement phase
                buff_applied = "movement_effect";
                break;
        }

        if (logger_) {
            logger_->on_spell_effect(
                spell.name.c_str(),
                effect_type,
                result.hits_dealt,
                result.wounds_dealt,
                result.models_killed,
                buff_applied);
        }
    }

    // Apply wounds from spell to target
    void apply_spell_wounds(
        UnitView& target,
        u32 wounds,
        SpellCastResult& result,
        const SpellDef& spell
    ) {
        (void)spell;  // May be used for spell-specific wound effects in future

        // Get wound allocation order
        std::array<u8, MAX_MODELS_PER_UNIT> allocation_order;
        u8 allocation_count;
        target.get_wound_allocation_order(allocation_order, allocation_count);

        u32 remaining_wounds = wounds;
        u8 model_idx = 0;

        while (remaining_wounds > 0 && model_idx < allocation_count) {
            u8 target_model = allocation_order[model_idx];
            const Model& model = target.get_model(target_model);

            if (!target.model_is_alive(target_model)) {
                model_idx++;
                continue;
            }

            // Apply wounds to model
            u8 wounds_to_apply = static_cast<u8>(std::min(
                static_cast<u32>(model.tough - target.model_wounds_taken(target_model)),
                remaining_wounds));

            for (u8 w = 0; w < wounds_to_apply; ++w) {
                if (target.apply_wound_to_model(target_model)) {
                    result.models_killed++;
                }
            }

            remaining_wounds -= wounds_to_apply;
            model_idx++;
        }
    }
};

} // namespace battle
