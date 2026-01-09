#include "rules/factions/faction_rule_ids.hpp"
#include "core/rule_registry.hpp"
#include "core/contexts.hpp"
#include "core/modifiers.hpp"
#include "core/aura_processor.hpp"
#include "core/unit.hpp"
#include <unordered_map>
#include <string>

namespace battle {

// ==============================================================================
// Faction Rule Info Storage
// ==============================================================================

namespace {

// Storage for all faction rule info
std::vector<FactionRuleInfo> g_faction_rules;
std::unordered_map<u16, size_t> g_rule_id_to_index;
std::unordered_map<std::string, std::unordered_map<std::string, u16>> g_faction_name_to_rule;

void add_faction_rule(const FactionRuleInfo& info) {
    size_t index = g_faction_rules.size();
    g_faction_rules.push_back(info);
    g_rule_id_to_index[info.rule_id] = index;

    // Add to name lookup
    std::string faction_name = get_faction_name(info.faction);
    g_faction_name_to_rule[faction_name][info.name] = info.rule_id;
}

// ==============================================================================
// Faction Rule Effect Functions
// ==============================================================================

// Generic hit modifier effect
void faction_hit_modifier_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) {
    ctx.hit_modifier += static_cast<i8>(value);
}

void faction_hit_penalty_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) {
    ctx.hit_modifier -= static_cast<i8>(value);
}

// Generic AP modifier effect
void faction_ap_modifier_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) {
    ctx.ap_modifier += static_cast<i8>(value);
}

// Generic defense modifier effect
void faction_defense_modifier_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) {
    ctx.defense_modifier += static_cast<i8>(value);
}

// Morale modifier for end-round context
void faction_morale_modifier_effect(EndRoundContext& /*ctx*/, Unit& /*unit*/, u8 /*value*/) {
    // Morale modifiers are handled via the modifier layer
    // This is a placeholder for rules that need special morale handling
}

// ==============================================================================
// Register Alien Hives Faction Rules
// ==============================================================================

void register_alien_hives_rules(RuleRegistry& registry, AuraProcessor& auras) {
    using namespace alien_hives;

    // Hive Bond - Army-wide +1 morale
    add_faction_rule({
        HiveBond, "Hive Bond", FactionId::AlienHives,
        RuleId::None, 0, 0, 0, 1, false, false, false, 0
    });

    // Breath Attack - +1 AP once per activation
    add_faction_rule({
        BreathAttack, "Breath Attack", FactionId::AlienHives,
        RuleId::None, 0, 0, 1, 0, false, false, false, 0
    });

    // Piercing Growth - +1 AP at round start
    add_faction_rule({
        PiercingGrowth, "Piercing Growth", FactionId::AlienHives,
        RuleId::None, 0, 0, 1, 0, false, false, false, 0
    });

    // Stealth Buff - Grants Stealth once per activation
    add_faction_rule({
        StealthBuff, "Stealth Buff", FactionId::AlienHives,
        RuleId::Stealth, 0, 0, 0, 0, false, false, false, 0
    });

    // Surprise Attack - +1 AP
    add_faction_rule({
        SurpriseAttack, "Surprise Attack", FactionId::AlienHives,
        RuleId::None, 0, 0, 1, 0, false, false, false, 0
    });

    // Takedown Strike - +2 AP once per game
    add_faction_rule({
        TakedownStrike, "Takedown Strike", FactionId::AlienHives,
        RuleId::None, 0, 0, 2, 0, false, false, false, 0
    });

    // Furious Aura - Grants Furious to nearby friendlies
    add_faction_rule({
        FuriousAura, "Furious Aura", FactionId::AlienHives,
        RuleId::Furious, 0, 0, 0, 0, false, false, true, 12
    });
    AuraDefinition furious_aura;
    furious_aura.source_rule = static_cast<RuleId>(FuriousAura);
    furious_aura.grants_rule = RuleId::Furious;
    furious_aura.range = 12;
    furious_aura.affects_friendly = true;
    auras.register_aura(furious_aura);

    // Shielded Aura - Grants Shielded to nearby friendlies
    add_faction_rule({
        ShieldedAura, "Shielded Aura", FactionId::AlienHives,
        RuleId::Shielded, 0, 0, 0, 0, false, false, true, 12
    });
    AuraDefinition shielded_aura;
    shielded_aura.source_rule = static_cast<RuleId>(ShieldedAura);
    shielded_aura.grants_rule = RuleId::Shielded;
    shielded_aura.range = 12;
    shielded_aura.affects_friendly = true;
    auras.register_aura(shielded_aura);

    // Overwhelming Strike spell - +1 AP
    add_faction_rule({
        OverwhelmingStrike, "Overwhelming Strike (1)", FactionId::AlienHives,
        RuleId::None, 0, 0, 1, 0, false, false, false, 0
    });

    // Register hot data for rules that need combat effects
    // These will be processed during combat when units have these rules

    // Note: Most faction rules work by granting base rules or providing
    // modifiers that are applied through the modifier layer
}

