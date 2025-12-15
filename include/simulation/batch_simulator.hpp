#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "engine/dice.hpp"
#include "engine/game_runner.hpp"
#include "simulation/thread_pool.hpp"
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include <mutex>
#include <functional>

namespace battle {

// ==============================================================================
// Batch Configuration
// ==============================================================================

struct BatchConfig {
    u32 batch_size = 10000;          // Matchups per batch
    u32 checkpoint_interval = 1000000; // Save progress every N matchups
    bool enable_progress = true;
    std::string output_file = "results.bin";
    std::string checkpoint_file = "checkpoint.bin";
};

// ==============================================================================
// Checkpoint Data
// ==============================================================================

struct CheckpointData {
    u64 completed = 0;
    u64 total = 0;
    u32 units_a_count = 0;
    u32 units_b_count = 0;
    bool valid = false;
};

// ==============================================================================
// Compact Result for Storage (8 bytes)
// ==============================================================================

struct CompactMatchResult {
    u32 unit_a_id;        // Unit A ID
    u32 unit_b_id : 20;   // Unit B ID (20 bits for up to 1M units)
    u32 winner : 2;       // 0=A, 1=B, 2=Draw
    u32 games_a : 2;      // Games won by A (0-2)
    u32 games_b : 2;      // Games won by B (0-2)
    u32 padding : 6;      // Reserved

    CompactMatchResult() : unit_a_id(0), unit_b_id(0), winner(2),
                          games_a(0), games_b(0), padding(0) {}

    static CompactMatchResult from_match(const MatchResult& r) {
        CompactMatchResult c;
        c.unit_a_id = r.unit_a_id;
        c.unit_b_id = r.unit_b_id & 0xFFFFF;
        c.winner = static_cast<u32>(r.overall_winner);
        c.games_a = r.games_won_a;
        c.games_b = r.games_won_b;
        return c;
    }
};

static_assert(sizeof(CompactMatchResult) == 8, "CompactMatchResult must be 8 bytes");

// ==============================================================================
// Progress Callback
// ==============================================================================

struct ProgressInfo {
    u64 completed;
    u64 total;
    f64 matchups_per_second;
    f64 elapsed_seconds;
    f64 estimated_remaining_seconds;
    bool resumed;  // True if this is a resumed simulation
};

using ProgressCallback = std::function<void(const ProgressInfo&)>;

// ==============================================================================
// Batch Simulator - Parallel simulation of matchups with resume support
// ==============================================================================

class BatchSimulator {
public:
    explicit BatchSimulator(const BatchConfig& config = BatchConfig())
        : config_(config), pool_() {}

    // Check if we can resume from checkpoint
    CheckpointData check_checkpoint(size_t units_a_count, size_t units_b_count) {
        CheckpointData data;

        std::ifstream in(config_.checkpoint_file, std::ios::binary);
        if (!in) return data;

        in.read(reinterpret_cast<char*>(&data.completed), sizeof(data.completed));
        in.read(reinterpret_cast<char*>(&data.total), sizeof(data.total));
        in.read(reinterpret_cast<char*>(&data.units_a_count), sizeof(data.units_a_count));
        in.read(reinterpret_cast<char*>(&data.units_b_count), sizeof(data.units_b_count));

        if (!in) return CheckpointData{};

        // Verify checkpoint matches current configuration
        u64 expected_total = static_cast<u64>(units_a_count) * units_b_count;
        if (data.total == expected_total &&
            data.units_a_count == units_a_count &&
            data.units_b_count == units_b_count &&
            data.completed < data.total) {
            data.valid = true;
        }

        return data;
    }

