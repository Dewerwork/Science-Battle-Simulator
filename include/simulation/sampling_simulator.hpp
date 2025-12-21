#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "engine/dice.hpp"
#include "engine/game_runner.hpp"
#include "simulation/thread_pool.hpp"
#include "simulation/batch_simulator.hpp"
#include "simulation/matchup_sample.hpp"
#include "simulation/showcase_replay.hpp"
#include "simulation/sampling_config.hpp"
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <cstring>

namespace battle {

// ==============================================================================
// Sampling Simulator
// Produces all three tiers of output in a single pass:
//   Tier 1: Aggregated per-unit stats (128 bytes/unit) - ~280MB for 2.2M units
//   Tier 2: Random matchup samples (16 bytes/sample, 0.3%) - for ELO analysis
//   Tier 3: Showcase replays (one per unit) - for storytelling
// ==============================================================================

class SamplingSimulator {
public:
    explicit SamplingSimulator(
        const BatchConfig& batch_config,
        const SamplingConfig& sampling_config
    )
        : batch_config_(batch_config)
        , sampling_config_(sampling_config)
        , pool_()
        , sampler_(sampling_config)
    {}

    // Get thread count
    size_t thread_count() const { return pool_.thread_count(); }

    // Get aggregate game stats
    const AggregateGameStats& game_stats() const { return game_stats_; }

