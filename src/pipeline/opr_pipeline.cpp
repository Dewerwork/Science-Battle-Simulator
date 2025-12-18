/**
 * OPR Pipeline - C++ Implementation
 *
 * Port of run_opr_pipeline_all_units_v3_mt.py
 */

#include "pipeline/opr_pipeline.hpp"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <future>
#include <set>

namespace battle::pipeline {

// ==============================================================================
// JSON Implementation
// ==============================================================================

void JsonValue::skip_whitespace(const std::string& json, size_t& pos) {
    while (pos < json.size() && std::isspace(json[pos])) {
        ++pos;
    }
}

std::string JsonValue::parse_string_token(const std::string& json, size_t& pos) {
    if (pos >= json.size() || json[pos] != '"') return "";
    ++pos;  // Skip opening quote

    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            switch (json[pos]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                default: result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        ++pos;
    }
    if (pos < json.size()) ++pos;  // Skip closing quote
    return result;
}

JsonValue JsonValue::parse_impl(const std::string& json, size_t& pos) {
    skip_whitespace(json, pos);
    if (pos >= json.size()) return JsonValue();

    char c = json[pos];

    // Null
    if (c == 'n' && json.substr(pos, 4) == "null") {
        pos += 4;
        return JsonValue();
    }

    // Boolean
    if (c == 't' && json.substr(pos, 4) == "true") {
        pos += 4;
        return JsonValue(true);
    }
    if (c == 'f' && json.substr(pos, 5) == "false") {
        pos += 5;
        return JsonValue(false);
    }

    // String
    if (c == '"') {
        return JsonValue(parse_string_token(json, pos));
    }

    // Number
    if (c == '-' || std::isdigit(c)) {
        size_t start = pos;
        if (json[pos] == '-') ++pos;
        while (pos < json.size() && std::isdigit(json[pos])) ++pos;
        if (pos < json.size() && json[pos] == '.') {
            ++pos;
            while (pos < json.size() && std::isdigit(json[pos])) ++pos;
        }
        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            ++pos;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) ++pos;
            while (pos < json.size() && std::isdigit(json[pos])) ++pos;
        }
        return JsonValue(std::stod(json.substr(start, pos - start)));
    }

    // Array
    if (c == '[') {
        ++pos;
        std::vector<JsonValue> arr;
        skip_whitespace(json, pos);
        while (pos < json.size() && json[pos] != ']') {
            arr.push_back(parse_impl(json, pos));
            skip_whitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') ++pos;
            skip_whitespace(json, pos);
        }
        if (pos < json.size()) ++pos;  // Skip ]
        return JsonValue(std::move(arr));
    }

    // Object
    if (c == '{') {
        ++pos;
        std::map<std::string, JsonValue> obj;
        skip_whitespace(json, pos);
        while (pos < json.size() && json[pos] != '}') {
            skip_whitespace(json, pos);
            std::string key = parse_string_token(json, pos);
            skip_whitespace(json, pos);
            if (pos < json.size() && json[pos] == ':') ++pos;
            obj[key] = parse_impl(json, pos);
            skip_whitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') ++pos;
            skip_whitespace(json, pos);
        }
        if (pos < json.size()) ++pos;  // Skip }
        return JsonValue(std::move(obj));
    }

    return JsonValue();
}

JsonValue JsonValue::parse(const std::string& json) {
    size_t pos = 0;
    return parse_impl(json, pos);
}

std::string JsonValue::dump_impl(int indent, int current_indent) const {
    std::string indent_str(current_indent, ' ');
    std::string next_indent_str(current_indent + indent, ' ');

    switch (type_) {
        case Type::Null:
            return "null";
        case Type::Bool:
            return bool_val_ ? "true" : "false";
        case Type::Number: {
            if (num_val_ == static_cast<int64_t>(num_val_)) {
                return std::to_string(static_cast<int64_t>(num_val_));
            }
            std::ostringstream oss;
            oss << std::setprecision(15) << num_val_;
            return oss.str();
        }
        case Type::String: {
            std::string result = "\"";
            for (char c : str_val_) {
                switch (c) {
                    case '"': result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\n': result += "\\n"; break;
                    case '\t': result += "\\t"; break;
                    case '\r': result += "\\r"; break;
                    default: result += c; break;
                }
            }
            result += "\"";
            return result;
        }
        case Type::Array: {
            if (arr_val_.empty()) return "[]";
            std::string result = "[\n";
            for (size_t i = 0; i < arr_val_.size(); ++i) {
                result += next_indent_str + arr_val_[i].dump_impl(indent, current_indent + indent);
                if (i + 1 < arr_val_.size()) result += ",";
                result += "\n";
            }
            result += indent_str + "]";
            return result;
        }
        case Type::Object: {
            if (obj_val_.empty()) return "{}";
            std::string result = "{\n";
            size_t i = 0;
            for (const auto& [key, val] : obj_val_) {
                result += next_indent_str + "\"" + key + "\": " + val.dump_impl(indent, current_indent + indent);
                if (++i < obj_val_.size()) result += ",";
                result += "\n";
            }
            result += indent_str + "}";
            return result;
        }
    }
    return "null";
}

std::string JsonValue::dump(int indent) const {
    return dump_impl(indent, 0);
}

// ==============================================================================
// OprPipeline Constructor
// ==============================================================================

OprPipeline::OprPipeline(const PipelineConfig& config)
    : config_(config), pool_(get_thread_pool()) {}

// ==============================================================================
// Load Units from JSON
// ==============================================================================

std::vector<UnitData> OprPipeline::load_units_from_json(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Cannot open JSON file: " << json_path << "\n";
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    JsonValue data = JsonValue::parse(content);
    if (!data.contains("units")) {
        std::cerr << "[ERROR] JSON file does not contain 'units' array\n";
        return {};
    }

    std::vector<UnitData> units;
    const auto& units_array = data["units"].as_array();

    for (const auto& u : units_array) {
        UnitData unit;
        unit.name = u.get<std::string>("name", "");
        unit.size = static_cast<int>(u.get<int64_t>("size", 1));
        unit.base_points = static_cast<int>(u.get<int64_t>("base_points", 0));

        if (u.contains("quality") && !u["quality"].is_null()) {
            unit.quality = static_cast<int>(u["quality"].as_int());
        }
        if (u.contains("defense") && !u["defense"].is_null()) {
            unit.defense = static_cast<int>(u["defense"].as_int());
        }
        if (u.contains("tough") && !u["tough"].is_null()) {
            unit.tough = static_cast<int>(u["tough"].as_int());
        }

        // Special rules
        if (u.contains("special_rules") && u["special_rules"].is_array()) {
            for (const auto& rule : u["special_rules"].as_array()) {
                if (rule.is_string()) {
                    unit.special_rules.push_back(rule.as_string());
                }
            }
        }

        // Weapons
        if (u.contains("weapons") && u["weapons"].is_array()) {
            for (const auto& w : u["weapons"].as_array()) {
                WeaponData weapon;
                weapon.count = static_cast<int>(w.get<int64_t>("count", 1));
                weapon.name = w.get<std::string>("name", "");
                weapon.attacks = static_cast<int>(w.get<int64_t>("attacks", 0));

                // Range: can be int or null
                if (w.contains("range") && !w["range"].is_null()) {
                    int rng = static_cast<int>(w["range"].as_int());
                    weapon.range = std::to_string(rng) + "\"";
                } else {
                    weapon.range = "-";
                }

                // AP
                if (w.contains("ap") && !w["ap"].is_null()) {
                    weapon.ap = static_cast<int>(w["ap"].as_int());
                }

                // Special rules
                if (w.contains("special_rules") && w["special_rules"].is_array()) {
                    for (const auto& rule : w["special_rules"].as_array()) {
                        if (rule.is_string()) {
                            weapon.special_rules.push_back(rule.as_string());
                        }
                    }
                }

                unit.weapons.push_back(std::move(weapon));
            }
        }

        // Upgrade groups
        if (u.contains("upgrade_groups") && u["upgrade_groups"].is_array()) {
            for (const auto& ug : u["upgrade_groups"].as_array()) {
                UpgradeGroup group;
                group.header = ug.get<std::string>("header", "");

                if (ug.contains("options") && ug["options"].is_array()) {
                    for (const auto& opt : ug["options"].as_array()) {
                        UpgradeOption option;
                        option.text = opt.get<std::string>("text", "");
                        option.pts = static_cast<int>(opt.get<int64_t>("cost", 0));
                        group.options.push_back(std::move(option));
                    }
                }

                unit.options.push_back(std::move(group));
            }
        }

        units.push_back(std::move(unit));
    }

    return units;
}

