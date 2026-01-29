#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "engine/dice.hpp"
#include "engine/match_logger.hpp"
#include "simulation/sim_state.hpp"
#include <algorithm>
#include <string>

namespace battle {

// Forward declare CombatResult (defined in game_state.hpp)
struct CombatResult;

// ==============================================================================
// Combat Engine - DEPRECATED
// ==============================================================================
// This class is DEPRECATED as of Phase 8 of the Rule Registry Architecture.
// Use RegistryCombatResolver instead (from registry_combat_resolver.hpp).
//
// The old CombatEngine uses hardcoded conditional checks for each rule,
// which doesn't scale well as new rules are added. The new registry-based
// resolver uses a data-driven approach with rule effects defined in a
// central registry.
//
// Migration guide:
//   OLD: CombatEngine engine(dice, logger);
//        auto result = engine.resolve_shooting(attacker, defender, distance, moved);
//
//   NEW: RuleRegistry registry = create_default_registry();
//        RegistryCombatResolver resolver(registry, dice, logger);
//        auto result = resolver.resolve_combat(attacker, defender, weapon,
//                                               CombatType::SHOOTING, distance, false);
//
// This file will be removed in a future release.
// ==============================================================================

class [[deprecated("Use RegistryCombatResolver instead - see registry_combat_resolver.hpp")]] CombatEngine {
public:
    explicit CombatEngine(DiceRoller& dice, MatchLogger* logger = nullptr)
        : dice_(dice), logger_(logger) {}

    // Resolve shooting attack
    CombatResult resolve_shooting(UnitView attacker, UnitView defender, i8 distance, bool moved) {
        CombatResult result;

        // Collect all ranged weapons in range
        u32 total_attacks = 0;
        u8 models_shooting = attacker.alive_count();
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (w.is_ranged() && w.range >= static_cast<u8>(distance)) {
                total_attacks += w.attacks;
            }
        }

        if (total_attacks == 0) return result;

        if (logger_) {
            logger_->on_shooting_start(true, attacker.unit->name.c_str(),
                                       defender.unit->name.c_str(), distance, models_shooting);
        }

        // Process each weapon
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (!w.is_ranged() || w.range < static_cast<u8>(distance)) continue;

            // Limited: skip if already used this game
            if (w.has_rule(RuleId::Limited)) {
                if (attacker.is_limited_weapon_used(i)) {
                    if (logger_) logger_->on_rule_triggered("Limited", "weapon_already_used", i);
                    continue;
                }
                attacker.mark_limited_weapon_used(i);
                if (logger_) logger_->on_rule_triggered("Limited", "using_one_time_weapon", i);
            }

            // Calculate total attacks: min(weapon_count, alive_models) * attacks_per_model
            u8 models_with_weapon = std::min(w.count, models_shooting);
            u32 attacks = static_cast<u32>(models_with_weapon) * w.attacks;
            if (attacks == 0) continue;

            // Log weapon attack start
            if (logger_) {
                std::string rules_str = get_weapon_rules_str(w);
                logger_->on_weapon_attack_start(w.name.c_str(), false, w.range, static_cast<u8>(distance), w.attacks, w.ap, rules_str.c_str());
                logger_->on_attack_count(models_with_weapon, w.attacks, attacks);
            }

            // Roll to hit
            u8 base_quality = attacker.quality();
            u8 quality = base_quality;
            i8 hit_modifier = 0;

            // VersatileAttack: roll d6 to choose AP+1 (1-3) or +1 hit (4-6)
            u8 versatile_ap_bonus = 0;
            if (attacker.has_rule(RuleId::VersatileAttack)) {
                u8 versatile_roll = dice_.roll_d6();
                if (versatile_roll <= 3) {
                    versatile_ap_bonus = 1;
                    if (logger_) logger_->on_rule_triggered("VersatileAttack", "rolled_ap+1", versatile_roll);
                } else {
                    hit_modifier += 1;
                    if (logger_) logger_->on_hit_modifier("VersatileAttack", +1, "rolled_+1_hit");
                }
            }

            // Reliable: Quality becomes 2+
            if (w.has_rule(RuleId::Reliable)) {
                quality = 2;
                if (logger_) logger_->on_hit_modifier("Reliable", 0, "quality_becomes_2+");
            }

            // Stealth: -1 to hit from >9"
            if (defender.has_rule(RuleId::Stealth) && distance > 9) {
                hit_modifier -= 1;
                if (logger_) logger_->on_hit_modifier("Stealth", -1, "target_beyond_9\"");
            }

            // RangedShrouding: -1 to be hit when shot
            if (defender.has_rule(RuleId::RangedShrouding)) {
                hit_modifier -= 1;
                if (logger_) logger_->on_hit_modifier("RangedShrouding", -1, "harder_to_hit_at_range");
            }

            // Precise: +1 to hit (weapon rule)
            if (w.has_rule(RuleId::Precise)) {
                hit_modifier += 1;
                if (logger_) logger_->on_hit_modifier("Precise", +1, "weapon_accuracy");
            }

            // GoodShot: +1 to hit when shooting (unit rule)
            if (attacker.has_rule(RuleId::GoodShot)) {
                hit_modifier += 1;
                if (logger_) logger_->on_hit_modifier("GoodShot", +1, "skilled_shooter");
            }

            // BadShot: -1 to hit when shooting (unit rule)
            if (attacker.has_rule(RuleId::BadShot)) {
                hit_modifier -= 1;
                if (logger_) logger_->on_hit_modifier("BadShot", -1, "poor_shooter");
            }

            // Purge: +1 to hit vs Tough(3+)
            u8 defender_tough = defender.get_rule_value(RuleId::Tough);
            if (w.has_rule(RuleId::Purge) && defender_tough >= 3) {
                hit_modifier += 1;
                if (logger_) logger_->on_hit_modifier("Purge", +1, "targeting_tough_3+");
            }

            // Enable roll recording for logging
            if (logger_) dice_.enable_roll_recording(true);

            auto hit_result = dice_.roll_quality_test(attacks, quality, hit_modifier);
            u32 hits = hit_result.hits;
            u32 sixes = hit_result.sixes;

            // Log hit rolls
            if (logger_) {
                std::vector<u8> hit_rolls = dice_.take_recorded_rolls();
                i8 effective = static_cast<i8>(quality) - hit_modifier;
                effective = std::max(i8(2), std::min(i8(6), effective));
                logger_->on_hit_rolls(base_quality, hit_modifier, static_cast<u8>(effective), hit_rolls, hits, sixes);
            }

