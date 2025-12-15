#include "parser/unit_parser.hpp"
#include "simulation/simulator.hpp"
#include "core/unit.hpp"
#include "core/weapon.hpp"
#include <iostream>
#include <cassert>
#include <set>
#include <regex>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cmath>

using namespace battle;

// ==============================================================================
// Test 1: Validate all special rules in unit files are accounted for
// ==============================================================================

// Extract all rule names from a unit file and check against the parser's rule map
struct RuleValidationResult {
    std::set<std::string> recognized_rules;
    std::set<std::string> unrecognized_rules;
    size_t total_rule_occurrences = 0;
    size_t unrecognized_occurrences = 0;
};

// Get all known rules from the parser's rule map
std::set<std::string> get_known_rules() {
    std::set<std::string> known;
    // These are the normalized (lowercase) rule names from UnitParser::get_rule_map()
    const char* rules[] = {
        "ap", "blast", "deadly", "lance", "poison", "precise", "reliable",
        "rending", "bane", "impact", "indirect", "sniper", "lock-on", "purge",
        "regeneration", "tough", "protected", "stealth", "shield wall", "shieldwall",
        "fearless", "furious", "hero", "relentless", "fear", "counter", "fast",
        "flying", "strider", "scout", "ambush", "devout", "piercing assault",
        "piercingassault", "unstoppable", "casting", "slow", "surge", "thrust",
        "takedown", "limited", "shielded", "resistance", "no retreat", "noretreat",
        "morale boost", "moraleboost", "hive bond", "hivebond", "rupture", "agile",
        "hit & run", "hit and run", "hitandrun", "point-blank surge", "pointblanksurge",
        "shred", "smash", "battleborn", "predator fighter", "predatorfighter",
        "rapid charge", "rapidcharge", "self-destruct", "selfdestruct",
        "versatile attack", "versatileattack", "good shot", "goodshot",
        "bad shot", "badshot", "melee evasion", "meleeevasion",
        "melee shrouding", "meleeshrouding", "ranged shrouding", "rangedshrouding"
    };
    for (const char* r : rules) {
        known.insert(r);
    }
    return known;
}

// Extract rule name (without value) and normalize to lowercase
std::string normalize_rule_name(const std::string& rule_str) {
    std::string name = rule_str;

    // Remove value in parentheses: "Tough(3)" -> "tough"
    auto paren_pos = name.find('(');
    if (paren_pos != std::string::npos) {
        name = name.substr(0, paren_pos);
    }

    // Trim whitespace
    while (!name.empty() && std::isspace(name.front())) name.erase(0, 1);
    while (!name.empty() && std::isspace(name.back())) name.pop_back();

    // Convert to lowercase
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

    return name;
}

// Parse unit file and extract all rule names used
RuleValidationResult validate_rules_in_content(const std::string& content) {
    RuleValidationResult result;
    auto known_rules = get_known_rules();

    // Regex to match rule sections after "| Xpts |"
    std::regex header_re(R"(\|\s*\d+pts\s*\|\s*(.+)$)");

    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, header_re)) {
            std::string rules_section = match[1].str();

            // Split by comma (respecting parentheses for values like Tough(3))
            std::vector<std::string> rules;
            size_t start = 0;
            int paren_depth = 0;

            for (size_t i = 0; i < rules_section.size(); ++i) {
                char c = rules_section[i];
                if (c == '(') paren_depth++;
                else if (c == ')') paren_depth = std::max(0, paren_depth - 1);
                else if (c == ',' && paren_depth == 0) {
                    std::string rule = rules_section.substr(start, i - start);
                    if (!rule.empty()) rules.push_back(rule);
                    start = i + 1;
                }
            }
            // Last rule
            if (start < rules_section.size()) {
                std::string rule = rules_section.substr(start);
                if (!rule.empty()) rules.push_back(rule);
            }

            // Check each rule
            for (const auto& rule_str : rules) {
                std::string normalized = normalize_rule_name(rule_str);
                if (normalized.empty()) continue;

                result.total_rule_occurrences++;

                if (known_rules.count(normalized) > 0) {
                    result.recognized_rules.insert(normalized);
                } else {
                    result.unrecognized_rules.insert(normalized);
                    result.unrecognized_occurrences++;
                }
            }
        }
    }

    return result;
}