// ==============================================================================
// Build Base Weapon Multiset
// ==============================================================================

std::tuple<std::map<std::string, int>, std::map<std::string, std::string>>
OprPipeline::build_base_weapon_multiset(const UnitData& unit) {
    std::map<std::string, int> base;
    std::map<std::string, std::string> name_to_key;

    for (const auto& w : unit.weapons) {
        std::string key = w.to_key();
        base[key] += w.count;

        std::string normalized = normalize_name(w.name);
        if (name_to_key.find(normalized) == name_to_key.end()) {
            name_to_key[normalized] = key;
        }
    }

    return {base, name_to_key};
}

// ==============================================================================
// Rules Processing
// ==============================================================================

std::vector<std::string> OprPipeline::split_rules(const std::string& text) {
    std::vector<std::string> result;
    std::string cleaned = text;

    // Replace " ," with ","
    size_t pos;
    while ((pos = cleaned.find(" ,")) != std::string::npos) {
        cleaned.replace(pos, 2, ",");
    }

    // Trim and remove trailing commas
    cleaned = normalize_whitespace(cleaned);
    while (!cleaned.empty() && cleaned.back() == ',') cleaned.pop_back();
    while (!cleaned.empty() && cleaned.front() == ',') cleaned.erase(0, 1);

    // Split by comma, respecting parentheses
    int depth = 0;
    std::string current;
    for (char c : cleaned) {
        if (c == '(') depth++;
        else if (c == ')') depth = std::max(0, depth - 1);

        if (c == ',' && depth == 0) {
            std::string trimmed = normalize_whitespace(current);
            if (!trimmed.empty()) result.push_back(trimmed);
            current.clear();
        } else {
            current += c;
        }
    }
    std::string trimmed = normalize_whitespace(current);
    if (!trimmed.empty()) result.push_back(trimmed);

    return result;
}

std::optional<std::string> OprPipeline::clean_rule(const std::string& rule) {
    std::string r = normalize_whitespace(rule);
    if (r.empty()) return std::nullopt;

    // Filter cost modifiers (+20pts, +55pts)
    static const std::regex cost_re(R"(^\+\d+pts$)", std::regex::icase);
    if (std::regex_match(r, cost_re)) return std::nullopt;

    // Filter weapon profiles in rules
    static const std::regex weapon_profile_re(R"(\(\s*\d+"\s*,\s*A\d+)");
    if (std::regex_search(r, weapon_profile_re)) return std::nullopt;

    // Filter standalone (A1) artifacts
    static const std::regex atk_artifact_re(R"(^\(A\d+\)$)");
    if (std::regex_match(r, atk_artifact_re)) return std::nullopt;

    // Filter instruction keywords
    std::string r_lower = r;
    std::transform(r_lower.begin(), r_lower.end(), r_lower.begin(), ::tolower);

    static const std::vector<std::string> instruction_keywords = {
        "upgrade", "replace", "any model", "one model", "all models"
    };
    for (const auto& kw : instruction_keywords) {
        if (r_lower == kw || r_lower.substr(0, kw.size() + 1) == kw + " ") {
            return std::nullopt;
        }
    }

    // Fix unbalanced parentheses
    int open_count = std::count(r.begin(), r.end(), '(');
    int close_count = std::count(r.begin(), r.end(), ')');
    while (close_count > open_count && !r.empty() && r.back() == ')') {
        r.pop_back();
        --close_count;
    }
    while (open_count > close_count && !r.empty() && r.front() == '(') {
        r.erase(0, 1);
        --open_count;
    }

    r = normalize_whitespace(r);
    return r.empty() ? std::nullopt : std::optional<std::string>(r);
}

std::vector<std::string> OprPipeline::normalize_rules(const std::vector<std::string>& rules_in) {
    std::vector<std::string> out;
    std::set<std::string> seen_lower;

    for (const auto& r : rules_in) {
        auto cleaned = clean_rule(r);
        if (cleaned) {
            std::string key = *cleaned;
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            if (seen_lower.find(key) == seen_lower.end()) {
                seen_lower.insert(key);
                out.push_back(*cleaned);
            }
        }
    }

    std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
        std::string la = a, lb = b;
        std::transform(la.begin(), la.end(), la.begin(), ::tolower);
        std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
        return la < lb;
    });

    return out;
}

// ==============================================================================
// Variant Generation
// ==============================================================================

namespace {

bool looks_like_weapon_profile(const std::string& inside) {
    if (inside.empty()) return false;
    static const std::regex attacks_re(R"(\bA\d+\b)");
    static const std::regex ap_re(R"(\bAP\(\s*-?\d+\s*\))");
    return std::regex_search(inside, attacks_re) || std::regex_search(inside, ap_re);
}

std::pair<std::string, std::string> split_name_and_parens(const std::string& text) {
    std::string t = OprPipeline::normalize_whitespace(text);
    static const std::regex re(R"(^(.+?)\s*\((.+)\)\s*$)");
    std::smatch match;
    if (std::regex_match(t, match, re)) {
        return {match[1].str(), match[2].str()};
    }
    return {t, ""};
}

std::pair<int, std::string> parse_count_prefix(const std::string& text) {
    std::string t = OprPipeline::normalize_whitespace(text);
    static const std::regex re(R"(^(\d+)\s*[x×]\s*(.+)$)", std::regex::icase);
    std::smatch match;
    if (std::regex_match(t, match, re)) {
        return {std::stoi(match[1].str()), match[2].str()};
    }
    return {1, t};
}

std::vector<std::string> extract_rules_from_choice(const std::string& choice_text) {
    auto [name_part, inside] = split_name_and_parens(choice_text);
    if (!inside.empty()) {
        if (!looks_like_weapon_profile(inside)) {
            return OprPipeline::split_rules(inside);
        }
        return {};
    }
    std::string t = OprPipeline::normalize_whitespace(name_part);
    return t.empty() ? std::vector<std::string>{} : std::vector<std::string>{t};
}

int guess_upgrade_multiplier(const std::string& header, int unit_size) {
    std::string h = header;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    if (h.find("all models") != std::string::npos) return unit_size;
    return 1;
}

std::optional<int> header_pick_limit(const std::string& header) {
    std::string h = header;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);

    static const std::map<std::string, int> word_to_int = {
        {"one", 1}, {"two", 2}, {"three", 3}, {"four", 4}, {"five", 5},
        {"six", 6}, {"seven", 7}, {"eight", 8}, {"nine", 9}, {"ten", 10}
    };

    // Check "up to N" or "upto N"
    static const std::regex upto_num_re(R"(\b(?:up to|upto)\s+(\d+)\b)");
    std::smatch match;
    if (std::regex_search(h, match, upto_num_re)) {
        return std::stoi(match[1].str());
    }

