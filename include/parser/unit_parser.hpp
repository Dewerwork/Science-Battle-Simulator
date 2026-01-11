#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "core/weapon.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <fstream>
#include <sstream>
#include <regex>
#include <unordered_map>
#include <algorithm>
#include <cctype>

namespace battle {

// ==============================================================================
// UnitParser - Parses unit text files into Unit structures
// ==============================================================================
//
// Input format:
//   UnitName [model_count] QX+ DX+ | Xpts | Rule1, Rule2, Rule3(value), ...
//   WeaponList (comma-separated)
//
// Example:
//   Assault Walker [1] Q4+ D2+ | 350pts | Devout, Fear(2), Fearless, Tough(9)
//   Stomp (A3, AP(1)), Heavy Claw (A4, AP(1), Rending), Heavy Fist (A4, AP(4))
//

class UnitParser {
public:
    struct ParseResult {
        std::vector<Unit> units;
        std::vector<std::string> errors;
        size_t lines_processed = 0;
        size_t units_parsed = 0;
    };

    // Parse a file containing multiple units (auto-detects JSON or text format)
    static ParseResult parse_file(const std::string& filepath, std::string_view faction_name = "");

    // Parse a string containing multiple units (text format)
    static ParseResult parse_string(const std::string& content, std::string_view faction_name = "");

    // Parse a JSON string containing units
    static ParseResult parse_json_string(const std::string& content, std::string_view faction_name = "");

    // Parse a single unit from two lines (header + weapons)
    static std::optional<Unit> parse_unit(std::string_view header_line,
                                          std::string_view weapons_line,
                                          std::string_view faction_name = "");

private:
    // Check if content looks like JSON
    static bool is_json_content(const std::string& content);

    // Simple JSON value extraction helpers
    static std::string json_get_string(const std::string& json, const std::string& key);
    static int json_get_int(const std::string& json, const std::string& key, int default_val = 0);
    static std::vector<std::string> json_get_string_array(const std::string& json, const std::string& key);
    static std::vector<std::string> json_get_object_array(const std::string& json, const std::string& key);
    static std::string find_json_array(const std::string& json, const std::string& key);

    // Parse a single unit from JSON object string
    static std::optional<Unit> parse_unit_from_json(const std::string& json_obj, std::string_view faction_name);

    // Parse a weapon from JSON object string
    static std::optional<Weapon> parse_weapon_from_json(const std::string& json_obj);


    // Parse the header line: "UnitName [count] QX+ DX+ | Xpts | Rules..."
    static bool parse_header(std::string_view line, Unit& unit);

    // Parse the weapons line: "Weapon1 (stats), Weapon2 (stats), ..."
    static bool parse_weapons(std::string_view line, Unit& unit);

    // Parse a single weapon: "WeaponName (A3, AP(1), Rending)"
    static std::optional<Weapon> parse_weapon(std::string_view weapon_str);

    // Parse special rules from comma-separated string
    static void parse_rules(std::string_view rules_str, Unit& unit);

    // Parse a single rule: "RuleName" or "RuleName(X)"
    static std::optional<CompactRule> parse_rule(std::string_view rule_str);

    // Helper: trim whitespace
    static std::string_view trim(std::string_view sv);

    // Helper: split by delimiter (respecting parentheses)
    static std::vector<std::string_view> split_respecting_parens(std::string_view sv, char delim);

    // Rule name to RuleId mapping
    static const std::unordered_map<std::string, RuleId>& get_rule_map();
};

// ==============================================================================
// Implementation
// ==============================================================================

inline std::string_view UnitParser::trim(std::string_view sv) {
    while (!sv.empty() && std::isspace(sv.front())) sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(sv.back())) sv.remove_suffix(1);
    return sv;
}

inline std::vector<std::string_view> UnitParser::split_respecting_parens(std::string_view sv, char delim) {
    std::vector<std::string_view> result;
    size_t start = 0;
    int paren_depth = 0;

    for (size_t i = 0; i < sv.size(); ++i) {
        char c = sv[i];
        if (c == '(') paren_depth++;
        else if (c == ')') paren_depth = std::max(0, paren_depth - 1);
        else if (c == delim && paren_depth == 0) {
            auto part = trim(sv.substr(start, i - start));
            if (!part.empty()) result.push_back(part);
            start = i + 1;
        }
    }

    // Last part
    if (start < sv.size()) {
        auto part = trim(sv.substr(start));
        if (!part.empty()) result.push_back(part);
    }

    return result;
}