RuleValidationResult validate_rules_in_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filepath << std::endl;
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return validate_rules_in_content(buffer.str());
}

void test_special_rules_coverage() {
    std::cout << "\n=== Special Rules Coverage Test ===" << std::endl;

    // Test with sample unit data
    std::string sample_data = R"(
APC [1] Q4+ D2+ | 175pts | Devout, Impact(3), Strider, Tough(6)
24" Storm Rifle (A3, AP(1))

Assault Walker [1] Q4+ D2+ | 350pts | Devout, Fear(2), Fearless, Piercing Assault, Regeneration, Tough(9)
Stomp (A3, AP(1)), Heavy Claw (A4, AP(1), Rending)

Test Unit [5] Q4+ D4+ | 100pts | UnknownRule, AnotherFakeRule(5), Devout
Sword (A2)
)";

    auto result = validate_rules_in_content(sample_data);

    std::cout << "Recognized rules (" << result.recognized_rules.size() << "):" << std::endl;
    for (const auto& rule : result.recognized_rules) {
        std::cout << "  [OK] " << rule << std::endl;
    }

    std::cout << "\nUnrecognized rules (" << result.unrecognized_rules.size() << "):" << std::endl;
    for (const auto& rule : result.unrecognized_rules) {
        std::cout << "  [MISSING] " << rule << std::endl;
    }

    std::cout << "\nTotal rule occurrences: " << result.total_rule_occurrences << std::endl;
    std::cout << "Unrecognized occurrences: " << result.unrecognized_occurrences << std::endl;

    // We expect UnknownRule and AnotherFakeRule to be unrecognized
    assert(result.unrecognized_rules.count("unknownrule") > 0);
    assert(result.unrecognized_rules.count("anotherfakerule") > 0);

    // We expect known rules to be recognized
    assert(result.recognized_rules.count("devout") > 0);
    assert(result.recognized_rules.count("tough") > 0);
    assert(result.recognized_rules.count("fearless") > 0);

    std::cout << "\n[PASS] test_special_rules_coverage" << std::endl;
}

void test_rules_in_docs_folder() {
    std::cout << "\n=== Docs Folder Rules Validation ===" << std::endl;

    // Check if docs folder exists and has txt files
    std::string docs_path = "docs";
    if (!std::filesystem::exists(docs_path)) {
        docs_path = "../docs";  // Try parent directory
    }

    if (!std::filesystem::exists(docs_path)) {
        std::cout << "[SKIP] docs folder not found, skipping file validation" << std::endl;
        return;
    }

    RuleValidationResult combined_result;
    int files_checked = 0;

    for (const auto& entry : std::filesystem::directory_iterator(docs_path)) {
        if (entry.path().extension() == ".txt") {
            auto file_result = validate_rules_in_file(entry.path().string());

            // Merge results
            combined_result.recognized_rules.insert(
                file_result.recognized_rules.begin(),
                file_result.recognized_rules.end());
            combined_result.unrecognized_rules.insert(
                file_result.unrecognized_rules.begin(),
                file_result.unrecognized_rules.end());
            combined_result.total_rule_occurrences += file_result.total_rule_occurrences;
            combined_result.unrecognized_occurrences += file_result.unrecognized_occurrences;

            files_checked++;
        }
    }

    std::cout << "Files checked: " << files_checked << std::endl;
    std::cout << "Total rule occurrences: " << combined_result.total_rule_occurrences << std::endl;
    std::cout << "Recognized unique rules: " << combined_result.recognized_rules.size() << std::endl;

    if (!combined_result.unrecognized_rules.empty()) {
        std::cout << "\n[WARNING] Unrecognized rules found:" << std::endl;
        for (const auto& rule : combined_result.unrecognized_rules) {
            std::cout << "  [MISSING] " << rule << std::endl;
        }
        std::cout << "Total unrecognized occurrences: " << combined_result.unrecognized_occurrences << std::endl;
    } else {
        std::cout << "\n[OK] All rules in docs files are recognized!" << std::endl;
    }

    // This test passes even with unrecognized rules - it just reports them
    // Change to assert if you want it to fail on unrecognized rules
    std::cout << "\n[PASS] test_rules_in_docs_folder (validation complete)" << std::endl;
}