    static const std::regex upto_word_re(R"(\b(?:up to|upto)\s+(one|two|three|four|five|six|seven|eight|nine|ten)\b)");
    if (std::regex_search(h, match, upto_word_re)) {
        auto it = word_to_int.find(match[1].str());
        if (it != word_to_int.end()) return it->second;
    }

    // Check "with N"
    static const std::regex with_num_re(R"(\bwith\s+(\d+)\b)");
    if (std::regex_search(h, match, with_num_re)) {
        return std::stoi(match[1].str());
    }

    static const std::regex with_word_re(R"(\bwith\s+(one|two|three|four|five|six|seven|eight|nine|ten)\b)");
    if (std::regex_search(h, match, with_word_re)) {
        auto it = word_to_int.find(match[1].str());
        if (it != word_to_int.end()) return it->second;
    }

    return std::nullopt;
}

int weapon_occurrences(const UnitData& unit, const std::string& weapon_name) {
    std::string wn = OprPipeline::normalize_name(weapon_name);
    int occ = 0;
    for (const auto& w : unit.weapons) {
        if (OprPipeline::normalize_name(w.name) == wn) {
            occ += w.count;
        }
    }
    return occ;
}

} // anonymous namespace

std::vector<Variant> OprPipeline::generate_group_variants(
    const UnitData& unit,
    const UpgradeGroup& group,
    const std::map<std::string, std::string>& name_to_key)
{
    std::vector<Variant> out;
    std::string h = group.header;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);

    bool is_upgrade = h.find("upgrade") == 0;
    bool is_replace = (h.find("replace") == 0) ||
                      (h.find("any model may replace") != std::string::npos) ||
                      (h.find(" replace ") != std::string::npos);

    int multiplier = guess_upgrade_multiplier(group.header, unit.size);

    auto make_variant = [](int pts, const std::vector<std::string>& rules,
                           const std::map<std::string, int>& wpn_delta) -> Variant {
        Variant v;
        v.pts_delta = pts;
        v.add_rules = rules;
        for (const auto& [k, delta] : wpn_delta) {
            if (delta != 0) v.weapon_delta[k] = delta;
        }
        return v;
    };

    // UPGRADE groups
    if (is_upgrade && h.find("replace") == std::string::npos) {
        auto max_pick = header_pick_limit(group.header);

        if (!max_pick) {
            // Any subset
            size_t n = group.options.size();
            for (size_t r = 0; r <= n; ++r) {
                std::vector<bool> selector(n, false);
                std::fill(selector.begin(), selector.begin() + r, true);

                do {
                    std::vector<std::string> add_rules;
                    int pts = 0;
                    for (size_t i = 0; i < n; ++i) {
                        if (selector[i]) {
                            pts += group.options[i].pts * multiplier;
                            auto rules = extract_rules_from_choice(group.options[i].text);
                            add_rules.insert(add_rules.end(), rules.begin(), rules.end());
                        }
                    }
                    out.push_back(make_variant(pts, add_rules, {}));
                } while (std::prev_permutation(selector.begin(), selector.end()));
            }
            return out;
        }

        // Up to max_pick selections
        size_t n = group.options.size();
        for (size_t r = 0; r <= std::min(static_cast<size_t>(*max_pick), n); ++r) {
            std::vector<bool> selector(n, false);
            std::fill(selector.begin(), selector.begin() + r, true);

            do {
                std::vector<std::string> add_rules;
                int pts = 0;
                for (size_t i = 0; i < n; ++i) {
                    if (selector[i]) {
                        pts += group.options[i].pts * multiplier;
                        auto rules = extract_rules_from_choice(group.options[i].text);
                        add_rules.insert(add_rules.end(), rules.begin(), rules.end());
                    }
                }
                out.push_back(make_variant(pts, add_rules, {}));
            } while (std::prev_permutation(selector.begin(), selector.end()));
        }
        return out;
    }

    // REPLACE groups
    if (is_replace) {
        std::string mode = "bundle";
        int slots = 1;
        std::string target_name = group.header;

        // Parse replace patterns
        std::smatch match;

        static const std::regex any_model_re(R"(^any model may replace\s+(.+)$)", std::regex::icase);
        if (std::regex_match(h, match, any_model_re)) {
            mode = "per_slot";
            slots = unit.size;
            target_name = match[1].str();
        }

        static const std::regex replace_all_re(R"(^replace all\s+(.+)$)", std::regex::icase);
        if (std::regex_match(h, match, replace_all_re)) {
            mode = "bundle_all";
            target_name = match[1].str();
            slots = std::max(1, weapon_occurrences(unit, target_name));
        }

        static const std::regex replace_any_re(R"(^replace any\s+(.+)$)", std::regex::icase);
        if (std::regex_match(h, match, replace_any_re)) {
            mode = "per_slot";
            target_name = match[1].str();
            slots = std::max(1, weapon_occurrences(unit, target_name));
        }

        static const std::regex replace_one_re(R"(^replace one\s+(.+)$)", std::regex::icase);
        if (std::regex_match(h, match, replace_one_re)) {
            mode = "bundle";
            target_name = match[1].str();
            slots = 1;
        }

        static const std::regex replace_upto_re(R"(^replace\s+(?:up to|upto)\s+(\d+|one|two|three|four|five)\s+(.+)$)", std::regex::icase);
        if (std::regex_match(h, match, replace_upto_re)) {
            static const std::map<std::string, int> word_to_int = {
                {"one", 1}, {"two", 2}, {"three", 3}, {"four", 4}, {"five", 5}
            };
            std::string nraw = match[1].str();
            int n = 2;
            if (std::isdigit(nraw[0])) {
                n = std::stoi(nraw);
            } else {
                auto it = word_to_int.find(nraw);
                if (it != word_to_int.end()) n = it->second;
            }
            target_name = match[2].str();
            mode = "per_slot";
            slots = n;
        }

        static const std::regex replace_nx_re(R"(^replace\s+(\d+)\s*[x×]\s*(.+)$)", std::regex::icase);
        if (std::regex_match(h, match, replace_nx_re)) {
            mode = "bundle";
            slots = std::stoi(match[1].str());
            target_name = match[2].str();
        }

        static const std::regex replace_simple_re(R"(^replace\s+(.+)$)", std::regex::icase);
        if (target_name == group.header && std::regex_match(h, match, replace_simple_re)) {
            target_name = match[1].str();
            int occ = weapon_occurrences(unit, target_name);
            slots = std::max(1, occ);
            mode = "bundle";
        }

        std::string target_key;
        std::string target_normalized = normalize_name(target_name);
        auto it = name_to_key.find(target_normalized);
        if (it != name_to_key.end()) {
            target_key = it->second;
        }

        // Always add "no change" variant
        out.push_back(make_variant(0, {}, {}));

        if (group.options.empty()) return out;

        // Bundle modes
        if (mode == "bundle" || mode == "bundle_all") {
            for (const auto& opt : group.options) {
                auto [name_part, inside] = split_name_and_parens(opt.text);
                auto [c, item_name] = parse_count_prefix(name_part);

                std::map<std::string, int> weapon_delta;
                if (!target_key.empty()) {
                    weapon_delta[target_key] -= slots;
                }

                // Build add key
                std::string add_key;
                if (!inside.empty() && looks_like_weapon_profile(inside)) {
                    // Parse profile to build key
                    std::string normalized_name = normalize_whitespace(item_name);
                    std::transform(normalized_name.begin(), normalized_name.end(), normalized_name.begin(), ::tolower);
                    add_key = "N=" + normalized_name + "|R=|A=0|AP=";
                    // TODO: Parse actual profile values
                } else {
                    std::string normalized_name = normalize_whitespace(item_name);
                    std::transform(normalized_name.begin(), normalized_name.end(), normalized_name.begin(), ::tolower);
                    add_key = "N=" + normalized_name + "|R=|A=0|AP=";
                }

                if (add_key == target_key) continue;  // Self-replacement

                weapon_delta[add_key] += c;

                auto add_rules = extract_rules_from_choice(opt.text);
                out.push_back(make_variant(opt.pts, add_rules, weapon_delta));
            }
            return out;
        }

        // Per-slot mode (combinations with replacement)
        if (mode == "per_slot") {
            std::vector<const UpgradeOption*> choices;
            choices.push_back(nullptr);  // None option
            for (const auto& opt : group.options) {
                choices.push_back(&opt);
            }

            // Generate combinations_with_replacement
            std::function<void(int, std::vector<const UpgradeOption*>&)> generate;
            generate = [&](int start_idx, std::vector<const UpgradeOption*>& current) {
                if (static_cast<int>(current.size()) == slots) {
                    int pts_delta = 0;
                    std::map<std::string, int> weapon_delta;
                    std::vector<std::string> add_rules;

                    for (const auto* pick : current) {
                        if (!pick) continue;

                        pts_delta += pick->pts;
                        auto [name_part, inside] = split_name_and_parens(pick->text);
                        auto [c, item_name] = parse_count_prefix(name_part);

                        if (!target_key.empty()) {
                            weapon_delta[target_key] -= 1;
                        }

                        std::string normalized_name = normalize_whitespace(item_name);
                        std::transform(normalized_name.begin(), normalized_name.end(), normalized_name.begin(), ::tolower);
                        std::string add_key = "N=" + normalized_name + "|R=|A=0|AP=";
                        weapon_delta[add_key] += c;

                        auto rules = extract_rules_from_choice(pick->text);
                        add_rules.insert(add_rules.end(), rules.begin(), rules.end());
                    }

                    // Check for net change
                    bool has_change = false;
                    for (const auto& [k, v] : weapon_delta) {
                        if (v != 0) { has_change = true; break; }
                    }
                    if (has_change || pts_delta != 0 || !add_rules.empty()) {
                        out.push_back(make_variant(pts_delta, add_rules, weapon_delta));
                    }
                    return;
                }

                for (int i = start_idx; i < static_cast<int>(choices.size()); ++i) {
                    current.push_back(choices[i]);
                    generate(i, current);
                    current.pop_back();
                }
            };

            std::vector<const UpgradeOption*> current;
            generate(0, current);
            return out;
        }
    }

    // Fallback: treat each option as a rule-only choice
    out.push_back(make_variant(0, {}, {}));
    for (const auto& opt : group.options) {
        auto rules = extract_rules_from_choice(opt.text);
        out.push_back(make_variant(opt.pts, rules, {}));
    }

    return out;
}

