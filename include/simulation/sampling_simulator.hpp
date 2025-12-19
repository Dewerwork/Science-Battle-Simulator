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
// Extends BatchSimulator with Tier 2 (sampling) and Tier 3 (showcase) support
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

    // Main simulation entry point with sampling support
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

        // Initialize showcase candidates (one per unit in units_a)
        if (sampling_config_.enable_showcases) {
            showcase_candidates_.clear();
            showcase_candidates_.resize(num_units_a);
        }

        // Open sample output file if sampling enabled
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

        // Track progress
        std::atomic<u64> completed{0};
        auto start_time = std::chrono::high_resolution_clock::now();

        // Sample buffer for batched writes
        std::vector<MatchupSample> sample_buffer;
        sample_buffer.reserve(100000);  // ~1.6 MB buffer
        std::mutex sample_mutex;

        // Chunk size for L3 cache optimization (like aggregated format)
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
                        process_batch_with_sampling(
                            units_a, units_b, matchups,
                            sample_buffer, sample_mutex,
                            sample_out
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
                process_batch_with_sampling(
                    units_a, units_b, matchups,
                    sample_buffer, sample_mutex,
                    sample_out
                );
                completed += matchups.size();
                matchups.clear();
            }
        }

        // Flush remaining samples
        if (sampling_config_.enable_sampling && !sample_buffer.empty()) {
            sample_out.write(
                reinterpret_cast<const char*>(sample_buffer.data()),
                sample_buffer.size() * sizeof(MatchupSample)
            );
            samples_written_ += sample_buffer.size();
            sample_buffer.clear();
        }

        // Update sample file header with final count
        if (sampling_config_.enable_sampling) {
            sample_out.seekp(offsetof(SampleFileHeader, sampled_count));
            sample_out.write(reinterpret_cast<const char*>(&samples_written_), sizeof(samples_written_));
            sample_out.close();
        }

        // Write showcase file
        if (sampling_config_.enable_showcases) {
            write_showcases(units_a);
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

    // Get sample count
    u64 samples_written() const { return samples_written_; }

    // Get showcase candidates (for merging)
    const std::vector<ShowcaseCandidate>& showcase_candidates() const {
        return showcase_candidates_;
    }

    std::vector<ShowcaseCandidate>& showcase_candidates() {
        return showcase_candidates_;
    }

private:
    BatchConfig batch_config_;
    SamplingConfig sampling_config_;
    ThreadPool pool_;
    AggregateGameStats game_stats_;
    SamplingDecider sampler_;

    // Sampling state
    std::atomic<u64> samples_written_{0};
    std::vector<ShowcaseCandidate> showcase_candidates_;

    // Number of mutex shards for showcase candidates
    static constexpr size_t SHOWCASE_MUTEX_SHARDS = 4096;
    std::array<std::mutex, SHOWCASE_MUTEX_SHARDS> showcase_mutexes_;

    void process_batch_with_sampling(
        const std::vector<Unit>& units_a,
        const std::vector<Unit>& units_b,
        const std::vector<std::pair<u32, u32>>& matchups,
        std::vector<MatchupSample>& sample_buffer,
        std::mutex& sample_mutex,
        std::ofstream& sample_out
    ) {
        const size_t batch_size = matchups.size();
        const size_t num_threads = pool_.thread_count();
        const size_t chunk_size = (batch_size + num_threads - 1) / num_threads;

        std::atomic<size_t> threads_done{0};

        // Thread-local sample buffers to reduce contention
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

                // Thread-local stats accumulators
                u64 local_games = 0;
                u64 local_wounds = 0;
                u64 local_models_killed = 0;

                // Reserve space for thread-local samples
                thread_samples[t].reserve((end - start) / 300 + 10);  // ~0.3% sample rate

                for (size_t i = start; i < end; ++i) {
                    auto [a_idx, b_idx] = matchups[i];
                    const Unit& unit_a = units_a[a_idx];
                    const Unit& unit_b = units_b[b_idx];

                    // Track per-game winners for sample data
                    u8 game_winners[3] = {3, 3, 3};  // 3 = not played
                    u8 games_played = 0;

                    MatchResult result = run_match_with_tracking(
                        runner, unit_a, unit_b,
                        game_winners, games_played
                    );

                    // Update stats
                    local_games += games_played;
                    local_wounds += result.total_wounds_dealt_a + result.total_wounds_dealt_b;
                    local_models_killed += result.total_models_killed_a + result.total_models_killed_b;

                    // Check if this matchup should be sampled (Tier 2)
                    if (sampling_config_.enable_sampling && sampler_.should_sample(a_idx, b_idx)) {
                        MatchupSample sample = MatchupSample::from_match(
                            result, a_idx, b_idx,
                            unit_a.points_cost, unit_b.points_cost,
                            game_winners, games_played
                        );
                        thread_samples[t].push_back(sample);
                    }

                    // Check for showcase update (Tier 3)
                    if (sampling_config_.enable_showcases) {
                        // Only consider wins for unit_a
                        if (result.overall_winner == GameWinner::UnitA) {
                            maybe_update_showcase(
                                a_idx, b_idx, unit_a, unit_b,
                                result, game_winners, games_played
                            );
                        }
                    }
                }

                // Update global stats
                game_stats_.total_games_played.fetch_add(local_games, std::memory_order_relaxed);
                game_stats_.total_wounds_dealt.fetch_add(local_wounds, std::memory_order_relaxed);
                game_stats_.total_models_killed.fetch_add(local_models_killed, std::memory_order_relaxed);

                threads_done.fetch_add(1, std::memory_order_release);
            });
        }

        // Wait for all threads
        while (threads_done.load(std::memory_order_acquire) < num_threads) {
            std::this_thread::yield();
        }

        // Merge thread-local samples into main buffer
        if (sampling_config_.enable_sampling) {
            std::lock_guard<std::mutex> lock(sample_mutex);
            for (const auto& ts : thread_samples) {
                sample_buffer.insert(sample_buffer.end(), ts.begin(), ts.end());
            }

            // Flush to disk if buffer is large
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

    // Run match with per-game tracking
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
                // Flip perspective
                if (game_result.winner == GameWinner::UnitA) {
                    game_result.winner = GameWinner::UnitB;
                } else if (game_result.winner == GameWinner::UnitB) {
                    game_result.winner = GameWinner::UnitA;
                }
                std::swap(game_result.stats.wounds_dealt_a, game_result.stats.wounds_dealt_b);
                std::swap(game_result.stats.models_killed_a, game_result.stats.models_killed_b);
                std::swap(game_result.stats.rounds_holding_a, game_result.stats.rounds_holding_b);
            }

            // Track game winner
            game_winners[game] = static_cast<u8>(game_result.winner);
            games_played++;

            result.add_game(game_result);

            // Early exit if match decided
            if (result.games_won_a == 2 || result.games_won_b == 2) {
                break;
            }
        }

        result.determine_winner();
        return result;
    }

    void maybe_update_showcase(
        u32 unit_idx,
        u32 opponent_idx,
        const Unit& unit,
        const Unit& opponent,
        const MatchResult& result,
        const u8 game_winners[3],
        u8 games_played
    ) {
        // Calculate ELO differential (placeholder - use points as proxy)
        // In real implementation, you'd use actual ELO values
        i16 elo_diff = static_cast<i16>(opponent.points_cost) - static_cast<i16>(unit.points_cost);

        // Get mutex shard for this unit
        size_t shard = unit_idx % SHOWCASE_MUTEX_SHARDS;
        std::lock_guard<std::mutex> lock(showcase_mutexes_[shard]);

        ShowcaseCandidate& candidate = showcase_candidates_[unit_idx];

        // Quick check if this could be better
        if (!candidate.should_replace(elo_diff, 0, sampling_config_.showcase_strategy)) {
            return;
        }

        // Create showcase replay
        ShowcaseReplay replay;
        replay.unit_id = unit_idx;
        replay.opponent_id = opponent_idx;
        replay.unit_points = unit.points_cost;
        replay.opponent_points = opponent.points_cost;
        replay.unit_elo = unit.points_cost;  // Placeholder
        replay.opponent_elo = opponent.points_cost;  // Placeholder
        replay.elo_differential = elo_diff;
        replay.selection_reason = static_cast<u8>(sampling_config_.showcase_strategy);
        replay.match_winner = 0;  // Unit won
        replay.games_won_unit = result.games_won_a;
        replay.games_won_opponent = result.games_won_b;
        replay.games_played = games_played;

        replay.total_wounds_dealt = static_cast<u16>(std::min(result.total_wounds_dealt_a, 65535u));
        replay.total_wounds_received = static_cast<u16>(std::min(result.total_wounds_dealt_b, 65535u));
        replay.total_kills = static_cast<u8>(std::min(result.total_models_killed_a, u16(255)));
        replay.total_deaths = static_cast<u8>(std::min(result.total_models_killed_b, u16(255)));
        replay.objective_rounds = result.total_rounds_holding_a;

        // Create game replays (simplified - no per-round data without game_runner modification)
        for (u8 g = 0; g < games_played && g < 3; ++g) {
            replay.games[g].winner = game_winners[g];
            replay.games[g].rounds_played = 4;  // Placeholder
            replay.games[g].ending_type = 0;    // Placeholder
        }

        // Check if actually better with full data
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

    void write_showcases(const std::vector<Unit>& units) {
        std::ofstream out(sampling_config_.showcase_output_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("Cannot open showcase output file: " + sampling_config_.showcase_output_path);
        }

        // Count valid showcases
        u32 valid_count = 0;
        for (const auto& candidate : showcase_candidates_) {
            if (candidate.has_replay) ++valid_count;
        }

        // Write header
        ShowcaseFileHeader header;
        header.unit_count = valid_count;
        header.strategy = static_cast<u8>(sampling_config_.showcase_strategy);
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));

        // Write index
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

        // Write replay data
        for (const auto& candidate : showcase_candidates_) {
            if (candidate.has_replay) {
                out.write(reinterpret_cast<const char*>(&candidate.replay), sizeof(ShowcaseReplay));
            }
        }

        out.close();
    }
};

} // namespace battle