    // Simulate all matchups with optional resume
    void simulate_all(
        const std::vector<Unit>& units_a,
        const std::vector<Unit>& units_b,
        ProgressCallback progress = nullptr,
        bool try_resume = false
    ) {
        u64 total_matchups = static_cast<u64>(units_a.size()) * units_b.size();
        u64 resume_from = 0;
        bool resumed = false;

        // Check for resume
        if (try_resume) {
            CheckpointData checkpoint = check_checkpoint(units_a.size(), units_b.size());
            if (checkpoint.valid) {
                // Verify output file exists and has correct size
                std::ifstream check_out(config_.output_file, std::ios::binary | std::ios::ate);
                if (check_out) {
                    size_t file_size = check_out.tellg();
                    size_t expected_size = 16 + checkpoint.completed * sizeof(CompactMatchResult);
                    if (file_size >= expected_size) {
                        resume_from = checkpoint.completed;
                        resumed = true;
                    }
                }
            }
        }

        // Use a large write buffer (4MB) to reduce syscall frequency
        // This prevents progressive slowdown caused by OS-level I/O throttling
        static constexpr size_t WRITE_BUFFER_SIZE = 4 * 1024 * 1024;  // 4MB
        std::vector<char> write_buffer(WRITE_BUFFER_SIZE);

        // Open output file (append if resuming, truncate if starting fresh)
        std::ofstream out;
        out.rdbuf()->pubsetbuf(write_buffer.data(), write_buffer.size());

        if (resumed) {
            out.open(config_.output_file, std::ios::binary | std::ios::in | std::ios::out);
            if (out) {
                // Seek to position after existing results
                out.seekp(16 + resume_from * sizeof(CompactMatchResult));
            }
        } else {
            out.open(config_.output_file, std::ios::binary | std::ios::trunc);
            if (out) {
                write_header(out, units_a.size(), units_b.size());
            }
        }

        if (!out) {
            throw std::runtime_error("Cannot open output file: " + config_.output_file);
        }

        // Track progress
        std::atomic<u64> completed{resume_from};
        auto start_time = std::chrono::high_resolution_clock::now();

        // Create batches
        std::vector<std::pair<u32, u32>> matchups;
        matchups.reserve(config_.batch_size);

        std::mutex output_mutex;  // Kept for API compatibility with process_batch signature
        std::vector<CompactMatchResult> results_buffer;
        results_buffer.reserve(config_.batch_size + 16);  // Extra space to avoid reallocations

        // Calculate starting position if resuming
        u32 start_i = static_cast<u32>(resume_from / units_b.size());
        u32 start_j = static_cast<u32>(resume_from % units_b.size());

        // Process all matchups
        u64 current_index = resume_from;
        for (u32 i = start_i; i < units_a.size(); ++i) {
            u32 j_start = (i == start_i) ? start_j : 0;
            for (u32 j = j_start; j < units_b.size(); ++j) {
                matchups.emplace_back(i, j);

                // Process batch when full
                if (matchups.size() >= config_.batch_size) {
                    process_batch(units_a, units_b, matchups, results_buffer, output_mutex);

                    // Write results
                    {
                        std::lock_guard<std::mutex> lock(output_mutex);
                        out.write(reinterpret_cast<const char*>(results_buffer.data()),
                                 results_buffer.size() * sizeof(CompactMatchResult));
                        // Don't flush every batch - causes progressive slowdown as file grows
                        completed += results_buffer.size();
                        results_buffer.clear();
                    }

                    matchups.clear();

                    // Report progress
                    if (progress && config_.enable_progress) {
                        auto now = std::chrono::high_resolution_clock::now();
                        f64 elapsed = std::chrono::duration<f64>(now - start_time).count();
                        u64 done = completed.load();
                        u64 done_this_session = done - resume_from;
                        f64 rate = done_this_session / elapsed;
                        f64 remaining = (total_matchups - done) / rate;

                        progress({done, total_matchups, rate, elapsed, remaining, resumed});
                    }

                    // Checkpoint - also flush here to ensure data safety
                    u64 current_completed = completed.load();
                    if (current_completed % config_.checkpoint_interval == 0) {
                        out.flush();  // Only flush at checkpoints
                        write_checkpoint(current_completed, total_matchups,
                                        units_a.size(), units_b.size());
                    }
                }
            }
        }

        // Process remaining matchups
        if (!matchups.empty()) {
            process_batch(units_a, units_b, matchups, results_buffer, output_mutex);
            out.write(reinterpret_cast<const char*>(results_buffer.data()),
                     results_buffer.size() * sizeof(CompactMatchResult));
            out.flush();
            completed += results_buffer.size();
        }

        // Final checkpoint (mark as complete)
        write_checkpoint(completed.load(), total_matchups, units_a.size(), units_b.size());

        // Final progress report
        if (progress) {
            auto now = std::chrono::high_resolution_clock::now();
            f64 elapsed = std::chrono::duration<f64>(now - start_time).count();
            u64 done = completed.load();
            u64 done_this_session = done - resume_from;
            f64 rate = done_this_session / elapsed;
            progress({done, total_matchups, rate, elapsed, 0.0, resumed});
        }
    }

    // Legacy method for backwards compatibility
    u64 resume_from_checkpoint() {
        std::ifstream in(config_.checkpoint_file, std::ios::binary);
        if (!in) return 0;

        u64 completed;
        in.read(reinterpret_cast<char*>(&completed), sizeof(completed));
        return in ? completed : 0;
    }

