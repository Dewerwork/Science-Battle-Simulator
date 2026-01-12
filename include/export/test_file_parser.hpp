#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <fstream>
#include <sstream>
#include <optional>

namespace battle {

// ==============================================================================
// Test Matchup Definition
// ==============================================================================

struct TestMatchup {
    std::string name;
    std::string unit_a_header;   // First line of unit A definition
    std::string unit_a_weapons;  // Second line of unit A definition
    std::string unit_b_header;   // First line of unit B definition
    std::string unit_b_weapons;  // Second line of unit B definition
    int iterations = 100;        // Default iterations
    std::optional<uint64_t> seed; // Optional fixed seed
};

// ==============================================================================
// TestFileParser - Parses batch test input files
// ==============================================================================
//
// Input format:
//   Matchup_name: <name>
//   [iterations: N]      (optional, defaults to 100)
//   [seed: N]            (optional)
//
//   <Unit A header line>
//   <Unit A weapons line>
//
//   <Unit B header line>
//   <Unit B weapons line>
//
// Example:
//   Matchup_name: Piercing_Tag_Test
//
//   Hive Lord [1] Q3+ D2+ | 525pts | Tough(300), Piercing Tag
//   Stomp (A4, AP(1))
//
//   Hive Lord [1] Q3+ D2+ | 525pts | Tough(300)
//   Stomp (A4, AP(-1))
//

class TestFileParser {
public:
    struct ParseResult {
        std::vector<TestMatchup> matchups;
        std::vector<std::string> errors;
        size_t lines_processed = 0;
    };