inline const std::unordered_map<std::string, RuleId>& UnitParser::get_rule_map() {
    static const std::unordered_map<std::string, RuleId> map = {
        // Weapon rules
        {"ap", RuleId::AP},
        {"blast", RuleId::Blast},
        {"deadly", RuleId::Deadly},
        {"lance", RuleId::Lance},
        {"poison", RuleId::Poison},
        {"precise", RuleId::Precise},
        {"reliable", RuleId::Reliable},
        {"rending", RuleId::Rending},
        {"bane", RuleId::Bane},
        {"impact", RuleId::Impact},
        {"indirect", RuleId::Indirect},
        {"sniper", RuleId::Sniper},
        {"lock-on", RuleId::Lock_On},
        {"purge", RuleId::Purge},

        // Defense rules
        {"regeneration", RuleId::Regeneration},
        {"tough", RuleId::Tough},
        {"protected", RuleId::Protected},
        {"stealth", RuleId::Stealth},
        {"shield wall", RuleId::ShieldWall},
        {"shieldwall", RuleId::ShieldWall},

        // Unit rules
        {"fearless", RuleId::Fearless},
        {"furious", RuleId::Furious},
        {"hero", RuleId::Hero},
        {"relentless", RuleId::Relentless},
        {"fear", RuleId::Fear},
        {"counter", RuleId::Counter},
        {"fast", RuleId::Fast},
        {"flying", RuleId::Flying},
        {"strider", RuleId::Strider},
        {"scout", RuleId::Scout},
        {"ambush", RuleId::Ambush},
        {"devout", RuleId::Devout},
        {"piercing assault", RuleId::PiercingAssault},
        {"piercingassault", RuleId::PiercingAssault},
        {"unstoppable", RuleId::Unstoppable},
        {"casting", RuleId::Casting},
        {"slow", RuleId::Slow},
        {"surge", RuleId::Surge},
        {"thrust", RuleId::Thrust},
        {"takedown", RuleId::Takedown},
        {"limited", RuleId::Limited},
        {"immobile", RuleId::Immobile},

        // Faction-specific rules
        {"shielded", RuleId::Shielded},
        {"resistance", RuleId::Resistance},
        {"no retreat", RuleId::NoRetreat},
        {"noretreat", RuleId::NoRetreat},
        {"morale boost", RuleId::MoraleBoost},
        {"moraleboost", RuleId::MoraleBoost},
        {"hive bond", RuleId::MoraleBoost},  // Alias for Hive Bond
        {"hivebond", RuleId::MoraleBoost},
        {"rupture", RuleId::Rupture},
        {"agile", RuleId::Agile},
        {"hit & run", RuleId::HitAndRun},
        {"hit and run", RuleId::HitAndRun},
        {"hitandrun", RuleId::HitAndRun},
        {"point-blank surge", RuleId::PointBlankSurge},
        {"pointblanksurge", RuleId::PointBlankSurge},
        {"shred", RuleId::Shred},
        {"smash", RuleId::Smash},
        {"battleborn", RuleId::Battleborn},
        {"predator fighter", RuleId::PredatorFighter},
        {"predatorfighter", RuleId::PredatorFighter},
        {"rapid charge", RuleId::RapidCharge},
        {"rapidcharge", RuleId::RapidCharge},
        {"self-destruct", RuleId::SelfDestruct},
        {"selfdestruct", RuleId::SelfDestruct},
        {"versatile attack", RuleId::VersatileAttack},
        {"versatileattack", RuleId::VersatileAttack},
        {"good shot", RuleId::GoodShot},
        {"goodshot", RuleId::GoodShot},
        {"bad shot", RuleId::BadShot},
        {"badshot", RuleId::BadShot},
        {"melee evasion", RuleId::MeleeEvasion},
        {"meleeevasion", RuleId::MeleeEvasion},
        {"melee shrouding", RuleId::MeleeShrouding},
        {"meleeshrouding", RuleId::MeleeShrouding},
        {"ranged shrouding", RuleId::RangedShrouding},
        {"rangedshrouding", RuleId::RangedShrouding},
        {"bane in melee", RuleId::BaneInMelee},
        {"baneinmelee", RuleId::BaneInMelee},
        {"hold the line", RuleId::HoldTheLine},
        {"holdtheline", RuleId::HoldTheLine},

        // === Category A: Simple Modifiers ===
        {"evasive", RuleId::Evasive},
        {"steadfast", RuleId::Steadfast},
        {"swift", RuleId::Swift},
        {"ferocious", RuleId::Ferocious},
        {"lacerate", RuleId::Lacerate},
        {"mischievous", RuleId::Mischievous},
        {"scrapper", RuleId::Scrapper},

        // === Category B: Weapon Conditionals ===
        {"bash", RuleId::Bash},
        {"thrash", RuleId::Thrash},
        {"crack", RuleId::Crack},
        {"destructive", RuleId::Destructive},
        {"fracture", RuleId::Fracture},
        {"break", RuleId::Break},
        {"slash", RuleId::Slash},
        {"butcher", RuleId::Butcher},
        {"slam", RuleId::Slam},
        {"quake", RuleId::Quake},
        {"tear", RuleId::Tear},
        {"scratch", RuleId::Scratch},
        {"puncture", RuleId::Puncture},
        {"shatter", RuleId::Shatter},
        {"demolish", RuleId::Demolish},
        {"impale", RuleId::Impale},
        {"skewer", RuleId::Skewer},
        {"reap", RuleId::Reap},
        {"disintegrate", RuleId::Disintegrate},
        {"decimate", RuleId::Decimate},
        {"fragment", RuleId::Fragment},
        {"wreck", RuleId::Wreck},

        // === Category C: Movement Bonuses ===
        {"lustbound", RuleId::Lustbound},
        {"highborn", RuleId::Highborn},
        {"scurry", RuleId::Scurry},
        {"darkborn", RuleId::Darkborn},
        {"hit & run shooter", RuleId::HitAndRunShooter},
        {"hit and run shooter", RuleId::HitAndRunShooter},
        {"hitandrunshooter", RuleId::HitAndRunShooter},

        // Courage Aura maps to MoraleBoost
        {"courage", RuleId::MoraleBoost},
        {"courage aura", RuleId::MoraleBoost},
        {"courageaura", RuleId::MoraleBoost},

        // Type 1 Auras - Map to their underlying rules
        // These auras simply grant another rule, so we treat them as the underlying rule
        {"furious aura", RuleId::Furious},
        {"furiousaura", RuleId::Furious},
        {"shielded aura", RuleId::Shielded},
        {"shieldedaura", RuleId::Shielded},
        {"regeneration aura", RuleId::Regeneration},
        {"regenerationaura", RuleId::Regeneration},
        {"relentless aura", RuleId::Relentless},
        {"relentlessaura", RuleId::Relentless},
        {"scout aura", RuleId::Scout},
        {"scoutaura", RuleId::Scout},
        {"stealth aura", RuleId::Stealth},
        {"stealthaura", RuleId::Stealth},
        {"counter-attack aura", RuleId::Counter},
        {"counterattack aura", RuleId::Counter},
        {"counter attack aura", RuleId::Counter},
        {"fearless aura", RuleId::Fearless},
        {"fearlessaura", RuleId::Fearless},
        {"ambush aura", RuleId::Ambush},
        {"ambushaura", RuleId::Ambush},
        {"strider aura", RuleId::Strider},
        {"strideraura", RuleId::Strider},

        // Bane auras (melee and shooting variants)
        {"bane in melee aura", RuleId::Bane},
        {"baneinmeleeaura", RuleId::Bane},
        {"bane when shooting aura", RuleId::Bane},
        {"banewhenshootingaura", RuleId::Bane},

        // Rending auras (melee and shooting variants)
        {"rending in melee aura", RuleId::Rending},
        {"rendinginmeleeaura", RuleId::Rending},
        {"rending when shooting aura", RuleId::Rending},
        {"rendingwhenshootingaura", RuleId::Rending},

        // Shred auras (melee and shooting variants)
        {"shred in melee aura", RuleId::Shred},
        {"shredinmeleeaura", RuleId::Shred},
        {"shred when shooting aura", RuleId::Shred},
        {"shredwhenshootingaura", RuleId::Shred},

        // Defense auras
        {"no retreat aura", RuleId::NoRetreat},
        {"noretreat aura", RuleId::NoRetreat},
        {"noretreataua", RuleId::NoRetreat},
        {"resistance aura", RuleId::Resistance},
        {"resistanceaura", RuleId::Resistance},
        {"melee evasion aura", RuleId::MeleeEvasion},
        {"meleeevasionaura", RuleId::MeleeEvasion},
        {"melee shrouding aura", RuleId::MeleeShrouding},
        {"meleeshroudingaura", RuleId::MeleeShrouding},
        {"ranged shrouding aura", RuleId::RangedShrouding},
        {"rangedshroudingaura", RuleId::RangedShrouding},

        // Combat auras
        {"piercing assault aura", RuleId::PiercingAssault},
        {"piercingassaultaura", RuleId::PiercingAssault},
        {"hit & run fighter aura", RuleId::HitAndRun},
        {"hit and run fighter aura", RuleId::HitAndRun},
        {"hitandrunfighteraura", RuleId::HitAndRun},
        {"thrust in melee aura", RuleId::Thrust},
        {"thrustinmeleeaura", RuleId::Thrust},
        {"unstoppable in melee aura", RuleId::Unstoppable},
        {"unstoppableinmeleeaura", RuleId::Unstoppable},
        {"unstoppable when shooting aura", RuleId::Unstoppable},
        {"unstoppablewhenshootingaura", RuleId::Unstoppable},

        // Movement auras
        {"rapid charge aura", RuleId::RapidCharge},
        {"rapidchargeaura", RuleId::RapidCharge},
        {"fast aura", RuleId::Fast},
        {"fastaura", RuleId::Fast},

        // Cover/Indirect auras
        {"ignores cover aura", RuleId::Indirect},
        {"ignorescoveraura", RuleId::Indirect},
        {"ignores cover when shooting aura", RuleId::Indirect},
        {"ignorescoverwhenshootingaura", RuleId::Indirect},
        {"indirect when shooting aura", RuleId::Indirect},
        {"indirectwhenshootingaura", RuleId::Indirect},

        // Precision auras (map to hit modifier rules)
        {"precision shooter aura", RuleId::GoodShot},
        {"precisionshooteraura", RuleId::GoodShot},

        // Buff variants (same effect as auras)
        {"stealth buff", RuleId::Stealth},
        {"stealthbuff", RuleId::Stealth},
        {"regeneration buff", RuleId::Regeneration},
        {"regenerationbuff", RuleId::Regeneration},
        {"bane in melee buff", RuleId::Bane},
        {"baneinmeleebuff", RuleId::Bane},

        // Mark variants (same effect as auras)
        {"relentless mark", RuleId::Relentless},
        {"relentlessmark", RuleId::Relentless},
        {"rending in melee mark", RuleId::Rending},
        {"rendinginmeleemark", RuleId::Rending},
        {"furious mark", RuleId::Furious},
        {"furiousmark", RuleId::Furious},
        {"bane mark", RuleId::Bane},
        {"banemark", RuleId::Bane},

        // === Category A: Simple Modifier Auras ===
        {"evasive aura", RuleId::Evasive},
        {"evasiveaura", RuleId::Evasive},
        {"steadfast aura", RuleId::Steadfast},
        {"steadfastaura", RuleId::Steadfast},
        {"ferocious aura", RuleId::Ferocious},
        {"ferociousaura", RuleId::Ferocious},
        {"lacerate aura", RuleId::Lacerate},
        {"lacerateaura", RuleId::Lacerate},
        {"mischievous aura", RuleId::Mischievous},
        {"mischievousaura", RuleId::Mischievous},
        {"scrapper aura", RuleId::Scrapper},
        {"scrapperaura", RuleId::Scrapper},

        // === Category C: Movement Bonus Auras ===
        {"scurry aura", RuleId::Scurry},
        {"scurryaura", RuleId::Scurry},
        {"darkborn aura", RuleId::Darkborn},
        {"darkbornaura", RuleId::Darkborn},
        {"lustbound aura", RuleId::Lustbound},
        {"lustboundaura", RuleId::Lustbound},
        {"highborn aura", RuleId::Highborn},
        {"highbornaura", RuleId::Highborn},
        {"hit & run shooter aura", RuleId::HitAndRunShooter},
        {"hit and run shooter aura", RuleId::HitAndRunShooter},
        {"hitandrunshooteraura", RuleId::HitAndRunShooter},

        // === Category D: Defense Modifiers ===
        {"fortified", RuleId::Fortified},
        {"guardian", RuleId::Guardian},
        {"guardian boost", RuleId::GuardianBoost},
        {"guardianboost", RuleId::GuardianBoost},
        {"sturdy", RuleId::Sturdy},
        {"knightborn", RuleId::Knightborn},
        {"plaguebound", RuleId::Plaguebound},
        {"changebound", RuleId::Changebound},

        // === Category D: Defense Modifier Auras ===
        {"fortified aura", RuleId::Fortified},
        {"fortifiedaura", RuleId::Fortified},
        {"guardian aura", RuleId::Guardian},
        {"guardianaura", RuleId::Guardian},
        {"guardian boost aura", RuleId::GuardianBoost},
        {"guardianboostaura", RuleId::GuardianBoost},
        {"sturdy aura", RuleId::Sturdy},
        {"sturdyaura", RuleId::Sturdy},
        {"sturdy boost aura", RuleId::Sturdy},
        {"sturdyboostaura", RuleId::Sturdy},
        {"changebound aura", RuleId::Changebound},
        {"changeboundaura", RuleId::Changebound},
        {"changebound boost aura", RuleId::Changebound},
        {"changeboundboostaura", RuleId::Changebound},
        {"machine-fog", RuleId::Changebound},
        {"machinefog", RuleId::Changebound},
        {"machine-fog aura", RuleId::Changebound},
        {"machinefogaura", RuleId::Changebound},

        // === Category E: Extra Attack Generation & Distance-based Combat ===
        {"bloodborn", RuleId::Bloodborn},
        {"clan warrior", RuleId::Bloodborn},
        {"clanwarrior", RuleId::Bloodborn},
        {"primal", RuleId::Bloodborn},
        {"targeting visor", RuleId::TargetingVisor},
        {"targetingvisor", RuleId::TargetingVisor},
        {"havocbound", RuleId::Havocbound},
        {"warbound", RuleId::Warbound},
        {"infected", RuleId::Warbound},
        {"brutal fighter", RuleId::BrutalFighter},
        {"brutalfighter", RuleId::BrutalFighter},

        // === Category E: Aura Mappings ===
        {"bloodborn aura", RuleId::Bloodborn},
        {"bloodbornaura", RuleId::Bloodborn},
        {"clan warrior aura", RuleId::Bloodborn},
        {"clanwarrioraura", RuleId::Bloodborn},
        {"clan warrior boost aura", RuleId::Bloodborn},
        {"clanwarriorboostaura", RuleId::Bloodborn},
        {"primal aura", RuleId::Bloodborn},
        {"primalaura", RuleId::Bloodborn},
        {"targeting visor aura", RuleId::TargetingVisor},
        {"targetingvisoraura", RuleId::TargetingVisor},
        {"targeting visor boost aura", RuleId::TargetingVisor},
        {"targetingvisorboostaura", RuleId::TargetingVisor},
        {"havocbound aura", RuleId::Havocbound},
        {"havocboundaura", RuleId::Havocbound},
        {"havocbound boost aura", RuleId::Havocbound},
        {"havocboundboostaura", RuleId::Havocbound},
        {"warbound aura", RuleId::Warbound},
        {"warboundaura", RuleId::Warbound},
        {"warbound boost aura", RuleId::Warbound},
        {"warboundboostaura", RuleId::Warbound},
        {"infected aura", RuleId::Warbound},
        {"infectedaura", RuleId::Warbound},
        {"infected boost aura", RuleId::Warbound},
        {"infectedboostaura", RuleId::Warbound},
        {"brutal fighter aura", RuleId::BrutalFighter},
        {"brutalfighteraura", RuleId::BrutalFighter},
    };
    return map;
}