    // Get thread count
    size_t thread_count() const { return pool_.thread_count(); }

private:
    BatchConfig config_;
    ThreadPool pool_;

    void write_header(std::ofstream& out, size_t units_a_count, size_t units_b_count) {
        u32 magic = 0x42415453;  // "SABS" = Science Battle Sim
        u32 version = 1;
        u32 a_count = static_cast<u32>(units_a_count);
        u32 b_count = static_cast<u32>(units_b_count);

        out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        out.write(reinterpret_cast<const char*>(&a_count), sizeof(a_count));
        out.write(reinterpret_cast<const char*>(&b_count), sizeof(b_count));
    }

    void write_checkpoint(u64 completed, u64 total, size_t units_a, size_t units_b) {
        std::ofstream out(config_.checkpoint_file, std::ios::binary);
        if (out) {
            u32 a_count = static_cast<u32>(units_a);
            u32 b_count = static_cast<u32>(units_b);
            out.write(reinterpret_cast<const char*>(&completed), sizeof(completed));
            out.write(reinterpret_cast<const char*>(&total), sizeof(total));
            out.write(reinterpret_cast<const char*>(&a_count), sizeof(a_count));
            out.write(reinterpret_cast<const char*>(&b_count), sizeof(b_count));
        }
    }

    void process_batch(
        const std::vector<Unit>& units_a,
        const std::vector<Unit>& units_b,
        const std::vector<std::pair<u32, u32>>& matchups,
        std::vector<CompactMatchResult>& results,
        std::mutex& /* unused - kept for API compatibility */
    ) {
        const size_t batch_size = matchups.size();
        const size_t num_threads = pool_.thread_count();
        const size_t chunk_size = (batch_size + num_threads - 1) / num_threads;

        // Pre-allocate results array - threads write directly to their slots
        results.resize(batch_size);

        // Atomic counter for completion tracking (no futures needed)
        std::atomic<size_t> threads_done{0};

        for (size_t t = 0; t < num_threads; ++t) {
            size_t start = t * chunk_size;
            size_t end = std::min(start + chunk_size, batch_size);

            if (start >= end) {
                ++threads_done;  // Empty chunk, count as done
                continue;
            }

            // Fire-and-forget task (no packaged_task allocation)
            pool_.submit_detached([&, start, end, t]() {
                // Use thread_local to reuse GameRunner across batches
                thread_local DiceRoller dice(
                    std::hash<std::thread::id>{}(std::this_thread::get_id()) * 2654435761ULL +
                    static_cast<u64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
                );
                thread_local GameRunner runner(dice);

                // Write directly to pre-allocated result slots (no vector allocation)
                for (size_t i = start; i < end; ++i) {
                    auto [a_idx, b_idx] = matchups[i];
                    MatchResult mr = runner.run_match(units_a[a_idx], units_b[b_idx]);
                    results[i] = CompactMatchResult::from_match(mr);
                }

                ++threads_done;
            });
        }

        // Wait for all threads to complete (simple spin-wait with yield)
        while (threads_done.load(std::memory_order_acquire) < num_threads) {
            std::this_thread::yield();
        }
    }
};

// ==============================================================================
// Benchmark Helper
// ==============================================================================

inline void benchmark_simulation(const std::vector<Unit>& units, size_t num_matchups = 10000) {
    ThreadPool pool;
    std::atomic<u64> completed{0};

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::future<void>> futures;
    size_t matchups_per_thread = num_matchups / pool.thread_count();

    for (size_t t = 0; t < pool.thread_count(); ++t) {
        futures.push_back(pool.submit([&, matchups_per_thread]() {
            DiceRoller dice;
            GameRunner runner(dice);

            for (size_t i = 0; i < matchups_per_thread; ++i) {
                size_t a = i % units.size();
                size_t b = (i + 1) % units.size();
                runner.run_match(units[a], units[b]);
                ++completed;
            }
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    f64 elapsed = std::chrono::duration<f64>(end - start).count();
    f64 rate = completed.load() / elapsed;

    std::cout << "Benchmark Results:\n";
    std::cout << "  Threads: " << pool.thread_count() << "\n";
    std::cout << "  Matchups: " << completed.load() << "\n";
    std::cout << "  Time: " << elapsed << " seconds\n";
    std::cout << "  Rate: " << rate << " matchups/second\n";
    std::cout << "  Estimated for 1T matchups: " << (1e12 / rate / 86400) << " days\n";
}

} // namespace battle
