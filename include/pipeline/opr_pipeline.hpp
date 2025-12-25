#pragma once

/**
 * OPR Pipeline - C++ port of run_opr_pipeline_all_units_v3_mt.py
 *
 * Processes OPR (One Page Rules) unit data from JSON files, generating
 * all loadout combinations with grouping/reduction stages.
 *
 * Features:
 * - Parallel processing using ThreadPool
 * - Stage-1 reduction: Group by weapon signature
 * - Stage-2 reduction: Supergroups with attack/rule agnostic options
 * - Raw loadout mode: Each combo gets a unique UID
 * - JSON and TXT output formats
 */

#include "core/types.hpp"
#include "simulation/thread_pool.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <tuple>
#include <functional>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <mutex>
#include <atomic>
#include <cmath>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cstring>

namespace battle::pipeline {

// ==============================================================================
// Configuration
// ==============================================================================

struct PipelineConfig {
    // Input/Output paths
    std::filesystem::path input_path;
    std::filesystem::path output_dir;

    // Limits
    size_t max_loadouts_per_unit = 0;  // 0 = no limit

    // Parallelism
    size_t workers_per_unit = 32;
    size_t tasks_per_unit = 256;

    // Output options
    bool write_ungrouped_loadouts = false;
    bool include_points_in_stage1_signature = true;

    // Stage-2 settings
    std::vector<int> range_buckets = {6, 12, 18, 24};
    std::string range_bucket_high = "32+";

    // Grouping modes
    bool attack_agnostic_grouping = true;
    bool rule_agnostic_grouping = true;
    bool raw_loadout_mode = true;

    // TXT formatting
    bool add_blank_line_between_units = true;

    // Merge settings
    bool merge_final_txts = true;
    bool strip_sg_labels = true;
    bool add_blank_line_between_files = true;
};

// ==============================================================================
// JSON Helper (minimal implementation)
// ==============================================================================

// Simple JSON value representation
class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    JsonValue() : type_(Type::Null) {}
    JsonValue(bool b) : type_(Type::Bool), bool_val_(b) {}
    JsonValue(int64_t n) : type_(Type::Number), num_val_(static_cast<double>(n)) {}
    JsonValue(double n) : type_(Type::Number), num_val_(n) {}
    JsonValue(const std::string& s) : type_(Type::String), str_val_(s) {}
    JsonValue(std::string&& s) : type_(Type::String), str_val_(std::move(s)) {}
    JsonValue(const char* s) : type_(Type::String), str_val_(s) {}
    JsonValue(std::vector<JsonValue>&& arr) : type_(Type::Array), arr_val_(std::move(arr)) {}
    JsonValue(std::map<std::string, JsonValue>&& obj) : type_(Type::Object), obj_val_(std::move(obj)) {}

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_bool() const { return type_ == Type::Bool; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    bool as_bool() const { return bool_val_; }
    double as_number() const { return num_val_; }
    int64_t as_int() const { return static_cast<int64_t>(num_val_); }
    const std::string& as_string() const { return str_val_; }
    const std::vector<JsonValue>& as_array() const { return arr_val_; }
    const std::map<std::string, JsonValue>& as_object() const { return obj_val_; }

    std::vector<JsonValue>& as_array() { return arr_val_; }
    std::map<std::string, JsonValue>& as_object() { return obj_val_; }

    // Object access
    const JsonValue& operator[](const std::string& key) const {
        static JsonValue null_val;
        if (type_ != Type::Object) return null_val;
        auto it = obj_val_.find(key);
        return it != obj_val_.end() ? it->second : null_val;
    }

    JsonValue& operator[](const std::string& key) {
        if (type_ != Type::Object) {
            type_ = Type::Object;
            obj_val_.clear();
        }
        return obj_val_[key];
    }

    // Array access
    const JsonValue& operator[](size_t idx) const {
        static JsonValue null_val;
        if (type_ != Type::Array || idx >= arr_val_.size()) return null_val;
        return arr_val_[idx];
    }

    bool contains(const std::string& key) const {
        return type_ == Type::Object && obj_val_.find(key) != obj_val_.end();
    }

    size_t size() const {
        if (type_ == Type::Array) return arr_val_.size();
        if (type_ == Type::Object) return obj_val_.size();
        return 0;
    }

    // Get with default
    template<typename T>
    T get(const std::string& key, T default_val) const;

    // Serialize to JSON string
    std::string dump(int indent = 2) const;

    // Parse from JSON string
    static JsonValue parse(const std::string& json);

private:
    Type type_;
    bool bool_val_ = false;
    double num_val_ = 0.0;
    std::string str_val_;
    std::vector<JsonValue> arr_val_;
    std::map<std::string, JsonValue> obj_val_;

    std::string dump_impl(int indent, int current_indent) const;
    static JsonValue parse_impl(const std::string& json, size_t& pos);
    static void skip_whitespace(const std::string& json, size_t& pos);
    static std::string parse_string_token(const std::string& json, size_t& pos);
};

// ==============================================================================
// Unit Data Structures
// ==============================================================================

struct WeaponData {
    int count = 1;
    std::string name;
    std::string range;  // e.g., "18\"" or "-"
    int attacks = 0;
    std::optional<int> ap;
    std::vector<std::string> special_rules;

