#pragma once

#include "core/types.hpp"
#include <string_view>

namespace battle {

// ==============================================================================
// Faction Rule IDs - Extended rule IDs for faction-specific rules
// ==============================================================================
// These rules extend beyond the base RuleId enum to include all faction-specific
// rules. They are organized by faction and mapped to effects in the rule registry.
//
// Organization:
// - Base rules (RuleId): 0-127 (includes PRIMARY and some EXTENDED)
// - Faction rules: 128-319 (organized by faction blocks)
//
// Each faction gets a block of 4 IDs per rule type to allow for growth.

// Starting offset for faction rules (after base rules)
constexpr u16 FACTION_RULE_BASE = 128;

// IDs per faction (allows room for growth)
constexpr u16 IDS_PER_FACTION = 32;

// ==============================================================================
// Faction Identifiers
// ==============================================================================

enum class FactionId : u8 {
    AlienHives = 0,
    BattleBrothers = 1,
    BlessedSisters = 2,
    BloodBrothers = 3,
    BloodPrimeBrothers = 4,
    ChangeDisciples = 5,
    CustodianBrothers = 6,
    DAOUnion = 7,
    DarkBrothers = 8,
    DarkElfRaiders = 9,
    DarkPrimeBrothers = 10,
    DwarfGuilds = 11,
    ElvenJesters = 12,
    EternalDynasty = 13,
    GoblinReclaimers = 14,
    HavocBrothers = 15,
    HighElfFleets = 16,
    HumanDefenseForce = 17,
    HumanInquisition = 18,
    InfectedColonies = 19,
    Jackals = 20,
    KnightBrothers = 21,
    KnightPrimeBrothers = 22,
    LustDisciples = 23,
    MachineCult = 24,
    OrcMarauders = 25,
    PlagueDisciples = 26,
    PrimeBrothers = 27,
    RatmenClans = 28,
    RebelGuerrillas = 29,
    RobotLegions = 30,
    SoulSnatcher = 31,
    VoidBrothers = 32,
    WarBrothers = 33,
    WarClanOrks = 34,
    WatchBrothers = 35,
    WolfBrothers = 36,
    WoodElfHighborn = 37,
    WorldEaters = 38,
    COUNT
};

// Get faction name from ID
inline const char* get_faction_name(FactionId id) {
    switch (id) {
        case FactionId::AlienHives: return "Alien Hives";
        case FactionId::BattleBrothers: return "Battle Brothers";
        case FactionId::BlessedSisters: return "Blessed Sisters";
        case FactionId::BloodBrothers: return "Blood Brothers";
        case FactionId::BloodPrimeBrothers: return "Blood Prime Brothers";
        case FactionId::ChangeDisciples: return "Change Disciples";
        case FactionId::CustodianBrothers: return "Custodian Brothers";
        case FactionId::DAOUnion: return "DAO Union";
        case FactionId::DarkBrothers: return "Dark Brothers";
        case FactionId::DarkElfRaiders: return "Dark Elf Raiders";
        case FactionId::DarkPrimeBrothers: return "Dark Prime Brothers";
        case FactionId::DwarfGuilds: return "Dwarf Guilds";
        case FactionId::ElvenJesters: return "Elven Jesters";
        case FactionId::EternalDynasty: return "Eternal Dynasty";
        case FactionId::GoblinReclaimers: return "Goblin Reclaimers";
        case FactionId::HavocBrothers: return "Havoc Brothers";
        case FactionId::HighElfFleets: return "High Elf Fleets";
        case FactionId::HumanDefenseForce: return "Human Defense Force";
        case FactionId::HumanInquisition: return "Human Inquisition";
        case FactionId::InfectedColonies: return "Infected Colonies";
        case FactionId::Jackals: return "Jackals";
        case FactionId::KnightBrothers: return "Knight Brothers";
        case FactionId::KnightPrimeBrothers: return "Knight Prime Brothers";
        case FactionId::LustDisciples: return "Lust Disciples";
        case FactionId::MachineCult: return "Machine Cult";
        case FactionId::OrcMarauders: return "Orc Marauders";
        case FactionId::PlagueDisciples: return "Plague Disciples";
        case FactionId::PrimeBrothers: return "Prime Brothers";
        case FactionId::RatmenClans: return "Ratmen Clans";
        case FactionId::RebelGuerrillas: return "Rebel Guerrillas";
        case FactionId::RobotLegions: return "Robot Legions";
        case FactionId::SoulSnatcher: return "Soul Snatcher";
        case FactionId::VoidBrothers: return "Void Brothers";
        case FactionId::WarBrothers: return "War Brothers";
        case FactionId::WarClanOrks: return "War Clan Orks";
        case FactionId::WatchBrothers: return "Watch Brothers";
        case FactionId::WolfBrothers: return "Wolf Brothers";
        case FactionId::WoodElfHighborn: return "Wood Elf Highborn";
        case FactionId::WorldEaters: return "World Eaters";
        default: return "Unknown";
    }
}

// ==============================================================================
// Faction Rule Type within a faction
// ==============================================================================

enum class FactionRuleType : u8 {
    ArmyWide = 0,   // 0-7: Army-wide rules
    Special = 8,    // 8-23: Special rules
    Aura = 24       // 24-31: Aura/spell rules
};

// ==============================================================================
// Calculate rule ID from faction and local rule index
// ==============================================================================

constexpr u16 make_faction_rule_id(FactionId faction, FactionRuleType type, u8 local_index) {
    return FACTION_RULE_BASE +
           (static_cast<u16>(faction) * IDS_PER_FACTION) +
           static_cast<u16>(type) +
           local_index;
}

// Extract faction from rule ID
constexpr FactionId get_faction_from_rule_id(u16 rule_id) {
    if (rule_id < FACTION_RULE_BASE) return static_cast<FactionId>(255);
    return static_cast<FactionId>((rule_id - FACTION_RULE_BASE) / IDS_PER_FACTION);
}

// ==============================================================================
// Alien Hives Faction Rules
// ==============================================================================

namespace alien_hives {
    // Army-wide rules
    constexpr u16 HiveBond = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::ArmyWide, 0);