            // Rending: 6s to hit get AP(+4) - track separately
            bool has_rending = w.has_rule(RuleId::Rending);
            u32 rending_hits = has_rending ? sixes : 0;

            // Rupture: 6s to hit deal +1 wound per wound and bypass regen
            // Note: Rupture and Rending can stack - 6s get both AP+4 AND +1 wound
            bool has_rupture = w.has_rule(RuleId::Rupture);
            u32 rupture_hits = has_rupture ? sixes : 0;

            // If both Rending and Rupture, the 6s go to rending (AP+4) and also get rupture bonus
            u32 normal_hits = hits - rending_hits;
            u32 bonus_hits = 0;

            // Relentless: extra hits on 6s when shooting >9" (no movement restriction per rules)
            if (attacker.has_rule(RuleId::Relentless) && distance > 9) {
                bonus_hits += sixes;
                hits += sixes;
                if (logger_) logger_->on_rule_triggered("Relentless", "extra_hits_on_6s", sixes);
            }

            // Surge: extra hits on 6s to hit
            if (w.has_rule(RuleId::Surge)) {
                bonus_hits += sixes;
                hits += sixes;
                if (logger_) logger_->on_rule_triggered("Surge", "extra_hits_on_6s", sixes);
            }

            // PointBlankSurge: extra hits on 6s at 0-9" range
            if (attacker.has_rule(RuleId::PointBlankSurge) && distance <= 9) {
                bonus_hits += sixes;
                hits += sixes;
                if (logger_) logger_->on_rule_triggered("PointBlankSurge", "extra_hits_on_6s_at_close_range", sixes);
            }

            // Takedown: treats target as single model (Blast capped at 1)
            bool has_takedown = w.has_rule(RuleId::Takedown);
            i8 takedown_target_idx = -1;  // -1 = normal allocation
            if (has_takedown) {
                // AI picks most valuable target (hero > wounded tough > first alive)
                for (u8 m = 0; m < defender.model_count(); ++m) {
                    if (defender.model_is_alive(m)) {
                        const Model& model = defender.get_model(m);
                        if (model.is_hero) {
                            takedown_target_idx = m;
                            break;  // Heroes are highest priority
                        }
                        if (takedown_target_idx < 0 || defender.model_wounds_taken(m) > 0) {
                            takedown_target_idx = m;  // Wounded models or first alive
                        }
                    }
                }
                if (logger_ && takedown_target_idx >= 0) {
                    logger_->on_rule_triggered("Takedown", "targeting_specific_model", takedown_target_idx);
                }
            }

            // Blast: multiply hits by X, where X is capped at target model count
            // Takedown: Blast is capped at 1 (treating target as single model)
            u8 blast_value = w.get_rule_value(RuleId::Blast);
            if (blast_value > 0) {
                u8 max_multiplier = has_takedown ? u8(1) : static_cast<u8>(defender.alive_count());
                u8 multiplier = std::min(blast_value, max_multiplier);
                u32 old_hits = hits;
                hits *= multiplier;
                // Rending and Rupture hits also multiply with Blast
                rending_hits *= multiplier;
                rupture_hits *= multiplier;
                normal_hits = hits - rending_hits;
                if (logger_) logger_->on_rule_triggered("Blast", "multiplied_hits", hits - old_hits);
            }

            // Log hits after modifiers
            if (logger_) {
                logger_->on_hits_after_modifiers(normal_hits, rending_hits, hits);
            }

            // Roll defense for normal hits
            u8 ap = w.ap;

            // VersatileAttack AP bonus (if rolled earlier)
            ap += versatile_ap_bonus;

            bool poison = w.has_rule(RuleId::Poison);
            bool has_bane = w.has_rule(RuleId::Bane);
            // Bane: reroll defense 6s (like Poison)
            bool reroll_def_sixes = poison || has_bane;

            // Calculate effective defense with modifiers
            u8 effective_defense = defender.defense();

            // Shielded: +1 to Defense vs non-spell attacks (easier saves)
            if (defender.has_rule(RuleId::Shielded)) {
                effective_defense = std::max(u8(2), static_cast<u8>(effective_defense - 1));
                if (logger_) logger_->on_defense_modifier("Shielded", -1, "easier_saves_vs_shooting");
            }

            // Protected: 6+ to reduce AP by 1 (roll for each hit)
            u8 protected_ap_reduction = 0;
            if (defender.has_rule(RuleId::Protected) && ap > 0 && normal_hits > 0) {
                u32 protected_successes = dice_.roll_regeneration(normal_hits, 6);  // Uses same 6+ logic
                if (protected_successes > 0) {
                    protected_ap_reduction = 1;
                    if (logger_) logger_->on_rule_triggered("Protected", "reduced_ap_by_1", protected_successes);
                }
            }
            u8 final_ap = (ap > protected_ap_reduction) ? (ap - protected_ap_reduction) : 0;

            // Shred: track 1s on defense rolls for bonus wounds
            bool has_shred = w.has_rule(RuleId::Shred);
            u32 shred_bonus_wounds = 0;

            if (logger_) dice_.enable_roll_recording(true);
            u32 wounds_from_normal = 0;
            if (has_shred && normal_hits > 0) {
                auto def_result = dice_.roll_defense_test_with_ones(normal_hits, effective_defense, final_ap, 0, reroll_def_sixes);
                wounds_from_normal = def_result.wounds;
                shred_bonus_wounds += def_result.ones;
                if (def_result.ones > 0 && logger_) {
                    logger_->on_rule_triggered("Shred", "bonus_wounds_from_1s", def_result.ones);
                }
            } else {
                wounds_from_normal = dice_.roll_defense_test(normal_hits, effective_defense, final_ap, 0, reroll_def_sixes);
            }

            // Log defense rolls for normal hits
            if (logger_) {
                std::vector<u8> all_rolls = dice_.take_recorded_rolls();
                i8 effective = static_cast<i8>(effective_defense) + static_cast<i8>(final_ap);
                effective = std::max(i8(2), std::min(i8(6), effective));
                u32 saves = normal_hits - wounds_from_normal;

                // Split recorded rolls into initial rolls and rerolls
                // First normal_hits rolls are initial, rest are rerolls of 6s
                std::vector<u8> def_rolls;
                std::vector<u8> rerolls;
                u32 sixes_count = 0;

                if (reroll_def_sixes && all_rolls.size() > normal_hits) {
                    def_rolls.assign(all_rolls.begin(), all_rolls.begin() + normal_hits);
                    rerolls.assign(all_rolls.begin() + normal_hits, all_rolls.end());
                    sixes_count = static_cast<u32>(rerolls.size());
                } else {
                    def_rolls = std::move(all_rolls);
                }

                u32 reroll_saves = 0;
                for (u8 r : rerolls) {
                    reroll_saves += (r >= static_cast<u8>(effective));
                }

                logger_->on_defense_rolls(effective_defense, final_ap, static_cast<u8>(effective), reroll_def_sixes,
                                          def_rolls, saves, wounds_from_normal, sixes_count, rerolls, reroll_saves);
            }