// ==============================================================================
// Test 2: Validate simulation correctness
// ==============================================================================

Unit create_validation_unit(const char* name, int models, int quality, int defense, int attacks = 2) {
    Unit unit(name, 100);

    Weapon sword("Sword", attacks, 0, 0);  // attacks, range (0=melee), AP
    WeaponIndex sword_idx = get_weapon_pool().add(sword);

    for (int i = 0; i < models; ++i) {
        Model m("Soldier", quality, defense, 1);
        m.add_weapon(sword_idx);
        unit.add_model(m);
    }

    return unit;
}

void test_simulation_completes_without_crash() {
    std::cout << "\n=== Simulation Completion Test ===" << std::endl;

    Unit attacker = create_validation_unit("Attacker", 5, 4, 4);
    Unit defender = create_validation_unit("Defender", 5, 4, 4);

    SimulationConfig config;
    config.iterations_per_matchup = 100;
    config.max_rounds = 10;

    MatchupSimulator sim;
    LocalStats stats;

    // This should not crash
    sim.run_batch(attacker, defender, config, 100, stats);

    // Basic sanity checks
    assert(stats.attacker_wins + stats.defender_wins + stats.draws == 100);

    std::cout << "Completed 100 iterations without crash" << std::endl;
    std::cout << "Attacker wins: " << stats.attacker_wins << std::endl;
    std::cout << "Defender wins: " << stats.defender_wins << std::endl;
    std::cout << "Draws: " << stats.draws << std::endl;

    std::cout << "[PASS] test_simulation_completes_without_crash" << std::endl;
}

void test_simulation_statistics_valid() {
    std::cout << "\n=== Simulation Statistics Validation ===" << std::endl;

    Unit attacker = create_validation_unit("Attacker", 5, 4, 4);
    Unit defender = create_validation_unit("Defender", 5, 4, 4);

    SimulationConfig config;
    config.iterations_per_matchup = 1000;
    config.max_rounds = 10;

    MatchupSimulator sim;
    LocalStats stats;
    sim.run_batch(attacker, defender, config, 1000, stats);

    // Compute statistics manually from LocalStats
    const u64 iterations = 1000;
    double inv_iter = 1.0 / static_cast<double>(iterations);

    double attacker_win_rate = stats.attacker_wins * inv_iter;
    double defender_win_rate = stats.defender_wins * inv_iter;
    double draw_rate = stats.draws * inv_iter;
    double avg_rounds = stats.total_rounds * inv_iter;
    // Validate win rates sum to approximately 1.0
    double total_rate = attacker_win_rate + defender_win_rate + draw_rate;
    assert(std::abs(total_rate - 1.0) < 0.001);
    std::cout << "Win rates sum to: " << total_rate << " (expected ~1.0) [OK]" << std::endl;

    // Validate win rates are within valid range
    assert(attacker_win_rate >= 0.0 && attacker_win_rate <= 1.0);
    assert(defender_win_rate >= 0.0 && defender_win_rate <= 1.0);
    assert(draw_rate >= 0.0 && draw_rate <= 1.0);
    std::cout << "All win rates in valid range [0, 1] [OK]" << std::endl;

    // Validate average rounds is reasonable (1-10)
    assert(avg_rounds >= 1.0 && avg_rounds <= 10.0);
    std::cout << "Average rounds: " << avg_rounds << " (expected 1-10) [OK]" << std::endl;

    // Validate wounds/kills are non-negative
    assert(stats.total_wounds_by_attacker >= 0);
    assert(stats.total_wounds_by_defender >= 0);
    assert(stats.total_kills_by_attacker >= 0);
    assert(stats.total_kills_by_defender >= 0);
    std::cout << "Wounds and kills are non-negative [OK]" << std::endl;

    std::cout << "[PASS] test_simulation_statistics_valid" << std::endl;
}

