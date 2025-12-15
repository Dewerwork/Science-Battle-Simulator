#include "core/faction_rules.hpp"
#include <cstring>

namespace battle {

// ==============================================================================
// Helper functions to create rules
// ==============================================================================

namespace {

FactionRule make_rule(
    std::string_view name,
    FactionRuleType type,
    FactionRuleCategory category,
    TriggerTiming trigger,
    const FactionRuleEffect& effect
) {
    FactionRule rule(name, type, category);
    rule.trigger = trigger;
    rule.add_effect(effect);
    return rule;
}

FactionRuleEffect effect_grants_rule(RuleId id) {
    FactionRuleEffect e;
    e.grants_rule = id;
    return e;
}

FactionRuleEffect effect_hit_mod(i8 mod, bool melee = false, bool shooting = false) {
    FactionRuleEffect e;
    e.hit_modifier = mod;
    e.melee_only = melee;
    e.shooting_only = shooting;
    return e;
}

FactionRuleEffect effect_defense_mod(i8 mod) {
    FactionRuleEffect e;
    e.defense_modifier = mod;
    return e;
}

FactionRuleEffect effect_morale_mod(i8 mod) {
    FactionRuleEffect e;
    e.morale_modifier = mod;
    return e;
}

FactionRuleEffect effect_ap_mod(i8 mod) {
    FactionRuleEffect e;
    e.ap_modifier = mod;
    return e;
}

FactionRuleEffect effect_deals_hits(u8 hits, u8 ap = 0, TargetType target = TargetType::EnemyWithin, u8 range = 12) {
    FactionRuleEffect e;
    e.deals_hits = hits;
    e.ap_for_dealt_hits = ap;
    e.target = target;
    e.range = range;
    return e;
}

FactionRuleEffect effect_aura(RuleId id, TargetType target = TargetType::Unit) {
    FactionRuleEffect e;
    e.grants_rule = id;
    e.target = target;
    return e;
}

} // anonymous namespace

// ==============================================================================
// Initialize all faction rules
// ==============================================================================

void initialize_faction_rules() {
    auto& registry = get_faction_registry();
    if (registry.is_initialized()) return;

    // Alien Hives
    {
        FactionArmyRules faction("Alien Hives");
        faction.add_army_wide_rule(make_rule("Hive Bond", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_special_rule(make_rule("Agile", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Breath Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Caster Group", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Increased Shooting Range", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Infiltrate", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("No Retreat", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Piercing Growth", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::StartOfRound, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Precise", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Predator Fighter", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, effect_hit_mod(6, true, false)));
        faction.add_special_rule(make_rule("Rapid Charge", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Ravage", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Regenerative Strength", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Resistance", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Rupture", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Self-Destruct", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OnModelKilled, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Shred", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Spawn", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Spell Conduit", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Stealth Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, effect_grants_rule(RuleId::Stealth)));
        faction.add_special_rule(make_rule("Strafing", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Surprise Attack", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Takedown Strike", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Unpredictable Fighter Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Furious Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Furious)));
        faction.add_aura_rule(make_rule("Hive Bond Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Increased Shooting Range Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Charge Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Shielded Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Shielded)));
        faction.add_aura_rule(make_rule("Animate Spirit (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Overwhelming Strike (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Infuse Bloodthirst (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Psychic Blast (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_aura_rule(make_rule("Terror Seeker (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Hive Shriek (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // Battle Brothers
    {
        FactionArmyRules faction("Battle Brothers");
        faction.add_army_wide_rule(make_rule("Battleborn", FactionRuleType::ArmyWide, FactionRuleCategory::Defense, TriggerTiming::StartOfRound, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Smash", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unstoppable Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Bane in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Bane when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Rush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Versatile Reach Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Advanced Sight (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Cerebral Trauma (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(1, 0, TargetType::EnemyWithin, 18)));
        faction.add_aura_rule(make_rule("Blessed Ammo (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Lightning Fog (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(4, 0, TargetType::EnemyWithin, 9)));
        faction.add_aura_rule(make_rule("Protective Dome (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Psychic Terror (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(9, 0, TargetType::EnemyWithin, 6)));
        registry.register_faction(faction);
    }

    // Blessed Sisters
    {
        FactionArmyRules faction("Blessed Sisters");
        faction.add_army_wide_rule(make_rule("Devout", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Casting Debuff", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Courage Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, effect_morale_mod(1)));
        faction.add_special_rule(make_rule("Devout Boost", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Fortified", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Guarded Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Piercing Assault", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OnCharge, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Point-Blank Surge", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Precision Fighter Buff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_hit_mod(1, true, false)));
        faction.add_special_rule(make_rule("Precision Shooter Buff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_hit_mod(1, false, true)));
        faction.add_special_rule(make_rule("Precision Target", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Purge", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Spell Conduit", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Devout Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Fast Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Fortified Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Increased Shooting Range Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Point-Blank Piercing Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Shred in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Burn the Heretic (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Righteous Wrath (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(4)));
        faction.add_aura_rule(make_rule("Holy Rage (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Eternal Flame (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::EnemyWithin, 6)));
        faction.add_aura_rule(make_rule("Litanies of War (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Searing Admonition (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(3, 0, TargetType::EnemyWithin, 18)));
        registry.register_faction(faction);
    }

    // Blood Brothers
    {
        FactionArmyRules faction("Blood Brothers");
        faction.add_army_wide_rule(make_rule("Bloodborn", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Smash", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unstoppable Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Bane in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Bane when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Piercing Assault Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Rush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Versatile Reach Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Blood Sight (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Blood Trauma (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(1, 0, TargetType::EnemyWithin, 18)));
        faction.add_aura_rule(make_rule("Burst of Rage (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Furious)));
        faction.add_aura_rule(make_rule("Heavenly Lance (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Blood Dome (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Shield Breaker (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // Blood Prime Brothers
    {
        FactionArmyRules faction("Blood Prime Brothers");
        faction.add_army_wide_rule(make_rule("Bloodborn", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_army_wide_rule(make_rule("Reinforced", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Demolish", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Melee Slayer", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Precise", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Ravage", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Retaliate", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Unstoppable Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Bane in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Bane when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Piercing Assault Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Rush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Versatile Reach Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Blood Sight (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Blood Wound (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Burst of Rage (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Furious)));
        faction.add_aura_rule(make_rule("Heavenly Lance (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Blood Dome (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Shield Breaker (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // Change Disciples
    {
        FactionArmyRules faction("Change Disciples");
        faction.add_army_wide_rule(make_rule("Changebound", FactionRuleType::ArmyWide, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(-1, false, false)));
        faction.add_special_rule(make_rule("Changebound Boost Aura", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Dangerous Terrain Debuff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shred", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Slam", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Steadfast Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unpredictable", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Unpredictable Fighter", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Furious Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Furious)));
        faction.add_aura_rule(make_rule("Relentless Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Relentless)));
        faction.add_aura_rule(make_rule("Resistance Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Scout Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Scout)));
        faction.add_aura_rule(make_rule("Versatile Defense Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Shifting Form (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Sky Blaze (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Breath of Change (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Mutating Inferno (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(4, 0, TargetType::EnemyWithin, 9)));
        faction.add_aura_rule(make_rule("Change Boon (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Power Bolt (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        registry.register_faction(faction);
    }

    // Custodian Brothers
    {
        FactionArmyRules faction("Custodian Brothers");
        faction.add_army_wide_rule(make_rule("Guardian", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Guardian Boost", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Hit & Run Shooter", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Piercing Target", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Shred Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Steadfast", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::StartOfRound, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Tear", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(4)));
        faction.add_special_rule(make_rule("Teleport", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Guardian Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Hit & Run Shooter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Ranged Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Shred when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Steadfast Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Stealth Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Stealth)));
        faction.add_aura_rule(make_rule("Teleport Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Unstoppable in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("The Founder's Curse (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Thunderous Mist (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::EnemyWithin, 18)));
        faction.add_aura_rule(make_rule("Focused Defender (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Dread Strike (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::Self, 24)));
        faction.add_aura_rule(make_rule("Guardian Protection (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Mind Gash (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // DAO Union
    {
        FactionArmyRules faction("DAO Union");
        faction.add_army_wide_rule(make_rule("Targeting Visor", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Ambush Beacon", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Decimate", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Evasive", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(-1, false, false)));
        faction.add_special_rule(make_rule("Good Shot", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, true)));
        faction.add_special_rule(make_rule("Melee Shrouding", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Piercing Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Precision Spotter", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Strafing", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Counter-Attack Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Counter)));
        faction.add_aura_rule(make_rule("Fortified Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Hit & Run Shooter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Increased Shooting Range Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Ranged Slayer Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Stealth Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Stealth)));
        faction.add_aura_rule(make_rule("Targeting Visor Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Aura of Peace (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Fearless)));
        faction.add_aura_rule(make_rule("Killing Blow (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Psychic Stabilization (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Deadly Surge (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::EnemyWithin, 6)));
        faction.add_aura_rule(make_rule("Coordinated Aggression (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Devastating Strike (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::EnemyWithin, 12)));
        registry.register_faction(faction);
    }

    // Dark Brothers
    {
        FactionArmyRules faction("Dark Brothers");
        faction.add_army_wide_rule(make_rule("Darkborn", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Rapid Ambush", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Smash", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Stealth Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, effect_grants_rule(RuleId::Stealth)));
        faction.add_special_rule(make_rule("Strafing", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unstoppable Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Bane in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Bane when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Rush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Versatile Reach Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Dark Sight (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Dark Trauma (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(1, 0, TargetType::EnemyWithin, 18)));
        faction.add_aura_rule(make_rule("Blessed Ammo (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Lightning Fog (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(4, 0, TargetType::EnemyWithin, 9)));
        faction.add_aura_rule(make_rule("Dark Dome (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Psychic Terror (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(9, 0, TargetType::EnemyWithin, 6)));
        registry.register_faction(faction);
    }

    // Dark Elf Raiders
    {
        FactionArmyRules faction("Dark Elf Raiders");
        faction.add_army_wide_rule(make_rule("Harassing", FactionRuleType::ArmyWide, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Infiltrate", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Lacerate", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Martial Prowess", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Melee Evasion", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(-1, true, false)));
        faction.add_special_rule(make_rule("Melee Slayer", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Precision Fighting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_hit_mod(1, true, false)));
        faction.add_special_rule(make_rule("Regeneration Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, effect_grants_rule(RuleId::Regeneration)));
        faction.add_special_rule(make_rule("Regenerative Strength", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Ruinous Frenzy", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Strafing", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unpredictable Fighter", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Furious Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Furious)));
        faction.add_aura_rule(make_rule("Harassing Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Piercing Hunter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Psy-Adrenaline (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Snake Bite (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Raiding Drugs (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_aura_rule(make_rule("Art of Pain (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_aura_rule(make_rule("Fade in the Dark (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Stealth)));
        faction.add_aura_rule(make_rule("Holistic Suffering (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(9, 0, TargetType::EnemyWithin, 12)));
        registry.register_faction(faction);
    }

    // Dark Prime Brothers
    {
        FactionArmyRules faction("Dark Prime Brothers");
        faction.add_army_wide_rule(make_rule("Darkborn", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_army_wide_rule(make_rule("Reinforced", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Demolish", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Melee Slayer", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Precise", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Rapid Ambush", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Ravage", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Retaliate", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Unstoppable Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Bane in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Bane when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Rush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Versatile Reach Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Dark Sight (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Dark Wound (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Blessed Ammo (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Lightning Fog (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(4, 0, TargetType::EnemyWithin, 9)));
        faction.add_aura_rule(make_rule("Dark Dome (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Psychic Terror (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(9, 0, TargetType::EnemyWithin, 6)));
        registry.register_faction(faction);
    }

    // Dwarf Guilds
    {
        FactionArmyRules faction("Dwarf Guilds");
        faction.add_army_wide_rule(make_rule("Sturdy", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Devastating Frenzy", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Fortified", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Infiltrate", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Melee Slayer", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Quake", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Quake when Shooting", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Speed Debuff", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Surge", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Swift", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Ignores Cover when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Increased Shooting Range Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Infiltrate Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Stealth Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Stealth)));
        faction.add_aura_rule(make_rule("Sturdy Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Swift Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Unpredictable Fighter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Battle Rune  (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Breaking Rune  (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(4)));
        faction.add_aura_rule(make_rule("Armor Rune  (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Smiting Rune  (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Deceleration Rune  (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Cleaving Rune  (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(3, 0, TargetType::EnemyWithin, 18)));
        registry.register_faction(faction);
    }

    // Elven Jesters
    {
        FactionArmyRules faction("Elven Jesters");
        faction.add_army_wide_rule(make_rule("Evasive", FactionRuleType::ArmyWide, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(-1, false, false)));
        faction.add_army_wide_rule(make_rule("Rapid Blink", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Ambush Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Fragment", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Point-Blank Surge", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Slayer Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Takedown Strike", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, effect_ap_mod(2)));
        faction.add_aura_rule(make_rule("Rapid Blink Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Shielded Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Shielded)));
        faction.add_aura_rule(make_rule("Shred when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Teleport Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Unpredictable Fighter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Asphyxiating Fog (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Counter)));
        faction.add_aura_rule(make_rule("Blades of Discord (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_aura_rule(make_rule("Shadow Dance (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Light Fragments (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Veil of Madness (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Fatal Sorrow (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::EnemyWithin, 18)));
        registry.register_faction(faction);
    }

    // Eternal Dynasty
    {
        FactionArmyRules faction("Eternal Dynasty");
        faction.add_army_wide_rule(make_rule("Clan Warrior", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Ambush Beacon", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Bounding", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Casting Buff", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Fortified", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Increased Shooting Range Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Puncture", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(4)));
        faction.add_special_rule(make_rule("Repel Ambushers", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Shred", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Surge", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Teleport", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unpredictable Fighter", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_special_rule(make_rule("Vengeance", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Clan Warrior Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Counter-Attack Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Counter)));
        faction.add_aura_rule(make_rule("Fearless Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Fearless)));
        faction.add_aura_rule(make_rule("Ignores Cover Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Melee Evasion Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Piercing Hunter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Precision Fighter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_aura_rule(make_rule("Rapid Advance Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Charge Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Stealth Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Stealth)));
        faction.add_aura_rule(make_rule("Spirit Power (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Soul Spear (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(1, 0, TargetType::Self, 24)));
        faction.add_aura_rule(make_rule("Spirit Resolve (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Mind Vortex (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_aura_rule(make_rule("Eternal Guidance (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Dragon Breath (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(9, 0, TargetType::EnemyWithin, 12)));
        registry.register_faction(faction);
    }

    // Goblin Reclaimers
    {
        FactionArmyRules faction("Goblin Reclaimers");
        faction.add_army_wide_rule(make_rule("Mischievous", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Bounding", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Dangerous Terrain Debuff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Good Shot", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, true)));
        faction.add_special_rule(make_rule("Instinctive", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Piercing Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Retaliate", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shred", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Skewer", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unpredictable Fighter", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Melee Evasion Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Mischievous Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Precision Fighter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_aura_rule(make_rule("Quick Shot Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Ammo Boost (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Zap! (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Mob Frenzy (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Boom! (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::EnemyWithin, 18)));
        faction.add_aura_rule(make_rule("Shroud Field (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_aura_rule(make_rule("Pow! (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        registry.register_faction(faction);
    }

    // Havoc Brothers
    {
        FactionArmyRules faction("Havoc Brothers");
        faction.add_army_wide_rule(make_rule("Havocbound", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Dangerous Terrain Debuff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Resistance", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Slam", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Steadfast Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unpredictable", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Unpredictable Fighter", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Furious Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Furious)));
        faction.add_aura_rule(make_rule("Havocbound Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Relentless Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Relentless)));
        faction.add_aura_rule(make_rule("Resistance Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Scout Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Scout)));
        faction.add_aura_rule(make_rule("Versatile Defense Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Cursed Stride (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Havoc Trauma (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_aura_rule(make_rule("Dark Assault (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Havoc Terror (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(4, 0, TargetType::EnemyWithin, 9)));
        faction.add_aura_rule(make_rule("Havoc Boon (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Havoc Fog (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // High Elf Fleets
    {
        FactionArmyRules faction("High Elf Fleets");
        faction.add_army_wide_rule(make_rule("Highborn", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Caster Group", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Crack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Crossing Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_deals_hits(1, 0, TargetType::EnemyWithin, 0)));
        faction.add_special_rule(make_rule("Fortified", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Piercing Spotter", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Spell Conduit", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Teleport", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unwieldy Debuff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Highborn Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Hit & Run Shooter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Increased Shooting Range Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Resistance Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Scout Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Scout)));
        faction.add_aura_rule(make_rule("Shred in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Stealth Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Stealth)));
        faction.add_aura_rule(make_rule("Creator of Illusions (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Elemental Seeker (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(1, 0, TargetType::EnemyWithin, 18)));
        faction.add_aura_rule(make_rule("Hidden Spirits (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Psy-Destruction (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(4)));
        faction.add_aura_rule(make_rule("Blessing of Souls (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Shattering Curse (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // Human Defense Force
    {
        FactionArmyRules faction("Human Defense Force");
        faction.add_army_wide_rule(make_rule("Hold the Line", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_special_rule(make_rule("Bane in Melee Buff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_grants_rule(RuleId::Bane)));
        faction.add_special_rule(make_rule("Coordinate", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Entrenched Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Extended Buff Range", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Fortified", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Fracture", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Good Shot", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, true)));
        faction.add_special_rule(make_rule("Mobile Artillery", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Morale Debuff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, effect_morale_mod(-1)));
        faction.add_special_rule(make_rule("No Retreat Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Precision Shooter Buff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_hit_mod(1, false, true)));
        faction.add_special_rule(make_rule("Rapid Advance Buff", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Relentless Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_grants_rule(RuleId::Relentless)));
        faction.add_special_rule(make_rule("Repel Ambushers", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_aura_rule(make_rule("Hold the Line Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Psy-Injected Courage (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Electric Tempest (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Calculated Foresight (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Relentless)));
        faction.add_aura_rule(make_rule("Searing Burst (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Shock Speed (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Expel Threat (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // Human Inquisition
    {
        FactionArmyRules faction("Human Inquisition");
        faction.add_army_wide_rule(make_rule("Inquisitorial Agent", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Bounding", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Brutal Fighter", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, true, false)));
        faction.add_special_rule(make_rule("Caster Group", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Casting Debuff", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Delayed Action", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Evasive", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(-1, false, false)));
        faction.add_special_rule(make_rule("Fortified", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Hit & Run", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Infiltrate", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Piercing Tag", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Protected", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Quick Readjustment", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Repel Ambushers", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Resistance", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shred", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Spell Accumulator", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Spell Conduit", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Surge", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Surprise Attack", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Teleport", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unpredictable Shooter", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, true)));
        faction.add_aura_rule(make_rule("Bounding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Defensive Growth Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Furious Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Furious)));
        faction.add_aura_rule(make_rule("Precision Fighter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_aura_rule(make_rule("Precision Shooter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_hit_mod(1, false, true)));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Resistance Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Shred in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Stealth Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Stealth)));
        faction.add_aura_rule(make_rule("Psy-Injected Courage (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Electric Tempest (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Calculated Foresight (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Relentless)));
        faction.add_aura_rule(make_rule("Searing Burst (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Shock Speed (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Expel Threat (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // Infected Colonies
    {
        FactionArmyRules faction("Infected Colonies");
        faction.add_army_wide_rule(make_rule("Infected", FactionRuleType::ArmyWide, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Bash", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Deathstrike", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OnModelKilled, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Precision Debuff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, effect_hit_mod(-1, false, false)));
        faction.add_special_rule(make_rule("Precision Growth", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::StartOfRound, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Fast Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Fortified Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Infected Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("No Retreat Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Thrust in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Unpredictable Shooter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Violent Onslaught (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Bio-Horror (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Brain Infestation (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_hit_mod(-1, false, false)));
        faction.add_aura_rule(make_rule("Spread Plague (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::EnemyWithin, 18)));
        faction.add_aura_rule(make_rule("Rapid Mutation (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Volatile Infection (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        registry.register_faction(faction);
    }

    // Jackals
    {
        FactionArmyRules faction("Jackals");
        faction.add_army_wide_rule(make_rule("Scrapper", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Bounding", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Crossing Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_deals_hits(1, 0, TargetType::EnemyWithin, 0)));
        faction.add_special_rule(make_rule("Crossing Strike", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Destructive", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Fortified", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Melee Evasion", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(-1, true, false)));
        faction.add_special_rule(make_rule("Morale Debuff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, effect_morale_mod(-1)));
        faction.add_special_rule(make_rule("Precise", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Precision Tag", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Repel Ambushers", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Scratch", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Surge", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Surprise Piercing Shot", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_aura_rule(make_rule("Precision Fighter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_aura_rule(make_rule("Ranged Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Scout Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Scout)));
        faction.add_aura_rule(make_rule("Scrapper Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Unpredictable Shooter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Psy-Hunter (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Power Maw (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_aura_rule(make_rule("Mind Shaper (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(-1)));
        faction.add_aura_rule(make_rule("Quill Blast (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(4, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Power Field (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Shielded)));
        faction.add_aura_rule(make_rule("Feral Strike (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        registry.register_faction(faction);
    }

    // Knight Brothers
    {
        FactionArmyRules faction("Knight Brothers");
        faction.add_army_wide_rule(make_rule("Knightborn", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Ravage", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Smash", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Teleport", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unstoppable Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Bane in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Bane when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Rush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Unpredictable Fighter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Versatile Reach Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Knight Sight (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Knight Trauma (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(1, 0, TargetType::EnemyWithin, 18)));
        faction.add_aura_rule(make_rule("Banishing Sigil (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_defense_mod(-1)));
        faction.add_aura_rule(make_rule("Doom Strike (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_aura_rule(make_rule("Knight Dome (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Purge the Impure (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        registry.register_faction(faction);
    }

    // Knight Prime Brothers
    {
        FactionArmyRules faction("Knight Prime Brothers");
        faction.add_army_wide_rule(make_rule("Knightborn", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_army_wide_rule(make_rule("Reinforced", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Demolish", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Melee Slayer", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Precise", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Ravage", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Retaliate", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Unstoppable Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Bane in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Bane when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Rush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Unpredictable Fighter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Versatile Reach Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Knight Sight (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Knight Wound (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Banishing Sigil (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_defense_mod(-1)));
        faction.add_aura_rule(make_rule("Doom Strike (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_aura_rule(make_rule("Knight Dome (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Purge the Impure (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        registry.register_faction(faction);
    }

    // Lust Disciples
    {
        FactionArmyRules faction("Lust Disciples");
        faction.add_army_wide_rule(make_rule("Lustbound", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Dangerous Terrain Debuff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Slam", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Steadfast Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unpredictable", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Unpredictable Fighter", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Furious Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Furious)));
        faction.add_aura_rule(make_rule("Lustbound Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Relentless Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Relentless)));
        faction.add_aura_rule(make_rule("Resistance Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Scout Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Scout)));
        faction.add_aura_rule(make_rule("Versatile Defense Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Combat Ecstasy (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Beautiful Pain (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Blissful Dance (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Total Seizure (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Lust Boon (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Overpowering Lash (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // Machine Cult
    {
        FactionArmyRules faction("Machine Cult");
        faction.add_army_wide_rule(make_rule("Machine-Fog", FactionRuleType::ArmyWide, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(-1, false, false)));
        faction.add_special_rule(make_rule("Bounding", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Crossing Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_deals_hits(1, 0, TargetType::EnemyWithin, 0)));
        faction.add_special_rule(make_rule("Grounded Stealth", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(-1, false, false)));
        faction.add_special_rule(make_rule("Melee Slayer", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Piercing Shooting Debuff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Shred", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Strafing", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Surge", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Wreck", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Machine-Fog Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rending when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Rending)));
        faction.add_aura_rule(make_rule("Teleport Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Unpredictable Fighter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Cyborg Assault (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Power Beam (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Shrouding Incense (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Searing Shrapnel (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Corrode Weapons (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Crushing Force (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        registry.register_faction(faction);
    }

    // Orc Marauders
    {
        FactionArmyRules faction("Orc Marauders");
        faction.add_army_wide_rule(make_rule("Ferocious", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Bad Shot", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(-1, false, true)));
        faction.add_special_rule(make_rule("Bounding", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Crossing Barrage", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Ferocious Boost", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Impale", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Piercing Assault", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OnCharge, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Protected", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Ravage", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Rending in Melee Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_grants_rule(RuleId::Rending)));
        faction.add_special_rule(make_rule("Repel Ambushers", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Shred", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Strafing", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Surge", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_aura_rule(make_rule("Ferocious Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Hit & Run Fighter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Piercing Fighter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Precision Charge Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::OnCharge, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Ranged Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Speed Feat Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Unpredictable Shooter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Elder Protection (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Death Bolt (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_aura_rule(make_rule("Path of War (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Psychic Vomit (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::EnemyWithin, 6)));
        faction.add_aura_rule(make_rule("Head Bang (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Rending)));
        faction.add_aura_rule(make_rule("Crackling Bolt (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(3, 0, TargetType::EnemyWithin, 18)));
        registry.register_faction(faction);
    }

    // Plague Disciples
    {
        FactionArmyRules faction("Plague Disciples");
        faction.add_army_wide_rule(make_rule("Plaguebound", FactionRuleType::ArmyWide, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Dangerous Terrain Debuff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Protected", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Slam", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Steadfast Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unpredictable", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Unpredictable Fighter", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Bounding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Furious Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Furious)));
        faction.add_aura_rule(make_rule("Plaguebound Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Relentless Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Relentless)));
        faction.add_aura_rule(make_rule("Resistance Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Scout Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Scout)));
        faction.add_aura_rule(make_rule("Versatile Defense Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Aura of Pestilence (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Putrefaction (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Blessed Virus (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Plague Malediction (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(4)));
        faction.add_aura_rule(make_rule("Plague Boon (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rot Wave (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::EnemyWithin, 18)));
        registry.register_faction(faction);
    }

    // Prime Brothers
    {
        FactionArmyRules faction("Prime Brothers");
        faction.add_army_wide_rule(make_rule("Battleborn", FactionRuleType::ArmyWide, FactionRuleCategory::Defense, TriggerTiming::StartOfRound, FactionRuleEffect{}));
        faction.add_army_wide_rule(make_rule("Reinforced", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Demolish", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Melee Slayer", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Precise", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Ravage", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Retaliate", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Unstoppable Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Bane in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Bane when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Rush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Versatile Reach Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Advanced Sight (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Mind Wound (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Blessed Ammo (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Lightning Fog (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(4, 0, TargetType::EnemyWithin, 9)));
        faction.add_aura_rule(make_rule("Protective Dome (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Psychic Terror (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(9, 0, TargetType::EnemyWithin, 6)));
        registry.register_faction(faction);
    }

    // Ratmen Clans
    {
        FactionArmyRules faction("Ratmen Clans");
        faction.add_army_wide_rule(make_rule("Scurry", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Crush", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Defense Debuff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_defense_mod(-1)));
        faction.add_special_rule(make_rule("Hazardous", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Heavy Impact", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("No Retreat", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Quick Readjustment", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Ravage", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Reinforcement", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::StartOfRound, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Self-Destruct", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OnModelKilled, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Shred", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Spawn", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Surge", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Takedown Strike", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Unpredictable Fighter", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_aura_rule(make_rule("Ambush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Ambush)));
        faction.add_aura_rule(make_rule("Evasive Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Melee Evasion Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Piercing Assault Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Precision Shooter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_hit_mod(1, false, true)));
        faction.add_aura_rule(make_rule("Scurry Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Weapon Booster (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Focused Shock (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Tech-Sickness (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_defense_mod(-1)));
        faction.add_aura_rule(make_rule("System Takeover (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Enhance Serum (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Power Surge (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::Self, 12)));
        registry.register_faction(faction);
    }

    // Rebel Guerrillas
    {
        FactionArmyRules faction("Rebel Guerrillas");
        faction.add_army_wide_rule(make_rule("Guerrilla", FactionRuleType::ArmyWide, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Counter-Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Courage Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, effect_morale_mod(1)));
        faction.add_special_rule(make_rule("Fortified", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Furious Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_grants_rule(RuleId::Furious)));
        faction.add_special_rule(make_rule("Piercing Tag", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Precise", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Surprise Piercing Shot", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Takedown Strike", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Thrash", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Bane when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Guerrilla Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Rending in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Rending)));
        faction.add_aura_rule(make_rule("Strider Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Strider)));
        faction.add_aura_rule(make_rule("Aura of Peace (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Mind Breaker (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Bad Omen (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Furious)));
        faction.add_aura_rule(make_rule("Wave of Discord (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::EnemyWithin, 18)));
        faction.add_aura_rule(make_rule("Deep Meditation (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_hit_mod(1, false, true)));
        faction.add_aura_rule(make_rule("Piercing Pulse (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(4)));
        registry.register_faction(faction);
    }

    // Robot Legions
    {
        FactionArmyRules faction("Robot Legions");
        faction.add_army_wide_rule(make_rule("Self-Repair", FactionRuleType::ArmyWide, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Casting Buff", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Destructive", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Indirect Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Infiltrate", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mobile Artillery", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Ravage", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Repel Ambushers", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Self-Repair Boost Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Shred", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Spawn", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Surge", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Surprise Piercing Shot", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Swift", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Swift Buff", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Ambush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Ambush)));
        faction.add_aura_rule(make_rule("No Retreat Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Piercing Assault Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Reanimation Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Relentless Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Relentless)));
        faction.add_aura_rule(make_rule("Stealth Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Stealth)));
        faction.add_aura_rule(make_rule("Triangulation Bots (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Piercing Bots (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_aura_rule(make_rule("Inspiring Bots (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Flame Bots (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(4, 0, TargetType::EnemyWithin, 9)));
        faction.add_aura_rule(make_rule("Mending Bots (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Gauss Bots (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(9, 0, TargetType::EnemyWithin, 6)));
        registry.register_faction(faction);
    }

    // Saurian Starhost
    {
        FactionArmyRules faction("Saurian Starhost");
        faction.add_army_wide_rule(make_rule("Primal", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Ambush Beacon", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Bane Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_grants_rule(RuleId::Bane)));
        faction.add_special_rule(make_rule("Bounding", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Breath Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Crossing Strike", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Disintegrate", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Evasive", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(-1, false, false)));
        faction.add_special_rule(make_rule("Fortified", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Good Shot", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, true)));
        faction.add_special_rule(make_rule("Heavy Impact", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Precise", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Precision Target", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Primal Boost Buff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Protection Feat", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Ravage", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Spell Conduit", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Teleport", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unpredictable Fighter", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_aura_rule(make_rule("Counter-Attack Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Counter)));
        faction.add_aura_rule(make_rule("Rapid Charge Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rending when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Rending)));
        faction.add_aura_rule(make_rule("Scout Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Scout)));
        faction.add_aura_rule(make_rule("Toxin Mist (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Serpent Comet (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Fateful Guidance (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Furious)));
        faction.add_aura_rule(make_rule("Piranha Curse (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(4, 0, TargetType::EnemyWithin, 9)));
        faction.add_aura_rule(make_rule("Celestial Roar (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Jaguar Blaze (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // Soul-Snatcher Cults
    {
        FactionArmyRules faction("Soul-Snatcher Cults");
        faction.add_army_wide_rule(make_rule("Fanatic", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Bounding", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Increased Shooting Range Buff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mind Control", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Precision Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_hit_mod(1, false, true)));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Reap", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Repel Ambushers", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Speed Buff", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Spell Conduit", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Takedown Strike", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, effect_ap_mod(2)));
        faction.add_aura_rule(make_rule("Grounded Precision Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Grounded Reinforcement Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Melee Slayer Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Quick Shot Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Insidious Protection (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Mind Corruption (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Deep Hypnosis (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Psychic Onslaught (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(4, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Bio-Displacer (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Brain Burst (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        registry.register_faction(faction);
    }

    // Titan Lords
    {
        FactionArmyRules faction("Titan Lords");
        faction.add_army_wide_rule(make_rule("Honor Code", FactionRuleType::ArmyWide, FactionRuleCategory::Defense, TriggerTiming::StartOfRound, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Changebound", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(-1, false, false)));
        faction.add_special_rule(make_rule("Delayed Action", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Fortified", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Lustbound", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Plaguebound", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Warbound", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Psy-Injected Courage (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Electric Tempest (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Calculated Foresight (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Relentless)));
        faction.add_aura_rule(make_rule("Searing Burst (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Shock Speed (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Expel Threat (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // War Disciples
    {
        FactionArmyRules faction("War Disciples");
        faction.add_army_wide_rule(make_rule("Warbound", FactionRuleType::ArmyWide, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Dangerous Terrain Debuff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Piercing Assault", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OnCharge, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Slam", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Steadfast Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unpredictable", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Unpredictable Fighter", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, true, false)));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Furious Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Furious)));
        faction.add_aura_rule(make_rule("Relentless Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Relentless)));
        faction.add_aura_rule(make_rule("Resistance Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Scout Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Scout)));
        faction.add_aura_rule(make_rule("Versatile Defense Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Warbound Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Terrifying Fury (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Flame of Destruction (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(1, 0, TargetType::EnemyWithin, 18)));
        faction.add_aura_rule(make_rule("Fiery Protection (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Brutal Massacre (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::EnemyWithin, 6)));
        faction.add_aura_rule(make_rule("War Boon (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Headtaker Strike (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        registry.register_faction(faction);
    }

    // Watch Brothers
    {
        FactionArmyRules faction("Watch Brothers");
        faction.add_army_wide_rule(make_rule("Watchborn", FactionRuleType::ArmyWide, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Smash", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Strafing", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unstoppable Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Bane in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Bane when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Rush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Shred when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Versatile Reach Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Watch Sight (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Watch Trauma (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(1, 0, TargetType::EnemyWithin, 18)));
        faction.add_aura_rule(make_rule("Blessed Ammo (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Lightning Fog (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(4, 0, TargetType::EnemyWithin, 9)));
        faction.add_aura_rule(make_rule("Watch Dome (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Psychic Terror (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(9, 0, TargetType::EnemyWithin, 6)));
        registry.register_faction(faction);
    }

    // Watch Prime Brothers
    {
        FactionArmyRules faction("Watch Prime Brothers");
        faction.add_army_wide_rule(make_rule("Reinforced", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_army_wide_rule(make_rule("Watchborn", FactionRuleType::ArmyWide, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Demolish", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Melee Slayer", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Precise", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Ravage", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Retaliate", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Unstoppable Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Bane in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Bane when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Rush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Shred when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Versatile Reach Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Watch Sight (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Watch Wound (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Blessed Ammo (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Lightning Fog (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(4, 0, TargetType::EnemyWithin, 9)));
        faction.add_aura_rule(make_rule("Watch Dome (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Psychic Terror (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(9, 0, TargetType::EnemyWithin, 6)));
        registry.register_faction(faction);
    }

    // Wolf Brothers
    {
        FactionArmyRules faction("Wolf Brothers");
        faction.add_army_wide_rule(make_rule("Wolfborn", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Fortified", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Smash", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Unstoppable Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Bane in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Bane when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Counter-Attack Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Counter)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Rush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Versatile Reach Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Wolf Sight (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Wolf Trauma (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(1, 0, TargetType::EnemyWithin, 18)));
        faction.add_aura_rule(make_rule("Righteous Fury (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Godly Thunder (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(4)));
        faction.add_aura_rule(make_rule("Wolf Dome (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Storm of Power (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // Wolf Prime Brothers
    {
        FactionArmyRules faction("Wolf Prime Brothers");
        faction.add_army_wide_rule(make_rule("Reinforced", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_army_wide_rule(make_rule("Wolfborn", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Demolish", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Melee Slayer", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Mend", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Precise", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Ravage", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Deployment", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Re-Position Artillery", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Retaliate", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Unstoppable Shooting Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Versatile Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_aura_rule(make_rule("Bane in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Bane when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Counter-Attack Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Counter)));
        faction.add_aura_rule(make_rule("Courage Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_morale_mod(1)));
        faction.add_aura_rule(make_rule("Melee Shrouding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Rush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Versatile Reach Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Wolf Sight (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Wolf Wound (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Righteous Fury (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Godly Thunder (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(4)));
        faction.add_aura_rule(make_rule("Wolf Dome (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Storm of Power (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // Wormhole Daemons of Change
    {
        FactionArmyRules faction("Wormhole Daemons of Change");
        faction.add_army_wide_rule(make_rule("Changebound", FactionRuleType::ArmyWide, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(-1, false, false)));
        faction.add_special_rule(make_rule("Casting Debuff", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Changebound Boost", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_hit_mod(-1, false, false)));
        faction.add_special_rule(make_rule("Changebound Boost Aura", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Resistance", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shred", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Slash", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Spell Accumulator", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Split", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Storm of Change", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, effect_deals_hits(3, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Ambush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Ambush)));
        faction.add_aura_rule(make_rule("Bounding Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Fearless Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Fearless)));
        faction.add_aura_rule(make_rule("Indirect when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Resistance Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Teleport Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Unstoppable in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Shifting Form (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Sky Blaze (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Breath of Change (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Bane)));
        faction.add_aura_rule(make_rule("Mutating Inferno (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(4, 0, TargetType::EnemyWithin, 9)));
        faction.add_aura_rule(make_rule("Change Boon (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Power Bolt (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        registry.register_faction(faction);
    }

    // Wormhole Daemons of Lust
    {
        FactionArmyRules faction("Wormhole Daemons of Lust");
        faction.add_army_wide_rule(make_rule("Lustbound", FactionRuleType::ArmyWide, FactionRuleCategory::None, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Counter-Attack", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Hit & Run Fighter", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Lustbound Boost", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Mind Control", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Piercing Assault", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OnCharge, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Quick Shot Mark", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Resistance", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Retaliate", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shatter", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Spawn", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::OncePerGame, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Storm of Lust", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, effect_deals_hits(3, 0, TargetType::EnemyWithin, 12)));
        faction.add_special_rule(make_rule("Surge", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_aura_rule(make_rule("Ambush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Ambush)));
        faction.add_aura_rule(make_rule("Counter-Attack Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Counter)));
        faction.add_aura_rule(make_rule("Hit & Run Fighter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Lustbound Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Piercing Assault Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Strider Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Strider)));
        faction.add_aura_rule(make_rule("Unstoppable when Shooting Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Combat Ecstasy (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Beautiful Pain (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(2, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Blissful Dance (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Total Seizure (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Lust Boon (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Overpowering Lash (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        registry.register_faction(faction);
    }

    // Wormhole Daemons of Plague
    {
        FactionArmyRules faction("Wormhole Daemons of Plague");
        faction.add_army_wide_rule(make_rule("Plaguebound", FactionRuleType::ArmyWide, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Butcher", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Difficult Terrain Debuff", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Fortified Growth", FactionRuleType::Special, FactionRuleCategory::None, TriggerTiming::StartOfRound, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Plaguebound Boost", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Precision Attacks Buff", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerActivation, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Regeneration Buff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, effect_grants_rule(RuleId::Regeneration)));
        faction.add_special_rule(make_rule("Steadfast", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::StartOfRound, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Storm of Plague", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, effect_deals_hits(3, 0, TargetType::EnemyWithin, 12)));
        faction.add_aura_rule(make_rule("Ambush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Ambush)));
        faction.add_aura_rule(make_rule("Plaguebound Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Regeneration Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Regeneration)));
        faction.add_aura_rule(make_rule("Relentless Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Relentless)));
        faction.add_aura_rule(make_rule("Rending in Melee Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Rending)));
        faction.add_aura_rule(make_rule("Steadfast Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Aura of Pestilence (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rapid Putrefaction (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Blessed Virus (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Plague Malediction (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(4)));
        faction.add_aura_rule(make_rule("Plague Boon (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Rot Wave (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::EnemyWithin, 18)));
        registry.register_faction(faction);
    }

    // Wormhole Daemons of War
    {
        FactionArmyRules faction("Wormhole Daemons of War");
        faction.add_army_wide_rule(make_rule("Warbound", FactionRuleType::ArmyWide, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Break", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_hit_mod(6, false, false)));
        faction.add_special_rule(make_rule("Crush", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, effect_ap_mod(2)));
        faction.add_special_rule(make_rule("Destructive Frenzy", FactionRuleType::Special, FactionRuleCategory::Unit, TriggerTiming::Always, effect_hit_mod(1, false, false)));
        faction.add_special_rule(make_rule("Fatigue Debuff", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::OncePerActivation, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Rapid Charge", FactionRuleType::Special, FactionRuleCategory::Movement, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Repel Ambushers", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Resistance", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_special_rule(make_rule("Shielded", FactionRuleType::Special, FactionRuleCategory::Defense, TriggerTiming::Always, effect_defense_mod(1)));
        faction.add_special_rule(make_rule("Storm of War", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::OncePerGame, effect_ap_mod(1)));
        faction.add_special_rule(make_rule("Warbound Boost", FactionRuleType::Special, FactionRuleCategory::Weapon, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Ambush Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Ambush)));
        faction.add_aura_rule(make_rule("Furious Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Furious)));
        faction.add_aura_rule(make_rule("Piercing Fighter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Piercing Shooter Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(1)));
        faction.add_aura_rule(make_rule("Rapid Charge Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Shielded Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_grants_rule(RuleId::Shielded)));
        faction.add_aura_rule(make_rule("Warbound Boost Aura", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Terrifying Fury (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Flame of Destruction (1)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(1, 0, TargetType::EnemyWithin, 18)));
        faction.add_aura_rule(make_rule("Fiery Protection (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Brutal Massacre (2)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_deals_hits(6, 0, TargetType::EnemyWithin, 6)));
        faction.add_aura_rule(make_rule("War Boon (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, FactionRuleEffect{}));
        faction.add_aura_rule(make_rule("Headtaker Strike (3)", FactionRuleType::Aura, FactionRuleCategory::AuraEffect, TriggerTiming::Always, effect_ap_mod(2)));
        registry.register_faction(faction);
    }

    registry.set_initialized(true);
}

} // namespace battle