    // Special rules
    constexpr u16 BreathAttack = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 0);
    constexpr u16 CasterGroup = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 1);
    constexpr u16 IncreasedShootingRange = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 2);
    constexpr u16 Infiltrate = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 3);
    constexpr u16 PiercingGrowth = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 4);
    constexpr u16 Ravage = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 5);
    constexpr u16 RegenerativeStrength = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 6);
    constexpr u16 Spawn = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 7);
    constexpr u16 SpellConduit = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 8);
    constexpr u16 StealthBuff = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 9);
    constexpr u16 Strafing = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 10);
    constexpr u16 SurpriseAttack = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 11);
    constexpr u16 TakedownStrike = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 12);
    constexpr u16 UnpredictableFighterMark = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Special, 13);

    // Aura/spell rules
    constexpr u16 FuriousAura = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Aura, 0);
    constexpr u16 HiveBondBoostAura = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Aura, 1);
    constexpr u16 IncreasedShootingRangeAura = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Aura, 2);
    constexpr u16 RapidChargeAura = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Aura, 3);
    constexpr u16 ShieldedAura = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Aura, 4);
    constexpr u16 AnimateSpirit = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Aura, 5);
    constexpr u16 OverwhelmingStrike = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Aura, 6);
    constexpr u16 InfuseBloodthirst = make_faction_rule_id(FactionId::AlienHives, FactionRuleType::Aura, 7);
}

// ==============================================================================
// Battle Brothers Faction Rules
// ==============================================================================

namespace battle_brothers {
    // Army-wide rules
    // Note: Battleborn is already a base rule (RuleId::Battleborn)

    // Special rules
    constexpr u16 Mend = make_faction_rule_id(FactionId::BattleBrothers, FactionRuleType::Special, 0);
    constexpr u16 ReDeployment = make_faction_rule_id(FactionId::BattleBrothers, FactionRuleType::Special, 1);
    constexpr u16 RePositionArtillery = make_faction_rule_id(FactionId::BattleBrothers, FactionRuleType::Special, 2);
    constexpr u16 UnstoppableShootingMark = make_faction_rule_id(FactionId::BattleBrothers, FactionRuleType::Special, 3);

    // Aura/spell rules
    constexpr u16 BaneInMeleeAura = make_faction_rule_id(FactionId::BattleBrothers, FactionRuleType::Aura, 0);
    constexpr u16 BaneWhenShootingAura = make_faction_rule_id(FactionId::BattleBrothers, FactionRuleType::Aura, 1);
    constexpr u16 CourageAura = make_faction_rule_id(FactionId::BattleBrothers, FactionRuleType::Aura, 2);
    constexpr u16 MeleeShroudingAura = make_faction_rule_id(FactionId::BattleBrothers, FactionRuleType::Aura, 3);
    constexpr u16 RapidRushAura = make_faction_rule_id(FactionId::BattleBrothers, FactionRuleType::Aura, 4);
    constexpr u16 RegenerationAura = make_faction_rule_id(FactionId::BattleBrothers, FactionRuleType::Aura, 5);
    constexpr u16 VersatileReachAura = make_faction_rule_id(FactionId::BattleBrothers, FactionRuleType::Aura, 6);
    constexpr u16 AdvancedSight = make_faction_rule_id(FactionId::BattleBrothers, FactionRuleType::Aura, 7);
}

// ==============================================================================
// Rule Name Mapping - For parsing and display
// ==============================================================================

struct FactionRuleInfo {
    u16 rule_id;
    const char* name;
    FactionId faction;
    RuleId grants_base_rule;    // If this rule grants a base rule
    i8 hit_modifier;            // +/- to hit
    i8 defense_modifier;        // +/- to defense
    i8 ap_modifier;             // +/- to AP
    i8 morale_modifier;         // +/- to morale
    bool melee_only;
    bool shooting_only;
    bool is_aura;
    u8 aura_range;
};

// Get rule info by faction rule ID
// Returns nullptr if not found
const FactionRuleInfo* get_faction_rule_info(u16 rule_id);

// Find a faction rule by name within a faction
// Returns 0 if not found
u16 find_faction_rule_by_name(FactionId faction, std::string_view name);

// Register all faction rules with the main rule registry
void register_all_faction_rules();

} // namespace battle
