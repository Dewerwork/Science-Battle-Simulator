#pragma once

#include "core/types.hpp"
#include <string_view>
#include <array>

namespace battle {

// ==============================================================================
// Faction Rule IDs - Extended rule IDs for faction-specific rules
// ==============================================================================
// Organization:
// - Base rules (RuleId): 0-127
// - Faction rules: 128+ (organized by faction blocks of 32 IDs each)

constexpr u16 FACTION_RULE_BASE = 128;
constexpr u16 IDS_PER_FACTION = 32;

// ==============================================================================
// Faction Identifiers (43 factions)
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
    SaurianStarhost = 31,
    SoulSnatcherCults = 32,
    TitanLords = 33,
    WarDisciples = 34,
    WatchBrothers = 35,
    WatchPrimeBrothers = 36,
    WolfBrothers = 37,
    WolfPrimeBrothers = 38,
    WormholeDaemonsChange = 39,
    WormholeDaemonsLust = 40,
    WormholeDaemonsPlague = 41,
    WormholeDaemonsWar = 42,
    COUNT = 43
};

// Faction name lookup
inline const char* get_faction_name(FactionId id) {
    static const char* names[] = {
        "Alien Hives",
        "Battle Brothers",
        "Blessed Sisters",
        "Blood Brothers",
        "Blood Prime Brothers",
        "Change Disciples",
        "Custodian Brothers",
        "DAO Union",
        "Dark Brothers",
        "Dark Elf Raiders",
        "Dark Prime Brothers",
        "Dwarf Guilds",
        "Elven Jesters",
        "Eternal Dynasty",
        "Goblin Reclaimers",
        "Havoc Brothers",
        "High Elf Fleets",
        "Human Defense Force",
        "Human Inquisition",
        "Infected Colonies",
        "Jackals",
        "Knight Brothers",
        "Knight Prime Brothers",
        "Lust Disciples",
        "Machine Cult",
        "Orc Marauders",
        "Plague Disciples",
        "Prime Brothers",
        "Ratmen Clans",
        "Rebel Guerrillas",
        "Robot Legions",
        "Saurian Starhost",
        "Soul-Snatcher Cults",
        "Titan Lords",
        "War Disciples",
        "Watch Brothers",
        "Watch Prime Brothers",
        "Wolf Brothers",
        "Wolf Prime Brothers",
        "Wormhole Daemons of Change",
        "Wormhole Daemons of Lust",
        "Wormhole Daemons of Plague",
        "Wormhole Daemons of War"
    };
    if (static_cast<u8>(id) >= static_cast<u8>(FactionId::COUNT)) return "Unknown";
    return names[static_cast<u8>(id)];
}

// Find faction by name
inline FactionId find_faction_by_name(std::string_view name) {
    for (u8 i = 0; i < static_cast<u8>(FactionId::COUNT); ++i) {
        if (name == get_faction_name(static_cast<FactionId>(i))) {
            return static_cast<FactionId>(i);
        }
    }
    return FactionId::COUNT; // Not found
}

// ==============================================================================
// Rule Slot Offsets within faction block
// ==============================================================================

enum class FactionRuleSlot : u8 {
    ArmyWide = 0,   // 0-7: Army-wide rules (8 slots)
    Special = 8,    // 8-23: Special rules (16 slots)
    Aura = 24       // 24-31: Aura/spell rules (8 slots)
};

// ==============================================================================
// Rule ID Calculation
// ==============================================================================

constexpr u16 make_faction_rule_id(FactionId faction, FactionRuleSlot slot, u8 index) {
    return FACTION_RULE_BASE +
           (static_cast<u16>(faction) * IDS_PER_FACTION) +
           static_cast<u16>(slot) +
           index;
}

constexpr FactionId get_faction_from_rule_id(u16 rule_id) {
    if (rule_id < FACTION_RULE_BASE) return FactionId::COUNT;
    return static_cast<FactionId>((rule_id - FACTION_RULE_BASE) / IDS_PER_FACTION);
}

constexpr bool is_faction_rule(u16 rule_id) {
    return rule_id >= FACTION_RULE_BASE;
}

// ==============================================================================
// Faction Rule Info - Describes a faction-specific rule
// ==============================================================================

struct FactionRuleInfo {
    u16 rule_id;
    const char* name;
    FactionId faction;
    RuleId grants_base_rule;    // Base rule granted (or RuleId::None)
    i8 hit_modifier;            // +/- to hit
    i8 defense_modifier;        // +/- to defense
    i8 ap_modifier;             // +/- to AP
    i8 morale_modifier;         // +/- to morale
    u8 deals_hits;              // Hits dealt to enemies
    u8 deals_hits_ap;           // AP for dealt hits
    u8 deals_hits_range;        // Range for dealt hits
    bool melee_only;
    bool shooting_only;
    bool is_aura;
    u8 aura_range;
    bool once_per_game;
    bool once_per_activation;
};

// ==============================================================================
// Faction Rule Definitions - All rules for all factions
// ==============================================================================
// This array contains all faction-specific rules, indexed by faction rule ID offset

// Maximum number of faction rules (43 factions * 32 IDs each)
constexpr size_t MAX_FACTION_RULES = static_cast<size_t>(FactionId::COUNT) * IDS_PER_FACTION;

// Get faction rule info by rule ID
const FactionRuleInfo* get_faction_rule_info(u16 rule_id);

// Find faction rule by name
u16 find_faction_rule_by_name(FactionId faction, std::string_view name);

// Register all faction rules
void register_all_faction_rules();

// Check if faction rules are initialized
bool faction_rules_initialized();

// ==============================================================================
// Common Faction Rule Names (used across multiple factions)
// ==============================================================================
// These are rule names that appear in multiple factions with similar effects

namespace common_rules {
    constexpr const char* Mend = "Mend";
    constexpr const char* Shielded = "Shielded";
    constexpr const char* ReDeployment = "Re-Deployment";
    constexpr const char* RePositionArtillery = "Re-Position Artillery";
    constexpr const char* VersatileAttack = "Versatile Attack";
    constexpr const char* CourageAura = "Courage Aura";
    constexpr const char* RegenerationAura = "Regeneration Aura";
    constexpr const char* BaneInMeleeAura = "Bane in Melee Aura";
    constexpr const char* BaneWhenShootingAura = "Bane when Shooting Aura";
    constexpr const char* MeleeShroudingAura = "Melee Shrouding Aura";
    constexpr const char* RapidRushAura = "Rapid Rush Aura";
    constexpr const char* VersatileReachAura = "Versatile Reach Aura";
    constexpr const char* SpellConduit = "Spell Conduit";
    constexpr const char* Precise = "Precise";
    constexpr const char* Smash = "Smash";
}

} // namespace battle