// ==============================================================================
// Stage-1 Signature Building
// ==============================================================================

std::string OprPipeline::build_stage1_signature(int points, const std::vector<std::string>& rules,
                                                 const std::map<std::string, int>& weapons) {
    std::vector<std::pair<std::string, int>> items;
    for (const auto& [k, c] : weapons) {
        if (c > 0) items.push_back({k, c});
    }
    std::sort(items.begin(), items.end());

    std::string weapons_key;
    for (const auto& [k, c] : items) {
        if (!weapons_key.empty()) weapons_key += "|";
        weapons_key += k + "*" + std::to_string(c);
    }

    std::string rules_str;
    for (const auto& r : rules) {
        if (!rules_str.empty()) rules_str += ",";
        rules_str += r;
    }

    std::vector<std::string> parts;
    if (config_.include_points_in_stage1_signature) {
        parts.push_back("PTS=" + std::to_string(points));
    }
    parts.push_back("RULES=" + rules_str);
    parts.push_back("W=" + weapons_key);

    std::string sig;
    for (const auto& p : parts) {
        if (!sig.empty()) sig += "||";
        sig += p;
    }
    return sig;
}

std::string OprPipeline::build_header(const UnitData& unit, int points, const std::vector<std::string>& rules) {
    std::string rules_str;
    for (const auto& r : rules) {
        if (!rules_str.empty()) rules_str += ", ";
        rules_str += r;
    }

    std::ostringstream oss;
    oss << unit.name << " [" << unit.size << "] Q"
        << (unit.quality ? std::to_string(*unit.quality) : "?") << "+ D"
        << (unit.defense ? std::to_string(*unit.defense) : "?") << "+ | "
        << points << "pts | " << rules_str;
    return oss.str();
}

// ==============================================================================
// UID Generation
// ==============================================================================

std::string OprPipeline::generate_uid(const std::string& unit_name, size_t combo_idx,
                                       const std::string& signature) {
    std::string input = unit_name + "|" + std::to_string(combo_idx) + "|" + signature;
    std::string hash = sha1_hex(input);
    std::string uid = hash.substr(0, 8);
    std::transform(uid.begin(), uid.end(), uid.begin(), ::toupper);
    return uid;
}

// ==============================================================================
// Raw Loadout Building
// ==============================================================================

RawLoadout OprPipeline::build_raw_loadout(
    const UnitData& unit, size_t combo_idx,
    const std::vector<std::vector<Variant>>& group_variants,
    int base_pts, const std::vector<std::string>& base_rules,
    const std::map<std::string, int>& base_weapons)
{
    std::vector<size_t> radices;
    for (const auto& gv : group_variants) {
        radices.push_back(gv.size());
    }

    auto choice_indices = index_to_choice_indices(combo_idx, radices);

    int pts_delta = 0;
    std::vector<std::string> add_rules;
    std::map<std::string, int> weapon_delta;

    for (size_t g_i = 0; g_i < choice_indices.size(); ++g_i) {
        const auto& v = group_variants[g_i][choice_indices[g_i]];
        pts_delta += v.pts_delta;
        add_rules.insert(add_rules.end(), v.add_rules.begin(), v.add_rules.end());
        for (const auto& [k, dv] : v.weapon_delta) {
            weapon_delta[k] += dv;
        }
    }

    int points = base_pts + pts_delta;

    std::vector<std::string> all_rules = base_rules;
    all_rules.insert(all_rules.end(), add_rules.begin(), add_rules.end());
    auto rules_sig = normalize_rules(all_rules);

    std::map<std::string, int> weapons = base_weapons;
    for (const auto& [k, dv] : weapon_delta) {
        weapons[k] += dv;
        if (weapons[k] < 0) weapons[k] = 0;
    }

    // Convert weapon keys back to WeaponData
    std::vector<WeaponData> weapon_list;
    for (const auto& [wkey, count] : weapons) {
        if (count <= 0) continue;

        WeaponData wd;
        wd.count = count;

        // Parse key: N=name|R=range|A=attacks|AP=ap|T=tags
        std::map<std::string, std::string> parts;
        size_t pos = 0;
        while (pos < wkey.size()) {
            size_t eq = wkey.find('=', pos);
            if (eq == std::string::npos) break;
            size_t pipe = wkey.find('|', eq);
            if (pipe == std::string::npos) pipe = wkey.size();
            std::string key = wkey.substr(pos, eq - pos);
            std::string val = wkey.substr(eq + 1, pipe - eq - 1);
            parts[key] = val;
            pos = pipe + 1;
        }

        wd.name = parts["N"];
        if (!parts["R"].empty()) {
            wd.range = parts["R"] + "\"";
        } else {
            wd.range = "-";
        }
        if (!parts["A"].empty()) {
            wd.attacks = std::stoi(parts["A"]);
        }
        if (!parts["AP"].empty()) {
            wd.ap = std::stoi(parts["AP"]);
        }
        if (!parts["T"].empty()) {
            // Split by semicolon
            std::string t = parts["T"];
            size_t p = 0;
            while (p < t.size()) {
                size_t semi = t.find(';', p);
                if (semi == std::string::npos) semi = t.size();
                std::string tag = t.substr(p, semi - p);
                if (!tag.empty()) wd.special_rules.push_back(tag);
                p = semi + 1;
            }
        }

        weapon_list.push_back(std::move(wd));
    }

    // Sort weapons (melee first)
    std::sort(weapon_list.begin(), weapon_list.end(), [](const WeaponData& a, const WeaponData& b) {
        bool a_melee = (a.range == "-");
        bool b_melee = (b.range == "-");
        if (a_melee != b_melee) return a_melee;
        return a.range < b.range;
    });

    std::string sig = build_stage1_signature(points, rules_sig, weapons);
    std::string uid = generate_uid(unit.name, combo_idx, sig);

    RawLoadout loadout;
    loadout.uid = uid;
    loadout.combo_index = combo_idx;
    loadout.unit_name = unit.name;
    loadout.points = points;
    loadout.quality = unit.quality.value_or(0);
    loadout.defense = unit.defense.value_or(0);
    loadout.size = unit.size;
    loadout.tough = unit.tough;
    loadout.rules = rules_sig;
    loadout.weapons = std::move(weapon_list);
    loadout.signature = sig;

    return loadout;
}