    std::string to_key() const;  // Generate weapon key for signatures
};

struct UpgradeOption {
    std::string text;
    int pts = 0;
    std::optional<WeaponData> weapon;        // Structured weapon data from JSON
    std::vector<std::string> rules_granted;  // Pre-parsed special rules from JSON
};

struct UpgradeGroup {
    std::string header;
    std::vector<UpgradeOption> options;
};

struct UnitData {
    std::string name;
    int size = 1;
    int base_points = 0;
    std::optional<int> quality;
    std::optional<int> defense;
    std::optional<int> tough;
    std::vector<std::string> special_rules;
    std::vector<WeaponData> weapons;
    std::vector<UpgradeGroup> options;
};

// ==============================================================================
// Variant (delta from base loadout)
// ==============================================================================

struct Variant {
    int pts_delta = 0;
    std::vector<std::string> add_rules;
    std::map<std::string, int> weapon_delta;  // weapon_key -> count delta

    bool operator==(const Variant& other) const {
        return pts_delta == other.pts_delta &&
               add_rules == other.add_rules &&
               weapon_delta == other.weapon_delta;
    }
};

// ==============================================================================
// Loadout and Group Structures
// ==============================================================================

struct RawLoadout {
    std::string uid;
    size_t combo_index = 0;
    std::string unit_name;
    int points = 0;
    int quality = 0;
    int defense = 0;
    int size = 1;
    std::optional<int> tough;
    std::vector<std::string> rules;
    std::vector<WeaponData> weapons;
    std::string signature;
};

struct Stage1Group {
    std::string group_id;
    std::string signature;
    std::string unit_name;
    int points = 0;
    size_t count = 0;
    size_t rep_combo_index = 0;
    std::string rep_header;
};

struct WeaponGroup {
    std::string group_id;
    std::optional<int> range;
    std::optional<int> ap;
    std::vector<std::string> tags;
    int total_attacks = 0;
    int total_count = 0;
    std::vector<std::tuple<std::string, int, int>> source_weapons;  // (name, attacks, count)
};

struct Stage2SuperGroup {
    std::string sg_id;
    std::string supergroup_hash;
    std::string signature;
    std::string unit_name;
    size_t count_child_groups = 0;
    size_t count_members = 0;
    int points_min = 0;
    int points_max = 0;
    std::vector<std::string> rules_variations;
    std::vector<std::string> child_group_ids;
    std::string condensed_weapons_line;
    std::map<std::string, std::vector<std::string>> weapon_lineage;
};

// ==============================================================================
// Pipeline Results
// ==============================================================================

struct UnitPipelineResult {
    std::string unit_name;
    size_t total_combinations = 0;
    size_t total_groups = 0;
    size_t total_supergroups = 0;
    std::vector<Stage1Group> stage1_groups;
    std::vector<Stage2SuperGroup> supergroups;
    std::vector<RawLoadout> raw_loadouts;
};

struct FactionPipelineResult {
    std::string faction_name;
    std::string version;
    std::filesystem::path output_dir;
    std::vector<UnitPipelineResult> unit_results;
    size_t total_units_processed = 0;
};

// ==============================================================================
// OPR Pipeline Class
// ==============================================================================

class OprPipeline {
public:
    explicit OprPipeline(const PipelineConfig& config);

    // Run the pipeline on input path (file or directory)
    std::vector<FactionPipelineResult> run();

    // Process a single JSON file
    FactionPipelineResult process_json_file(const std::filesystem::path& json_path);

    // Process a single unit
    UnitPipelineResult process_unit(const UnitData& unit);

    // Progress callback
    using ProgressCallback = std::function<void(const std::string& unit_name, size_t combos, size_t groups)>;
    void set_progress_callback(ProgressCallback cb) { progress_callback_ = std::move(cb); }