    // Main simulation entry point - produces all three tiers
    void simulate_all_with_sampling(
        const std::vector<Unit>& units_a,
        const std::vector<Unit>& units_b,
        ProgressCallback progress = nullptr
    ) {
        const size_t num_units_a = units_a.size();
        const size_t num_units_b = units_b.size();
        const u64 total_matchups = static_cast<u64>(num_units_a) * num_units_b;

        // Reset stats
        game_stats_.reset();
        samples_written_ = 0;

        // =====================================================================
        // TIER 1: Initialize aggregated results (per-unit, not per-matchup!)
        // =====================================================================
        std::vector<AggregatedUnitResult> aggregated_results;
        aggregated_results.resize(num_units_a);

        // Use sharded mutexes for thread-safe updates
        std::vector<std::mutex> unit_mutexes(AGGREGATED_MUTEX_SHARDS);

        // Initialize each result with unit info
        for (size_t i = 0; i < num_units_a; ++i) {
            aggregated_results[i].unit_id = static_cast<u32>(i);
            aggregated_results[i].points_cost = units_a[i].points_cost;
        }

        // =====================================================================
        // TIER 2: Open sample output file if sampling enabled
        // =====================================================================
        std::ofstream sample_out;
        std::vector<char> sample_write_buffer(4 * 1024 * 1024);  // 4MB buffer

        if (sampling_config_.enable_sampling) {
            sample_out.rdbuf()->pubsetbuf(sample_write_buffer.data(), sample_write_buffer.size());
            sample_out.open(sampling_config_.sample_output_path, std::ios::binary | std::ios::trunc);

            if (!sample_out) {
                throw std::runtime_error("Cannot open sample output file: " + sampling_config_.sample_output_path);
            }

            // Write header (will update sampled_count at end)
            SampleFileHeader header;
            header.sample_rate = sampling_config_.sample_rate;
            header.total_matchups = total_matchups;
            header.sampled_count = 0;  // Will update at end
            sample_out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        }

        // =====================================================================
        // TIER 3: Initialize showcase candidates
        // =====================================================================
        if (sampling_config_.enable_showcases) {
            showcase_candidates_.clear();
            showcase_candidates_.resize(num_units_a);
        }

        // Track progress
        std::atomic<u64> completed{0};
        auto start_time = std::chrono::high_resolution_clock::now();

        // Sample buffer for batched writes
        std::vector<MatchupSample> sample_buffer;
        sample_buffer.reserve(100000);
        std::mutex sample_mutex;

        // Chunk size for L3 cache optimization
        constexpr size_t UNITS_PER_CHUNK = 40000;
        const size_t num_chunks = (num_units_a + UNITS_PER_CHUNK - 1) / UNITS_PER_CHUNK;

        // Process in cache-friendly chunks
        for (size_t chunk = 0; chunk < num_chunks; ++chunk) {
            const u32 chunk_start = static_cast<u32>(chunk * UNITS_PER_CHUNK);
            const u32 chunk_end = static_cast<u32>(std::min((chunk + 1) * UNITS_PER_CHUNK, num_units_a));

            // Create batches for this chunk
            std::vector<std::pair<u32, u32>> matchups;
            matchups.reserve(batch_config_.batch_size);

            for (u32 i = chunk_start; i < chunk_end; ++i) {
                for (u32 j = 0; j < num_units_b; ++j) {
                    matchups.emplace_back(i, j);

                    if (matchups.size() >= batch_config_.batch_size) {
                        process_batch_all_tiers(
                            units_a, units_b, matchups,
                            aggregated_results, unit_mutexes,
                            sample_buffer, sample_mutex, sample_out
                        );

                        completed += matchups.size();
                        matchups.clear();

                        // Report progress
                        if (progress && batch_config_.enable_progress) {
                            auto now = std::chrono::high_resolution_clock::now();
                            f64 elapsed = std::chrono::duration<f64>(now - start_time).count();
                            u64 done = completed.load();
                            f64 rate = done / elapsed;
                            f64 remaining = (total_matchups - done) / rate;
                            progress({done, total_matchups, rate, elapsed, remaining, false, &game_stats_});
                        }
                    }
                }
            }

            // Process remaining matchups in chunk
            if (!matchups.empty()) {
                process_batch_all_tiers(
                    units_a, units_b, matchups,
                    aggregated_results, unit_mutexes,
                    sample_buffer, sample_mutex, sample_out
                );
                completed += matchups.size();
                matchups.clear();
            }
        }

        // =====================================================================
        // Write Tier 1: Aggregated results
        // =====================================================================
        {
            std::ofstream out(batch_config_.output_file, std::ios::binary | std::ios::trunc);
            if (!out) {
                throw std::runtime_error("Cannot open results output file: " + batch_config_.output_file);
            }

            // Write header (version 5 = aggregated 128-byte format)
            u32 magic = 0x42415453;  // "SABS"
            u32 version = 5;         // Aggregated format
            u32 unit_count = static_cast<u32>(num_units_a);
            u32 opponent_count = static_cast<u32>(num_units_b);

            out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
            out.write(reinterpret_cast<const char*>(&version), sizeof(version));
            out.write(reinterpret_cast<const char*>(&unit_count), sizeof(unit_count));
            out.write(reinterpret_cast<const char*>(&opponent_count), sizeof(opponent_count));

            // Write all aggregated results
            out.write(reinterpret_cast<const char*>(aggregated_results.data()),
                      aggregated_results.size() * sizeof(AggregatedUnitResult));
            out.flush();
        }

        // =====================================================================
        // Finalize Tier 2: Samples
        // =====================================================================
        if (sampling_config_.enable_sampling) {
            // Flush remaining samples
            if (!sample_buffer.empty()) {
                sample_out.write(
                    reinterpret_cast<const char*>(sample_buffer.data()),
                    sample_buffer.size() * sizeof(MatchupSample)
                );
                samples_written_ += sample_buffer.size();
                sample_buffer.clear();
            }

            // Update header with final count
            sample_out.seekp(offsetof(SampleFileHeader, sampled_count));
            sample_out.write(reinterpret_cast<const char*>(&samples_written_), sizeof(samples_written_));
            sample_out.close();
        }

        // =====================================================================
        // Write Tier 3: Showcases
        // =====================================================================
        if (sampling_config_.enable_showcases) {
            write_showcases();
        }

        // Final progress report
        if (progress) {
            auto now = std::chrono::high_resolution_clock::now();
            f64 elapsed = std::chrono::duration<f64>(now - start_time).count();
            u64 done = completed.load();
            f64 rate = done / elapsed;
            progress({done, total_matchups, rate, elapsed, 0.0, false, &game_stats_});
        }
    }