// ==============================================================================
// Process Unit
// ==============================================================================

UnitPipelineResult OprPipeline::process_unit(const UnitData& unit) {
    UnitPipelineResult result;
    result.unit_name = unit.name;

    auto [base_weapons, name_to_key] = build_base_weapon_multiset(unit);
    int base_pts = unit.base_points;
    std::vector<std::string> base_rules = unit.special_rules;

    // Generate variants for each option group
    std::vector<std::vector<Variant>> group_variants;
    for (const auto& group : unit.options) {
        group_variants.push_back(generate_group_variants(unit, group, name_to_key));
    }

    std::vector<size_t> radices;
    for (const auto& gv : group_variants) {
        radices.push_back(gv.empty() ? 1 : gv.size());
    }

    size_t total = total_combinations(radices);
    if (config_.max_loadouts_per_unit > 0 && total > config_.max_loadouts_per_unit) {
        total = config_.max_loadouts_per_unit;
    }
    result.total_combinations = total;

    if (config_.raw_loadout_mode) {
        // Raw loadout mode - parallel processing
        result.raw_loadouts.resize(total);

        size_t chunk_size = std::max(size_t(1), total / config_.tasks_per_unit);
        std::vector<std::future<void>> futures;

        for (size_t start = 0; start < total; start += chunk_size) {
            size_t end = std::min(start + chunk_size, total);
            futures.push_back(pool_.submit([this, &unit, &group_variants, base_pts, &base_rules,
                                            &base_weapons, &result, start, end]() {
                for (size_t i = start; i < end; ++i) {
                    result.raw_loadouts[i] = build_raw_loadout(
                        unit, i, group_variants, base_pts, base_rules, base_weapons);
                }
            }));
        }

        for (auto& f : futures) {
            f.get();
        }

        result.total_groups = result.raw_loadouts.size();
    } else {
        // Grouped mode - Stage-1 then Stage-2
        std::map<std::string, Stage1Group> sig_map;
        std::mutex mutex;

        size_t chunk_size = std::max(size_t(1), total / config_.tasks_per_unit);
        std::vector<std::future<void>> futures;

        for (size_t start = 0; start < total; start += chunk_size) {
            size_t end = std::min(start + chunk_size, total);
            futures.push_back(pool_.submit([this, &unit, &group_variants, base_pts, &base_rules,
                                            &base_weapons, &sig_map, &mutex, start, end]() {
                std::map<std::string, Stage1Group> local_map;

                for (size_t combo_idx = start; combo_idx < end; ++combo_idx) {
                    auto loadout = build_raw_loadout(unit, combo_idx, group_variants,
                                                      base_pts, base_rules, base_weapons);

                    auto it = local_map.find(loadout.signature);
                    if (it == local_map.end()) {
                        Stage1Group g;
                        g.group_id = sha1_hex(loadout.signature).substr(0, 10);
                        g.signature = loadout.signature;
                        g.unit_name = unit.name;
                        g.points = loadout.points;
                        g.count = 1;
                        g.rep_combo_index = combo_idx;
                        g.rep_header = build_header(unit, loadout.points, loadout.rules);
                        local_map[loadout.signature] = std::move(g);
                    } else {
                        it->second.count++;
                        if (combo_idx < it->second.rep_combo_index) {
                            it->second.rep_combo_index = combo_idx;
                            it->second.rep_header = build_header(unit, loadout.points, loadout.rules);
                        }
                    }
                }

                // Merge into global map
                std::lock_guard<std::mutex> lock(mutex);
                for (auto& [sig, local_g] : local_map) {
                    auto it = sig_map.find(sig);
                    if (it == sig_map.end()) {
                        sig_map[sig] = std::move(local_g);
                    } else {
                        it->second.count += local_g.count;
                        if (local_g.rep_combo_index < it->second.rep_combo_index) {
                            it->second.rep_combo_index = local_g.rep_combo_index;
                            it->second.rep_header = local_g.rep_header;
                        }
                    }
                }
            }));
        }

        for (auto& f : futures) {
            f.get();
        }

        // Convert to vector
        for (auto& [sig, g] : sig_map) {
            result.stage1_groups.push_back(std::move(g));
        }
        result.total_groups = result.stage1_groups.size();

        // Stage-2 reduction
        result.supergroups = stage2_reduce(result.stage1_groups, unit);
        result.total_supergroups = result.supergroups.size();
    }

    if (progress_callback_) {
        progress_callback_(unit.name, total, result.total_groups);
    }

    return result;
}

// ==============================================================================
// Stage-2 Reduction
// ==============================================================================

std::vector<Stage2SuperGroup> OprPipeline::stage2_reduce(
    const std::vector<Stage1Group>& stage1_groups,
    const UnitData& unit)
{
    std::map<std::string, std::vector<const Stage1Group*>> super_map;

    for (const auto& g : stage1_groups) {
        std::string wcond = condensed_weapons_key(g.signature);

        std::string super_sig;
        if (config_.rule_agnostic_grouping) {
            super_sig = "W=" + wcond;
        } else {
            // Extract rules from signature
            size_t rules_pos = g.signature.find("RULES=");
            size_t rules_end = g.signature.find("||", rules_pos);
            std::string rules;
            if (rules_pos != std::string::npos) {
                rules = g.signature.substr(rules_pos + 6,
                    rules_end == std::string::npos ? std::string::npos : rules_end - rules_pos - 6);
            }
            super_sig = "RULES=" + rules + "||W=" + wcond;
        }

        super_map[super_sig].push_back(&g);
    }

    std::vector<Stage2SuperGroup> result;
    int idx = 1;

    for (auto& [super_sig, children] : super_map) {
        Stage2SuperGroup sg;
        sg.sg_id = "SG" + std::to_string(idx++);
        sg.supergroup_hash = sha1_hex(super_sig).substr(0, 10);
        sg.signature = super_sig;
        sg.unit_name = unit.name;
        sg.count_child_groups = children.size();

        size_t members_total = 0;
        int pts_min = INT_MAX, pts_max = INT_MIN;
        std::set<std::string> rules_vars;

        const Stage1Group* rep = children[0];
        for (const auto* g : children) {
            members_total += g->count;
            pts_min = std::min(pts_min, g->points);
            pts_max = std::max(pts_max, g->points);
            sg.child_group_ids.push_back(g->group_id);

            // Collect rules variations
            size_t rules_pos = g->signature.find("RULES=");
            size_t rules_end = g->signature.find("||", rules_pos);
            if (rules_pos != std::string::npos) {
                std::string rules = g->signature.substr(rules_pos + 6,
                    rules_end == std::string::npos ? std::string::npos : rules_end - rules_pos - 6);
                if (!rules.empty()) rules_vars.insert(rules);
            }

            if (g->rep_combo_index < rep->rep_combo_index) {
                rep = g;
            }
        }

        sg.count_members = members_total;
        sg.points_min = pts_min;
        sg.points_max = pts_max;
        sg.rules_variations = std::vector<std::string>(rules_vars.begin(), rules_vars.end());
        sg.condensed_weapons_line = condensed_weapons_line(rep->signature);

        result.push_back(std::move(sg));
    }

    return result;
}

