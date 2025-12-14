#include "simulation/simulator.hpp"
#include "simulation/thread_pool.hpp"
#include <fstream>

namespace battle {

SimulationStatistics Simulator::simulate_matchup(
    const Unit& attacker,
    const Unit& defender,
    ProgressCallback progress
) {
    AtomicStats atomic_stats;
    atomic_stats.reset();

    const u64 total_iterations = config_.iterations_per_matchup;
    const u32 batch_size = config_.batch_size;
    const u32 num_batches = (total_iterations + batch_size - 1) / batch_size;

    auto& pool = get_thread_pool();
    std::vector<std::future<LocalStats>> futures;
    futures.reserve(num_batches);

    std::atomic<u64> completed{0};
    auto start_time = std::chrono::steady_clock::now();

    // Submit batches to thread pool
    for (u32 b = 0; b < num_batches; ++b) {
        u32 batch_iters = std::min(batch_size, static_cast<u32>(total_iterations - b * batch_size));

        futures.push_back(pool.submit([&attacker, &defender, batch_iters, this]() {
            LocalStats local;
            MatchupSimulator sim;
            sim.run_batch(attacker, defender, config_, batch_iters, local);
            return local;
        }));
    }

    // Collect results with progress updates
    for (auto& future : futures) {
        LocalStats local = future.get();
        local.merge_into(atomic_stats);

        u64 done = completed.fetch_add(local.attacker_wins + local.defender_wins + local.draws);
        done += local.attacker_wins + local.defender_wins + local.draws;

        if (progress) {
            auto now = std::chrono::steady_clock::now();
            f64 elapsed = std::chrono::duration<f64>(now - start_time).count();
            f64 rate = done / elapsed;
            progress(done, total_iterations, rate);
        }
    }

    return SimulationStatistics::compute(atomic_stats, total_iterations);
}

std::vector<MatchupResult> Simulator::simulate_matrix(
    const std::vector<Unit>& units_a,
    const std::vector<Unit>& units_b,
    ProgressCallback progress
) {
    const size_t total_matchups = units_a.size() * units_b.size();
    std::vector<MatchupResult> results;
    results.reserve(total_matchups);

    std::atomic<u64> completed{0};
    auto start_time = std::chrono::steady_clock::now();

    // Process each matchup
    for (size_t i = 0; i < units_a.size(); ++i) {
        for (size_t j = 0; j < units_b.size(); ++j) {
            auto stats = simulate_matchup(units_a[i], units_b[j], nullptr);

            results.emplace_back(
                static_cast<u16>(i),
                static_cast<u16>(j),
                stats.attacker_win_rate,
                stats.defender_win_rate
            );

            if (progress) {
                u64 done = ++completed;
                auto now = std::chrono::steady_clock::now();
                f64 elapsed = std::chrono::duration<f64>(now - start_time).count();
                f64 rate = done / elapsed;
                progress(done, total_matchups, rate);
            }
        }
    }

    return results;
}

void Simulator::simulate_massive(
    const std::vector<Unit>& units_a,
    const std::vector<Unit>& units_b,
    const std::string& output_file,
    ProgressCallback progress
) {
    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + output_file);
    }

    const size_t total_matchups = units_a.size() * units_b.size();
    std::atomic<u64> completed{0};
    auto start_time = std::chrono::steady_clock::now();

    // Write header
    u64 num_a = units_a.size();
    u64 num_b = units_b.size();
    out.write(reinterpret_cast<const char*>(&num_a), sizeof(num_a));
    out.write(reinterpret_cast<const char*>(&num_b), sizeof(num_b));

    // Process in chunks to avoid memory issues
    const size_t chunk_size = 10000;
    std::vector<MatchupResult> chunk;
    chunk.reserve(chunk_size);

    for (size_t i = 0; i < units_a.size(); ++i) {
        for (size_t j = 0; j < units_b.size(); ++j) {
            auto stats = simulate_matchup(units_a[i], units_b[j], nullptr);

            chunk.emplace_back(
                static_cast<u16>(i),
                static_cast<u16>(j),
                stats.attacker_win_rate,
                stats.defender_win_rate
            );

            // Flush chunk to disk
            if (chunk.size() >= chunk_size) {
                out.write(reinterpret_cast<const char*>(chunk.data()),
                          chunk.size() * sizeof(MatchupResult));
                chunk.clear();
            }

            if (progress) {
                u64 done = ++completed;
                if (done % config_.checkpoint_interval == 0 ||
                    done == total_matchups) {
                    auto now = std::chrono::steady_clock::now();
                    f64 elapsed = std::chrono::duration<f64>(now - start_time).count();
                    f64 rate = done / elapsed;
                    progress(done, total_matchups, rate);
                }
            }
        }
    }

    // Write remaining chunk
    if (!chunk.empty()) {
        out.write(reinterpret_cast<const char*>(chunk.data()),
                  chunk.size() * sizeof(MatchupResult));
    }

    out.close();
}

} // namespace battle