            // Roll defense for rending hits (AP+4) - Rending adds AP(4) to base
            u32 wounds_from_rending = 0;
            if (rending_hits > 0) {
                u8 rending_ap = final_ap + 4;  // Rending adds +4 AP to (potentially reduced) base
                if (logger_) dice_.enable_roll_recording(true);
                if (has_shred) {
                    auto def_result = dice_.roll_defense_test_with_ones(rending_hits, effective_defense, rending_ap, 0, reroll_def_sixes);
                    wounds_from_rending = def_result.wounds;
                    shred_bonus_wounds += def_result.ones;
                    if (def_result.ones > 0 && logger_) {
                        logger_->on_rule_triggered("Shred", "bonus_wounds_from_1s_rending", def_result.ones);
                    }
                } else {
                    wounds_from_rending = dice_.roll_defense_test(rending_hits, effective_defense, rending_ap, 0, reroll_def_sixes);
                }

                if (logger_) {
                    std::vector<u8> rend_rolls = dice_.take_recorded_rolls();
                    i8 effective = static_cast<i8>(effective_defense) + static_cast<i8>(rending_ap);
                    effective = std::max(i8(2), std::min(i8(6), effective));
                    u32 saves = rending_hits - wounds_from_rending;
                    logger_->on_defense_rolls_rending(defender.defense(), rending_ap, static_cast<u8>(effective),
                                                      rend_rolls, saves, wounds_from_rending);
                }
            }

            // Disable roll recording
            if (logger_) dice_.enable_roll_recording(false);

            u32 total_wounds = wounds_from_normal + wounds_from_rending + shred_bonus_wounds;

            // Rupture: wounds from 6s to hit deal +1 wound each (double the wounds)
            // If weapon has Rending, the 6s went through rending path, so double wounds_from_rending
            // If weapon has Rupture without Rending, we need to estimate rupture wounds from normal
            u32 rupture_bonus_wounds = 0;
            if (has_rupture) {
                if (has_rending) {
                    // With Rending, all 6s went to rending_hits, so double those wounds
                    rupture_bonus_wounds = wounds_from_rending;
                    if (logger_ && rupture_bonus_wounds > 0) {
                        logger_->on_rule_triggered("Rupture", "bonus_wounds_from_6s", rupture_bonus_wounds);
                    }
                } else {
                    // Without Rending, rupture_hits were part of normal_hits
                    // We need to estimate based on the proportion of rupture_hits to normal_hits
                    if (normal_hits > 0 && rupture_hits > 0) {
                        // Calculate proportion of 6s in normal hits and apply to wounds
                        // This is an approximation - rupture wounds = wounds * (rupture_hits / normal_hits)
                        u32 estimated_rupture_wounds = (wounds_from_normal * rupture_hits + normal_hits - 1) / normal_hits;
                        estimated_rupture_wounds = std::min(estimated_rupture_wounds, wounds_from_normal);
                        rupture_bonus_wounds = estimated_rupture_wounds;
                        if (logger_ && rupture_bonus_wounds > 0) {
                            logger_->on_rule_triggered("Rupture", "bonus_wounds_from_6s", rupture_bonus_wounds);
                        }
                    }
                }
                total_wounds += rupture_bonus_wounds;
            }

            // Deadly: handled separately in apply_wounds_deadly
            u8 deadly_value = w.get_rule_value(RuleId::Deadly);

            // Determine if regeneration is bypassed (Bane, Rending, Rupture, Shred, or Unstoppable)
            bool bypass_regen = has_bane || has_rending || has_rupture || has_shred || w.has_rule(RuleId::Unstoppable);

            // Apply wounds to defender
            u8 weapon_models_killed = 0;
            if (total_wounds > 0) {
                WoundResult wound_result;
                if (deadly_value > 1) {
                    // Deadly wounds don't carry over - apply per-wound with multiplier
                    // Note: Takedown not compatible with Deadly (would be redundant)
                    wound_result = apply_wounds_deadly(defender, total_wounds, deadly_value, bypass_regen);
                } else {
                    wound_result = apply_wounds(defender, total_wounds, bypass_regen, takedown_target_idx);
                }
                result.wounds_dealt += wound_result.wounds_dealt;
                result.models_killed += wound_result.models_killed;
                weapon_models_killed = wound_result.models_killed;
            }

            if (logger_) {
                logger_->on_weapon_attack_end(w.name.c_str(), total_wounds, weapon_models_killed);
            }
        }

        result.target_destroyed = defender.is_destroyed();
        result.target_shaken = defender.is_shaken();
        result.target_routed = defender.is_routed();

        if (logger_) {
            logger_->on_shooting_end(true, result.wounds_dealt, result.models_killed, result.target_destroyed);
        }