std::string OprPipeline::condensed_weapons_key(const std::string& stage1_sig) {
    auto weapon_groups = group_weapons_by_rules(stage1_sig);

    std::vector<std::string> parts;
    for (const auto& wg : weapon_groups) {
        std::string r_str = wg.range ? std::to_string(*wg.range) : "";
        std::string ap_str = wg.ap ? std::to_string(*wg.ap) : "";
        std::string tags_str;
        for (const auto& t : wg.tags) {
            if (!tags_str.empty()) tags_str += ";";
            tags_str += t;
        }

        std::string part;
        if (config_.attack_agnostic_grouping) {
            part = "R=" + r_str + ",AP=" + ap_str + ",T=" + tags_str;
        } else {
            part = "R=" + r_str + ",AP=" + ap_str + ",T=" + tags_str +
                   ",A=" + std::to_string(wg.total_attacks) + ",C=" + std::to_string(wg.total_count);
        }
        parts.push_back(part);
    }

    std::sort(parts.begin(), parts.end());
    std::string result;
    for (const auto& p : parts) {
        if (!result.empty()) result += " | ";
        result += p;
    }
    return result;
}

std::string OprPipeline::condensed_weapons_line(const std::string& stage1_sig) {
    auto weapon_groups = group_weapons_by_rules(stage1_sig);

    std::vector<std::string> chunks;
    for (const auto& wg : weapon_groups) {
        std::string display_name;
        if (wg.source_weapons.size() == 1) {
            display_name = std::get<0>(wg.source_weapons[0]);
            if (display_name.empty()) {
                display_name = wg.range ? "Ranged" : "Melee";
            }
        } else {
            std::vector<std::string> names;
            for (const auto& [name, atk, cnt] : wg.source_weapons) {
                if (!name.empty()) names.push_back(name);
            }
            if (names.size() <= 2) {
                for (size_t i = 0; i < names.size(); ++i) {
                    if (i > 0) display_name += " + ";
                    display_name += names[i];
                }
            } else {
                display_name = wg.group_id;
            }
        }

        std::vector<std::string> inner;
        inner.push_back("A" + std::to_string(wg.total_attacks));
        if (wg.ap) {
            inner.push_back("AP(" + std::to_string(*wg.ap) + ")");
        }
        inner.insert(inner.end(), wg.tags.begin(), wg.tags.end());

        std::string inner_str;
        for (const auto& s : inner) {
            if (!inner_str.empty()) inner_str += ", ";
            inner_str += s;
        }

        std::string weapon_str;
        if (wg.range) {
            weapon_str = std::to_string(*wg.range) + "\" " + display_name + " (" + inner_str + ")";
        } else {
            weapon_str = display_name + " (" + inner_str + ")";
        }

        if (wg.total_count > 1 && wg.source_weapons.size() == 1) {
            weapon_str = std::to_string(wg.total_count) + "x " + weapon_str;
        }

        chunks.push_back(weapon_str);
    }

    std::string result;
    for (const auto& c : chunks) {
        if (!result.empty()) result += ", ";
        result += c;
    }
    return result;
}

std::vector<WeaponGroup> OprPipeline::group_weapons_by_rules(const std::string& stage1_sig) {
    // Parse W= section from signature
    size_t w_pos = stage1_sig.find("W=");
    if (w_pos == std::string::npos) return {};

    std::string w_value = stage1_sig.substr(w_pos + 2);
    size_t end_pos = w_value.find("||");
    if (end_pos != std::string::npos) {
        w_value = w_value.substr(0, end_pos);
    }

    if (w_value.empty()) return {};

    // Parse weapon items
    struct WeaponItem {
        std::string name;
        std::optional<int> range;
        std::optional<int> ap;
        std::vector<std::string> tags;
        int attacks;
        int count;
    };

    std::vector<WeaponItem> items;

    // Parse each weapon entry: N=name|R=range|A=attacks|AP=ap|T=tags*count
    size_t pos = 0;
    while (pos < w_value.size()) {
        WeaponItem item;

        // Find the count at the end (*N)
        size_t star_pos = w_value.find('*', pos);
        if (star_pos == std::string::npos) break;

        // Find the end of count
        size_t next_pipe = w_value.find('|', star_pos);
        if (next_pipe == std::string::npos) next_pipe = w_value.size();

        // The count might be followed by another weapon entry (starting with N=)
        size_t next_n = w_value.find("|N=", star_pos);
        size_t count_end = (next_n != std::string::npos && next_n < next_pipe) ? next_n : next_pipe;

        std::string count_str = w_value.substr(star_pos + 1, count_end - star_pos - 1);
        // Remove any trailing | or next weapon prefix
        size_t pipe_in_count = count_str.find('|');
        if (pipe_in_count != std::string::npos) {
            count_str = count_str.substr(0, pipe_in_count);
        }
        item.count = std::stoi(count_str);

        // Parse the fields before the *
        std::string fields = w_value.substr(pos, star_pos - pos);
        std::map<std::string, std::string> kv;

        size_t fpos = 0;
        while (fpos < fields.size()) {
            size_t eq = fields.find('=', fpos);
            if (eq == std::string::npos) break;
            size_t pipe = fields.find('|', eq);
            if (pipe == std::string::npos) pipe = fields.size();

            std::string key = fields.substr(fpos, eq - fpos);
            std::string val = fields.substr(eq + 1, pipe - eq - 1);
            kv[key] = val;
            fpos = pipe + 1;
        }

        item.name = kv["N"];
        if (!kv["R"].empty()) {
            item.range = std::stoi(kv["R"]);
        }
        if (!kv["A"].empty()) {
            item.attacks = std::stoi(kv["A"]);
        } else {
            item.attacks = 0;
        }
        if (!kv["AP"].empty()) {
            item.ap = std::stoi(kv["AP"]);
        }
        if (!kv["T"].empty()) {
            // Split by semicolon
            std::string t = kv["T"];
            size_t p = 0;
            while (p < t.size()) {
                size_t semi = t.find(';', p);
                if (semi == std::string::npos) semi = t.size();
                std::string tag = t.substr(p, semi - p);
                if (!tag.empty()) item.tags.push_back(tag);
                p = semi + 1;
            }
            std::sort(item.tags.begin(), item.tags.end());
        }

        items.push_back(std::move(item));

        // Move to next weapon
        pos = count_end;
        if (pos < w_value.size() && w_value[pos] == '|') {
            pos++;
        }
    }

    // Group by rules key
    auto rules_key = [](const WeaponItem& w) -> std::string {
        std::string r = w.range ? std::to_string(*w.range) : "";
        std::string ap = w.ap ? std::to_string(*w.ap) : "";
        std::string tags;
        for (const auto& t : w.tags) {
            if (!tags.empty()) tags += ";";
            tags += t;
        }
        return "R=" + r + "|AP=" + ap + "|T=" + tags;
    };

    std::map<std::string, std::vector<const WeaponItem*>> groups_map;
    for (const auto& item : items) {
        groups_map[rules_key(item)].push_back(&item);
    }

    std::vector<WeaponGroup> result;
    int idx = 1;
    for (auto& [key, group_items] : groups_map) {
        WeaponGroup wg;
        wg.group_id = "WG" + std::to_string(idx++);

        if (!group_items.empty()) {
            wg.range = group_items[0]->range;
            wg.ap = group_items[0]->ap;
            wg.tags = group_items[0]->tags;
        }

        int total_attacks = 0;
        int total_count = 0;
        for (const auto* item : group_items) {
            total_attacks += item->attacks * item->count;
            total_count += item->count;
            wg.source_weapons.push_back({item->name, item->attacks, item->count});
        }

        wg.total_attacks = total_attacks;
        wg.total_count = total_count;

        result.push_back(std::move(wg));
    }

    return result;
}