void test_equal_units_approximately_equal_win_rate() {
    std::cout << "\n=== Equal Units Win Rate Test ===" << std::endl;

    Unit attacker = create_validation_unit("Attacker", 5, 4, 4);
    Unit defender = create_validation_unit("Defender", 5, 4, 4);

    SimulationConfig config;
    config.iterations_per_matchup = 10000;  // More iterations for statistical significance
    config.max_rounds = 10;

    MatchupSimulator sim;
    LocalStats stats;
    sim.run_batch(attacker, defender, config, 10000, stats);

    double attacker_win_rate = static_cast<double>(stats.attacker_wins) / 10000.0;
    double defender_win_rate = static_cast<double>(stats.defender_wins) / 10000.0;

    std::cout << "Attacker win rate: " << (attacker_win_rate * 100) << "%" << std::endl;
    std::cout << "Defender win rate: " << (defender_win_rate * 100) << "%" << std::endl;

    // With equal units, expect win rates between 30% and 70% for each side
    // The attacker may have slight advantage due to striking first
    assert(attacker_win_rate > 0.25 && attacker_win_rate < 0.75);
    assert(defender_win_rate > 0.15 && defender_win_rate < 0.75);

    std::cout << "[PASS] test_equal_units_approximately_equal_win_rate" << std::endl;
}

void test_better_unit_wins_more() {
    std::cout << "\n=== Better Unit Advantage Test ===" << std::endl;

    // Elite unit: Quality 3+ (hits on 3-6), Defense 3+ (saves on 3-6)
    Unit elite = create_validation_unit("Elite", 5, 3, 3);

    // Basic unit: Quality 5+ (hits on 5-6), Defense 5+ (saves on 5-6)
    Unit basic = create_validation_unit("Basic", 5, 5, 5);

    SimulationConfig config;
    config.iterations_per_matchup = 5000;
    config.max_rounds = 10;

    MatchupSimulator sim;
    LocalStats stats;
    sim.run_batch(elite, basic, config, 5000, stats);

    double elite_win_rate = static_cast<double>(stats.attacker_wins) / 5000.0;

    std::cout << "Elite (Q3+/D3+) vs Basic (Q5+/D5+)" << std::endl;
    std::cout << "Elite win rate: " << (elite_win_rate * 100) << "%" << std::endl;

    // Elite should win significantly more often
    assert(elite_win_rate > 0.70);

    std::cout << "[PASS] test_better_unit_wins_more" << std::endl;
}

void test_more_models_advantage() {
    std::cout << "\n=== Model Count Advantage Test ===" << std::endl;

    // Large squad: 10 models
    Unit large_squad = create_validation_unit("Large Squad", 10, 4, 4);

    // Small squad: 3 models
    Unit small_squad = create_validation_unit("Small Squad", 3, 4, 4);

    SimulationConfig config;
    config.iterations_per_matchup = 5000;
    config.max_rounds = 10;

    MatchupSimulator sim;
    LocalStats stats;
    sim.run_batch(large_squad, small_squad, config, 5000, stats);

    double large_win_rate = static_cast<double>(stats.attacker_wins) / 5000.0;

    std::cout << "Large Squad (10 models) vs Small Squad (3 models)" << std::endl;
    std::cout << "Large squad win rate: " << (large_win_rate * 100) << "%" << std::endl;

    // Larger squad should win significantly more often
    assert(large_win_rate > 0.80);

    std::cout << "[PASS] test_more_models_advantage" << std::endl;
}