        return result;
    }

    // Resolve melee attack
    // counter_models: number of models with Counter in defender (reduces Impact)
    CombatResult resolve_melee(UnitView attacker, UnitView defender, bool is_charging, u8 counter_models = 0) {
        CombatResult result;

        // Impact: separate roll hitting on 2+ when charging (before normal attacks)
        if (is_charging && !attacker.is_fatigued()) {
            u8 base_impact = attacker.get_rule_value(RuleId::Impact);
            u8 impact = base_impact;
            // Counter reduces Impact by 1 per model with Counter
            if (impact > counter_models) {
                impact -= counter_models;
            } else {
                impact = 0;
            }

            if (logger_ && base_impact > 0) {
                logger_->on_impact_start(true, base_impact, counter_models, impact);
            }

            if (impact > 0) {
                if (logger_) dice_.enable_roll_recording(true);
                u32 impact_hits = dice_.roll_impact(impact);

                if (logger_) {
                    std::vector<u8> impact_rolls = dice_.take_recorded_rolls();
                    logger_->on_impact_rolls(impact_rolls, 2, impact_hits);
                }

                if (impact_hits > 0) {
                    // Impact hits use base defense (no AP)
                    u8 effective_defense = defender.defense();
                    if (defender.has_rule(RuleId::ShieldWall)) {
                        effective_defense = std::max(u8(2), static_cast<u8>(effective_defense - 1));
                    }

                    if (logger_) dice_.enable_roll_recording(true);
                    u32 impact_wounds = dice_.roll_defense_test(impact_hits, effective_defense, 0, 0, false);

                    if (logger_) {
                        std::vector<u8> def_rolls = dice_.take_recorded_rolls();
                        u32 saves = impact_hits - impact_wounds;
                        logger_->on_impact_defense(def_rolls, defender.defense(), effective_defense, saves, impact_wounds);
                    }

                    if (impact_wounds > 0) {
                        auto wound_result = apply_wounds(defender, impact_wounds, false);
                        result.wounds_dealt += wound_result.wounds_dealt;
                        result.models_killed += wound_result.models_killed;

                        if (logger_) {
                            logger_->on_impact_end(wound_result.wounds_dealt, wound_result.models_killed);
                        }
                    } else if (logger_) {
                        logger_->on_impact_end(0, 0);
                    }
                } else if (logger_) {
                    logger_->on_impact_end(0, 0);
                }
            }

            if (logger_) dice_.enable_roll_recording(false);
        }

        // Collect all melee weapons
        u8 models_attacking = attacker.alive_count();
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (!w.is_melee()) continue;

            // Limited: skip if already used this game
            if (w.has_rule(RuleId::Limited)) {
                if (attacker.is_limited_weapon_used(i)) {
                    if (logger_) logger_->on_rule_triggered("Limited", "weapon_already_used", i);
                    continue;
                }
                attacker.mark_limited_weapon_used(i);
                if (logger_) logger_->on_rule_triggered("Limited", "using_one_time_weapon", i);
            }

            // Calculate total attacks: min(weapon_count, alive_models) * attacks_per_model
            u8 models_with_weapon = std::min(w.count, models_attacking);
            u32 attacks = static_cast<u32>(models_with_weapon) * w.attacks;
            if (attacks == 0) continue;

            // Log weapon attack start
            if (logger_) {
                std::string rules_str = get_weapon_rules_str(w);
                logger_->on_weapon_attack_start(w.name.c_str(), true, 0, 0, w.attacks, w.ap, rules_str.c_str());
                logger_->on_attack_count(models_with_weapon, w.attacks, attacks);
            }

            // Roll to hit
            u8 base_quality = attacker.quality();
            u8 quality = base_quality;
            i8 hit_modifier = 0;

            // VersatileAttack: roll d6 to choose AP+1 (1-3) or +1 hit (4-6)
            u8 versatile_ap_bonus = 0;
            if (attacker.has_rule(RuleId::VersatileAttack)) {
                u8 versatile_roll = dice_.roll_d6();
                if (versatile_roll <= 3) {
                    versatile_ap_bonus = 1;
                    if (logger_) logger_->on_rule_triggered("VersatileAttack", "rolled_ap+1", versatile_roll);
                } else {
                    hit_modifier += 1;
                    if (logger_) logger_->on_hit_modifier("VersatileAttack", +1, "rolled_+1_hit");
                }
            }

            // Reliable: Quality becomes 2+
            if (w.has_rule(RuleId::Reliable)) {
                quality = 2;
                if (logger_) logger_->on_hit_modifier("Reliable", 0, "quality_becomes_2+");
            }

            // Thrust: +1 to hit when charging
            if (is_charging && w.has_rule(RuleId::Thrust)) {
                hit_modifier += 1;
                if (logger_) logger_->on_hit_modifier("Thrust", +1, "charging_bonus");
            }

            // Precise: +1 to hit (weapon rule)
            if (w.has_rule(RuleId::Precise)) {
                hit_modifier += 1;
                if (logger_) logger_->on_hit_modifier("Precise", +1, "weapon_accuracy");
            }

            // MeleeEvasion: -1 to be hit in melee (defender rule)
            if (defender.has_rule(RuleId::MeleeEvasion)) {
                hit_modifier -= 1;
                if (logger_) logger_->on_hit_modifier("MeleeEvasion", -1, "evasive_target");
            }

            // MeleeShrouding: -1 to be hit in melee (defender rule)
            if (defender.has_rule(RuleId::MeleeShrouding)) {
                hit_modifier -= 1;
                if (logger_) logger_->on_hit_modifier("MeleeShrouding", -1, "shrouded_target");
            }

            // Purge: +1 to hit vs Tough(3+)
            u8 defender_tough = defender.get_rule_value(RuleId::Tough);
            if (w.has_rule(RuleId::Purge) && defender_tough >= 3) {
                hit_modifier += 1;
                if (logger_) logger_->on_hit_modifier("Purge", +1, "targeting_tough_3+");
            }

            // Shaken/Fatigued: Only hit on 6s (unmodified)
            bool only_sixes = attacker.is_shaken() || attacker.is_fatigued();
            if (only_sixes) {
                quality = 6;
                hit_modifier = 0;  // No modifiers when fatigued
                if (logger_) logger_->on_hit_modifier("Fatigued/Shaken", 0, "only_hit_on_6s");
            }

            // Enable roll recording for logging
            if (logger_) dice_.enable_roll_recording(true);

            auto hit_result = dice_.roll_quality_test(attacks, quality, hit_modifier);
            u32 hits = hit_result.hits;
            u32 sixes = hit_result.sixes;

            // Log hit rolls
            if (logger_) {
                std::vector<u8> hit_rolls = dice_.take_recorded_rolls();
                i8 effective = static_cast<i8>(quality) - hit_modifier;
                effective = std::max(i8(2), std::min(i8(6), effective));
                logger_->on_hit_rolls(base_quality, hit_modifier, static_cast<u8>(effective), hit_rolls, hits, sixes);
            }

            // Rending: 6s to hit get AP(+4)
            bool has_rending = w.has_rule(RuleId::Rending);
            u32 rending_hits = has_rending ? sixes : 0;

            // Rupture: 6s to hit deal +1 wound per wound and bypass regen
            bool has_rupture = w.has_rule(RuleId::Rupture);
            u32 rupture_hits = has_rupture ? sixes : 0;

            u32 normal_hits = hits - rending_hits;

            // Furious: extra hits on 6s when charging (bonus hits don't get Rending)
            if (is_charging && attacker.has_rule(RuleId::Furious)) {
                hits += sixes;
                normal_hits = hits - rending_hits;
                if (logger_) logger_->on_rule_triggered("Furious", "extra_hits_on_6s_when_charging", sixes);
            }

            // PredatorFighter: 6s generate extra attacks (recursive in melee)
            if (attacker.has_rule(RuleId::PredatorFighter) && sixes > 0) {
                u32 total_bonus = 0;
                u32 current_sixes = sixes;
                // Recursive: roll for each 6, new 6s generate more attacks
                while (current_sixes > 0) {
                    auto bonus_result = dice_.roll_quality_test(current_sixes, quality, hit_modifier);
                    total_bonus += bonus_result.hits;
                    current_sixes = bonus_result.sixes;  // Continue if we rolled more 6s
                }
                hits += total_bonus;
                normal_hits = hits - rending_hits;
                if (logger_) logger_->on_rule_triggered("PredatorFighter", "recursive_extra_attacks", total_bonus);
            }

            // Surge: extra hits on 6s to hit
            if (w.has_rule(RuleId::Surge)) {
                hits += sixes;
                normal_hits = hits - rending_hits;
                if (logger_) logger_->on_rule_triggered("Surge", "extra_hits_on_6s", sixes);
            }

            // Calculate AP
            u8 ap = w.ap;

            // VersatileAttack AP bonus (if rolled earlier)
            ap += versatile_ap_bonus;

            // Lance: +2 AP when charging
            if (is_charging && w.has_rule(RuleId::Lance)) {
                ap += 2;
                if (logger_) logger_->on_rule_triggered("Lance", "ap_+2_when_charging", 2);
            }

            // Thrust: AP(+1) when charging
            if (is_charging && w.has_rule(RuleId::Thrust)) {
                ap += 1;
                if (logger_) logger_->on_rule_triggered("Thrust", "ap_+1_when_charging", 1);
            }

            // Piercing Assault: AP(1) on melee when charging
            if (is_charging && attacker.has_rule(RuleId::PiercingAssault)) {
                u8 old_ap = ap;
                ap = std::max(ap, u8(1));
                if (logger_ && ap > old_ap) logger_->on_rule_triggered("PiercingAssault", "minimum_ap_1", 1);
            }

            // Takedown: treats target as single model (Blast capped at 1)
            bool has_takedown = w.has_rule(RuleId::Takedown);
            i8 takedown_target_idx = -1;  // -1 = normal allocation
            if (has_takedown) {
                // AI picks most valuable target (hero > wounded tough > first alive)
                for (u8 m = 0; m < defender.model_count(); ++m) {
                    if (defender.model_is_alive(m)) {
                        const Model& model = defender.get_model(m);
                        if (model.is_hero) {
                            takedown_target_idx = m;
                            break;  // Heroes are highest priority
                        }
                        if (takedown_target_idx < 0 || defender.model_wounds_taken(m) > 0) {
                            takedown_target_idx = m;  // Wounded models or first alive
                        }
                    }
                }
                if (logger_ && takedown_target_idx >= 0) {
                    logger_->on_rule_triggered("Takedown", "targeting_specific_model", takedown_target_idx);
                }
            }

            // Blast: multiply hits by X, where X is capped at target model count
            // Takedown: Blast is capped at 1 (treating target as single model)
            u8 blast_value = w.get_rule_value(RuleId::Blast);
            if (blast_value > 0) {
                u8 max_multiplier = has_takedown ? u8(1) : static_cast<u8>(defender.alive_count());
                u8 multiplier = std::min(blast_value, max_multiplier);
                u32 old_hits = hits;
                hits *= multiplier;
                rending_hits *= multiplier;
                rupture_hits *= multiplier;
                normal_hits = hits - rending_hits;
                if (logger_) logger_->on_rule_triggered("Blast", "multiplied_hits", hits - old_hits);
            }

            // Log hits after modifiers
            if (logger_) {
                logger_->on_hits_after_modifiers(normal_hits, rending_hits, hits);
            }

            // Roll defense
            bool poison = w.has_rule(RuleId::Poison);
            bool has_bane = w.has_rule(RuleId::Bane);
            // Bane in Melee: unit rule that gives all melee attacks Bane effect
            bool has_bane_in_melee = attacker.has_rule(RuleId::BaneInMelee);
            bool reroll_def_sixes = poison || has_bane || has_bane_in_melee;

            // Shield Wall: +1 to Defense rolls in melee (easier to save)
            u8 effective_defense = defender.defense();
            if (defender.has_rule(RuleId::ShieldWall)) {
                effective_defense = std::max(u8(2), static_cast<u8>(effective_defense - 1));
                if (logger_) logger_->on_defense_modifier("ShieldWall", -1, "easier_saves_in_melee");
            }

            // Protected: 6+ to reduce AP by 1 (roll for each hit)
            u8 protected_ap_reduction = 0;
            if (defender.has_rule(RuleId::Protected) && ap > 0 && normal_hits > 0) {
                u32 protected_successes = dice_.roll_regeneration(normal_hits, 6);
                if (protected_successes > 0) {
                    protected_ap_reduction = 1;
                    if (logger_) logger_->on_rule_triggered("Protected", "reduced_ap_by_1", protected_successes);
                }
            }
            u8 final_ap = (ap > protected_ap_reduction) ? (ap - protected_ap_reduction) : 0;

            // Shred: track 1s on defense rolls for bonus wounds
            bool has_shred = w.has_rule(RuleId::Shred);
            u32 shred_bonus_wounds = 0;

            if (logger_) dice_.enable_roll_recording(true);
            u32 wounds_from_normal = 0;
            if (has_shred && normal_hits > 0) {
                auto def_result = dice_.roll_defense_test_with_ones(normal_hits, effective_defense, final_ap, 0, reroll_def_sixes);
                wounds_from_normal = def_result.wounds;
                shred_bonus_wounds += def_result.ones;
                if (def_result.ones > 0 && logger_) {
                    logger_->on_rule_triggered("Shred", "bonus_wounds_from_1s", def_result.ones);
                }
            } else {
                wounds_from_normal = dice_.roll_defense_test(normal_hits, effective_defense, final_ap, 0, reroll_def_sixes);
            }

            // Log defense rolls for normal hits
            if (logger_) {
                std::vector<u8> all_rolls = dice_.take_recorded_rolls();
                i8 effective = static_cast<i8>(effective_defense) + static_cast<i8>(final_ap);
                effective = std::max(i8(2), std::min(i8(6), effective));
                u32 saves = normal_hits - wounds_from_normal;

                // Split recorded rolls into initial rolls and rerolls
                // First normal_hits rolls are initial, rest are rerolls of 6s
                std::vector<u8> def_rolls;
                std::vector<u8> rerolls;
                u32 sixes_count = 0;

                if (reroll_def_sixes && all_rolls.size() > normal_hits) {
                    def_rolls.assign(all_rolls.begin(), all_rolls.begin() + normal_hits);
                    rerolls.assign(all_rolls.begin() + normal_hits, all_rolls.end());
                    sixes_count = static_cast<u32>(rerolls.size());
                } else {
                    def_rolls = std::move(all_rolls);
                }

                u32 reroll_saves = 0;
                for (u8 r : rerolls) {
                    reroll_saves += (r >= static_cast<u8>(effective));
                }

                logger_->on_defense_rolls(effective_defense, final_ap, static_cast<u8>(effective), reroll_def_sixes,
                                          def_rolls, saves, wounds_from_normal, sixes_count, rerolls, reroll_saves);
            }

            u32 wounds_from_rending = 0;
            if (rending_hits > 0) {
                u8 rending_ap = final_ap + 4;  // Rending adds +4 AP to (potentially reduced) base
                if (logger_) dice_.enable_roll_recording(true);
                if (has_shred) {
                    auto def_result = dice_.roll_defense_test_with_ones(rending_hits, effective_defense, rending_ap, 0, reroll_def_sixes);
                    wounds_from_rending = def_result.wounds;
                    shred_bonus_wounds += def_result.ones;
                    if (def_result.ones > 0 && logger_) {
                        logger_->on_rule_triggered("Shred", "bonus_wounds_from_1s_rending", def_result.ones);
                    }
                } else {
                    wounds_from_rending = dice_.roll_defense_test(rending_hits, effective_defense, rending_ap, 0, reroll_def_sixes);
                }

                if (logger_) {
                    std::vector<u8> rend_rolls = dice_.take_recorded_rolls();
                    i8 effective = static_cast<i8>(effective_defense) + static_cast<i8>(rending_ap);
                    effective = std::max(i8(2), std::min(i8(6), effective));
                    u32 saves = rending_hits - wounds_from_rending;
                    logger_->on_defense_rolls_rending(defender.defense(), rending_ap, static_cast<u8>(effective),
                                                      rend_rolls, saves, wounds_from_rending);
                }
            }

            // Disable roll recording
            if (logger_) dice_.enable_roll_recording(false);

            u32 total_wounds = wounds_from_normal + wounds_from_rending + shred_bonus_wounds;

            // Rupture: wounds from 6s to hit deal +1 wound each (double the wounds)
            u32 rupture_bonus_wounds = 0;
            if (has_rupture) {
                if (has_rending) {
                    // With Rending, all 6s went to rending_hits, so double those wounds
                    rupture_bonus_wounds = wounds_from_rending;
                    if (logger_ && rupture_bonus_wounds > 0) {
                        logger_->on_rule_triggered("Rupture", "bonus_wounds_from_6s", rupture_bonus_wounds);
                    }
                } else {
                    // Without Rending, rupture_hits were part of normal_hits
                    if (normal_hits > 0 && rupture_hits > 0) {
                        u32 estimated_rupture_wounds = (wounds_from_normal * rupture_hits + normal_hits - 1) / normal_hits;
                        estimated_rupture_wounds = std::min(estimated_rupture_wounds, wounds_from_normal);
                        rupture_bonus_wounds = estimated_rupture_wounds;
                        if (logger_ && rupture_bonus_wounds > 0) {
                            logger_->on_rule_triggered("Rupture", "bonus_wounds_from_6s", rupture_bonus_wounds);
                        }
                    }
                }
                total_wounds += rupture_bonus_wounds;
            }

            // Deadly: handled separately in apply_wounds_deadly
            u8 deadly_value = w.get_rule_value(RuleId::Deadly);

            // Determine if regeneration is bypassed (Bane, Bane in Melee, Rending, Rupture, Shred, or Unstoppable)
            bool bypass_regen = has_bane || has_bane_in_melee || has_rending || has_rupture || has_shred || w.has_rule(RuleId::Unstoppable);

            // Apply wounds
            u8 weapon_models_killed = 0;
            if (total_wounds > 0) {
                WoundResult wound_result;
                if (deadly_value > 1) {
                    // Note: Takedown not compatible with Deadly (would be redundant)
                    wound_result = apply_wounds_deadly(defender, total_wounds, deadly_value, bypass_regen);
                } else {
                    wound_result = apply_wounds(defender, total_wounds, bypass_regen, takedown_target_idx);
                }
                result.wounds_dealt += wound_result.wounds_dealt;
                result.models_killed += wound_result.models_killed;
                result.self_destruct_hits += wound_result.self_destruct_hits;
                weapon_models_killed = wound_result.models_killed;
            }

            if (logger_) {
                logger_->on_weapon_attack_end(w.name.c_str(), total_wounds, weapon_models_killed);
            }
        }

        result.target_destroyed = defender.is_destroyed();
        result.target_shaken = defender.is_shaken();
        result.target_routed = defender.is_routed();

        return result;
    }

    // Apply wounds to a unit with proper wound allocation
    struct WoundResult {
        u16 wounds_dealt = 0;
        u8 models_killed = 0;
        u32 self_destruct_hits = 0;  // Hits to return from SelfDestruct models
    };

    // takedown_target: -1 = normal allocation, >= 0 = specific model index (Takedown rule)
    // from_spell: if true, Knightborn blocks on 4+ instead of 6+
    WoundResult apply_wounds(UnitView unit, u32 wounds, bool bypass_regeneration = false, i8 takedown_target = -1, bool from_spell = false) {
        WoundResult result;

        // Regeneration check
        if (!bypass_regeneration && unit.has_rule(RuleId::Regeneration)) {
            u32 original_wounds = wounds;
            wounds = dice_.roll_regeneration(wounds, 5);
            if (logger_) {
                u32 blocked = original_wounds - wounds;
                logger_->on_rule_triggered("Regeneration", "blocked_wounds", blocked);
            }
        }

        // Resistance: 6+ to ignore each wound (after regeneration)
        if (unit.has_rule(RuleId::Resistance) && wounds > 0) {
            u32 original_wounds = wounds;
            wounds = dice_.roll_regeneration(wounds, 6);  // Uses same mechanic as regen
            if (logger_) {
                u32 blocked = original_wounds - wounds;
                logger_->on_rule_triggered("Resistance", "resisted_wounds", blocked);
            }
        }

        // Knightborn: 6+ to ignore wounds (4+ vs spells)
        if (unit.has_rule(RuleId::Knightborn) && wounds > 0) {
            u32 original_wounds = wounds;
            u8 target = from_spell ? 4 : 6;
            wounds = dice_.roll_regeneration(wounds, target);
            if (logger_) {
                u32 blocked = original_wounds - wounds;
                const char* action = from_spell ? "blocked_spell_wounds" : "blocked_wounds";
                logger_->on_rule_triggered("Knightborn", action, blocked);
            }
        }

        // Plaguebound: 6+ to ignore wounds (5+ with Plaguebound Boost)
        if (unit.has_rule(RuleId::Plaguebound) && wounds > 0) {
            u32 original_wounds = wounds;
            u8 target = unit.has_rule(RuleId::PlaegueboundBoost) ? 5 : 6;
            wounds = dice_.roll_regeneration(wounds, target);
            if (logger_) {
                u32 blocked = original_wounds - wounds;
                logger_->on_rule_triggered("Plaguebound", "blocked_wounds", blocked);
            }
        }

        result.wounds_dealt = static_cast<u16>(wounds);

        // Takedown: apply wounds to specific target first
        u32 remaining_wounds = wounds;
        if (takedown_target >= 0 && unit.model_is_alive(static_cast<u8>(takedown_target))) {
            u8 model_idx = static_cast<u8>(takedown_target);
            u8 wounds_to_kill = unit.model_remaining_wounds(model_idx);
            u8 wounds_applied = static_cast<u8>(std::min(remaining_wounds, static_cast<u32>(wounds_to_kill)));

            for (u8 w = 0; w < wounds_applied && remaining_wounds > 0; ++w) {
                if (unit.apply_wound_to_model(model_idx)) {
                    result.models_killed++;
                    // SelfDestruct: when model dies, queue hits for attacker
                    if (unit.has_rule(RuleId::SelfDestruct)) {
                        u8 destruct_value = unit.get_rule_value(RuleId::SelfDestruct);
                        result.self_destruct_hits += destruct_value;
                        if (logger_) logger_->on_rule_triggered("SelfDestruct", "queued_hits_for_attacker", destruct_value);
                    }
                    break;  // Model died
                }
                remaining_wounds--;
            }
            // Note: With Takedown, excess wounds don't carry over to other models
            // But we still track them as dealt for stats purposes
            return result;
        }

        // Get wound allocation order (normal allocation)
        std::array<u8, MAX_MODELS_PER_UNIT> order;
        u8 order_count = 0;
        unit.get_wound_allocation_order(order, order_count);

        // Apply wounds in order
        for (u8 i = 0; i < order_count && remaining_wounds > 0; ++i) {
            u8 model_idx = order[i];
            if (!unit.model_is_alive(model_idx)) continue;

            u8 wounds_to_kill = unit.model_remaining_wounds(model_idx);
            u8 wounds_applied = static_cast<u8>(std::min(remaining_wounds, static_cast<u32>(wounds_to_kill)));

            for (u8 w = 0; w < wounds_applied; ++w) {
                if (unit.apply_wound_to_model(model_idx)) {
                    result.models_killed++;
                    // SelfDestruct: when model dies, queue hits for attacker
                    if (unit.has_rule(RuleId::SelfDestruct)) {
                        u8 destruct_value = unit.get_rule_value(RuleId::SelfDestruct);
                        result.self_destruct_hits += destruct_value;
                        if (logger_) logger_->on_rule_triggered("SelfDestruct", "queued_hits_for_attacker", destruct_value);
                    }
                    break;  // Model died, move to next
                }
            }

            remaining_wounds -= wounds_applied;
        }

        return result;
    }

    // Apply wounds with Deadly - wounds don't carry over to other models
    // Each wound is multiplied by deadly_value and assigned to one model
    // from_spell: if true, Knightborn blocks on 4+ instead of 6+
    WoundResult apply_wounds_deadly(UnitView unit, u32 wounds, u8 deadly_value, bool bypass_regeneration = false, bool from_spell = false) {
        WoundResult result;

        // Get wound allocation order
        std::array<u8, MAX_MODELS_PER_UNIT> order;
        u8 order_count = 0;
        unit.get_wound_allocation_order(order, order_count);

        // Regeneration check (before multiplying for Deadly)
        if (!bypass_regeneration && unit.has_rule(RuleId::Regeneration)) {
            u32 original_wounds = wounds;
            wounds = dice_.roll_regeneration(wounds, 5);
            if (logger_) {
                u32 blocked = original_wounds - wounds;
                logger_->on_rule_triggered("Regeneration", "blocked_wounds", blocked);
            }
        }

        // Resistance: 6+ to ignore each wound (after regeneration)
        if (unit.has_rule(RuleId::Resistance) && wounds > 0) {
            u32 original_wounds = wounds;
            wounds = dice_.roll_regeneration(wounds, 6);
            if (logger_) {
                u32 blocked = original_wounds - wounds;
                logger_->on_rule_triggered("Resistance", "resisted_wounds", blocked);
            }
        }

        // Knightborn: 6+ to ignore wounds (4+ vs spells)
        if (unit.has_rule(RuleId::Knightborn) && wounds > 0) {
            u32 original_wounds = wounds;
            u8 target = from_spell ? 4 : 6;
            wounds = dice_.roll_regeneration(wounds, target);
            if (logger_) {
                u32 blocked = original_wounds - wounds;
                const char* action = from_spell ? "blocked_spell_wounds" : "blocked_wounds";
                logger_->on_rule_triggered("Knightborn", action, blocked);
            }
        }

        // Plaguebound: 6+ to ignore wounds (5+ with Plaguebound Boost)
        if (unit.has_rule(RuleId::Plaguebound) && wounds > 0) {
            u32 original_wounds = wounds;
            u8 target = unit.has_rule(RuleId::PlaegueboundBoost) ? 5 : 6;
            wounds = dice_.roll_regeneration(wounds, target);
            if (logger_) {
                u32 blocked = original_wounds - wounds;
                logger_->on_rule_triggered("Plaguebound", "blocked_wounds", blocked);
            }
        }

        // Each wound is multiplied by deadly_value but doesn't carry over
        u8 order_idx = 0;
        for (u32 w = 0; w < wounds && order_idx < order_count; ++w) {
            // Get next alive model
            while (order_idx < order_count && !unit.model_is_alive(order[order_idx])) {
                order_idx++;
            }
            if (order_idx >= order_count) break;

            u8 model_idx = order[order_idx];
            u8 model_wounds_remaining = unit.model_remaining_wounds(model_idx);

            // Apply deadly_value wounds to this model (capped at what would kill it)
            u8 wounds_to_apply = std::min(deadly_value, model_wounds_remaining);
            result.wounds_dealt += wounds_to_apply;

            for (u8 d = 0; d < wounds_to_apply; ++d) {
                if (unit.apply_wound_to_model(model_idx)) {
                    result.models_killed++;
                    // SelfDestruct: when model dies, queue hits for attacker
                    if (unit.has_rule(RuleId::SelfDestruct)) {
                        u8 destruct_value = unit.get_rule_value(RuleId::SelfDestruct);
                        result.self_destruct_hits += destruct_value;
                        if (logger_) logger_->on_rule_triggered("SelfDestruct", "queued_hits_for_attacker", destruct_value);
                    }
                    order_idx++;  // Move to next model for next wound
                    break;
                }
            }
            // Note: excess wounds from deadly are lost (don't carry over)
        }

        return result;
    }

    // Morale check
    // is_from_melee: true if this check is from losing melee combat
    // is_unit_a: for logging purposes
    bool check_morale(UnitView unit, bool is_from_melee = false, u32 melee_wounds_taken = 0, u32 melee_wounds_dealt = 0, bool is_unit_a = true) {
        // Check if morale test is needed
        bool needs_test = false;
        const char* trigger_reason = "";

        // At half strength (wounds or models)
        if (unit.is_at_half_strength() && !unit.is_shaken() && !unit.is_routed()) {
            needs_test = true;
            trigger_reason = "half_strength";
        }

        // Lost melee (dealt fewer wounds)
        if (is_from_melee && melee_wounds_taken > melee_wounds_dealt) {
            needs_test = true;
            trigger_reason = "lost_melee";
        }

        if (!needs_test) return true;  // Passed (no test needed)

        UnitStatus old_status = unit.state->status;

        if (logger_) {
            logger_->on_morale_check_start(is_unit_a, unit.unit->name.c_str(), trigger_reason,
                                           unit.alive_count(), unit.unit->model_count,
                                           static_cast<u16>(melee_wounds_taken), static_cast<u16>(melee_wounds_dealt));
        }

        // Roll morale test
        u8 roll = dice_.roll_d6();
        u8 target = unit.quality();

        // MoraleBoost: +1 to morale tests (easier to pass)
        if (unit.has_rule(RuleId::MoraleBoost)) {
            roll += 1;
            if (logger_) logger_->on_rule_triggered("MoraleBoost", "morale_bonus", 1);
        }

        // Hive Bond: +1 to morale tests (faction rule)
        if (unit.has_rule(RuleId::HiveBond)) {
            roll += 1;
            if (logger_) logger_->on_rule_triggered("HiveBond", "morale_bonus", 1);
        }

        bool passed = roll >= target;

        if (logger_) {
            logger_->on_morale_roll(roll, target, passed);
        }

        // Fearless: reroll failed test, pass on 4+
        u8 fearless_roll = 0;
        bool fearless_passed = false;
        if (!passed && unit.has_rule(RuleId::Fearless)) {
            fearless_roll = dice_.roll_d6();
            fearless_passed = fearless_roll >= 4;
            passed = fearless_passed;

            if (logger_) {
                logger_->on_fearless_roll(fearless_roll, 4, fearless_passed);
            }
        }

        // Hold the Line: reroll failed morale test using quality
        u8 hold_line_roll = 0;
        bool hold_line_passed = false;
        if (!passed && unit.has_rule(RuleId::HoldTheLine)) {
            hold_line_roll = dice_.roll_d6();
            hold_line_passed = hold_line_roll >= unit.quality();
            passed = hold_line_passed;

            if (logger_) {
                // Reuse fearless roll logging - similar mechanic
                logger_->on_fearless_roll(hold_line_roll, unit.quality(), hold_line_passed);
            }
        }

        if (passed) {
            if (logger_) {
                logger_->on_morale_check_end(true, old_status, old_status, "passed");
            }
            return true;
        }

        // Failed morale - different outcomes for melee vs shooting
        UnitStatus new_status = old_status;
        const char* result_desc = "";

        // NoRetreat: Take wounds instead of becoming Shaken
        if (unit.has_rule(RuleId::NoRetreat)) {
            // Roll d3 wounds (1-3)
            u8 wounds_taken = (dice_.roll_d6() + 1) / 2;  // Maps 1-6 to 1-3
            if (logger_) logger_->on_rule_triggered("NoRetreat", "wounds_instead_of_shaken", wounds_taken);

            // Apply wounds
            for (u8 w = 0; w < wounds_taken && !unit.is_destroyed(); ++w) {
                // Find first alive model and apply wound
                for (u8 m = 0; m < unit.unit->model_count; ++m) {
                    if (unit.model_is_alive(m)) {
                        unit.apply_wound_to_model(m);
                        break;
                    }
                }
            }
            result_desc = "no_retreat_wounds_taken";
            // Don't change status - unit stays Normal
        } else if (is_from_melee) {
            // Melee morale: Rout if at half strength, Shaken otherwise
            if (unit.is_at_half_strength()) {
                unit.rout();
                new_status = UnitStatus::Routed;
                result_desc = "routed_at_half_strength";
            } else {
                unit.become_shaken();
                new_status = UnitStatus::Shaken;
                result_desc = "shaken_from_melee";
            }
        } else {
            // General morale (from shooting): Always Shaken, never immediate Rout
            unit.become_shaken();
            new_status = UnitStatus::Shaken;
            result_desc = "shaken_from_shooting";
        }

        if (logger_) {
            logger_->on_morale_check_end(false, old_status, new_status, result_desc);
            logger_->on_status_changed(is_unit_a, old_status, new_status, result_desc);
        }

        return false;
    }

private:
    DiceRoller& dice_;
    MatchLogger* logger_;

    // Helper to build weapon rules string
    std::string get_weapon_rules_str(const Weapon& w) {
        std::string rules;
        if (w.has_rule(RuleId::Rending)) rules += "Rending,";
        if (w.has_rule(RuleId::Deadly)) {
            rules += "Deadly(" + std::to_string(w.get_rule_value(RuleId::Deadly)) + "),";
        }
        if (w.has_rule(RuleId::Blast)) {
            rules += "Blast(" + std::to_string(w.get_rule_value(RuleId::Blast)) + "),";
        }
        if (w.has_rule(RuleId::Poison)) rules += "Poison,";
        if (w.has_rule(RuleId::Bane)) rules += "Bane,";
        if (w.has_rule(RuleId::Reliable)) rules += "Reliable,";
        if (w.has_rule(RuleId::Surge)) rules += "Surge,";
        if (w.has_rule(RuleId::Lance)) rules += "Lance,";
        if (w.has_rule(RuleId::Thrust)) rules += "Thrust,";
        if (w.has_rule(RuleId::Unstoppable)) rules += "Unstoppable,";
        if (!rules.empty()) rules.pop_back(); // Remove trailing comma
        return rules;
    }
};

} // namespace battle