inline std::optional<CompactRule> UnitParser::parse_rule(std::string_view rule_str) {
    auto trimmed = trim(rule_str);
    if (trimmed.empty()) return std::nullopt;

    std::string rule_name;
    u8 value = 0;

    // Check for value in parentheses: "RuleName(X)"
    auto paren_pos = trimmed.find('(');
    if (paren_pos != std::string_view::npos) {
        rule_name = std::string(trimmed.substr(0, paren_pos));
        auto close_pos = trimmed.find(')', paren_pos);
        if (close_pos != std::string_view::npos) {
            auto value_str = trimmed.substr(paren_pos + 1, close_pos - paren_pos - 1);
            try {
                value = static_cast<u8>(std::stoi(std::string(value_str)));
            } catch (...) {
                value = 0;
            }
        }
    } else {
        rule_name = std::string(trimmed);
    }

    // Normalize to lowercase
    std::transform(rule_name.begin(), rule_name.end(), rule_name.begin(), ::tolower);
    // Trim trailing spaces
    while (!rule_name.empty() && std::isspace(rule_name.back())) {
        rule_name.pop_back();
    }

    const auto& rule_map = get_rule_map();
    auto it = rule_map.find(rule_name);
    if (it != rule_map.end()) {
        return CompactRule(it->second, value);
    }

    return std::nullopt; // Unknown rule
}