    // Get counts
    u64 samples_written() const { return samples_written_; }

    // Get showcase candidates (for merging)
    const std::vector<ShowcaseCandidate>& showcase_candidates() const {
        return showcase_candidates_;
    }

private:
    BatchConfig batch_config_;
    SamplingConfig sampling_config_;
    ThreadPool pool_;
    AggregateGameStats game_stats_;
    SamplingDecider sampler_;

    // Output state
    std::atomic<u64> samples_written_{0};
    std::vector<ShowcaseCandidate> showcase_candidates_;

    // Number of mutex shards
    static constexpr size_t AGGREGATED_MUTEX_SHARDS = 65536;
    static constexpr size_t SHOWCASE_MUTEX_SHARDS = 4096;
    std::array<std::mutex, SHOWCASE_MUTEX_SHARDS> showcase_mutexes_;

    // Helper function to compute CRC16 hash for faction names
    static u16 crc16_hash(std::string_view str) {
        u16 crc = 0xFFFF;
        for (char c : str) {
            crc ^= static_cast<u16>(c) << 8;
            for (int i = 0; i < 8; ++i) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ 0x1021;
                } else {
                    crc <<= 1;
                }
            }
        }
        return crc == 0 ? 1 : crc;
    }

    void process_batch_all_tiers(
        const std::vector<Unit>& units_a,
        const std::vector<Unit>& units_b,
        const std::vector<std::pair<u32, u32>>& matchups,
        std::vector<AggregatedUnitResult>& aggregated,
        std::vector<std::mutex>& unit_mutexes,
        std::vector<MatchupSample>& sample_buffer,
        std::mutex& sample_mutex,
        std::ofstream& sample_out
    ) {
        const size_t batch_size = matchups.size();
        const size_t num_threads = pool_.thread_count();
        const size_t chunk_size = (batch_size + num_threads - 1) / num_threads;

        std::atomic<size_t> threads_done{0};

        // Thread-local sample buffers
        std::vector<std::vector<MatchupSample>> thread_samples(num_threads);

        for (size_t t = 0; t < num_threads; ++t) {
            size_t start = t * chunk_size;
            size_t end = std::min(start + chunk_size, batch_size);

            if (start >= end) {
                ++threads_done;
                continue;
            }

            pool_.submit_detached([&, start, end, t]() {
                // Thread-local resources
                thread_local DiceRoller dice(
                    std::hash<std::thread::id>{}(std::this_thread::get_id()) * 2654435761ULL +
                    static_cast<u64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
                );
                thread_local GameRunner runner(dice);

                // Thread-local stats
                u64 local_games = 0;
                u64 local_wounds = 0;
                u64 local_models_killed = 0;
                u64 local_obj_rounds = 0;

                // Thread-local accumulator for aggregated results
                LocalAggregatedAccumulator current_accum;
                u32 current_unit_idx = UINT32_MAX;

                // Faction hash cache
                thread_local std::vector<u16> faction_hash_cache;
                if (faction_hash_cache.size() < units_b.size()) {
                    faction_hash_cache.resize(units_b.size(), 0);
                }

                // Reserve sample buffer
                thread_samples[t].reserve((end - start) / 300 + 10);

                // Lambda to flush accumulator
                auto flush_accumulator = [&]() {
                    if (current_unit_idx != UINT32_MAX && current_accum.total_matchups > 0) {
                        std::lock_guard<std::mutex> lock(unit_mutexes[current_unit_idx % AGGREGATED_MUTEX_SHARDS]);
                        current_accum.merge_into(aggregated[current_unit_idx]);
                    }
                };

                for (size_t i = start; i < end; ++i) {
                    auto [a_idx, b_idx] = matchups[i];
                    const Unit& unit_a = units_a[a_idx];
                    const Unit& unit_b = units_b[b_idx];

                    // Check if switched to new unit
                    if (a_idx != current_unit_idx) {
                        flush_accumulator();
                        current_accum = LocalAggregatedAccumulator{};
                        current_unit_idx = a_idx;
                    }

                    // Track per-game winners for samples
                    u8 game_winners[3] = {3, 3, 3};
                    u8 games_played = 0;

                    MatchResult result = run_match_with_tracking(
                        runner, unit_a, unit_b,
                        game_winners, games_played
                    );

                    // Update global stats
                    local_games += games_played;
                    local_wounds += result.total_wounds_dealt_a + result.total_wounds_dealt_b;
                    local_models_killed += result.total_models_killed_a + result.total_models_killed_b;
                    local_obj_rounds += result.total_rounds_holding_a + result.total_rounds_holding_b;

                    // TIER 1: Accumulate aggregated result
                    u16 faction_hash = faction_hash_cache[b_idx];
                    if (faction_hash == 0) {
                        faction_hash = crc16_hash(unit_b.faction.view());
                        faction_hash_cache[b_idx] = faction_hash;
                    }
                    current_accum.add_matchup(result, unit_a, unit_b, faction_hash);

                    // TIER 2: Check if should sample
                    if (sampling_config_.enable_sampling && sampler_.should_sample(a_idx, b_idx)) {
                        MatchupSample sample = MatchupSample::from_match(
                            result, a_idx, b_idx,
                            unit_a.points_cost, unit_b.points_cost,
                            game_winners, games_played
                        );
                        thread_samples[t].push_back(sample);
                    }

                    // TIER 3: Check for showcase update
                    if (sampling_config_.enable_showcases && result.overall_winner == GameWinner::UnitA) {
                        maybe_update_showcase(a_idx, b_idx, unit_a, unit_b, result, game_winners, games_played);
                    }
                }

                // Flush final accumulator
                flush_accumulator();

                // Update global stats
                game_stats_.total_games_played.fetch_add(local_games, std::memory_order_relaxed);
                game_stats_.total_wounds_dealt.fetch_add(local_wounds, std::memory_order_relaxed);
                game_stats_.total_models_killed.fetch_add(local_models_killed, std::memory_order_relaxed);
                game_stats_.total_objective_rounds.fetch_add(local_obj_rounds, std::memory_order_relaxed);

                threads_done.fetch_add(1, std::memory_order_release);
            });
        }

        // Wait for threads
        while (threads_done.load(std::memory_order_acquire) < num_threads) {
            std::this_thread::yield();
        }

        // Merge samples
        if (sampling_config_.enable_sampling) {
            std::lock_guard<std::mutex> lock(sample_mutex);
            for (const auto& ts : thread_samples) {
                sample_buffer.insert(sample_buffer.end(), ts.begin(), ts.end());
            }

            if (sample_buffer.size() >= 50000) {
                sample_out.write(
                    reinterpret_cast<const char*>(sample_buffer.data()),
                    sample_buffer.size() * sizeof(MatchupSample)
                );
                samples_written_ += sample_buffer.size();
                sample_buffer.clear();
            }
        }
    }

    MatchResult run_match_with_tracking(
        GameRunner& runner,
        const Unit& unit_a,
        const Unit& unit_b,
        u8 game_winners[3],
        u8& games_played
    ) {
        MatchResult result;
        result.unit_a_id = unit_a.unit_id;
        result.unit_b_id = unit_b.unit_id;

        for (int game = 0; game < 3; ++game) {
            GameResult game_result;
            if (game % 2 == 0) {
                game_result = runner.run_game(unit_a, unit_b);
            } else {
                game_result = runner.run_game(unit_b, unit_a);
                if (game_result.winner == GameWinner::UnitA) {
                    game_result.winner = GameWinner::UnitB;
                } else if (game_result.winner == GameWinner::UnitB) {
                    game_result.winner = GameWinner::UnitA;
                }
                std::swap(game_result.stats.wounds_dealt_a, game_result.stats.wounds_dealt_b);
                std::swap(game_result.stats.models_killed_a, game_result.stats.models_killed_b);
                std::swap(game_result.stats.rounds_holding_a, game_result.stats.rounds_holding_b);
            }

            game_winners[game] = static_cast<u8>(game_result.winner);
            games_played++;
            result.add_game(game_result);

            if (result.games_won_a == 2 || result.games_won_b == 2) {
                break;
            }
        }

        result.determine_winner();
        return result;
    }

    void maybe_update_showcase(
        u32 unit_idx, u32 opponent_idx,
        const Unit& unit, const Unit& opponent,
        const MatchResult& result,
        const u8 game_winners[3], u8 games_played
    ) {
        i16 elo_diff = static_cast<i16>(opponent.points_cost) - static_cast<i16>(unit.points_cost);

        size_t shard = unit_idx % SHOWCASE_MUTEX_SHARDS;
        std::lock_guard<std::mutex> lock(showcase_mutexes_[shard]);

        ShowcaseCandidate& candidate = showcase_candidates_[unit_idx];

        if (!candidate.should_replace(elo_diff, 0, sampling_config_.showcase_strategy)) {
            return;
        }

        ShowcaseReplay replay;
        replay.unit_id = unit_idx;
        replay.opponent_id = opponent_idx;
        replay.unit_points = unit.points_cost;
        replay.opponent_points = opponent.points_cost;
        replay.unit_elo = unit.points_cost;
        replay.opponent_elo = opponent.points_cost;
        replay.elo_differential = elo_diff;
        replay.selection_reason = static_cast<u8>(sampling_config_.showcase_strategy);
        replay.match_winner = 0;
        replay.games_won_unit = result.games_won_a;
        replay.games_won_opponent = result.games_won_b;
        replay.games_played = games_played;

        replay.total_wounds_dealt = static_cast<u16>(std::min(result.total_wounds_dealt_a, 65535u));
        replay.total_wounds_received = static_cast<u16>(std::min(result.total_wounds_dealt_b, 65535u));
        replay.total_kills = static_cast<u8>(std::min(result.total_models_killed_a, u16(255)));
        replay.total_deaths = static_cast<u8>(std::min(result.total_models_killed_b, u16(255)));
        replay.objective_rounds = result.total_rounds_holding_a;

        for (u8 g = 0; g < games_played && g < 3; ++g) {
            replay.games[g].winner = game_winners[g];
            replay.games[g].rounds_played = 4;
            replay.games[g].ending_type = 0;
        }

        i32 new_score = replay.score(sampling_config_.showcase_strategy);
        if (!candidate.has_replay || new_score > candidate.cached_score) {
            candidate.opponent_id = opponent_idx;
            candidate.opponent_points = opponent.points_cost;
            candidate.elo_differential = elo_diff;
            candidate.cached_score = new_score;
            candidate.has_replay = true;
            candidate.replay = replay;
        }
    }

    void write_showcases() {
        std::ofstream out(sampling_config_.showcase_output_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("Cannot open showcase output file: " + sampling_config_.showcase_output_path);
        }

        u32 valid_count = 0;
        for (const auto& candidate : showcase_candidates_) {
            if (candidate.has_replay) ++valid_count;
        }

        ShowcaseFileHeader header;
        header.unit_count = valid_count;
        header.strategy = static_cast<u8>(sampling_config_.showcase_strategy);
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));

        std::vector<ShowcaseIndexEntry> index;
        index.reserve(valid_count);
        u32 data_offset = 0;

        for (size_t i = 0; i < showcase_candidates_.size(); ++i) {
            if (showcase_candidates_[i].has_replay) {
                index.emplace_back(static_cast<u32>(i), data_offset);
                data_offset += sizeof(ShowcaseReplay);
            }
        }

        out.write(reinterpret_cast<const char*>(index.data()), index.size() * sizeof(ShowcaseIndexEntry));

        for (const auto& candidate : showcase_candidates_) {
            if (candidate.has_replay) {
                out.write(reinterpret_cast<const char*>(&candidate.replay), sizeof(ShowcaseReplay));
            }
        }

        out.close();
    }
};

} // namespace battle