// ==============================================================================
// Output Writing
// ==============================================================================

void OprPipeline::write_raw_loadouts_json(const std::vector<RawLoadout>& loadouts,
                                           const std::filesystem::path& path) {
    JsonValue output;
    output["unit"] = loadouts.empty() ? "" : loadouts[0].unit_name;
    output["total_loadouts"] = static_cast<int64_t>(loadouts.size());
    output["total_combinations_processed"] = static_cast<int64_t>(loadouts.size());

    JsonValue settings;
    settings["RAW_LOADOUT_MODE"] = true;
    settings["UID_FORMAT"] = "8-char hex hash";
    output["settings"] = std::move(settings);

    std::vector<JsonValue> loadouts_array;
    for (const auto& lo : loadouts) {
        JsonValue lo_obj;
        lo_obj["uid"] = lo.uid;
        lo_obj["combo_index"] = static_cast<int64_t>(lo.combo_index);
        lo_obj["points"] = static_cast<int64_t>(lo.points);

        std::vector<JsonValue> rules_arr;
        for (const auto& r : lo.rules) {
            rules_arr.push_back(JsonValue(r));
        }
        lo_obj["rules"] = JsonValue(std::move(rules_arr));

        std::vector<JsonValue> weapons_arr;
        for (const auto& w : lo.weapons) {
            JsonValue w_obj;
            w_obj["name"] = w.name;
            w_obj["count"] = static_cast<int64_t>(w.count);
            w_obj["range"] = w.range;
            w_obj["attacks"] = static_cast<int64_t>(w.attacks);
            if (w.ap) {
                w_obj["ap"] = static_cast<int64_t>(*w.ap);
            }

            std::vector<JsonValue> special_arr;
            for (const auto& s : w.special_rules) {
                special_arr.push_back(JsonValue(s));
            }
            w_obj["special"] = JsonValue(std::move(special_arr));

            weapons_arr.push_back(std::move(w_obj));
        }
        lo_obj["weapons"] = JsonValue(std::move(weapons_arr));

        loadouts_array.push_back(std::move(lo_obj));
    }
    output["loadouts"] = JsonValue(std::move(loadouts_array));

    std::ofstream file(path);
    file << output.dump(2);
}

void OprPipeline::write_raw_loadouts_txt(const std::vector<RawLoadout>& loadouts,
                                          const std::filesystem::path& path) {
    std::ofstream file(path);

    for (const auto& lo : loadouts) {
        // Header line
        std::string rules_str;
        for (const auto& r : lo.rules) {
            if (!rules_str.empty()) rules_str += ", ";
            rules_str += r;
        }
        if (rules_str.empty()) rules_str = "-";

        file << lo.unit_name << " [UID:" << lo.uid << "] [" << lo.size << "] Q"
             << lo.quality << "+ D" << lo.defense << "+ | " << lo.points << "pts | "
             << rules_str << "\n";

        // Weapons line
        std::string weapons_str;
        for (const auto& w : lo.weapons) {
            if (!weapons_str.empty()) weapons_str += ", ";

            std::vector<std::string> inner;
            inner.push_back("A" + std::to_string(w.attacks));
            if (w.ap) {
                inner.push_back("AP(" + std::to_string(*w.ap) + ")");
            }
            inner.insert(inner.end(), w.special_rules.begin(), w.special_rules.end());

            std::string inner_str;
            for (const auto& s : inner) {
                if (!inner_str.empty()) inner_str += ", ";
                inner_str += s;
            }

            std::string weapon_str;
            if (w.range != "-") {
                weapon_str = w.range + " " + w.name + " (" + inner_str + ")";
            } else {
                weapon_str = w.name + " (" + inner_str + ")";
            }

            if (w.count > 1) {
                weapon_str = std::to_string(w.count) + "x " + weapon_str;
            }

            weapons_str += weapon_str;
        }
        if (weapons_str.empty()) weapons_str = "-";
        file << weapons_str << "\n";

        if (config_.add_blank_line_between_units) {
            file << "\n";
        }
    }
}

void OprPipeline::write_stage1_json(const UnitPipelineResult& result, const std::filesystem::path& path) {
    JsonValue output;
    output["unit"] = result.unit_name;
    output["total_combinations_processed"] = static_cast<int64_t>(result.total_combinations);
    output["total_groups"] = static_cast<int64_t>(result.stage1_groups.size());

    std::vector<JsonValue> groups_arr;
    for (const auto& g : result.stage1_groups) {
        JsonValue g_obj;
        g_obj["group_id"] = g.group_id;
        g_obj["signature"] = g.signature;
        g_obj["unit"] = g.unit_name;
        g_obj["points"] = static_cast<int64_t>(g.points);
        g_obj["count"] = static_cast<int64_t>(g.count);

        JsonValue rep_obj;
        rep_obj["combo_index_0based"] = static_cast<int64_t>(g.rep_combo_index);
        rep_obj["header"] = g.rep_header;
        g_obj["representative"] = std::move(rep_obj);

        groups_arr.push_back(std::move(g_obj));
    }
    output["groups"] = JsonValue(std::move(groups_arr));

    std::ofstream file(path);
    file << output.dump(2);
}

void OprPipeline::write_stage2_json(const UnitPipelineResult& result, const std::filesystem::path& path) {
    JsonValue output;
    output["total_stage1_groups"] = static_cast<int64_t>(result.stage1_groups.size());
    output["total_supergroups"] = static_cast<int64_t>(result.supergroups.size());

    std::vector<JsonValue> sg_arr;
    for (const auto& sg : result.supergroups) {
        JsonValue sg_obj;
        sg_obj["sg_id"] = sg.sg_id;
        sg_obj["supergroup_hash"] = sg.supergroup_hash;
        sg_obj["signature"] = sg.signature;
        sg_obj["unit"] = sg.unit_name;
        sg_obj["count_child_groups"] = static_cast<int64_t>(sg.count_child_groups);
        sg_obj["count_members"] = static_cast<int64_t>(sg.count_members);

        JsonValue pts_range;
        pts_range["min"] = static_cast<int64_t>(sg.points_min);
        pts_range["max"] = static_cast<int64_t>(sg.points_max);
        sg_obj["points_range"] = std::move(pts_range);

        std::vector<JsonValue> rules_arr;
        for (const auto& r : sg.rules_variations) {
            rules_arr.push_back(JsonValue(r));
        }
        sg_obj["rules_variations"] = JsonValue(std::move(rules_arr));

        std::vector<JsonValue> child_ids;
        for (const auto& id : sg.child_group_ids) {
            child_ids.push_back(JsonValue(id));
        }
        sg_obj["child_group_ids"] = JsonValue(std::move(child_ids));

        sg_obj["condensed_weapons_line"] = sg.condensed_weapons_line;

        sg_arr.push_back(std::move(sg_obj));
    }
    output["supergroups"] = JsonValue(std::move(sg_arr));

    std::ofstream file(path);
    file << output.dump(2);
}