inline void UnitParser::parse_rules(std::string_view rules_str, Unit& unit) {
    auto rules = split_respecting_parens(rules_str, ',');
    for (auto rule_sv : rules) {
        if (auto rule = parse_rule(rule_sv)) {
            unit.add_rule(rule->id, rule->value);

            // Handle special cases
            if (rule->id == RuleId::Tough) {
                // Set tough value on all models
                for (u8 i = 0; i < unit.model_count; ++i) {
                    unit.models[i].tough = rule->value;
                }
            } else if (rule->id == RuleId::Hero) {
                // Mark first model as hero (for single-model units)
                if (unit.model_count > 0) {
                    unit.models[0].is_hero = true;
                }
            }
        }
    }
}

inline std::optional<Weapon> UnitParser::parse_weapon(std::string_view weapon_str) {
    auto trimmed = trim(weapon_str);
    if (trimmed.empty()) return std::nullopt;

    Weapon weapon;
    std::string_view remaining = trimmed;

    // Check for count prefix: "2x WeaponName" or "5x WeaponName"
    // Count represents how many models have this weapon
    u8 weapon_count = 1;
    std::regex count_re(R"(^(\d+)x\s+)");
    std::string remaining_str(remaining);
    std::smatch count_match;
    if (std::regex_search(remaining_str, count_match, count_re)) {
        weapon_count = static_cast<u8>(std::stoi(count_match[1].str()));
        remaining = remaining.substr(count_match[0].length());
    }

    // Check for range prefix: '24" WeaponName' or '12" WeaponName'
    std::regex range_re("^(\\d+)\"\\s*");
    remaining_str = std::string(remaining);
    std::smatch range_match;
    if (std::regex_search(remaining_str, range_match, range_re)) {
        weapon.range = static_cast<u8>(std::stoi(range_match[1].str()));
        remaining = remaining.substr(range_match[0].length());
    }

    // Find the parentheses with stats
    auto paren_start = remaining.find('(');
    if (paren_start == std::string_view::npos) {
        // No stats parentheses - just a name
        weapon.name = Name(trim(remaining));
        return weapon;
    }

    // Extract weapon name
    weapon.name = Name(trim(remaining.substr(0, paren_start)));

    // Find matching closing paren
    auto paren_end = remaining.rfind(')');
    if (paren_end == std::string_view::npos || paren_end <= paren_start) {
        return weapon;
    }

    // Parse stats inside parentheses
    auto stats_str = remaining.substr(paren_start + 1, paren_end - paren_start - 1);
    auto stats = split_respecting_parens(stats_str, ',');

    for (auto stat : stats) {
        auto stat_trimmed = trim(stat);
        if (stat_trimmed.empty()) continue;

        // Check for attacks: "A3", "A10"
        if (stat_trimmed[0] == 'A' && stat_trimmed.size() > 1 && std::isdigit(stat_trimmed[1])) {
            try {
                weapon.attacks = static_cast<u8>(std::stoi(std::string(stat_trimmed.substr(1))));
            } catch (...) {}
            continue;
        }

        // Check for AP: "AP(1)", "AP(4)"
        if (stat_trimmed.substr(0, 3) == "AP(" || stat_trimmed.substr(0, 3) == "ap(") {
            auto close = stat_trimmed.find(')');
            if (close != std::string_view::npos) {
                try {
                    weapon.ap = static_cast<u8>(std::stoi(std::string(stat_trimmed.substr(3, close - 3))));
                } catch (...) {}
            }
            continue;
        }

        // Otherwise, treat as a special rule
        if (auto rule = parse_rule(stat_trimmed)) {
            weapon.add_rule(rule->id, rule->value);
        }
    }

    // Set the weapon count (how many models have this weapon)
    weapon.count = weapon_count;

    return weapon;
}

