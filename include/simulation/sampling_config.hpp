#pragma once

#include "core/types.hpp"
#include "simulation/showcase_replay.hpp"
#include <string>
#include <random>

namespace battle {

// ==============================================================================
// Sampling Configuration
// Configures Tier 2 (random sampling) and Tier 3 (showcase replays) data capture
// ==============================================================================

struct SamplingConfig {
    // === Tier 2: Random Sampling ===
    bool enable_sampling = false;       // Master switch for sampling
    f64 sample_rate = 0.003;            // 0.3% default
    u64 sample_seed = 12345;            // Reproducible random seed
    std::string sample_output_path;     // Output file for samples

    // === Tier 3: Showcase Replays ===
    bool enable_showcases = false;      // Master switch for showcases
    ShowcaseStrategy showcase_strategy = ShowcaseStrategy::BiggestUpset;
    std::string showcase_output_path;   // Output file for showcases

    // === Derived Settings ===

    // Check if any tier 2/3 data collection is enabled
    bool is_enabled() const {
        return enable_sampling || enable_showcases;
    }

    // Validate configuration
    bool validate() const {
        if (enable_sampling) {
            if (sample_rate <= 0.0 || sample_rate > 1.0) return false;
            if (sample_output_path.empty()) return false;
        }
        if (enable_showcases) {
            if (showcase_output_path.empty()) return false;
        }
        return true;
    }

    // Get strategy name for display
    static const char* strategy_name(ShowcaseStrategy s) {
        switch (s) {
            case ShowcaseStrategy::BiggestUpset: return "biggest_upset";
            case ShowcaseStrategy::ClosestWin: return "closest_win";
            case ShowcaseStrategy::HighestEloDefeated: return "highest_elo";
            case ShowcaseStrategy::MostDramatic: return "most_dramatic";
        }
        return "unknown";
    }

    // Parse strategy from string
    static ShowcaseStrategy parse_strategy(const std::string& s) {
        if (s == "biggest_upset" || s == "upset") return ShowcaseStrategy::BiggestUpset;
        if (s == "closest_win" || s == "closest") return ShowcaseStrategy::ClosestWin;
        if (s == "highest_elo" || s == "highest") return ShowcaseStrategy::HighestEloDefeated;
        if (s == "most_dramatic" || s == "dramatic") return ShowcaseStrategy::MostDramatic;
        return ShowcaseStrategy::BiggestUpset;  // Default
    }
};

// ==============================================================================
// Fast Sampling Decision
// Uses a fast hash-based approach for reproducible sampling decisions
// ==============================================================================

class SamplingDecider {
public:
    explicit SamplingDecider(const SamplingConfig& config)
        : config_(config)
        , threshold_(static_cast<u64>(config.sample_rate * static_cast<f64>(UINT64_MAX)))
    {}

    // Determine if a matchup should be sampled (deterministic based on indices)
    bool should_sample(u32 a_idx, u32 b_idx) const {
        if (!config_.enable_sampling) return false;

        // Fast hash combining unit indices and seed
        u64 hash = hash_combine(a_idx, b_idx, config_.sample_seed);
        return hash < threshold_;
    }

    // Get the sample rate
    f64 sample_rate() const { return config_.sample_rate; }

private:
    const SamplingConfig& config_;
    u64 threshold_;

    // Fast hash function for combining values
    static u64 hash_combine(u32 a, u32 b, u64 seed) {
        // Mix using FNV-1a style operations
        u64 h = seed ^ 0xcbf29ce484222325ULL;
        h ^= static_cast<u64>(a);
        h *= 0x100000001b3ULL;
        h ^= static_cast<u64>(b);
        h *= 0x100000001b3ULL;
        // Additional mixing
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33;
        return h;
    }
};

// ==============================================================================
// Extended Manifest for Sampling
// Adds sampling configuration to chunk manifest
// ==============================================================================

struct SamplingManifestExtension {
    // Tier 2 config
    bool sampling_enabled;
    f64 sample_rate;
    u64 sample_seed;

    // Tier 3 config
    bool showcases_enabled;
    u8 showcase_strategy;

    // Estimated counts (computed after planning)
    u64 estimated_samples;      // Expected number of samples
    u32 expected_showcases;     // Should equal unit_count if enabled

    SamplingManifestExtension()
        : sampling_enabled(false), sample_rate(0.003), sample_seed(12345)
        , showcases_enabled(false), showcase_strategy(0)
        , estimated_samples(0), expected_showcases(0)
    {}

    // Serialize to manifest format (append to existing manifest)
    std::string to_manifest_section() const {
        std::ostringstream ss;
        ss << "SAMPLING_EXTENSION_V1\n";
        ss << sampling_enabled << "\t" << sample_rate << "\t" << sample_seed << "\n";
        ss << showcases_enabled << "\t" << static_cast<int>(showcase_strategy) << "\n";
        ss << estimated_samples << "\t" << expected_showcases << "\n";
        return ss.str();
    }

    // Parse from manifest section
    static SamplingManifestExtension from_manifest_section(std::istream& in) {
        SamplingManifestExtension ext;
        std::string header;
        std::getline(in, header);
        if (header != "SAMPLING_EXTENSION_V1") return ext;

        int strategy;
        in >> ext.sampling_enabled >> ext.sample_rate >> ext.sample_seed;
        in >> ext.showcases_enabled >> strategy;
        ext.showcase_strategy = static_cast<u8>(strategy);
        in >> ext.estimated_samples >> ext.expected_showcases;

        return ext;
    }
};

} // namespace battle