    // Utility functions (public for use by WeaponData::to_key and helper functions)
    static std::string sha1_hex(const std::string& input);
    static std::string safe_filename(const std::string& name);
    static std::string normalize_whitespace(const std::string& s);
    static std::string normalize_name(const std::string& s);
    static std::vector<std::string> split_rules(const std::string& text);

private:
    PipelineConfig config_;
    ThreadPool& pool_;
    ProgressCallback progress_callback_;

    // JSON loading
    std::vector<UnitData> load_units_from_json(const std::filesystem::path& json_path);

    // UID generation
    std::string generate_uid(const std::string& unit_name, size_t combo_idx, const std::string& signature);

    // Weapon key generation
    static std::string weapon_key_from_profile(const std::string& profile, const std::string& weapon_name,
                                               std::optional<int> range_fallback = std::nullopt);
    static std::tuple<std::map<std::string, int>, std::map<std::string, std::string>>
        build_base_weapon_multiset(const UnitData& unit);

    // Variant generation
    std::vector<Variant> generate_group_variants(const UnitData& unit, const UpgradeGroup& group,
                                                  const std::map<std::string, std::string>& name_to_key);

    // Mixed-radix indexing
    static size_t total_combinations(const std::vector<size_t>& radices);
    static std::vector<size_t> index_to_choice_indices(size_t idx, const std::vector<size_t>& radices);

    // Stage-1 processing
    Stage1Group build_stage1_group(const UnitData& unit, size_t combo_idx,
                                   const std::vector<std::vector<Variant>>& group_variants,
                                   int base_pts, const std::vector<std::string>& base_rules,
                                   const std::map<std::string, int>& base_weapons);

    std::string build_stage1_signature(int points, const std::vector<std::string>& rules,
                                        const std::map<std::string, int>& weapons);

    std::string build_header(const UnitData& unit, int points, const std::vector<std::string>& rules);

    // Stage-2 processing
    std::vector<Stage2SuperGroup> stage2_reduce(const std::vector<Stage1Group>& stage1_groups,
                                                 const UnitData& unit);

    std::string condensed_weapons_key(const std::string& stage1_sig);
    std::string condensed_weapons_line(const std::string& stage1_sig);
    std::vector<WeaponGroup> group_weapons_by_rules(const std::string& stage1_sig);

    // Raw loadout mode
    RawLoadout build_raw_loadout(const UnitData& unit, size_t combo_idx,
                                  const std::vector<std::vector<Variant>>& group_variants,
                                  int base_pts, const std::vector<std::string>& base_rules,
                                  const std::map<std::string, int>& base_weapons);

    // Rules processing
    static std::vector<std::string> normalize_rules(const std::vector<std::string>& rules_in);
    static std::optional<std::string> clean_rule(const std::string& rule);

    // Output
    void write_raw_loadouts_json(const std::vector<RawLoadout>& loadouts,
                                  const std::filesystem::path& path);
    void write_raw_loadouts_txt(const std::vector<RawLoadout>& loadouts,
                                 const std::filesystem::path& path);
    void write_stage1_json(const UnitPipelineResult& result, const std::filesystem::path& path);
    void write_stage2_json(const UnitPipelineResult& result, const std::filesystem::path& path);
    void write_final_txt(const UnitPipelineResult& result, const std::filesystem::path& path);

    // File merging
    std::optional<std::filesystem::path> merge_final_txts(const std::filesystem::path& faction_dir,
                                                           const std::string& faction_name);

    // Faction/version parsing from filename
    static std::pair<std::string, std::string> parse_faction_from_filename(const std::string& filename);
};

// ==============================================================================
// Implementation - Utility Functions
// ==============================================================================

inline std::string OprPipeline::normalize_whitespace(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    bool in_space = false;
    for (char c : s) {
        if (std::isspace(c)) {
            if (!in_space && !result.empty()) {
                result += ' ';
                in_space = true;
            }
        } else {
            result += c;
            in_space = false;
        }
    }
    while (!result.empty() && std::isspace(result.back())) {
        result.pop_back();
    }
    return result;
}

inline std::string OprPipeline::normalize_name(const std::string& s) {
    std::string result = normalize_whitespace(s);
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    // Remove trailing 's' for plurals (unless it ends in 'ss')
    if (result.size() > 2 && result.back() == 's' && result[result.size()-2] != 's') {
        result.pop_back();
    }
    return result;
}

inline std::string OprPipeline::safe_filename(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        if (std::isalnum(c) || c == '.' || c == '_' || c == '-') {
            result += c;
        } else if (!result.empty() && result.back() != '_') {
            result += '_';
        }
    }
    while (!result.empty() && result.back() == '_') result.pop_back();
    while (!result.empty() && result.front() == '_') result.erase(0, 1);
    return result.empty() ? "unit" : result;
}