inline bool UnitParser::parse_header(std::string_view line, Unit& unit) {
    // Format: "UnitName [count] QX+ DX+ | Xpts | Rules..."
    // Reduced format: "UnitName [BKT:XXXXXXXX] [count] QX+ DX+ | Xpts | Rules..."
    // Merged format: "UnitName [FID:XXXXXXXX] [count] QX+ DX+ | Xpts | Rules..."
    // Full format: "UnitName [FID:XXXXXXXX] [BKT:YYYYYYYY] [count] QX+ DX+ | Xpts | Rules..."

    std::string line_str(line);
    std::smatch match;

    // Try format with both FID and BKT tags
    std::regex fid_bkt_re(R"(^(.+?)\s*\[FID:([A-Fa-f0-9]{8})\]\s*\[BKT:([A-Fa-f0-9]{8})\]\s*\[(\d+)\]\s*Q(\d)\+\s*D(\d)\+\s*\|\s*(\d+)\s*pts\s*\|\s*(.*)$)");
    if (std::regex_match(line_str, match, fid_bkt_re)) {
        unit.name = Name(trim(match[1].str()));
        unit.set_faction_id(match[2].str());
        unit.set_bucket_hash(match[3].str());
        unit.model_count = static_cast<u8>(std::stoi(match[4].str()));
        unit.quality = static_cast<u8>(std::stoi(match[5].str()));
        unit.defense = static_cast<u8>(std::stoi(match[6].str()));
        unit.points_cost = static_cast<u16>(std::stoi(match[7].str()));

        for (u8 i = 0; i < unit.model_count && i < MAX_MODELS_PER_UNIT; ++i) {
            Model model;
            model.quality = unit.quality;
            model.defense = unit.defense;
            model.tough = 1;
            unit.models[i] = model;
        }
        unit.alive_count = unit.model_count;
        parse_rules(match[8].str(), unit);
        return true;
    }

    // Try format with FID tag only (from merge_all_factions.py)
    std::regex fid_re(R"(^(.+?)\s*\[FID:([A-Fa-f0-9]{8})\]\s*\[(\d+)\]\s*Q(\d)\+\s*D(\d)\+\s*\|\s*(\d+)\s*pts\s*\|\s*(.*)$)");
    if (std::regex_match(line_str, match, fid_re)) {
        unit.name = Name(trim(match[1].str()));
        unit.set_faction_id(match[2].str());
        unit.model_count = static_cast<u8>(std::stoi(match[3].str()));
        unit.quality = static_cast<u8>(std::stoi(match[4].str()));
        unit.defense = static_cast<u8>(std::stoi(match[5].str()));
        unit.points_cost = static_cast<u16>(std::stoi(match[6].str()));

        for (u8 i = 0; i < unit.model_count && i < MAX_MODELS_PER_UNIT; ++i) {
            Model model;
            model.quality = unit.quality;
            model.defense = unit.defense;
            model.tough = 1;
            unit.models[i] = model;
        }
        unit.alive_count = unit.model_count;
        parse_rules(match[7].str(), unit);
        return true;
    }

    // Try reduced format (BKT tag only)
    std::regex reduced_re(R"(^(.+?)\s*\[BKT:([A-Fa-f0-9]{8})\]\s*\[(\d+)\]\s*Q(\d)\+\s*D(\d)\+\s*\|\s*(\d+)\s*pts\s*\|\s*(.*)$)");
    if (std::regex_match(line_str, match, reduced_re)) {
        unit.name = Name(trim(match[1].str()));
        unit.set_bucket_hash(match[2].str());
        unit.model_count = static_cast<u8>(std::stoi(match[3].str()));
        unit.quality = static_cast<u8>(std::stoi(match[4].str()));
        unit.defense = static_cast<u8>(std::stoi(match[5].str()));
        unit.points_cost = static_cast<u16>(std::stoi(match[6].str()));

        for (u8 i = 0; i < unit.model_count && i < MAX_MODELS_PER_UNIT; ++i) {
            Model model;
            model.quality = unit.quality;
            model.defense = unit.defense;
            model.tough = 1;
            unit.models[i] = model;
        }
        unit.alive_count = unit.model_count;
        parse_rules(match[7].str(), unit);
        return true;
    }

    // Fall back to original format (no tags)
    std::regex header_re(R"(^(.+?)\s*\[(\d+)\]\s*Q(\d)\+\s*D(\d)\+\s*\|\s*(\d+)\s*pts\s*\|\s*(.*)$)");
    if (!std::regex_match(line_str, match, header_re)) {
        return false;
    }

    unit.name = Name(trim(match[1].str()));
    unit.model_count = static_cast<u8>(std::stoi(match[2].str()));
    unit.quality = static_cast<u8>(std::stoi(match[3].str()));
    unit.defense = static_cast<u8>(std::stoi(match[4].str()));
    unit.points_cost = static_cast<u16>(std::stoi(match[5].str()));

    for (u8 i = 0; i < unit.model_count && i < MAX_MODELS_PER_UNIT; ++i) {
        Model model;
        model.quality = unit.quality;
        model.defense = unit.defense;
        model.tough = 1;
        unit.models[i] = model;
    }
    unit.alive_count = unit.model_count;
    parse_rules(match[6].str(), unit);

    return true;
}