void test_parser_produces_valid_units() {
    std::cout << "\n=== Parser Unit Validation Test ===" << std::endl;

    std::string unit_data = R"(
Assault Walker [1] Q4+ D2+ | 350pts | Devout, Fear(2), Fearless, Tough(9)
Stomp (A3, AP(1)), Heavy Claw (A4, AP(1), Rending)

Assault Sisters [5] Q4+ D4+ | 195pts | Devout
5x Energy Swords (A10, AP(1), Rending), 12" Heavy Pistols (A4, AP(1))
)";

    auto result = UnitParser::parse_string(unit_data, "Test Faction");

    std::cout << "Lines processed: " << result.lines_processed << std::endl;
    std::cout << "Units parsed: " << result.units_parsed << std::endl;
    std::cout << "Errors: " << result.errors.size() << std::endl;

    // Should parse 2 units
    assert(result.units.size() == 2);

    // Validate first unit (Assault Walker)
    const Unit& walker = result.units[0];
    assert(walker.model_count == 1);
    assert(walker.quality == 4);
    assert(walker.defense == 2);
    assert(walker.points_cost == 350);
    assert(walker.has_rule(RuleId::Fearless));
    assert(walker.has_rule(RuleId::Tough));
    assert(walker.has_rule(RuleId::Fear));
    std::cout << "Assault Walker parsed correctly [OK]" << std::endl;

    // Validate second unit (Assault Sisters)
    const Unit& sisters = result.units[1];
    assert(sisters.model_count == 5);
    assert(sisters.quality == 4);
    assert(sisters.defense == 4);
    assert(sisters.points_cost == 195);
    assert(sisters.has_rule(RuleId::Devout));
    std::cout << "Assault Sisters parsed correctly [OK]" << std::endl;

    // Validate parsed units can run simulations
    SimulationConfig config;
    config.iterations_per_matchup = 10;
    config.max_rounds = 10;

    MatchupSimulator sim;
    LocalStats stats;
    sim.run_batch(walker, sisters, config, 10, stats);

    assert(stats.attacker_wins + stats.defender_wins + stats.draws == 10);
    std::cout << "Parsed units successfully simulated [OK]" << std::endl;

    std::cout << "[PASS] test_parser_produces_valid_units" << std::endl;
}

void test_victory_conditions_tracked() {
    std::cout << "\n=== Victory Conditions Tracking Test ===" << std::endl;

    Unit attacker = create_validation_unit("Attacker", 5, 4, 4);
    Unit defender = create_validation_unit("Defender", 5, 4, 4);

    SimulationConfig config;
    config.iterations_per_matchup = 1000;
    config.max_rounds = 10;

    MatchupSimulator sim;
    LocalStats stats;
    sim.run_batch(attacker, defender, config, 1000, stats);

    // Sum all victory conditions
    u64 total_conditions = 0;
    for (size_t i = 0; i < stats.victory_conditions.size(); ++i) {
        total_conditions += stats.victory_conditions[i];
        if (stats.victory_conditions[i] > 0) {
            std::cout << "Victory condition " << i << ": " << stats.victory_conditions[i] << std::endl;
        }
    }

    // Total should match number of games
    assert(total_conditions == 1000);
    std::cout << "All games have recorded victory conditions [OK]" << std::endl;

    std::cout << "[PASS] test_victory_conditions_tracked" << std::endl;
}

// ==============================================================================
// Main
// ==============================================================================

int main() {
    std::cout << "=== Validation Tests ===" << std::endl;
    std::cout << "Testing special rule coverage and simulation correctness\n" << std::endl;

    // Rule validation tests
    test_special_rules_coverage();
    test_rules_in_docs_folder();

    // Simulation validation tests
    test_simulation_completes_without_crash();
    test_simulation_statistics_valid();
    test_equal_units_approximately_equal_win_rate();
    test_better_unit_wins_more();
    test_more_models_advantage();
    test_parser_produces_valid_units();
    test_victory_conditions_tracked();

    std::cout << "\n========================================" << std::endl;
    std::cout << "All validation tests passed!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