// ==============================================================================
// Register Battle Brothers Faction Rules
// ==============================================================================

void register_battle_brothers_rules(RuleRegistry& registry, AuraProcessor& auras) {
    using namespace battle_brothers;

    // Note: Battleborn is already a base rule (RuleId::Battleborn)
    // Battle Brothers get it as army-wide

    // Mend - Heal ability (once per activation)
    add_faction_rule({
        Mend, "Mend", FactionId::BattleBrothers,
        RuleId::None, 0, 0, 0, 0, false, false, false, 0
    });

    // Re-Deployment - Movement ability
    add_faction_rule({
        ReDeployment, "Re-Deployment", FactionId::BattleBrothers,
        RuleId::None, 0, 0, 0, 0, false, false, false, 0
    });

    // Bane in Melee Aura - Grants Bane in melee
    add_faction_rule({
        BaneInMeleeAura, "Bane in Melee Aura", FactionId::BattleBrothers,
        RuleId::Bane, 0, 0, 0, 0, true, false, true, 12
    });
    AuraDefinition bane_melee_aura;
    bane_melee_aura.source_rule = static_cast<RuleId>(BaneInMeleeAura);
    bane_melee_aura.grants_rule = RuleId::Bane;
    bane_melee_aura.range = 12;
    bane_melee_aura.affects_friendly = true;
    bane_melee_aura.condition = ModifierCondition::MELEE_ONLY;
    auras.register_aura(bane_melee_aura);

    // Bane when Shooting Aura - Grants Bane when shooting
    add_faction_rule({
        BaneWhenShootingAura, "Bane when Shooting Aura", FactionId::BattleBrothers,
        RuleId::Bane, 0, 0, 0, 0, false, true, true, 12
    });
    AuraDefinition bane_shooting_aura;
    bane_shooting_aura.source_rule = static_cast<RuleId>(BaneWhenShootingAura);
    bane_shooting_aura.grants_rule = RuleId::Bane;
    bane_shooting_aura.range = 12;
    bane_shooting_aura.affects_friendly = true;
    bane_shooting_aura.condition = ModifierCondition::SHOOTING_ONLY;
    auras.register_aura(bane_shooting_aura);

    // Courage Aura - +1 morale to nearby friendlies
    add_faction_rule({
        CourageAura, "Courage Aura", FactionId::BattleBrothers,
        RuleId::MoraleBoost, 0, 0, 0, 1, false, false, true, 12
    });
    AuraDefinition courage_aura;
    courage_aura.source_rule = static_cast<RuleId>(CourageAura);
    courage_aura.grants_rule = RuleId::MoraleBoost;
    courage_aura.grants_value = 1;
    courage_aura.range = 12;
    courage_aura.affects_friendly = true;
    auras.register_aura(courage_aura);

    // Regeneration Aura - Grants Regeneration
    add_faction_rule({
        RegenerationAura, "Regeneration Aura", FactionId::BattleBrothers,
        RuleId::Regeneration, 0, 0, 0, 0, false, false, true, 12
    });
    AuraDefinition regen_aura;
    regen_aura.source_rule = static_cast<RuleId>(RegenerationAura);
    regen_aura.grants_rule = RuleId::Regeneration;
    regen_aura.range = 12;
    regen_aura.affects_friendly = true;
    auras.register_aura(regen_aura);
}