inline bool UnitParser::parse_weapons(std::string_view line, Unit& unit) {
    auto weapons = split_respecting_parens(line, ',');

    for (auto weapon_sv : weapons) {
        if (auto weapon = parse_weapon(weapon_sv)) {
            u8 idx = unit.add_weapon(*weapon);
            // Add weapon reference to first model (simplified - all models share weapons)
            if (unit.model_count > 0) {
                unit.models[0].add_weapon(idx);
            }
        }
    }

    return unit.weapon_count > 0;
}

inline std::optional<Unit> UnitParser::parse_unit(std::string_view header_line,
                                                   std::string_view weapons_line,
                                                   std::string_view faction_name) {
    Unit unit;
    unit.faction = Name(faction_name);

    if (!parse_header(header_line, unit)) {
        return std::nullopt;
    }

    if (!parse_weapons(weapons_line, unit)) {
        return std::nullopt;
    }

    // Compute AI type based on weapons
    unit.compute_ai_type();

    return unit;
}

inline UnitParser::ParseResult UnitParser::parse_string(const std::string& content,
                                                         std::string_view faction_name) {
    ParseResult result;

    // Clean content: remove carriage returns and null bytes
    std::string clean_content;
    clean_content.reserve(content.size());
    for (char c : content) {
        if (c != '\r' && c != '\0') {
            clean_content.push_back(c);
        }
    }

    std::istringstream stream(clean_content);
    std::string line;
    std::string pending_header;
    u32 unit_id = 0;

    while (std::getline(stream, line)) {
        result.lines_processed++;

        auto trimmed = trim(line);
        if (trimmed.empty()) {
            pending_header.clear();
            continue;
        }

        // Check if this looks like a header line (contains "[" and "]" and "pts")
        bool looks_like_header = (line.find('[') != std::string::npos &&
                                  line.find(']') != std::string::npos &&
                                  line.find("pts") != std::string::npos);

        if (looks_like_header) {
            pending_header = line;
        } else if (!pending_header.empty()) {
            // This should be the weapons line
            if (auto unit = parse_unit(pending_header, line, faction_name)) {
                unit->unit_id = unit_id++;
                result.units.push_back(std::move(*unit));
                result.units_parsed++;
            } else {
                result.errors.push_back("Failed to parse unit at line " +
                                       std::to_string(result.lines_processed));
            }
            pending_header.clear();
        }
    }

    return result;
}