inline std::pair<std::string, std::string> OprPipeline::parse_faction_from_filename(const std::string& filename) {
    std::string name = filename;

    // Remove extensions
    for (const char* ext : {".pdf", ".json", "_units.json"}) {
        size_t pos = name.rfind(ext);
        if (pos != std::string::npos && pos + strlen(ext) == name.size()) {
            name = name.substr(0, pos);
            break;
        }
    }

    // Remove common prefixes like "GF - ", "AoF - ", etc.
    static const std::regex prefix_re(R"(^(GF|AoF|AoFS|GFF|FF)\s*-\s*)", std::regex::icase);
    name = std::regex_replace(name, prefix_re, "");

    // Extract version number at the end
    static const std::regex version_re(R"(\s+v?(\d+(?:\.\d+)+)\s*$)", std::regex::icase);
    std::smatch match;
    std::string version = "unknown";
    if (std::regex_search(name, match, version_re)) {
        version = match[1].str();
        name = name.substr(0, match.position());
    }

    // Clean up underscores
    std::replace(name.begin(), name.end(), '_', ' ');
    name = normalize_whitespace(name);

    return {name, version};
}

// ==============================================================================
// Implementation - SHA1 Hash (simple implementation)
// ==============================================================================

inline std::string OprPipeline::sha1_hex(const std::string& input) {
    // Simplified hash for signature generation (not cryptographic)
    // Uses FNV-1a hash and formats as hex
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (char c : input) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 0x100000001b3ULL;
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << hash;
    std::string result = oss.str();
    // Pad to 40 chars like SHA1
    while (result.size() < 40) result = result + result.substr(0, 40 - result.size());
    return result.substr(0, 40);
}

// ==============================================================================
// Implementation - Mixed-Radix Indexing
// ==============================================================================

inline size_t OprPipeline::total_combinations(const std::vector<size_t>& radices) {
    size_t total = 1;
    for (size_t r : radices) {
        if (r > 0) total *= r;
    }
    return total;
}

inline std::vector<size_t> OprPipeline::index_to_choice_indices(size_t idx, const std::vector<size_t>& radices) {
    std::vector<size_t> result(radices.size(), 0);
    for (int i = static_cast<int>(radices.size()) - 1; i >= 0; --i) {
        if (radices[i] > 0) {
            result[i] = idx % radices[i];
            idx /= radices[i];
        }
    }
    return result;
}

// ==============================================================================
// Implementation - Weapon Key Generation
// ==============================================================================

inline std::string WeaponData::to_key() const {
    std::string normalized_name = OprPipeline::normalize_whitespace(name);
    std::transform(normalized_name.begin(), normalized_name.end(), normalized_name.begin(), ::tolower);

    std::string rng_str;
    if (range != "-" && !range.empty()) {
        // Extract numeric part from "18\"" or "18"
        std::string range_num;
        for (char c : range) {
            if (std::isdigit(c)) range_num += c;
        }
        rng_str = range_num;
    }

    std::string ap_str = ap.has_value() ? std::to_string(*ap) : "";

    std::vector<std::string> sorted_rules = special_rules;
    std::sort(sorted_rules.begin(), sorted_rules.end(),
              [](const std::string& a, const std::string& b) {
                  std::string la = a, lb = b;
                  std::transform(la.begin(), la.end(), la.begin(), ::tolower);
                  std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
                  return la < lb;
              });

    std::string key = "N=" + normalized_name + "|R=" + rng_str + "|A=" + std::to_string(attacks) + "|AP=" + ap_str;
    if (!sorted_rules.empty()) {
        key += "|T=";
        for (size_t i = 0; i < sorted_rules.size(); ++i) {
            if (i > 0) key += ";";
            key += sorted_rules[i];
        }
    }
    return key;
}

// ==============================================================================
// Implementation - JSON Template Specializations
// ==============================================================================

template<>
inline int64_t JsonValue::get(const std::string& key, int64_t default_val) const {
    const auto& val = (*this)[key];
    return val.is_number() ? val.as_int() : default_val;
}

template<>
inline double JsonValue::get(const std::string& key, double default_val) const {
    const auto& val = (*this)[key];
    return val.is_number() ? val.as_number() : default_val;
}

template<>
inline std::string JsonValue::get(const std::string& key, std::string default_val) const {
    const auto& val = (*this)[key];
    return val.is_string() ? val.as_string() : default_val;
}

template<>
inline bool JsonValue::get(const std::string& key, bool default_val) const {
    const auto& val = (*this)[key];
    return val.is_bool() ? val.as_bool() : default_val;
}

} // namespace battle::pipeline
