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

    // Parse a file containing multiple units
    static ParseResult parse_file(const std::string& filepath, std::string_view faction_name = "");

    // Parse a string containing multiple units
    static ParseResult parse_string(const std::string& content, std::string_view faction_name = "");

    // Parse a single unit from two lines (header + weapons)
    static std::optional<Unit> parse_unit(std::string_view header_line,
                                          std::string_view weapons_line,
                                          std::string_view faction_name = "");

private:
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
    u8 count = 1;
    std::regex count_re(R"(^(\d+)x\s+)");
    std::string remaining_str(remaining);
    std::smatch count_match;
    if (std::regex_search(remaining_str, count_match, count_re)) {
        count = static_cast<u8>(std::stoi(count_match[1].str()));
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

    return weapon;
}

inline bool UnitParser::parse_header(std::string_view line, Unit& unit) {
    // Format: "UnitName [count] QX+ DX+ | Xpts | Rules..."
    // Example: "Assault Walker [1] Q4+ D2+ | 350pts | Devout, Fear(2), Fearless"

    std::string line_str(line);

    // Use regex to parse header
    std::regex header_re(R"(^(.+?)\s*\[(\d+)\]\s*Q(\d)\+\s*D(\d)\+\s*\|\s*(\d+)pts\s*\|\s*(.*)$)");
    std::smatch match;

    if (!std::regex_match(line_str, match, header_re)) {
        return false;
    }

    // Extract components
    unit.name = Name(trim(match[1].str()));
    unit.model_count = static_cast<u8>(std::stoi(match[2].str()));
    unit.quality = static_cast<u8>(std::stoi(match[3].str()));
    unit.defense = static_cast<u8>(std::stoi(match[4].str()));
    unit.points_cost = static_cast<u16>(std::stoi(match[5].str()));

    // Create models
    for (u8 i = 0; i < unit.model_count && i < MAX_MODELS_PER_UNIT; ++i) {
        Model model;
        model.quality = unit.quality;
        model.defense = unit.defense;
        model.tough = 1; // Default, will be overwritten by Tough rule
        unit.models[i] = model;
    }
    unit.alive_count = unit.model_count;

    // Parse rules
    std::string rules_str = match[6].str();
    parse_rules(rules_str, unit);

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

    // Try to extract faction name from filename if not provided
    std::string faction;
    if (faction_name.empty()) {
        // Extract from filename: "Blessed_Sisters_pipeline.final.merged.txt" -> "Blessed Sisters"
        size_t last_slash = filepath.find_last_of("/\\");
        std::string filename = (last_slash == std::string::npos) ? filepath : filepath.substr(last_slash + 1);

        // Remove extension and pipeline suffix
        size_t pos = filename.find("_pipeline");
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

    return parse_string(buffer.str(), faction);
}

} // namespace battle