inline UnitParser::ParseResult UnitParser::parse_file(const std::string& filepath,
                                                       std::string_view faction_name) {
    ParseResult result;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        result.errors.push_back("Could not open file: " + filepath);
        return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Try to extract faction name from filename if not provided
    std::string faction;
    if (faction_name.empty()) {
        // Extract from filename:
        // "Blessed_Sisters_pipeline.final.merged.txt" -> "Blessed Sisters"
        // "Blessed_Sisters.reduced.txt" -> "Blessed Sisters"
        // "all_faction.reduced.json" -> "all faction"
        size_t last_slash = filepath.find_last_of("/\\");
        std::string filename = (last_slash == std::string::npos) ? filepath : filepath.substr(last_slash + 1);

        // Remove extension and pipeline/reduced suffix
        size_t pos = filename.find(".reduced.json");
        if (pos == std::string::npos) pos = filename.find(".reduced.txt");
        if (pos == std::string::npos) pos = filename.find("_pipeline");
        if (pos == std::string::npos) pos = filename.find(".final.merged");
        if (pos == std::string::npos) pos = filename.find(".json");
        if (pos == std::string::npos) pos = filename.find(".txt");
        if (pos == std::string::npos) pos = filename.find(".");
        if (pos != std::string::npos) {
            faction = filename.substr(0, pos);
        }

        // Replace underscores with spaces
        std::replace(faction.begin(), faction.end(), '_', ' ');
    } else {
        faction = std::string(faction_name);
    }

    // Auto-detect JSON format
    if (is_json_content(content)) {
        return parse_json_string(content, faction);
    }

    return parse_string(content, faction);
}

// ==============================================================================
// JSON Parsing Implementation
// ==============================================================================

inline bool UnitParser::is_json_content(const std::string& content) {
    // Skip whitespace and check if content starts with '{'
    for (char c : content) {
        if (std::isspace(c)) continue;
        return c == '{';
    }
    return false;
}

inline std::string UnitParser::json_get_string(const std::string& json, const std::string& key) {
    // Find "key": "value" pattern
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos + search.length());
    if (pos == std::string::npos) return "";

    // Skip whitespace and find opening quote
    pos++;
    while (pos < json.length() && std::isspace(json[pos])) pos++;
    if (pos >= json.length() || json[pos] != '"') return "";

    pos++; // Skip opening quote
    size_t end = pos;
    while (end < json.length() && json[end] != '"') {
        if (json[end] == '\\' && end + 1 < json.length()) {
            end += 2; // Skip escaped character
        } else {
            end++;
        }
    }

    return json.substr(pos, end - pos);
}

inline int UnitParser::json_get_int(const std::string& json, const std::string& key, int default_val) {
    // Find "key": number pattern
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return default_val;

    pos = json.find(':', pos + search.length());
    if (pos == std::string::npos) return default_val;

    // Skip whitespace and find number
    pos++;
    while (pos < json.length() && std::isspace(json[pos])) pos++;
    if (pos >= json.length()) return default_val;

    // Handle negative numbers
    bool negative = false;
    if (json[pos] == '-') {
        negative = true;
        pos++;
    }

    if (!std::isdigit(json[pos])) return default_val;

    int value = 0;
    while (pos < json.length() && std::isdigit(json[pos])) {
        value = value * 10 + (json[pos] - '0');
        pos++;
    }

    return negative ? -value : value;
}

inline std::vector<std::string> UnitParser::json_get_string_array(const std::string& json, const std::string& key) {
    std::vector<std::string> result;

    std::string array_content = find_json_array(json, key);
    if (array_content.empty()) return result;

    // Parse strings from array
    size_t pos = 0;
    while (pos < array_content.length()) {
        // Find opening quote
        size_t start = array_content.find('"', pos);
        if (start == std::string::npos) break;

        start++; // Skip quote
        size_t end = start;
        while (end < array_content.length() && array_content[end] != '"') {
            if (array_content[end] == '\\' && end + 1 < array_content.length()) {
                end += 2;
            } else {
                end++;
            }
        }

        result.push_back(array_content.substr(start, end - start));
        pos = end + 1;
    }

    return result;
}

inline std::string UnitParser::find_json_array(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos + search.length());
    if (pos == std::string::npos) return "";

    // Skip whitespace and find opening bracket
    pos++;
    while (pos < json.length() && std::isspace(json[pos])) pos++;
    if (pos >= json.length() || json[pos] != '[') return "";

    // Find matching closing bracket
    size_t start = pos + 1;
    int depth = 1;
    pos++;
    while (pos < json.length() && depth > 0) {
        if (json[pos] == '[') depth++;
        else if (json[pos] == ']') depth--;
        else if (json[pos] == '"') {
            // Skip string content
            pos++;
            while (pos < json.length() && json[pos] != '"') {
                if (json[pos] == '\\' && pos + 1 < json.length()) pos++;
                pos++;
            }
        }
        pos++;
    }

    return json.substr(start, pos - start - 1);
}

inline std::vector<std::string> UnitParser::json_get_object_array(const std::string& json, const std::string& key) {
    std::vector<std::string> result;

    std::string array_content = find_json_array(json, key);
    if (array_content.empty()) return result;

    // Parse objects from array
    size_t pos = 0;
    while (pos < array_content.length()) {
        // Find opening brace
        size_t start = array_content.find('{', pos);
        if (start == std::string::npos) break;

        // Find matching closing brace
        int depth = 1;
        size_t end = start + 1;
        while (end < array_content.length() && depth > 0) {
            if (array_content[end] == '{') depth++;
            else if (array_content[end] == '}') depth--;
            else if (array_content[end] == '"') {
                // Skip string content
                end++;
                while (end < array_content.length() && array_content[end] != '"') {
                    if (array_content[end] == '\\' && end + 1 < array_content.length()) end++;
                    end++;
                }
            }
            end++;
        }

        result.push_back(array_content.substr(start, end - start));
        pos = end;
    }

    return result;
}