    // Parse a file containing test matchups
    static ParseResult parse_file(const std::string& filepath) {
        ParseResult result;

        std::ifstream file(filepath);
        if (!file.is_open()) {
            result.errors.push_back("Failed to open file: " + filepath);
            return result;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return parse_string(content);
    }

    // Parse a string containing test matchups
    static ParseResult parse_string(const std::string& content) {
        ParseResult result;

        std::istringstream stream(content);
        std::string line;

        TestMatchup current_matchup;
        bool in_matchup = false;
        int unit_phase = 0;  // 0 = none, 1 = unit A header, 2 = unit A weapons, 3 = unit B header, 4 = unit B weapons
        int blank_line_count = 0;

        while (std::getline(stream, line)) {
            result.lines_processed++;

            // Trim whitespace
            auto trimmed = trim(line);

            // Skip comments
            if (trimmed.empty() || trimmed[0] == '#') {
                if (trimmed.empty() && in_matchup) {
                    blank_line_count++;

                    // Two consecutive blank lines = end of matchup
                    if (blank_line_count >= 2) {
                        if (validate_and_add_matchup(current_matchup, result)) {
                            // Reset for next matchup
                            current_matchup = TestMatchup{};
                            in_matchup = false;
                            unit_phase = 0;
                            blank_line_count = 0;
                        }
                    }
                }
                continue;
            }

            blank_line_count = 0;

            // Check for matchup name
            if (starts_with_ignore_case(trimmed, "matchup_name:") ||
                starts_with_ignore_case(trimmed, "matchup:") ||
                starts_with_ignore_case(trimmed, "name:")) {
                // Save previous matchup if exists
                if (in_matchup && !current_matchup.name.empty()) {
                    validate_and_add_matchup(current_matchup, result);
                }

                // Start new matchup
                current_matchup = TestMatchup{};
                in_matchup = true;
                unit_phase = 0;

                // Extract name
                auto colon_pos = trimmed.find(':');
                if (colon_pos != std::string_view::npos) {
                    current_matchup.name = std::string(trim(trimmed.substr(colon_pos + 1)));
                }
                continue;
            }

            // Check for iterations
            if (starts_with_ignore_case(trimmed, "iterations:")) {
                auto colon_pos = trimmed.find(':');
                if (colon_pos != std::string_view::npos) {
                    try {
                        current_matchup.iterations = std::stoi(std::string(trim(trimmed.substr(colon_pos + 1))));
                    } catch (...) {
                        result.errors.push_back("Invalid iterations value at line " +
                                               std::to_string(result.lines_processed));
                    }
                }
                continue;
            }

            // Check for seed
            if (starts_with_ignore_case(trimmed, "seed:")) {
                auto colon_pos = trimmed.find(':');
                if (colon_pos != std::string_view::npos) {
                    try {
                        current_matchup.seed = std::stoull(std::string(trim(trimmed.substr(colon_pos + 1))));
                    } catch (...) {
                        result.errors.push_back("Invalid seed value at line " +
                                               std::to_string(result.lines_processed));
                    }
                }
                continue;
            }

            // Process unit definitions
            if (in_matchup) {
                // Determine what this line is based on content and phase
                // Unit header lines contain "Q" and "D" for quality/defense
                // Weapon lines contain parentheses with attack stats

                bool looks_like_header = (trimmed.find("Q") != std::string_view::npos &&
                                         trimmed.find("D") != std::string_view::npos &&
                                         trimmed.find("[") != std::string_view::npos);
                bool looks_like_weapons = (trimmed.find("(") != std::string_view::npos &&
                                          (trimmed.find("A") != std::string_view::npos ||
                                           trimmed.find("a") != std::string_view::npos));

                if (unit_phase == 0 && looks_like_header) {
                    // Unit A header
                    current_matchup.unit_a_header = std::string(trimmed);
                    unit_phase = 1;
                } else if (unit_phase == 1 && (looks_like_weapons || !looks_like_header)) {
                    // Unit A weapons
                    current_matchup.unit_a_weapons = std::string(trimmed);
                    unit_phase = 2;
                } else if (unit_phase == 2 && looks_like_header) {
                    // Unit B header
                    current_matchup.unit_b_header = std::string(trimmed);
                    unit_phase = 3;
                } else if (unit_phase == 3 && (looks_like_weapons || !looks_like_header)) {
                    // Unit B weapons
                    current_matchup.unit_b_weapons = std::string(trimmed);
                    unit_phase = 4;
                } else if (looks_like_header) {
                    // Unexpected header - might be start of a new unit in same matchup
                    // This handles cases where weapon lines might be missing
                    if (unit_phase == 1) {
                        // Unit A had no weapons, this is Unit B header
                        current_matchup.unit_b_header = std::string(trimmed);
                        unit_phase = 3;
                    }
                }
            }
        }

        // Don't forget the last matchup
        if (in_matchup && !current_matchup.name.empty()) {
            validate_and_add_matchup(current_matchup, result);
        }

        return result;
    }

private:
    static bool validate_and_add_matchup(TestMatchup& matchup, ParseResult& result) {
        // Validate
        if (matchup.name.empty()) {
            result.errors.push_back("Matchup has no name");
            return false;
        }
        if (matchup.unit_a_header.empty()) {
            result.errors.push_back("Matchup '" + matchup.name + "' has no Unit A header");
            return false;
        }
        if (matchup.unit_b_header.empty()) {
            result.errors.push_back("Matchup '" + matchup.name + "' has no Unit B header");
            return false;
        }

        // Default weapons if not specified
        if (matchup.unit_a_weapons.empty()) {
            matchup.unit_a_weapons = "Unarmed (A1)";
        }
        if (matchup.unit_b_weapons.empty()) {
            matchup.unit_b_weapons = "Unarmed (A1)";
        }

        result.matchups.push_back(matchup);
        return true;
    }

    static std::string_view trim(std::string_view sv) {
        while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
            sv.remove_prefix(1);
        }
        while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
            sv.remove_suffix(1);
        }
        return sv;
    }

    static bool starts_with_ignore_case(std::string_view str, std::string_view prefix) {
        if (str.size() < prefix.size()) return false;
        for (size_t i = 0; i < prefix.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(str[i])) !=
                std::tolower(static_cast<unsigned char>(prefix[i]))) {
                return false;
            }
        }
        return true;
    }
};

} // namespace battle