void OprPipeline::write_final_txt(const UnitPipelineResult& result, const std::filesystem::path& path) {
    std::ofstream file(path);

    for (const auto& sg : result.supergroups) {
        // Find representative header
        std::string header;
        for (const auto& g : result.stage1_groups) {
            if (std::find(sg.child_group_ids.begin(), sg.child_group_ids.end(), g.group_id)
                != sg.child_group_ids.end()) {
                header = g.rep_header;
                break;
            }
        }

        std::string meta = "child_groups=" + std::to_string(sg.count_child_groups) +
                          " members=" + std::to_string(sg.count_members);

        // Inject SG label
        size_t bracket_pos = header.find('[');
        if (bracket_pos != std::string::npos) {
            std::string name = header.substr(0, bracket_pos);
            while (!name.empty() && name.back() == ' ') name.pop_back();
            std::string rest = header.substr(bracket_pos);
            header = name + " - " + sg.sg_id + " (" + meta + ") " + rest;
        } else {
            header = sg.sg_id + " (" + meta + ") " + header;
        }

        file << header << "\n";
        file << sg.condensed_weapons_line << "\n";

        if (config_.add_blank_line_between_units) {
            file << "\n";
        }
    }
}

// ==============================================================================
// File Merging
// ==============================================================================

std::optional<std::filesystem::path> OprPipeline::merge_final_txts(
    const std::filesystem::path& faction_dir,
    const std::string& faction_name)
{
    if (!config_.merge_final_txts) return std::nullopt;

    std::vector<std::filesystem::path> final_files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(faction_dir)) {
        if (entry.is_regular_file()) {
            std::string name = entry.path().filename().string();
            if (name.size() > 10 && name.substr(name.size() - 10) == ".final.txt") {
                final_files.push_back(entry.path());
            }
        }
    }

    if (final_files.empty()) return std::nullopt;

    std::sort(final_files.begin(), final_files.end());

    std::filesystem::path out_file = faction_dir / (safe_filename(faction_name) + ".final.merged.txt");

    // Exclude output file if it exists
    final_files.erase(
        std::remove_if(final_files.begin(), final_files.end(),
                       [&out_file](const auto& p) { return p == out_file; }),
        final_files.end());

    if (final_files.empty()) return std::nullopt;

    std::ofstream out(out_file);
    bool first_file = true;

    for (const auto& f : final_files) {
        std::ifstream in(f);
        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());

        // Remove BOM
        if (content.size() >= 3 && content[0] == '\xEF' && content[1] == '\xBB' && content[2] == '\xBF') {
            content = content.substr(3);
        }

        // Strip SG labels if configured
        if (config_.strip_sg_labels) {
            static const std::regex sg_label_re(R"(\s+-\s+SG\d{4,}\s+\([^)]*\))");
            content = std::regex_replace(content, sg_label_re, "");
        }

        if (!first_file && config_.add_blank_line_between_files) {
            out << "\n";
        }
        out << content;
        first_file = false;
    }

    return out_file;
}

// ==============================================================================
// Process JSON File
// ==============================================================================

FactionPipelineResult OprPipeline::process_json_file(const std::filesystem::path& json_path) {
    FactionPipelineResult result;

    // Parse faction name from filename
    std::string filename = json_path.filename().string();
    if (filename.size() > 11 && filename.substr(filename.size() - 11) == "_units.json") {
        filename = filename.substr(0, filename.size() - 11) + ".pdf";
    }

    auto [faction_name, version] = parse_faction_from_filename(filename);
    result.faction_name = faction_name;
    result.version = version;

    // Only include version in folder name if it's not "unknown"
    std::string folder_name = (version != "unknown")
        ? faction_name + "_" + version
        : faction_name;
    std::filesystem::path faction_dir = config_.output_dir / safe_filename(folder_name);
    std::filesystem::create_directories(faction_dir);
    result.output_dir = faction_dir;

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Processing: " << json_path.filename().string() << "\n";
    std::cout << "Faction: " << faction_name << " | Version: " << version << "\n";
    std::cout << "Output: " << faction_dir << "\n";
    std::cout << std::string(60, '=') << "\n";

    // Load units
    auto units = load_units_from_json(json_path);
    if (units.empty()) {
        std::cerr << "[WARN] No units found, skipping...\n";
        return result;
    }

    std::cout << "[OK] Loaded " << units.size() << " units\n";

    // Process each unit
    for (const auto& unit : units) {
        if (unit.name.empty()) continue;

        std::filesystem::path unit_dir = faction_dir / safe_filename(unit.name);
        std::filesystem::create_directories(unit_dir);

        std::cout << "\n  --- " << unit.name << " ---\n";

        auto unit_result = process_unit(unit);
        std::string base_name = safe_filename(unit.name);

        if (config_.raw_loadout_mode) {
            write_raw_loadouts_json(unit_result.raw_loadouts,
                                    unit_dir / (base_name + ".raw_loadouts.json"));
            write_raw_loadouts_txt(unit_result.raw_loadouts,
                                   unit_dir / (base_name + ".final.txt"));

            std::cout << "  [OK] Raw loadouts: " << unit_result.raw_loadouts.size()
                      << " loadouts (from " << unit_result.total_combinations << " combos)\n";
        } else {
            write_stage1_json(unit_result, unit_dir / (base_name + ".loadouts.reduced.json"));
            write_stage2_json(unit_result, unit_dir / (base_name + ".final.supergroups.json"));
            write_final_txt(unit_result, unit_dir / (base_name + ".final.txt"));

            std::cout << "  [OK] Stage-1: " << unit_result.stage1_groups.size()
                      << " groups (from " << unit_result.total_combinations << " combos)\n";
            std::cout << "  [OK] Stage-2: " << unit_result.supergroups.size() << " supergroups\n";
        }

        result.unit_results.push_back(std::move(unit_result));
        result.total_units_processed++;
    }

    // Merge final TXTs
    auto merged_path = merge_final_txts(faction_dir, faction_name);
    if (merged_path) {
        std::cout << "\n  [OK] Merged all units -> " << merged_path->filename().string() << "\n";
    }

    return result;
}

// ==============================================================================
// Run Pipeline
// ==============================================================================

std::vector<FactionPipelineResult> OprPipeline::run() {
    std::vector<FactionPipelineResult> results;

    if (!std::filesystem::exists(config_.input_path)) {
        std::cerr << "[ERROR] Input path not found: " << config_.input_path << "\n";
        return results;
    }

    std::filesystem::create_directories(config_.output_dir);

    // Find input files
    std::vector<std::filesystem::path> input_files;

    if (std::filesystem::is_regular_file(config_.input_path)) {
        std::string ext = config_.input_path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".json") {
            input_files.push_back(config_.input_path);
        }
    } else if (std::filesystem::is_directory(config_.input_path)) {
        // Look for *_units.json files first
        for (const auto& entry : std::filesystem::directory_iterator(config_.input_path)) {
            if (entry.is_regular_file()) {
                std::string name = entry.path().filename().string();
                if (name.size() > 11 && name.substr(name.size() - 11) == "_units.json") {
                    input_files.push_back(entry.path());
                }
            }
        }
        std::sort(input_files.begin(), input_files.end());
    }

    std::cout << "[INFO] Found " << input_files.size() << " input file(s) to process\n";
    std::cout << "[INFO] WORKERS_PER_UNIT=" << config_.workers_per_unit
              << ", TASKS_PER_UNIT=" << config_.tasks_per_unit << "\n";
    std::cout << "[INFO] RAW_LOADOUT_MODE=" << (config_.raw_loadout_mode ? "true" : "false") << "\n";

    // Process each input file
    for (const auto& input_file : input_files) {
        try {
            auto result = process_json_file(input_file);
            results.push_back(std::move(result));
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to process " << input_file.filename().string()
                      << ": " << e.what() << "\n";
        }
    }

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "DONE. Processed " << input_files.size() << " input file(s).\n";
    std::cout << "Output directory: " << config_.output_dir << "\n";
    std::cout << std::string(60, '=') << "\n";

    return results;
}

} // namespace battle::pipeline