inline std::optional<Weapon> UnitParser::parse_weapon_from_json(const std::string& json_obj) {
    Weapon weapon;

    std::string name = json_get_string(json_obj, "name");
    if (name.empty()) return std::nullopt;

    weapon.name = Name(name);
    weapon.attacks = static_cast<u8>(json_get_int(json_obj, "attacks", 1));
    weapon.ap = static_cast<u8>(json_get_int(json_obj, "ap", 0));
    weapon.range = static_cast<u8>(json_get_int(json_obj, "range", 0));

    // Parse special rules
    auto specials = json_get_string_array(json_obj, "special");
    for (const auto& special : specials) {
        if (auto rule = parse_rule(special)) {
            weapon.add_rule(rule->id, rule->value);
        }
    }

    return weapon;
}

inline std::optional<Unit> UnitParser::parse_unit_from_json(const std::string& json_obj,
                                                             std::string_view faction_name) {
    Unit unit;

    std::string name = json_get_string(json_obj, "name");
    if (name.empty()) return std::nullopt;

    unit.name = Name(name);

    // Check for per-unit faction field first, then fall back to provided faction_name
    std::string unit_faction = json_get_string(json_obj, "faction");
    if (!unit_faction.empty()) {
        unit.faction = Name(unit_faction);
    } else {
        unit.faction = Name(faction_name);
    }

    // Get bucket hash if present
    std::string bucket_hash = json_get_string(json_obj, "bucket_hash");
    if (!bucket_hash.empty()) {
        unit.set_bucket_hash(bucket_hash);
    }

    unit.model_count = static_cast<u8>(json_get_int(json_obj, "size", 1));
    unit.quality = static_cast<u8>(json_get_int(json_obj, "quality", 4));
    unit.defense = static_cast<u8>(json_get_int(json_obj, "defense", 4));
    unit.points_cost = static_cast<u16>(json_get_int(json_obj, "points", 0));

    // Create models
    for (u8 i = 0; i < unit.model_count && i < MAX_MODELS_PER_UNIT; ++i) {
        Model model;
        model.quality = unit.quality;
        model.defense = unit.defense;
        model.tough = 1;
        unit.models[i] = model;
    }
    unit.alive_count = unit.model_count;

    // Parse rules
    auto rules = json_get_string_array(json_obj, "rules");
    for (const auto& rule_str : rules) {
        if (auto rule = parse_rule(rule_str)) {
            unit.add_rule(rule->id, rule->value);

            if (rule->id == RuleId::Tough) {
                for (u8 i = 0; i < unit.model_count; ++i) {
                    unit.models[i].tough = rule->value;
                }
            } else if (rule->id == RuleId::Hero) {
                if (unit.model_count > 0) {
                    unit.models[0].is_hero = true;
                }
            }
        }
    }

    // Parse weapons
    auto weapons = json_get_object_array(json_obj, "weapons");
    for (const auto& weapon_json : weapons) {
        if (auto weapon = parse_weapon_from_json(weapon_json)) {
            // Handle weapon count (multiple of same weapon)
            int count = json_get_int(weapon_json, "count", 1);
            for (int i = 0; i < count; ++i) {
                u8 idx = unit.add_weapon(*weapon);
                if (unit.model_count > 0) {
                    unit.models[0].add_weapon(idx);
                }
            }
        }
    }

    // Compute AI type based on weapons
    unit.compute_ai_type();

    return unit;
}

inline UnitParser::ParseResult UnitParser::parse_json_string(const std::string& content,
                                                              std::string_view faction_name) {
    ParseResult result;

    // Check for "factions" array format (from merge_all_factions.py)
    // This format has: { "factions": [ { "name": "...", "units": ["text lines..."] } ] }
    auto faction_objects = json_get_object_array(content, "factions");

    if (!faction_objects.empty()) {
        // Parse factions format - each faction contains text-based unit lines
        u32 unit_id = 0;

        for (const auto& faction_json : faction_objects) {
            std::string current_faction = json_get_string(faction_json, "name");

            // Get the "units" array which contains text lines (not structured objects)
            auto unit_lines = json_get_string_array(faction_json, "units");

            // Parse unit lines in pairs (header + weapons)
            std::string pending_header;
            for (const auto& line : unit_lines) {
                result.lines_processed++;

                if (line.empty()) {
                    pending_header.clear();
                    continue;
                }

                // Check if this looks like a header line
                bool looks_like_header = (line.find('[') != std::string::npos &&
                                          line.find(']') != std::string::npos &&
                                          line.find("pts") != std::string::npos);

                if (looks_like_header) {
                    pending_header = line;
                } else if (!pending_header.empty()) {
                    // This should be the weapons line
                    if (auto unit = parse_unit(pending_header, line, current_faction)) {
                        unit->unit_id = unit_id++;
                        result.units.push_back(std::move(*unit));
                        result.units_parsed++;
                    }
                    pending_header.clear();
                }
            }
        }

        return result;
    }

    // Fall back to "units" array format (from bucket_reduced_to_json.py)
    // This format has: { "units": [ { "name": "...", "size": 1, ... } ] }
    auto unit_objects = json_get_object_array(content, "units");

    if (unit_objects.empty()) {
        result.errors.push_back("No 'units' or 'factions' array found in JSON");
        return result;
    }

    u32 unit_id = 0;
    for (const auto& unit_json : unit_objects) {
        result.lines_processed++;

        if (auto unit = parse_unit_from_json(unit_json, faction_name)) {
            unit->unit_id = unit_id++;
            result.units.push_back(std::move(*unit));
            result.units_parsed++;
        } else {
            result.errors.push_back("Failed to parse unit from JSON object");
        }
    }

    return result;
}

} // namespace battle