// ==============================================================================
// Initialize all faction rules
// ==============================================================================

bool g_faction_rules_initialized = false;

} // anonymous namespace

// ==============================================================================
// Public API Implementation
// ==============================================================================

const FactionRuleInfo* get_faction_rule_info(u16 rule_id) {
    auto it = g_rule_id_to_index.find(rule_id);
    if (it == g_rule_id_to_index.end()) return nullptr;
    return &g_faction_rules[it->second];
}

u16 find_faction_rule_by_name(FactionId faction, std::string_view name) {
    std::string faction_name = get_faction_name(faction);
    auto faction_it = g_faction_name_to_rule.find(faction_name);
    if (faction_it == g_faction_name_to_rule.end()) return 0;

    auto rule_it = faction_it->second.find(std::string(name));
    if (rule_it == faction_it->second.end()) return 0;

    return rule_it->second;
}

void register_all_faction_rules() {
    if (g_faction_rules_initialized) return;

    // Clear any existing data
    g_faction_rules.clear();
    g_rule_id_to_index.clear();
    g_faction_name_to_rule.clear();

    // Reserve space for efficiency
    g_faction_rules.reserve(1200);  // ~1054 faction rules

    // Create a temporary registry and aura processor for registration
    // In practice, these would be the global instances
    RuleRegistry registry;
    AuraProcessor auras;

    // Register each faction's rules
    register_alien_hives_rules(registry, auras);
    register_battle_brothers_rules(registry, auras);

    // TODO: Register remaining factions
    // register_blessed_sisters_rules(registry, auras);
    // register_blood_brothers_rules(registry, auras);
    // ... etc for all 43 factions

    g_faction_rules_initialized = true;
}

// ==============================================================================
// Faction Rule Application Helpers
// ==============================================================================

namespace faction_helpers {

// Check if a unit has a specific faction rule (by ID)
bool unit_has_faction_rule(const Unit& unit, u16 faction_rule_id) {
    // First check if the rule grants a base rule
    const FactionRuleInfo* info = get_faction_rule_info(faction_rule_id);
    if (!info) return false;

    // If it grants a base rule, check for that
    if (info->grants_base_rule != RuleId::None) {
        return unit.has_rule(info->grants_base_rule);
    }

    // Otherwise check the unit's faction-specific rules
    // This would require the unit to store faction rule IDs
    // For now, return false - units need to be extended to store these
    return false;
}

// Apply faction rule modifiers to combat context
void apply_faction_modifiers(CombatContextCore& ctx, const Unit& attacker,
                              const Unit& defender, bool is_melee) {
    // This function would iterate through the attacker's faction rules
    // and apply their modifiers to the combat context

    // For now, this is a placeholder
    // Full implementation requires:
    // 1. Units to store their faction rule IDs
    // 2. Combat resolver to call this function during appropriate phases
    (void)ctx;
    (void)attacker;
    (void)defender;
    (void)is_melee;
}

// Get total stat modifiers from faction rules
StatModifiers get_faction_stat_modifiers(const Unit& unit, bool is_melee,
                                          bool is_charging, u8 distance) {
    StatModifiers result;

    // This would aggregate all faction rule modifiers
    // For now, return empty modifiers
    (void)unit;
    (void)is_melee;
    (void)is_charging;
    (void)distance;

    return result;
}

} // namespace faction_helpers

} // namespace battle
