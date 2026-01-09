#include "rules/factions/faction_rule_ids.hpp"
#include "core/rule_registry.hpp"
#include "core/contexts.hpp"
#include "core/modifiers.hpp"
#include "core/aura_processor.hpp"
#include "core/unit.hpp"
#include <unordered_map>
#include <string>
#include <vector>

namespace battle {

// ==============================================================================
// Faction Rule Storage
// ==============================================================================

namespace {

std::vector<FactionRuleInfo> g_faction_rules;
std::unordered_map<u16, size_t> g_rule_id_to_index;
std::unordered_map<std::string, std::unordered_map<std::string, u16>> g_faction_name_to_rule;
bool g_initialized = false;

// Counter for assigning rule IDs within each faction
std::array<u8, static_cast<size_t>(FactionId::COUNT)> g_army_wide_counter{};
std::array<u8, static_cast<size_t>(FactionId::COUNT)> g_special_counter{};
std::array<u8, static_cast<size_t>(FactionId::COUNT)> g_aura_counter{};

// Helper to add a faction rule
u16 add_rule(FactionId faction, FactionRuleType type, const char* name,
             RuleId grants = RuleId::None,
             i8 hit_mod = 0, i8 def_mod = 0, i8 ap_mod = 0, i8 morale_mod = 0,
             u8 deals_hits = 0, u8 hits_ap = 0, u8 hits_range = 0,
             bool melee_only = false, bool shooting_only = false,
             bool is_aura = false, u8 aura_range = 12,
             bool once_game = false, bool once_activation = false) {

    u8 fid = static_cast<u8>(faction);
    u8 index;

    switch (type) {
        case FactionRuleType::ArmyWide:
            index = g_army_wide_counter[fid]++;
            break;
        case FactionRuleType::Special:
            index = g_special_counter[fid]++;
            break;
        case FactionRuleType::Aura:
            index = g_aura_counter[fid]++;
            is_aura = true;  // Force is_aura for Aura type
            break;
        default:
            index = 0;
    }

    u16 rule_id = make_faction_rule_id(faction, type, index);

    FactionRuleInfo info{};
    info.rule_id = rule_id;
    info.name = name;
    info.faction = faction;
    info.grants_base_rule = grants;
    info.hit_modifier = hit_mod;
    info.defense_modifier = def_mod;
    info.ap_modifier = ap_mod;
    info.morale_modifier = morale_mod;
    info.deals_hits = deals_hits;
    info.deals_hits_ap = hits_ap;
    info.deals_hits_range = hits_range;
    info.melee_only = melee_only;
    info.shooting_only = shooting_only;
    info.is_aura = is_aura;
    info.aura_range = aura_range;
    info.once_per_game = once_game;
    info.once_per_activation = once_activation;

    size_t idx = g_faction_rules.size();
    g_faction_rules.push_back(info);
    g_rule_id_to_index[rule_id] = idx;

    std::string faction_name = get_faction_name(faction);
    g_faction_name_to_rule[faction_name][name] = rule_id;

    return rule_id;
}

// Shorthand macros for common patterns
#define ARMY_RULE(faction, name, ...) add_rule(FactionId::faction, FactionRuleType::ArmyWide, name, ##__VA_ARGS__)
#define SPECIAL_RULE(faction, name, ...) add_rule(FactionId::faction, FactionRuleType::Special, name, ##__VA_ARGS__)
#define AURA_RULE(faction, name, ...) add_rule(FactionId::faction, FactionRuleType::Aura, name, ##__VA_ARGS__)

// ==============================================================================
// Register All Faction Rules
// ==============================================================================

void register_alien_hives() {
    // Army-wide
    ARMY_RULE(AlienHives, "Hive Bond", RuleId::None, 0, 0, 0, 1);

    // Special rules
    SPECIAL_RULE(AlienHives, "Agile");
    SPECIAL_RULE(AlienHives, "Breath Attack", RuleId::None, 0, 0, 1, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(AlienHives, "Caster Group");
    SPECIAL_RULE(AlienHives, "Increased Shooting Range");
    SPECIAL_RULE(AlienHives, "Infiltrate");
    SPECIAL_RULE(AlienHives, "No Retreat");
    SPECIAL_RULE(AlienHives, "Piercing Growth", RuleId::None, 0, 0, 1);
    SPECIAL_RULE(AlienHives, "Precise", RuleId::None, 1);
    SPECIAL_RULE(AlienHives, "Predator Fighter", RuleId::None, 6, 0, 0, 0, 0, 0, 0, true);
    SPECIAL_RULE(AlienHives, "Rapid Charge");
    SPECIAL_RULE(AlienHives, "Ravage");
    SPECIAL_RULE(AlienHives, "Regenerative Strength");
    SPECIAL_RULE(AlienHives, "Resistance");
    SPECIAL_RULE(AlienHives, "Rupture", RuleId::None, 6);
    SPECIAL_RULE(AlienHives, "Self-Destruct");
    SPECIAL_RULE(AlienHives, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(AlienHives, "Shred");
    SPECIAL_RULE(AlienHives, "Spawn", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, true);
    SPECIAL_RULE(AlienHives, "Spell Conduit");
    SPECIAL_RULE(AlienHives, "Stealth Buff", RuleId::Stealth, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(AlienHives, "Strafing", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(AlienHives, "Surprise Attack", RuleId::None, 0, 0, 1);
    SPECIAL_RULE(AlienHives, "Takedown Strike", RuleId::None, 0, 0, 2, 0, 0, 0, 0, false, false, false, 0, true);
    SPECIAL_RULE(AlienHives, "Unpredictable Fighter Mark", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);

    // Aura rules
    AURA_RULE(AlienHives, "Furious Aura", RuleId::Furious);
    AURA_RULE(AlienHives, "Hive Bond Boost Aura");
    AURA_RULE(AlienHives, "Increased Shooting Range Aura");
    AURA_RULE(AlienHives, "Rapid Charge Aura");
    AURA_RULE(AlienHives, "Shielded Aura", RuleId::Shielded);
    AURA_RULE(AlienHives, "Animate Spirit (1)");
    AURA_RULE(AlienHives, "Overwhelming Strike (1)", RuleId::None, 0, 0, 1);
    AURA_RULE(AlienHives, "Infuse Bloodthirst (2)");
}

void register_battle_brothers() {
    // Army-wide
    ARMY_RULE(BattleBrothers, "Battleborn", RuleId::Battleborn);

    // Special rules
    SPECIAL_RULE(BattleBrothers, "Mend", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(BattleBrothers, "Re-Deployment");
    SPECIAL_RULE(BattleBrothers, "Re-Position Artillery", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(BattleBrothers, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(BattleBrothers, "Smash");
    SPECIAL_RULE(BattleBrothers, "Unstoppable Shooting Mark", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(BattleBrothers, "Versatile Attack", RuleId::None, 1);

    // Aura rules
    AURA_RULE(BattleBrothers, "Bane in Melee Aura", RuleId::Bane, 0, 0, 0, 0, 0, 0, 0, true);
    AURA_RULE(BattleBrothers, "Bane when Shooting Aura", RuleId::Bane, 0, 0, 0, 0, 0, 0, 0, false, true);
    AURA_RULE(BattleBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(BattleBrothers, "Melee Shrouding Aura");
    AURA_RULE(BattleBrothers, "Rapid Rush Aura");
    AURA_RULE(BattleBrothers, "Regeneration Aura", RuleId::Regeneration);
    AURA_RULE(BattleBrothers, "Versatile Reach Aura");
    AURA_RULE(BattleBrothers, "Advanced Sight (1)");
}

void register_blessed_sisters() {
    ARMY_RULE(BlessedSisters, "Devout", RuleId::Devout, 6);

    SPECIAL_RULE(BlessedSisters, "Casting Debuff", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(BlessedSisters, "Courage Buff", RuleId::None, 0, 0, 0, 1, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(BlessedSisters, "Devout Boost");
    SPECIAL_RULE(BlessedSisters, "Fortified");
    SPECIAL_RULE(BlessedSisters, "Guarded Buff", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(BlessedSisters, "Piercing Assault", RuleId::None, 0, 0, 1);
    SPECIAL_RULE(BlessedSisters, "Point-Blank Surge", RuleId::None, 6);
    SPECIAL_RULE(BlessedSisters, "Precision Fighter Buff", RuleId::None, 1, 0, 0, 0, 0, 0, 0, true, false, false, 0, false, true);
    SPECIAL_RULE(BlessedSisters, "Precision Shooter Buff", RuleId::None, 1, 0, 0, 0, 0, 0, 0, false, true, false, 0, false, true);
    SPECIAL_RULE(BlessedSisters, "Precision Target", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, true);
    SPECIAL_RULE(BlessedSisters, "Purge", RuleId::None, 0, 0, 1);
    SPECIAL_RULE(BlessedSisters, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(BlessedSisters, "Spell Conduit");

    AURA_RULE(BlessedSisters, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(BlessedSisters, "Devout Boost Aura");
    AURA_RULE(BlessedSisters, "Fast Aura", RuleId::Fast);
    AURA_RULE(BlessedSisters, "Fortified Aura");
    AURA_RULE(BlessedSisters, "Increased Shooting Range Aura");
    AURA_RULE(BlessedSisters, "Point-Blank Piercing Aura");
    AURA_RULE(BlessedSisters, "Regeneration Aura", RuleId::Regeneration);
    AURA_RULE(BlessedSisters, "Shred in Melee Aura");
}

void register_blood_brothers() {
    ARMY_RULE(BloodBrothers, "Bloodborn", RuleId::None, 6);

    SPECIAL_RULE(BloodBrothers, "Mend", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(BloodBrothers, "Re-Deployment");
    SPECIAL_RULE(BloodBrothers, "Re-Position Artillery", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(BloodBrothers, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(BloodBrothers, "Smash");
    SPECIAL_RULE(BloodBrothers, "Unstoppable Shooting Mark", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(BloodBrothers, "Versatile Attack", RuleId::None, 1);

    AURA_RULE(BloodBrothers, "Bane in Melee Aura", RuleId::Bane, 0, 0, 0, 0, 0, 0, 0, true);
    AURA_RULE(BloodBrothers, "Bane when Shooting Aura", RuleId::Bane, 0, 0, 0, 0, 0, 0, 0, false, true);
    AURA_RULE(BloodBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(BloodBrothers, "Melee Shrouding Aura");
    AURA_RULE(BloodBrothers, "Piercing Assault Aura");
    AURA_RULE(BloodBrothers, "Rapid Rush Aura");
    AURA_RULE(BloodBrothers, "Regeneration Aura", RuleId::Regeneration);
    AURA_RULE(BloodBrothers, "Versatile Reach Aura");
}

void register_blood_prime_brothers() {
    ARMY_RULE(BloodPrimeBrothers, "Bloodborn", RuleId::None, 6);
    ARMY_RULE(BloodPrimeBrothers, "Reinforced");

    SPECIAL_RULE(BloodPrimeBrothers, "Demolish", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(BloodPrimeBrothers, "Melee Slayer", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(BloodPrimeBrothers, "Mend", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(BloodPrimeBrothers, "Precise", RuleId::None, 1);
    SPECIAL_RULE(BloodPrimeBrothers, "Ravage");
    SPECIAL_RULE(BloodPrimeBrothers, "Re-Deployment");
    SPECIAL_RULE(BloodPrimeBrothers, "Re-Position Artillery", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(BloodPrimeBrothers, "Retaliate");
    SPECIAL_RULE(BloodPrimeBrothers, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(BloodPrimeBrothers, "Unstoppable Shooting Mark", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(BloodPrimeBrothers, "Versatile Attack", RuleId::None, 1);

    AURA_RULE(BloodPrimeBrothers, "Bane in Melee Aura", RuleId::Bane, 0, 0, 0, 0, 0, 0, 0, true);
    AURA_RULE(BloodPrimeBrothers, "Bane when Shooting Aura", RuleId::Bane, 0, 0, 0, 0, 0, 0, 0, false, true);
    AURA_RULE(BloodPrimeBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(BloodPrimeBrothers, "Melee Shrouding Aura");
    AURA_RULE(BloodPrimeBrothers, "Piercing Assault Aura");
    AURA_RULE(BloodPrimeBrothers, "Rapid Rush Aura");
    AURA_RULE(BloodPrimeBrothers, "Regeneration Aura", RuleId::Regeneration);
    AURA_RULE(BloodPrimeBrothers, "Versatile Reach Aura");
}

void register_change_disciples() {
    ARMY_RULE(ChangeDisciples, "Wormhole Possessed");

    SPECIAL_RULE(ChangeDisciples, "Blink");
    SPECIAL_RULE(ChangeDisciples, "Change Strike", RuleId::None, 0, 0, 1);
    SPECIAL_RULE(ChangeDisciples, "Possessed Weapons");
    SPECIAL_RULE(ChangeDisciples, "Spell Conduit");
    SPECIAL_RULE(ChangeDisciples, "Warp Strike");

    AURA_RULE(ChangeDisciples, "Rage Aura", RuleId::Furious);
    AURA_RULE(ChangeDisciples, "Regeneration Aura", RuleId::Regeneration);
    AURA_RULE(ChangeDisciples, "Warp Bolt (1)", RuleId::None, 0, 0, 0, 0, 3, 0, 18);
    AURA_RULE(ChangeDisciples, "Warp Fire (2)", RuleId::None, 0, 0, 0, 0, 6, 0, 12);
    AURA_RULE(ChangeDisciples, "Warp Storm (3)", RuleId::None, 0, 0, 0, 0, 9, 0, 9);
}

void register_custodian_brothers() {
    ARMY_RULE(CustodianBrothers, "Golden Legion");
    ARMY_RULE(CustodianBrothers, "Reinforced");

    SPECIAL_RULE(CustodianBrothers, "Deadly Mark", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(CustodianBrothers, "Destroyer Mark", RuleId::None, 0, 0, 2, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(CustodianBrothers, "Improved Focus Fire");
    SPECIAL_RULE(CustodianBrothers, "Lethal Strike", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(CustodianBrothers, "Precise", RuleId::None, 1);
    SPECIAL_RULE(CustodianBrothers, "Re-Deployment");
    SPECIAL_RULE(CustodianBrothers, "Resistance");
    SPECIAL_RULE(CustodianBrothers, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(CustodianBrothers, "Versatile Attack", RuleId::None, 1);

    AURA_RULE(CustodianBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(CustodianBrothers, "Regeneration Aura", RuleId::Regeneration);
    AURA_RULE(CustodianBrothers, "Shielded Aura", RuleId::Shielded);
}

void register_dao_union() {
    ARMY_RULE(DAOUnion, "For the Greater Good");

    SPECIAL_RULE(DAOUnion, "Ambush Mark", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(DAOUnion, "Destroyer", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(DAOUnion, "Improved Focus Fire");
    SPECIAL_RULE(DAOUnion, "Indirect Spotter");
    SPECIAL_RULE(DAOUnion, "Markerlight");
    SPECIAL_RULE(DAOUnion, "Precise", RuleId::None, 1);
    SPECIAL_RULE(DAOUnion, "Shield Drones", RuleId::None, 0, 1);
    SPECIAL_RULE(DAOUnion, "Stealth Mark", RuleId::Stealth, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);

    AURA_RULE(DAOUnion, "AP Aura", RuleId::None, 0, 0, 1);
    AURA_RULE(DAOUnion, "Focus Fire Aura");
    AURA_RULE(DAOUnion, "Furious Aura", RuleId::Furious);
    AURA_RULE(DAOUnion, "Markerlight Aura");
}

void register_dark_brothers() {
    ARMY_RULE(DarkBrothers, "Dark Veil");

    SPECIAL_RULE(DarkBrothers, "Fearless");
    SPECIAL_RULE(DarkBrothers, "Night Terror");
    SPECIAL_RULE(DarkBrothers, "Poison", RuleId::Poison);
    SPECIAL_RULE(DarkBrothers, "Precise", RuleId::None, 1);
    SPECIAL_RULE(DarkBrothers, "Re-Deployment");
    SPECIAL_RULE(DarkBrothers, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(DarkBrothers, "Stealth");

    AURA_RULE(DarkBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(DarkBrothers, "Fear Aura", RuleId::Fear);
    AURA_RULE(DarkBrothers, "Poison Aura", RuleId::Poison);
    AURA_RULE(DarkBrothers, "Stealth Aura", RuleId::Stealth);
}

void register_dark_elf_raiders() {
    ARMY_RULE(DarkElfRaiders, "Pain Hungry");

    SPECIAL_RULE(DarkElfRaiders, "Agile");
    SPECIAL_RULE(DarkElfRaiders, "Hit and Run", RuleId::HitAndRun);
    SPECIAL_RULE(DarkElfRaiders, "Night Terrors");
    SPECIAL_RULE(DarkElfRaiders, "Poison", RuleId::Poison);
    SPECIAL_RULE(DarkElfRaiders, "Stealth");

    AURA_RULE(DarkElfRaiders, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(DarkElfRaiders, "Fast Aura", RuleId::Fast);
    AURA_RULE(DarkElfRaiders, "Furious Aura", RuleId::Furious);
    AURA_RULE(DarkElfRaiders, "Pain Aura");
}

void register_dark_prime_brothers() {
    ARMY_RULE(DarkPrimeBrothers, "Dark Veil");
    ARMY_RULE(DarkPrimeBrothers, "Reinforced");

    SPECIAL_RULE(DarkPrimeBrothers, "Demolish", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(DarkPrimeBrothers, "Fearless");
    SPECIAL_RULE(DarkPrimeBrothers, "Melee Slayer", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(DarkPrimeBrothers, "Night Terror");
    SPECIAL_RULE(DarkPrimeBrothers, "Poison", RuleId::Poison);
    SPECIAL_RULE(DarkPrimeBrothers, "Precise", RuleId::None, 1);
    SPECIAL_RULE(DarkPrimeBrothers, "Ravage");
    SPECIAL_RULE(DarkPrimeBrothers, "Re-Deployment");
    SPECIAL_RULE(DarkPrimeBrothers, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(DarkPrimeBrothers, "Stealth");

    AURA_RULE(DarkPrimeBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(DarkPrimeBrothers, "Fear Aura", RuleId::Fear);
    AURA_RULE(DarkPrimeBrothers, "Poison Aura", RuleId::Poison);
    AURA_RULE(DarkPrimeBrothers, "Stealth Aura", RuleId::Stealth);
}

void register_dwarf_guilds() {
    ARMY_RULE(DwarfGuilds, "Ancestral Grudge");

    SPECIAL_RULE(DwarfGuilds, "Fortified");
    SPECIAL_RULE(DwarfGuilds, "Hatred", RuleId::None, 0, 0, 0, 0, 0, 0, 0, true);
    SPECIAL_RULE(DwarfGuilds, "Precise", RuleId::None, 1);
    SPECIAL_RULE(DwarfGuilds, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(DwarfGuilds, "Slow", RuleId::Slow);
    SPECIAL_RULE(DwarfGuilds, "Stubborn");

    AURA_RULE(DwarfGuilds, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(DwarfGuilds, "Hatred Aura");
    AURA_RULE(DwarfGuilds, "Shielded Aura", RuleId::Shielded);
}

void register_elven_jesters() {
    ARMY_RULE(ElvenJesters, "Dancing Death");

    SPECIAL_RULE(ElvenJesters, "Agile");
    SPECIAL_RULE(ElvenJesters, "Counter", RuleId::Counter);
    SPECIAL_RULE(ElvenJesters, "Fast", RuleId::Fast);
    SPECIAL_RULE(ElvenJesters, "Stealth");

    AURA_RULE(ElvenJesters, "Fast Aura", RuleId::Fast);
    AURA_RULE(ElvenJesters, "Fear Aura", RuleId::Fear);
    AURA_RULE(ElvenJesters, "Stealth Aura", RuleId::Stealth);
}

void register_eternal_dynasty() {
    ARMY_RULE(EternalDynasty, "Reanimation");

    SPECIAL_RULE(EternalDynasty, "Fearless");
    SPECIAL_RULE(EternalDynasty, "Lethal Strike", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(EternalDynasty, "Phase", RuleId::None, 0, 1);
    SPECIAL_RULE(EternalDynasty, "Precise", RuleId::None, 1);
    SPECIAL_RULE(EternalDynasty, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(EternalDynasty, "Teleport");

    AURA_RULE(EternalDynasty, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(EternalDynasty, "Phase Aura");
    AURA_RULE(EternalDynasty, "Reanimation Aura");
    AURA_RULE(EternalDynasty, "Teleport Aura");
}

void register_goblin_reclaimers() {
    ARMY_RULE(GoblinReclaimers, "Scrap Metal");

    SPECIAL_RULE(GoblinReclaimers, "Blast", RuleId::Blast);
    SPECIAL_RULE(GoblinReclaimers, "Deadly", RuleId::Deadly);
    SPECIAL_RULE(GoblinReclaimers, "Fast", RuleId::Fast);
    SPECIAL_RULE(GoblinReclaimers, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(GoblinReclaimers, "Sneaky");

    AURA_RULE(GoblinReclaimers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(GoblinReclaimers, "Furious Aura", RuleId::Furious);
    AURA_RULE(GoblinReclaimers, "Regeneration Aura", RuleId::Regeneration);
}

void register_havoc_brothers() {
    ARMY_RULE(HavocBrothers, "Wormhole Corruption");

    SPECIAL_RULE(HavocBrothers, "Fearless");
    SPECIAL_RULE(HavocBrothers, "Possessed Weapons");
    SPECIAL_RULE(HavocBrothers, "Precise", RuleId::None, 1);
    SPECIAL_RULE(HavocBrothers, "Ravage");
    SPECIAL_RULE(HavocBrothers, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(HavocBrothers, "Shielded", RuleId::None, 0, 1);

    AURA_RULE(HavocBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(HavocBrothers, "Furious Aura", RuleId::Furious);
    AURA_RULE(HavocBrothers, "Regeneration Aura", RuleId::Regeneration);
}

void register_high_elf_fleets() {
    ARMY_RULE(HighElfFleets, "Ancient Wisdom");

    SPECIAL_RULE(HighElfFleets, "Agile");
    SPECIAL_RULE(HighElfFleets, "Counter", RuleId::Counter);
    SPECIAL_RULE(HighElfFleets, "Fast", RuleId::Fast);
    SPECIAL_RULE(HighElfFleets, "Precise", RuleId::None, 1);
    SPECIAL_RULE(HighElfFleets, "Spell Conduit");
    SPECIAL_RULE(HighElfFleets, "Stealth");

    AURA_RULE(HighElfFleets, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(HighElfFleets, "Fast Aura", RuleId::Fast);
    AURA_RULE(HighElfFleets, "Precise Aura", RuleId::Precise);
    AURA_RULE(HighElfFleets, "Stealth Aura", RuleId::Stealth);
}

void register_human_defense_force() {
    ARMY_RULE(HumanDefenseForce, "For the Emperor");

    SPECIAL_RULE(HumanDefenseForce, "Fearless");
    SPECIAL_RULE(HumanDefenseForce, "Orders");
    SPECIAL_RULE(HumanDefenseForce, "Precise", RuleId::None, 1);
    SPECIAL_RULE(HumanDefenseForce, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(HumanDefenseForce, "Stealth");

    AURA_RULE(HumanDefenseForce, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(HumanDefenseForce, "Orders Aura");
    AURA_RULE(HumanDefenseForce, "Shielded Aura", RuleId::Shielded);
}

void register_human_inquisition() {
    ARMY_RULE(HumanInquisition, "Holy Mandate");

    SPECIAL_RULE(HumanInquisition, "Fearless");
    SPECIAL_RULE(HumanInquisition, "Hatred", RuleId::None, 0, 0, 0, 0, 0, 0, 0, true);
    SPECIAL_RULE(HumanInquisition, "Precise", RuleId::None, 1);
    SPECIAL_RULE(HumanInquisition, "Purge", RuleId::None, 0, 0, 1);
    SPECIAL_RULE(HumanInquisition, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(HumanInquisition, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(HumanInquisition, "Spell Conduit");
    SPECIAL_RULE(HumanInquisition, "Stealth");

    AURA_RULE(HumanInquisition, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(HumanInquisition, "Fear Aura", RuleId::Fear);
    AURA_RULE(HumanInquisition, "Hatred Aura");
    AURA_RULE(HumanInquisition, "Purge Aura", RuleId::None, 0, 0, 1);
}

void register_infected_colonies() {
    ARMY_RULE(InfectedColonies, "Hive Mind");

    SPECIAL_RULE(InfectedColonies, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(InfectedColonies, "Shred");
    SPECIAL_RULE(InfectedColonies, "Stealth");

    AURA_RULE(InfectedColonies, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(InfectedColonies, "Regeneration Aura", RuleId::Regeneration);
    AURA_RULE(InfectedColonies, "Stealth Aura", RuleId::Stealth);
}

void register_jackals() {
    ARMY_RULE(Jackals, "Desert Raiders");

    SPECIAL_RULE(Jackals, "Agile");
    SPECIAL_RULE(Jackals, "Fast", RuleId::Fast);
    SPECIAL_RULE(Jackals, "Precise", RuleId::None, 1);
    SPECIAL_RULE(Jackals, "Stealth");

    AURA_RULE(Jackals, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(Jackals, "Fast Aura", RuleId::Fast);
    AURA_RULE(Jackals, "Regeneration Aura", RuleId::Regeneration);
    AURA_RULE(Jackals, "Stealth Aura", RuleId::Stealth);
}

void register_knight_brothers() {
    ARMY_RULE(KnightBrothers, "Holy Warriors");

    SPECIAL_RULE(KnightBrothers, "Fearless");
    SPECIAL_RULE(KnightBrothers, "Hatred", RuleId::None, 0, 0, 0, 0, 0, 0, 0, true);
    SPECIAL_RULE(KnightBrothers, "Precise", RuleId::None, 1);
    SPECIAL_RULE(KnightBrothers, "Purge", RuleId::None, 0, 0, 1);
    SPECIAL_RULE(KnightBrothers, "Shielded", RuleId::None, 0, 1);

    AURA_RULE(KnightBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(KnightBrothers, "Hatred Aura");
    AURA_RULE(KnightBrothers, "Purge Aura", RuleId::None, 0, 0, 1);
    AURA_RULE(KnightBrothers, "Regeneration Aura", RuleId::Regeneration);
}

void register_knight_prime_brothers() {
    ARMY_RULE(KnightPrimeBrothers, "Holy Warriors");
    ARMY_RULE(KnightPrimeBrothers, "Reinforced");

    SPECIAL_RULE(KnightPrimeBrothers, "Demolish", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(KnightPrimeBrothers, "Fearless");
    SPECIAL_RULE(KnightPrimeBrothers, "Hatred", RuleId::None, 0, 0, 0, 0, 0, 0, 0, true);
    SPECIAL_RULE(KnightPrimeBrothers, "Melee Slayer", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(KnightPrimeBrothers, "Precise", RuleId::None, 1);
    SPECIAL_RULE(KnightPrimeBrothers, "Purge", RuleId::None, 0, 0, 1);
    SPECIAL_RULE(KnightPrimeBrothers, "Ravage");
    SPECIAL_RULE(KnightPrimeBrothers, "Shielded", RuleId::None, 0, 1);

    AURA_RULE(KnightPrimeBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(KnightPrimeBrothers, "Hatred Aura");
    AURA_RULE(KnightPrimeBrothers, "Purge Aura", RuleId::None, 0, 0, 1);
    AURA_RULE(KnightPrimeBrothers, "Regeneration Aura", RuleId::Regeneration);
}

void register_lust_disciples() {
    ARMY_RULE(LustDisciples, "Wormhole Possessed");

    SPECIAL_RULE(LustDisciples, "Counter", RuleId::Counter);
    SPECIAL_RULE(LustDisciples, "Fast", RuleId::Fast);
    SPECIAL_RULE(LustDisciples, "Possessed Weapons");
    SPECIAL_RULE(LustDisciples, "Spell Conduit");

    AURA_RULE(LustDisciples, "Fast Aura", RuleId::Fast);
    AURA_RULE(LustDisciples, "Furious Aura", RuleId::Furious);
    AURA_RULE(LustDisciples, "Regeneration Aura", RuleId::Regeneration);
}

void register_machine_cult() {
    ARMY_RULE(MachineCult, "Praise the Machine");

    SPECIAL_RULE(MachineCult, "Fearless");
    SPECIAL_RULE(MachineCult, "Precise", RuleId::None, 1);
    SPECIAL_RULE(MachineCult, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(MachineCult, "Shielded", RuleId::None, 0, 1);

    AURA_RULE(MachineCult, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(MachineCult, "Regeneration Aura", RuleId::Regeneration);
    AURA_RULE(MachineCult, "Shielded Aura", RuleId::Shielded);
}

void register_orc_marauders() {
    ARMY_RULE(OrcMarauders, "WAAAGH!");

    SPECIAL_RULE(OrcMarauders, "Fast", RuleId::Fast);
    SPECIAL_RULE(OrcMarauders, "Fearless");
    SPECIAL_RULE(OrcMarauders, "Furious", RuleId::Furious);
    SPECIAL_RULE(OrcMarauders, "Regeneration", RuleId::Regeneration);

    AURA_RULE(OrcMarauders, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(OrcMarauders, "Fast Aura", RuleId::Fast);
    AURA_RULE(OrcMarauders, "Furious Aura", RuleId::Furious);
    AURA_RULE(OrcMarauders, "WAAAGH Aura");
}

void register_plague_disciples() {
    ARMY_RULE(PlagueDisciples, "Wormhole Possessed");

    SPECIAL_RULE(PlagueDisciples, "Fearless");
    SPECIAL_RULE(PlagueDisciples, "Poison", RuleId::Poison);
    SPECIAL_RULE(PlagueDisciples, "Possessed Weapons");
    SPECIAL_RULE(PlagueDisciples, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(PlagueDisciples, "Spell Conduit");

    AURA_RULE(PlagueDisciples, "Poison Aura", RuleId::Poison);
    AURA_RULE(PlagueDisciples, "Regeneration Aura", RuleId::Regeneration);
}

void register_prime_brothers() {
    ARMY_RULE(PrimeBrothers, "Battleborn", RuleId::Battleborn);
    ARMY_RULE(PrimeBrothers, "Reinforced");

    SPECIAL_RULE(PrimeBrothers, "Demolish", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(PrimeBrothers, "Melee Slayer", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(PrimeBrothers, "Mend", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(PrimeBrothers, "Precise", RuleId::None, 1);
    SPECIAL_RULE(PrimeBrothers, "Ravage");
    SPECIAL_RULE(PrimeBrothers, "Re-Deployment");
    SPECIAL_RULE(PrimeBrothers, "Re-Position Artillery", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(PrimeBrothers, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(PrimeBrothers, "Unstoppable Shooting Mark", RuleId::None, 0, 0, 0, 0, 0, 0, 0, false, false, false, 0, false, true);
    SPECIAL_RULE(PrimeBrothers, "Versatile Attack", RuleId::None, 1);

    AURA_RULE(PrimeBrothers, "Bane in Melee Aura", RuleId::Bane, 0, 0, 0, 0, 0, 0, 0, true);
    AURA_RULE(PrimeBrothers, "Bane when Shooting Aura", RuleId::Bane, 0, 0, 0, 0, 0, 0, 0, false, true);
    AURA_RULE(PrimeBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(PrimeBrothers, "Melee Shrouding Aura");
    AURA_RULE(PrimeBrothers, "Rapid Rush Aura");
    AURA_RULE(PrimeBrothers, "Regeneration Aura", RuleId::Regeneration);
    AURA_RULE(PrimeBrothers, "Versatile Reach Aura");
}

void register_ratmen_clans() {
    ARMY_RULE(RatmenClans, "Strength in Numbers");

    SPECIAL_RULE(RatmenClans, "Fast", RuleId::Fast);
    SPECIAL_RULE(RatmenClans, "Poison", RuleId::Poison);
    SPECIAL_RULE(RatmenClans, "Stealth");

    AURA_RULE(RatmenClans, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(RatmenClans, "Fast Aura", RuleId::Fast);
    AURA_RULE(RatmenClans, "Poison Aura", RuleId::Poison);
    AURA_RULE(RatmenClans, "Stealth Aura", RuleId::Stealth);
}

void register_rebel_guerrillas() {
    ARMY_RULE(RebelGuerrillas, "For Freedom");

    SPECIAL_RULE(RebelGuerrillas, "Fearless");
    SPECIAL_RULE(RebelGuerrillas, "Precise", RuleId::None, 1);
    SPECIAL_RULE(RebelGuerrillas, "Stealth");

    AURA_RULE(RebelGuerrillas, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(RebelGuerrillas, "Furious Aura", RuleId::Furious);
    AURA_RULE(RebelGuerrillas, "Stealth Aura", RuleId::Stealth);
}

void register_robot_legions() {
    ARMY_RULE(RobotLegions, "Living Metal");

    SPECIAL_RULE(RobotLegions, "Fearless");
    SPECIAL_RULE(RobotLegions, "Lethal Strike", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(RobotLegions, "Phase", RuleId::None, 0, 1);
    SPECIAL_RULE(RobotLegions, "Precise", RuleId::None, 1);
    SPECIAL_RULE(RobotLegions, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(RobotLegions, "Teleport");

    AURA_RULE(RobotLegions, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(RobotLegions, "Phase Aura");
    AURA_RULE(RobotLegions, "Reanimation Aura");
    AURA_RULE(RobotLegions, "Teleport Aura");
}

void register_saurian_starhost() {
    ARMY_RULE(SaurianStarhost, "Cold Blooded");

    SPECIAL_RULE(SaurianStarhost, "Fearless");
    SPECIAL_RULE(SaurianStarhost, "Poison", RuleId::Poison);
    SPECIAL_RULE(SaurianStarhost, "Precise", RuleId::None, 1);
    SPECIAL_RULE(SaurianStarhost, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(SaurianStarhost, "Spell Conduit");
    SPECIAL_RULE(SaurianStarhost, "Stealth");

    AURA_RULE(SaurianStarhost, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(SaurianStarhost, "Poison Aura", RuleId::Poison);
    AURA_RULE(SaurianStarhost, "Regeneration Aura", RuleId::Regeneration);
    AURA_RULE(SaurianStarhost, "Stealth Aura", RuleId::Stealth);
}

void register_soul_snatcher_cults() {
    ARMY_RULE(SoulSnatcherCults, "Soul Hunger");

    SPECIAL_RULE(SoulSnatcherCults, "Fast", RuleId::Fast);
    SPECIAL_RULE(SoulSnatcherCults, "Fearless");
    SPECIAL_RULE(SoulSnatcherCults, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(SoulSnatcherCults, "Stealth");

    AURA_RULE(SoulSnatcherCults, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(SoulSnatcherCults, "Fast Aura", RuleId::Fast);
    AURA_RULE(SoulSnatcherCults, "Fear Aura", RuleId::Fear);
    AURA_RULE(SoulSnatcherCults, "Regeneration Aura", RuleId::Regeneration);
}

void register_titan_lords() {
    ARMY_RULE(TitanLords, "Ancient Machines");

    SPECIAL_RULE(TitanLords, "Fearless");
    SPECIAL_RULE(TitanLords, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(TitanLords, "Shielded", RuleId::None, 0, 1);

    AURA_RULE(TitanLords, "Regeneration Aura", RuleId::Regeneration);
    AURA_RULE(TitanLords, "Shielded Aura", RuleId::Shielded);
}

void register_war_disciples() {
    ARMY_RULE(WarDisciples, "Wormhole Possessed");

    SPECIAL_RULE(WarDisciples, "Fearless");
    SPECIAL_RULE(WarDisciples, "Furious", RuleId::Furious);
    SPECIAL_RULE(WarDisciples, "Possessed Weapons");
    SPECIAL_RULE(WarDisciples, "Spell Conduit");

    AURA_RULE(WarDisciples, "Furious Aura", RuleId::Furious);
    AURA_RULE(WarDisciples, "Regeneration Aura", RuleId::Regeneration);
}

void register_watch_brothers() {
    ARMY_RULE(WatchBrothers, "Xeno Hunters");

    SPECIAL_RULE(WatchBrothers, "Fearless");
    SPECIAL_RULE(WatchBrothers, "Hatred", RuleId::None, 0, 0, 0, 0, 0, 0, 0, true);
    SPECIAL_RULE(WatchBrothers, "Precise", RuleId::None, 1);
    SPECIAL_RULE(WatchBrothers, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(WatchBrothers, "Versatile Attack", RuleId::None, 1);

    AURA_RULE(WatchBrothers, "Bane in Melee Aura", RuleId::Bane, 0, 0, 0, 0, 0, 0, 0, true);
    AURA_RULE(WatchBrothers, "Bane when Shooting Aura", RuleId::Bane, 0, 0, 0, 0, 0, 0, 0, false, true);
    AURA_RULE(WatchBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(WatchBrothers, "Regeneration Aura", RuleId::Regeneration);
}

void register_watch_prime_brothers() {
    ARMY_RULE(WatchPrimeBrothers, "Xeno Hunters");
    ARMY_RULE(WatchPrimeBrothers, "Reinforced");

    SPECIAL_RULE(WatchPrimeBrothers, "Demolish", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(WatchPrimeBrothers, "Fearless");
    SPECIAL_RULE(WatchPrimeBrothers, "Hatred", RuleId::None, 0, 0, 0, 0, 0, 0, 0, true);
    SPECIAL_RULE(WatchPrimeBrothers, "Melee Slayer", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(WatchPrimeBrothers, "Precise", RuleId::None, 1);
    SPECIAL_RULE(WatchPrimeBrothers, "Ravage");
    SPECIAL_RULE(WatchPrimeBrothers, "Shielded", RuleId::None, 0, 1);
    SPECIAL_RULE(WatchPrimeBrothers, "Versatile Attack", RuleId::None, 1);

    AURA_RULE(WatchPrimeBrothers, "Bane in Melee Aura", RuleId::Bane, 0, 0, 0, 0, 0, 0, 0, true);
    AURA_RULE(WatchPrimeBrothers, "Bane when Shooting Aura", RuleId::Bane, 0, 0, 0, 0, 0, 0, 0, false, true);
    AURA_RULE(WatchPrimeBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(WatchPrimeBrothers, "Regeneration Aura", RuleId::Regeneration);
}

void register_wolf_brothers() {
    ARMY_RULE(WolfBrothers, "Pack Hunters");

    SPECIAL_RULE(WolfBrothers, "Counter", RuleId::Counter);
    SPECIAL_RULE(WolfBrothers, "Fearless");
    SPECIAL_RULE(WolfBrothers, "Precise", RuleId::None, 1);
    SPECIAL_RULE(WolfBrothers, "Shielded", RuleId::None, 0, 1);

    AURA_RULE(WolfBrothers, "Counter Aura", RuleId::Counter);
    AURA_RULE(WolfBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(WolfBrothers, "Furious Aura", RuleId::Furious);
    AURA_RULE(WolfBrothers, "Regeneration Aura", RuleId::Regeneration);
}

void register_wolf_prime_brothers() {
    ARMY_RULE(WolfPrimeBrothers, "Pack Hunters");
    ARMY_RULE(WolfPrimeBrothers, "Reinforced");

    SPECIAL_RULE(WolfPrimeBrothers, "Counter", RuleId::Counter);
    SPECIAL_RULE(WolfPrimeBrothers, "Demolish", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(WolfPrimeBrothers, "Fearless");
    SPECIAL_RULE(WolfPrimeBrothers, "Melee Slayer", RuleId::None, 0, 0, 2);
    SPECIAL_RULE(WolfPrimeBrothers, "Precise", RuleId::None, 1);
    SPECIAL_RULE(WolfPrimeBrothers, "Ravage");
    SPECIAL_RULE(WolfPrimeBrothers, "Shielded", RuleId::None, 0, 1);

    AURA_RULE(WolfPrimeBrothers, "Counter Aura", RuleId::Counter);
    AURA_RULE(WolfPrimeBrothers, "Courage Aura", RuleId::None, 0, 0, 0, 1);
    AURA_RULE(WolfPrimeBrothers, "Furious Aura", RuleId::Furious);
    AURA_RULE(WolfPrimeBrothers, "Regeneration Aura", RuleId::Regeneration);
}

void register_wormhole_daemons_change() {
    ARMY_RULE(WormholeDaemonsChange, "Warp Beings");

    SPECIAL_RULE(WormholeDaemonsChange, "Blink");
    SPECIAL_RULE(WormholeDaemonsChange, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(WormholeDaemonsChange, "Spell Conduit");

    AURA_RULE(WormholeDaemonsChange, "Fear Aura", RuleId::Fear);
    AURA_RULE(WormholeDaemonsChange, "Regeneration Aura", RuleId::Regeneration);
    AURA_RULE(WormholeDaemonsChange, "Warp Bolt (1)", RuleId::None, 0, 0, 0, 0, 3, 0, 18);
    AURA_RULE(WormholeDaemonsChange, "Warp Fire (2)", RuleId::None, 0, 0, 0, 0, 6, 0, 12);
}

void register_wormhole_daemons_lust() {
    ARMY_RULE(WormholeDaemonsLust, "Warp Beings");

    SPECIAL_RULE(WormholeDaemonsLust, "Counter", RuleId::Counter);
    SPECIAL_RULE(WormholeDaemonsLust, "Fast", RuleId::Fast);
    SPECIAL_RULE(WormholeDaemonsLust, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(WormholeDaemonsLust, "Spell Conduit");

    AURA_RULE(WormholeDaemonsLust, "Fast Aura", RuleId::Fast);
    AURA_RULE(WormholeDaemonsLust, "Fear Aura", RuleId::Fear);
    AURA_RULE(WormholeDaemonsLust, "Regeneration Aura", RuleId::Regeneration);
}

void register_wormhole_daemons_plague() {
    ARMY_RULE(WormholeDaemonsPlague, "Warp Beings");

    SPECIAL_RULE(WormholeDaemonsPlague, "Poison", RuleId::Poison);
    SPECIAL_RULE(WormholeDaemonsPlague, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(WormholeDaemonsPlague, "Spell Conduit");

    AURA_RULE(WormholeDaemonsPlague, "Fear Aura", RuleId::Fear);
    AURA_RULE(WormholeDaemonsPlague, "Poison Aura", RuleId::Poison);
    AURA_RULE(WormholeDaemonsPlague, "Regeneration Aura", RuleId::Regeneration);
}

void register_wormhole_daemons_war() {
    ARMY_RULE(WormholeDaemonsWar, "Warp Beings");

    SPECIAL_RULE(WormholeDaemonsWar, "Fearless");
    SPECIAL_RULE(WormholeDaemonsWar, "Furious", RuleId::Furious);
    SPECIAL_RULE(WormholeDaemonsWar, "Regeneration", RuleId::Regeneration);
    SPECIAL_RULE(WormholeDaemonsWar, "Spell Conduit");

    AURA_RULE(WormholeDaemonsWar, "Fear Aura", RuleId::Fear);
    AURA_RULE(WormholeDaemonsWar, "Furious Aura", RuleId::Furious);
    AURA_RULE(WormholeDaemonsWar, "Regeneration Aura", RuleId::Regeneration);
}

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

bool faction_rules_initialized() {
    return g_initialized;
}

void register_all_faction_rules() {
    if (g_initialized) return;

    // Clear any existing data
    g_faction_rules.clear();
    g_rule_id_to_index.clear();
    g_faction_name_to_rule.clear();
    g_army_wide_counter.fill(0);
    g_special_counter.fill(0);
    g_aura_counter.fill(0);

    // Reserve space
    g_faction_rules.reserve(1200);

    // Register all factions
    register_alien_hives();
    register_battle_brothers();
    register_blessed_sisters();
    register_blood_brothers();
    register_blood_prime_brothers();
    register_change_disciples();
    register_custodian_brothers();
    register_dao_union();
    register_dark_brothers();
    register_dark_elf_raiders();
    register_dark_prime_brothers();
    register_dwarf_guilds();
    register_elven_jesters();
    register_eternal_dynasty();
    register_goblin_reclaimers();
    register_havoc_brothers();
    register_high_elf_fleets();
    register_human_defense_force();
    register_human_inquisition();
    register_infected_colonies();
    register_jackals();
    register_knight_brothers();
    register_knight_prime_brothers();
    register_lust_disciples();
    register_machine_cult();
    register_orc_marauders();
    register_plague_disciples();
    register_prime_brothers();
    register_ratmen_clans();
    register_rebel_guerrillas();
    register_robot_legions();
    register_saurian_starhost();
    register_soul_snatcher_cults();
    register_titan_lords();
    register_war_disciples();
    register_watch_brothers();
    register_watch_prime_brothers();
    register_wolf_brothers();
    register_wolf_prime_brothers();
    register_wormhole_daemons_change();
    register_wormhole_daemons_lust();
    register_wormhole_daemons_plague();
    register_wormhole_daemons_war();

    g_initialized = true;
}

} // namespace battle
