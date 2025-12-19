/**
 * Chunk-Based Batch Simulator
 *
 * Designed for processing 5 trillion+ matchups by splitting the work into
 * manageable chunks that can be:
 *   - Run on different machines
 *   - Run at different times
 *   - Easily resumed if interrupted
 *   - Merged back together after completion
 *
 * Modes:
 *   plan    - Generate a chunk manifest for a simulation
 *   run     - Process a specific chunk (or next available)
 *   status  - Show progress of a chunked simulation
 *   merge   - Combine chunk results into final output
 */

#include "parser/unit_parser.hpp"
#include "simulation/batch_simulator.hpp"
#include "simulation/chunk_manager.hpp"
#include "simulation/sampling_simulator.hpp"
#include "simulation/sampling_config.hpp"
#include "simulation/matchup_sample.hpp"
#include "simulation/showcase_replay.hpp"
#include "core/faction_rules.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <cstdlib>

// Platform-specific includes for hostname and process ID
#ifdef _WIN32
    #define NOMINMAX
    #include <winsock2.h>
    #include <process.h>
    #pragma comment(lib, "ws2_32.lib")
    #define GET_PID() _getpid()
#else
    #include <unistd.h>
    #define GET_PID() getpid()
#endif

using namespace battle;
namespace fs = std::filesystem;

// ==============================================================================
// Utility Functions
// ==============================================================================

std::string get_hostname() {
#ifdef _WIN32
    // Initialize Winsock for gethostname
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return "unknown";
    }
#endif
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
#ifdef _WIN32
        WSACleanup();
#endif
        return std::string(hostname);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return "unknown";
}

std::string get_worker_id() {
    return get_hostname() + "_" + std::to_string(GET_PID());
}

void print_main_usage(const char* prog) {
    std::cout << "Chunk-Based Batch Simulator\n";
    std::cout << "For processing trillions of matchups in distributed chunks.\n\n";
    std::cout << "Usage: " << prog << " <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  plan      Generate a chunk manifest for simulation planning\n";
    std::cout << "  run       Process a chunk (specific or next available)\n";
    std::cout << "  status    Show progress of a chunked simulation\n";
    std::cout << "  merge     Combine chunk results into final output\n\n";
    std::cout << "Use '" << prog << " <command> -h' for command-specific help.\n";
}

// ==============================================================================
// PLAN Command - Generate chunk manifest
// ==============================================================================

void print_plan_usage(const char* prog) {
    std::cout << "Plan Command - Generate Chunk Manifest\n\n";
    std::cout << "Usage: " << prog << " plan <units_file> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o <dir>       Output directory for chunks (default: ./chunks)\n";
    std::cout << "  -m <file>      Manifest file path (default: <outdir>/manifest.txt)\n";
    std::cout << "  -n <count>     Number of chunks to create (default: auto)\n";
    std::cout << "  -s <size>      Target matchups per chunk (default: 1 billion)\n";
    std::cout << "  -r <rows>      Rows per chunk (for row-based chunking)\n";
    std::cout << "  --grid <r,c>   Grid chunking with r rows and c cols per chunk\n";
    std::cout << "  -f <format>    Result format: compact|extended|cextended|aggregated\n";
    std::cout << "  -h             Show this help\n\n";
    std::cout << "Chunking Strategies:\n";
    std::cout << "  -n <count>     Split into approximately N equal chunks\n";
    std::cout << "  -s <size>      Each chunk targets ~size matchups (e.g., -s 1000000000)\n";
    std::cout << "  -r <rows>      Each chunk processes <rows> units_a vs all units_b\n";
    std::cout << "  --grid <r,c>   Each chunk is r×c units (finest control)\n\n";
    std::cout << "Example:\n";
    std::cout << "  " << prog << " plan units.txt -n 1000 -o ./chunks\n";
    std::cout << "  " << prog << " plan units.txt -s 1000000000 -f cextended\n";
}

int cmd_plan(int argc, char* argv[]) {
    if (argc < 3 || std::string(argv[2]) == "-h") {
        print_plan_usage(argv[0]);
        return argc < 3 ? 1 : 0;
    }

    std::string units_file = argv[2];
    std::string output_dir = "./chunks";
    std::string manifest_file;
    u32 num_chunks = 0;
    u64 target_size = 1000000000;  // 1 billion default
    u32 rows_per_chunk = 0;
    u32 grid_rows = 0, grid_cols = 0;
    u8 result_format = 1;  // Compact

    bool use_num_chunks = false;
    bool use_rows = false;
    bool use_grid = false;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (arg == "-m" && i + 1 < argc) {
            manifest_file = argv[++i];
        } else if (arg == "-n" && i + 1 < argc) {
            num_chunks = std::stoul(argv[++i]);
            use_num_chunks = true;
        } else if (arg == "-s" && i + 1 < argc) {
            target_size = std::stoull(argv[++i]);
        } else if (arg == "-r" && i + 1 < argc) {
            rows_per_chunk = std::stoul(argv[++i]);
            use_rows = true;
        } else if (arg == "--grid" && i + 1 < argc) {
            std::string grid_spec = argv[++i];
            size_t comma = grid_spec.find(',');
            if (comma != std::string::npos) {
                grid_rows = std::stoul(grid_spec.substr(0, comma));
                grid_cols = std::stoul(grid_spec.substr(comma + 1));
                use_grid = true;
            }
        } else if (arg == "-f" && i + 1 < argc) {
            std::string fmt = argv[++i];
            if (fmt == "compact") result_format = 1;
            else if (fmt == "extended") result_format = 2;
            else if (fmt == "cextended") result_format = 3;
            else if (fmt == "aggregated") result_format = 4;
        }
    }

    if (manifest_file.empty()) {
        manifest_file = output_dir + "/manifest.txt";
    }

    // Initialize faction rules and load units
    initialize_faction_rules();

    std::cout << "Loading units from: " << units_file << "\n";
    auto parse_result = UnitParser::parse_file(units_file);

    if (parse_result.units.empty()) {
        std::cerr << "Error: Failed to load units from " << units_file << "\n";
        return 1;
    }

    u32 unit_count = static_cast<u32>(parse_result.units.size());
    std::cout << "Loaded " << unit_count << " units\n";

    // Generate chunks based on strategy
    std::vector<ChunkSpec> chunks;

    if (use_grid) {
        std::cout << "Using grid chunking: " << grid_rows << " rows x " << grid_cols << " cols per chunk\n";
        chunks = ChunkManager::generate_grid_chunks(unit_count, unit_count, grid_rows, grid_cols);
    } else if (use_rows) {
        std::cout << "Using row chunking: " << rows_per_chunk << " rows per chunk\n";
        chunks = ChunkManager::generate_row_chunks(unit_count, unit_count, rows_per_chunk);
    } else if (use_num_chunks) {
        std::cout << "Generating " << num_chunks << " chunks\n";
        chunks = ChunkManager::generate_n_chunks(unit_count, unit_count, num_chunks);
    } else {
        std::cout << "Targeting " << target_size << " matchups per chunk\n";
        chunks = ChunkManager::generate_sized_chunks(unit_count, unit_count, target_size);
    }

    // Create manifest
    ChunkManifest manifest = ChunkManager::create_manifest(
        units_file, output_dir, unit_count, unit_count, chunks, result_format);

    // Create output directory
    fs::create_directories(output_dir);

    // Save manifest
    if (!manifest.save(manifest_file)) {
        std::cerr << "Error: Failed to save manifest to " << manifest_file << "\n";
        return 1;
    }

    // Initialize status tracker
    ChunkStatusTracker tracker(output_dir + "/status.txt");
    tracker.initialize(manifest);

    // Print summary
    std::cout << "\n";
    ChunkManager::print_summary(manifest);
    std::cout << "\n";
    ChunkManager::print_storage_estimate(manifest);

    std::cout << "\nManifest saved to: " << manifest_file << "\n";
    std::cout << "Status file: " << output_dir << "/status.txt\n";

    std::cout << "\nNext steps:\n";
    std::cout << "  1. Run chunks:   " << argv[0] << " run " << manifest_file << "\n";
    std::cout << "  2. Check status: " << argv[0] << " status " << manifest_file << "\n";
    std::cout << "  3. Merge results: " << argv[0] << " merge " << manifest_file << " -o results.bin\n";

    return 0;
}

// ==============================================================================
// RUN Command - Process chunks
// ==============================================================================

void print_run_usage(const char* prog) {
    std::cout << "Run Command - Process Chunks\n\n";
    std::cout << "Usage: " << prog << " run <manifest_file> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -c <id>        Process specific chunk ID\n";
    std::cout << "  -n <count>     Process up to N chunks (default: 1)\n";
    std::cout << "  --all          Process all remaining chunks\n";
    std::cout << "  --auto         Automatically claim and process next available chunk\n";
    std::cout << "  -b <size>      Internal batch size (default: 10000)\n";
    std::cout << "  -r             Resume chunk if partially completed\n";
    std::cout << "  -q             Quiet mode\n";
    std::cout << "  -h             Show this help\n\n";
    std::cout << "Sampling Options (Tier 2/3 data):\n";
    std::cout << "  --sample-rate <rate>       Sample rate for matchups (default: 0.003 = 0.3%)\n";
    std::cout << "  --showcase-strategy <s>    Strategy: biggest_upset|closest_win|highest_elo|most_dramatic\n";
    std::cout << "  --no-sampling              Disable sampling even if configured\n";
    std::cout << "  --no-showcases             Disable showcases even if configured\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog << " run manifest.txt -c 0          # Run chunk 0\n";
    std::cout << "  " << prog << " run manifest.txt --auto -n 10  # Run next 10 available\n";
    std::cout << "  " << prog << " run manifest.txt --all         # Run all remaining\n";
    std::cout << "  " << prog << " run manifest.txt --all --sample-rate 0.003  # With sampling\n";
}

int cmd_run(int argc, char* argv[]) {
    if (argc < 3 || std::string(argv[2]) == "-h") {
        print_run_usage(argv[0]);
        return argc < 3 ? 1 : 0;
    }

    std::string manifest_file = argv[2];
    int specific_chunk = -1;
    int chunks_to_process = 1;
    bool process_all = false;
    bool auto_claim = false;
    u32 batch_size = 10000;
    bool try_resume = false;
    bool quiet = false;

    // Sampling configuration
    SamplingConfig sampling_config;
    sampling_config.enable_sampling = false;  // Disabled by default
    sampling_config.enable_showcases = false;
    sampling_config.sample_rate = 0.003;      // 0.3% default
    sampling_config.showcase_strategy = ShowcaseStrategy::BiggestUpset;
    bool no_sampling = false;
    bool no_showcases = false;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-c" && i + 1 < argc) {
            specific_chunk = std::stoi(argv[++i]);
        } else if (arg == "-n" && i + 1 < argc) {
            chunks_to_process = std::stoi(argv[++i]);
        } else if (arg == "--all") {
            process_all = true;
        } else if (arg == "--auto") {
            auto_claim = true;
        } else if (arg == "-b" && i + 1 < argc) {
            batch_size = std::stoul(argv[++i]);
        } else if (arg == "-r") {
            try_resume = true;
        } else if (arg == "-q") {
            quiet = true;
        } else if (arg == "--sample-rate" && i + 1 < argc) {
            sampling_config.sample_rate = std::stod(argv[++i]);
            sampling_config.enable_sampling = true;
        } else if (arg == "--showcase-strategy" && i + 1 < argc) {
            sampling_config.showcase_strategy = SamplingConfig::parse_strategy(argv[++i]);
            sampling_config.enable_showcases = true;
        } else if (arg == "--no-sampling") {
            no_sampling = true;
        } else if (arg == "--no-showcases") {
            no_showcases = true;
        }
    }

    // Apply overrides
    if (no_sampling) sampling_config.enable_sampling = false;
    if (no_showcases) sampling_config.enable_showcases = false;

    // Load manifest
    ChunkManifest manifest = ChunkManifest::load(manifest_file);
    if (manifest.chunks.empty()) {
        std::cerr << "Error: Failed to load manifest from " << manifest_file << "\n";
        return 1;
    }

    // Initialize
    initialize_faction_rules();

    if (!quiet) {
        std::cout << "=== Chunk Simulator ===\n\n";
        ChunkManager::print_summary(manifest);
        std::cout << "\n";
    }

    // Load units
    if (!quiet) std::cout << "Loading units from: " << manifest.units_file << "\n";
    auto parse_result = UnitParser::parse_file(manifest.units_file);
    if (parse_result.units.empty()) {
        std::cerr << "Error: Failed to load units\n";
        return 1;
    }
    if (!quiet) std::cout << "Loaded " << parse_result.units.size() << " units\n\n";

    // Setup status tracker
    ChunkStatusTracker tracker(manifest.output_dir + "/status.txt");
    std::string worker_id = get_worker_id();

    // Determine which chunks to process
    std::vector<int> chunks_to_run;

    if (specific_chunk >= 0) {
        chunks_to_run.push_back(specific_chunk);
    } else if (auto_claim || process_all) {
        // Get all pending/failed chunks
        auto status = tracker.load_status();
        for (const auto& prog : status) {
            if (prog.status == ChunkStatus::Pending || prog.status == ChunkStatus::Failed) {
                chunks_to_run.push_back(static_cast<int>(prog.chunk_id));
                if (!process_all && static_cast<int>(chunks_to_run.size()) >= chunks_to_process) {
                    break;
                }
            }
        }
    }

    if (chunks_to_run.empty()) {
        std::cout << "No chunks to process.\n";
        return 0;
    }

    if (!quiet) {
        std::cout << "Processing " << chunks_to_run.size() << " chunk(s): ";
        for (size_t i = 0; i < std::min(size_t(10), chunks_to_run.size()); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << chunks_to_run[i];
        }
        if (chunks_to_run.size() > 10) std::cout << ", ...";
        std::cout << "\n\n";
    }

    // Process each chunk
    int processed = 0;
    int failed = 0;

    for (int chunk_id : chunks_to_run) {
        if (chunk_id < 0 || static_cast<size_t>(chunk_id) >= manifest.chunks.size()) {
            std::cerr << "Invalid chunk ID: " << chunk_id << "\n";
            ++failed;
            continue;
        }

        const ChunkSpec& chunk = manifest.chunks[chunk_id];
        std::string output_file = ChunkManager::chunk_output_filename(manifest, chunk_id);
        std::string checkpoint_file = ChunkManager::chunk_checkpoint_filename(manifest, chunk_id);

        if (!quiet) {
            std::cout << "--- Processing Chunk " << chunk_id << " ---\n";
            std::cout << chunk.to_string() << "\n";
        }

        // Update status to in-progress
        ChunkProgress progress;
        progress.chunk_id = chunk_id;
        progress.status = ChunkStatus::InProgress;
        progress.matchups_completed = 0;
        progress.matchups_total = chunk.matchup_count();
        progress.output_file = output_file;
        progress.worker_id = worker_id;
        tracker.update_chunk(progress);

        // Configure batch simulator
        BatchConfig config;
        config.batch_size = batch_size;
        config.checkpoint_interval = 1000000;
        config.format = static_cast<ResultFormat>(manifest.result_format);
        config.output_file = output_file;
        config.checkpoint_file = checkpoint_file;
        config.enable_progress = !quiet;

        // Create unit subset for this chunk
        // Note: For efficiency, we create views into the original vectors
        // The simulator only needs unit_a[row_start:row_end] and unit_b[col_start:col_end]

        std::vector<Unit> chunk_units_a(
            parse_result.units.begin() + chunk.row_start,
            parse_result.units.begin() + chunk.row_end
        );
        std::vector<Unit> chunk_units_b(
            parse_result.units.begin() + chunk.col_start,
            parse_result.units.begin() + chunk.col_end
        );

        // Progress callback
        auto start_time = std::chrono::high_resolution_clock::now();
        auto last_status_update = start_time;

        auto progress_cb = [&](const ProgressInfo& info) {
            auto now = std::chrono::high_resolution_clock::now();
            f64 elapsed = std::chrono::duration<f64>(now - last_status_update).count();

            // Update status file periodically (every 30 seconds)
            if (elapsed >= 30.0) {
                progress.matchups_completed = info.completed;
                tracker.update_chunk(progress);
                last_status_update = now;
            }

            // Display progress
            f64 percent = 100.0 * info.completed / info.total;
            std::cout << "\r  Chunk " << chunk_id << ": " << info.completed << "/" << info.total
                      << " (" << std::fixed << std::setprecision(1) << percent << "%) "
                      << std::setprecision(0) << info.matchups_per_second << "/sec";

            if (info.estimated_remaining_seconds > 0 && info.estimated_remaining_seconds < 86400 * 365) {
                u64 remaining = static_cast<u64>(info.estimated_remaining_seconds);
                if (remaining >= 3600) {
                    std::cout << " | ETA: " << (remaining / 3600) << "h " << ((remaining % 3600) / 60) << "m";
                } else if (remaining >= 60) {
                    std::cout << " | ETA: " << (remaining / 60) << "m " << (remaining % 60) << "s";
                } else {
                    std::cout << " | ETA: " << remaining << "s";
                }
            }
            std::cout << "    " << std::flush;
        };

        // Run simulation
        try {
            // Check if sampling is enabled
            bool use_sampling = sampling_config.enable_sampling || sampling_config.enable_showcases;

            if (use_sampling) {
                // Set up output paths for this chunk
                std::string sample_file = manifest.output_dir + "/chunk_" +
                    std::to_string(chunk_id) + "_samples.bin";
                std::string showcase_file = manifest.output_dir + "/chunk_" +
                    std::to_string(chunk_id) + "_showcases.bin";

                sampling_config.sample_output_path = sample_file;
                sampling_config.showcase_output_path = showcase_file;

                SamplingSimulator sim(config, sampling_config);
                if (!quiet) {
                    std::cout << "  Using " << sim.thread_count() << " threads (with sampling)\n";
                    if (sampling_config.enable_sampling) {
                        std::cout << "  Sample rate: " << (sampling_config.sample_rate * 100) << "%\n";
                    }
                    if (sampling_config.enable_showcases) {
                        std::cout << "  Showcase strategy: "
                                  << SamplingConfig::strategy_name(sampling_config.showcase_strategy) << "\n";
                    }
                }

                if (quiet) {
                    sim.simulate_all_with_sampling(chunk_units_a, chunk_units_b, nullptr);
                } else {
                    sim.simulate_all_with_sampling(chunk_units_a, chunk_units_b, progress_cb);
                }

                if (!quiet && sampling_config.enable_sampling) {
                    std::cout << "\n  Samples written: " << sim.samples_written() << "\n";
                }
            } else {
                // Standard simulation without sampling
                BatchSimulator sim(config);
                if (!quiet) std::cout << "  Using " << sim.thread_count() << " threads\n";

                if (quiet) {
                    sim.simulate_all(chunk_units_a, chunk_units_b, nullptr, try_resume);
                } else {
                    sim.simulate_all(chunk_units_a, chunk_units_b, progress_cb, try_resume);
                }
            }

            // Mark as completed
            progress.status = ChunkStatus::Completed;
            progress.matchups_completed = progress.matchups_total;
            tracker.update_chunk(progress);

            if (!quiet) {
                auto end_time = std::chrono::high_resolution_clock::now();
                f64 elapsed = std::chrono::duration<f64>(end_time - start_time).count();
                std::cout << "\n  Completed in " << std::fixed << std::setprecision(1) << elapsed << "s\n";
                std::cout << "  Output: " << output_file << "\n\n";
            }

            ++processed;

        } catch (const std::exception& e) {
            std::cerr << "\nError processing chunk " << chunk_id << ": " << e.what() << "\n";

            progress.status = ChunkStatus::Failed;
            tracker.update_chunk(progress);
            ++failed;
        }
    }

    // Summary
    if (!quiet) {
        std::cout << "=== Summary ===\n";
        std::cout << "Processed: " << processed << " chunks\n";
        if (failed > 0) {
            std::cout << "Failed: " << failed << " chunks\n";
        }
    }

    return failed > 0 ? 1 : 0;
}

// ==============================================================================
// STATUS Command - Show progress
// ==============================================================================

void print_status_usage(const char* prog) {
    std::cout << "Status Command - Show Simulation Progress\n\n";
    std::cout << "Usage: " << prog << " status <manifest_file> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -v             Verbose - show all chunks\n";
    std::cout << "  --pending      Show only pending chunks\n";
    std::cout << "  --running      Show only in-progress chunks\n";
    std::cout << "  --completed    Show only completed chunks\n";
    std::cout << "  --failed       Show only failed chunks\n";
    std::cout << "  -h             Show this help\n";
}

int cmd_status(int argc, char* argv[]) {
    if (argc < 3 || std::string(argv[2]) == "-h") {
        print_status_usage(argv[0]);
        return argc < 3 ? 1 : 0;
    }

    std::string manifest_file = argv[2];
    bool verbose = false;
    int filter = -1;  // -1 = all, 0-3 = specific status

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-v") verbose = true;
        else if (arg == "--pending") filter = 0;
        else if (arg == "--running") filter = 1;
        else if (arg == "--completed") filter = 2;
        else if (arg == "--failed") filter = 3;
    }

    // Load manifest
    ChunkManifest manifest = ChunkManifest::load(manifest_file);
    if (manifest.chunks.empty()) {
        std::cerr << "Error: Failed to load manifest from " << manifest_file << "\n";
        return 1;
    }

    ChunkManager::print_summary(manifest);
    std::cout << "\n";

    // Load status
    ChunkStatusTracker tracker(manifest.output_dir + "/status.txt");
    auto summary = tracker.get_summary();

    std::cout << "=== Progress ===\n";
    std::cout << "Overall: " << std::fixed << std::setprecision(1)
              << summary.percent_complete() << "% complete\n";
    std::cout << "Matchups: " << summary.matchups_completed << " / " << summary.matchups_total << "\n\n";

    std::cout << "Chunks:\n";
    std::cout << "  Pending:     " << summary.pending << "\n";
    std::cout << "  In Progress: " << summary.in_progress << "\n";
    std::cout << "  Completed:   " << summary.completed << "\n";
    std::cout << "  Failed:      " << summary.failed << "\n";

    if (verbose || filter >= 0) {
        auto all_status = tracker.load_status();

        std::cout << "\n=== Chunk Details ===\n";
        const char* status_names[] = {"PENDING", "RUNNING", "COMPLETED", "FAILED"};

        for (const auto& prog : all_status) {
            int status_int = static_cast<int>(prog.status);
            if (filter >= 0 && status_int != filter) continue;

            std::cout << "  Chunk " << std::setw(6) << prog.chunk_id << ": "
                      << std::setw(9) << status_names[status_int];

            if (prog.status == ChunkStatus::InProgress) {
                std::cout << " (" << std::fixed << std::setprecision(1)
                          << prog.percent_complete() << "%)";
                if (!prog.worker_id.empty()) {
                    std::cout << " [" << prog.worker_id << "]";
                }
            } else if (prog.status == ChunkStatus::Completed) {
                std::cout << " -> " << prog.output_file;
            }
            std::cout << "\n";
        }
    }

    // Estimate remaining time (if we have rate data)
    if (summary.in_progress > 0 || summary.completed > 0) {
        std::cout << "\nTo continue processing:\n";
        std::cout << "  " << argv[0] << " run " << manifest_file << " --auto\n";
    }

    if (summary.pending == 0 && summary.in_progress == 0 && summary.failed == 0) {
        std::cout << "\nAll chunks completed! Ready to merge:\n";
        std::cout << "  " << argv[0] << " merge " << manifest_file << " -o results.bin\n";
    }

    return 0;
}

// ==============================================================================
// MERGE Command - Combine results
// ==============================================================================

void print_merge_usage(const char* prog) {
    std::cout << "Merge Command - Combine Chunk Results\n\n";
    std::cout << "Usage: " << prog << " merge <manifest_file> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o <file>      Output file for main results (required)\n";
    std::cout << "  --force        Merge even if some chunks incomplete\n";
    std::cout << "  --delete       Delete chunk files after successful merge\n";
    std::cout << "  -h             Show this help\n\n";
    std::cout << "Sampling Merge Options:\n";
    std::cout << "  --merge-samples <file>     Merge sample files into <file>\n";
    std::cout << "  --merge-showcases <file>   Merge showcase files into <file>\n";
    std::cout << "  --showcase-strategy <s>    Strategy for selecting best showcases\n";
}

// Helper function to merge sample files
void merge_sample_files(
    const ChunkManifest& manifest,
    const std::vector<ChunkProgress>& all_status,
    const std::string& output_file,
    bool force
) {
    std::cout << "\n=== Merging Sample Files ===\n";

    std::ofstream out(output_file, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "Error: Cannot open sample output file: " << output_file << "\n";
        return;
    }

    // Write header (will update counts at end)
    SampleFileHeader header;
    header.sample_rate = 0.003;  // Will be read from first chunk
    header.total_matchups = manifest.total_matchups();
    header.sampled_count = 0;
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    std::vector<char> buffer(4 * 1024 * 1024);
    u64 total_samples = 0;
    bool first_chunk = true;

    for (const auto& chunk : manifest.chunks) {
        std::string sample_file = manifest.output_dir + "/chunk_" +
            std::to_string(chunk.chunk_id) + "_samples.bin";

        // Check if file exists
        std::ifstream in(sample_file, std::ios::binary);
        if (!in) {
            if (!force) {
                std::cout << "  Chunk " << chunk.chunk_id << ": No sample file (skipped)\n";
            }
            continue;
        }

        // Read chunk header
        SampleFileHeader chunk_header;
        in.read(reinterpret_cast<char*>(&chunk_header), sizeof(chunk_header));

        if (first_chunk) {
            header.sample_rate = chunk_header.sample_rate;
            first_chunk = false;
        }

        // Copy sample data
        u64 bytes_to_copy = chunk_header.sampled_count * sizeof(MatchupSample);
        u64 bytes_copied = 0;

        while (bytes_copied < bytes_to_copy) {
            size_t to_read = std::min(static_cast<size_t>(bytes_to_copy - bytes_copied), buffer.size());
            in.read(buffer.data(), to_read);
            size_t actually_read = in.gcount();
            if (actually_read == 0) break;
            out.write(buffer.data(), actually_read);
            bytes_copied += actually_read;
        }

        total_samples += chunk_header.sampled_count;
        std::cout << "  Chunk " << chunk.chunk_id << ": " << chunk_header.sampled_count << " samples\n";
    }

    // Update header with final count
    header.sampled_count = total_samples;
    out.seekp(0);
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.close();

    std::cout << "Total samples merged: " << total_samples << "\n";
    std::cout << "Sample file: " << output_file << "\n";
}

// Helper function to merge showcase files (selecting best per unit)
void merge_showcase_files(
    const ChunkManifest& manifest,
    const std::vector<ChunkProgress>& all_status,
    const std::string& output_file,
    ShowcaseStrategy strategy,
    bool force
) {
    std::cout << "\n=== Merging Showcase Files ===\n";

    // Map to store best showcase per unit
    std::unordered_map<u32, ShowcaseReplay> best_showcases;

    for (const auto& chunk : manifest.chunks) {
        std::string showcase_file = manifest.output_dir + "/chunk_" +
            std::to_string(chunk.chunk_id) + "_showcases.bin";

        std::ifstream in(showcase_file, std::ios::binary);
        if (!in) {
            if (!force) {
                std::cout << "  Chunk " << chunk.chunk_id << ": No showcase file (skipped)\n";
            }
            continue;
        }

        // Read header
        ShowcaseFileHeader chunk_header;
        in.read(reinterpret_cast<char*>(&chunk_header), sizeof(chunk_header));

        if (chunk_header.magic != ShowcaseFileHeader::MAGIC) {
            std::cerr << "  Chunk " << chunk.chunk_id << ": Invalid showcase file\n";
            continue;
        }

        // Read index
        std::vector<ShowcaseIndexEntry> index(chunk_header.unit_count);
        in.read(reinterpret_cast<char*>(index.data()),
                index.size() * sizeof(ShowcaseIndexEntry));

        // Read and merge showcases
        u32 merged_count = 0;
        for (const auto& entry : index) {
            ShowcaseReplay replay;
            in.read(reinterpret_cast<char*>(&replay), sizeof(ShowcaseReplay));

            auto it = best_showcases.find(entry.unit_id);
            if (it == best_showcases.end() || replay.is_better_than(it->second, strategy)) {
                best_showcases[entry.unit_id] = replay;
                merged_count++;
            }
        }

        std::cout << "  Chunk " << chunk.chunk_id << ": " << chunk_header.unit_count
                  << " showcases, " << merged_count << " new best\n";
    }

    // Write merged output
    std::ofstream out(output_file, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "Error: Cannot open showcase output file: " << output_file << "\n";
        return;
    }

    // Write header
    ShowcaseFileHeader header;
    header.unit_count = static_cast<u32>(best_showcases.size());
    header.strategy = static_cast<u8>(strategy);
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Build index and write data
    std::vector<ShowcaseIndexEntry> final_index;
    final_index.reserve(best_showcases.size());

    u32 offset = 0;
    for (const auto& [unit_id, replay] : best_showcases) {
        final_index.emplace_back(unit_id, offset);
        offset += sizeof(ShowcaseReplay);
    }

    out.write(reinterpret_cast<const char*>(final_index.data()),
              final_index.size() * sizeof(ShowcaseIndexEntry));

    for (const auto& [unit_id, replay] : best_showcases) {
        out.write(reinterpret_cast<const char*>(&replay), sizeof(ShowcaseReplay));
    }

    out.close();

    std::cout << "Total showcases: " << best_showcases.size() << "\n";
    std::cout << "Showcase file: " << output_file << "\n";
}

int cmd_merge(int argc, char* argv[]) {
    if (argc < 3 || std::string(argv[2]) == "-h") {
        print_merge_usage(argv[0]);
        return argc < 3 ? 1 : 0;
    }

    std::string manifest_file = argv[2];
    std::string output_file;
    bool force = false;
    bool delete_chunks = false;

    // Sampling merge options
    std::string sample_output_file;
    std::string showcase_output_file;
    ShowcaseStrategy showcase_strategy = ShowcaseStrategy::BiggestUpset;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "--force") {
            force = true;
        } else if (arg == "--delete") {
            delete_chunks = true;
        } else if (arg == "--merge-samples" && i + 1 < argc) {
            sample_output_file = argv[++i];
        } else if (arg == "--merge-showcases" && i + 1 < argc) {
            showcase_output_file = argv[++i];
        } else if (arg == "--showcase-strategy" && i + 1 < argc) {
            showcase_strategy = SamplingConfig::parse_strategy(argv[++i]);
        }
    }

    if (output_file.empty()) {
        std::cerr << "Error: Output file required (-o)\n";
        return 1;
    }

    // Load manifest
    ChunkManifest manifest = ChunkManifest::load(manifest_file);
    if (manifest.chunks.empty()) {
        std::cerr << "Error: Failed to load manifest from " << manifest_file << "\n";
        return 1;
    }

    // Check status
    ChunkStatusTracker tracker(manifest.output_dir + "/status.txt");
    auto summary = tracker.get_summary();

    if (summary.completed != manifest.total_chunks && !force) {
        std::cerr << "Error: Not all chunks completed (" << summary.completed
                  << "/" << manifest.total_chunks << ")\n";
        std::cerr << "Use --force to merge anyway (incomplete data)\n";
        return 1;
    }

    std::cout << "=== Merging Chunk Results ===\n\n";
    std::cout << "Chunks to merge: " << summary.completed << "\n";
    std::cout << "Output: " << output_file << "\n\n";

    // Get result size for format
    size_t bytes_per_result[] = {0, 8, 24, 16, 128};
    size_t result_size = bytes_per_result[std::min(manifest.result_format, u8(4))];

    // Open output file
    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        std::cerr << "Error: Cannot open output file: " << output_file << "\n";
        return 1;
    }

    // Write header
    u32 magic = 0x42415453;  // "SABS"
    u32 version = manifest.result_format;
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&manifest.units_a_count), sizeof(manifest.units_a_count));
    out.write(reinterpret_cast<const char*>(&manifest.units_b_count), sizeof(manifest.units_b_count));

    // Merge chunks in order
    u64 total_results = 0;
    std::vector<char> buffer(4 * 1024 * 1024);  // 4MB buffer

    auto all_status = tracker.load_status();

    for (const auto& chunk : manifest.chunks) {
        std::string chunk_file = ChunkManager::chunk_output_filename(manifest, chunk.chunk_id);

        // Check if completed
        bool chunk_complete = false;
        for (const auto& prog : all_status) {
            if (prog.chunk_id == chunk.chunk_id && prog.status == ChunkStatus::Completed) {
                chunk_complete = true;
                break;
            }
        }

        if (!chunk_complete) {
            if (force) {
                std::cout << "  Chunk " << chunk.chunk_id << ": SKIPPED (incomplete)\n";
                continue;
            } else {
                std::cerr << "Error: Chunk " << chunk.chunk_id << " not completed\n";
                return 1;
            }
        }

        // Open and read chunk file
        std::ifstream in(chunk_file, std::ios::binary);
        if (!in) {
            std::cerr << "Error: Cannot open chunk file: " << chunk_file << "\n";
            if (!force) return 1;
            continue;
        }

        // Skip chunk header (16 bytes)
        in.seekg(16);

        // Copy data
        u64 chunk_results = chunk.matchup_count();
        u64 bytes_to_copy = chunk_results * result_size;
        u64 bytes_copied = 0;

        while (bytes_copied < bytes_to_copy) {
            size_t to_read = std::min(static_cast<size_t>(bytes_to_copy - bytes_copied), buffer.size());
            in.read(buffer.data(), to_read);
            size_t actually_read = in.gcount();
            if (actually_read == 0) break;
            out.write(buffer.data(), actually_read);
            bytes_copied += actually_read;
        }

        total_results += bytes_copied / result_size;
        std::cout << "  Chunk " << chunk.chunk_id << ": " << (bytes_copied / result_size) << " results\n";
    }

    out.close();

    std::cout << "\n=== Merge Complete ===\n";
    std::cout << "Total results: " << total_results << "\n";

    // Merge sample files if requested
    if (!sample_output_file.empty()) {
        merge_sample_files(manifest, all_status, sample_output_file, force);
    }

    // Merge showcase files if requested
    if (!showcase_output_file.empty()) {
        merge_showcase_files(manifest, all_status, showcase_output_file, showcase_strategy, force);
    }

    // Delete chunk files if requested
    if (delete_chunks) {
        std::cout << "\nDeleting chunk files...\n";
        for (const auto& chunk : manifest.chunks) {
            std::string chunk_file = ChunkManager::chunk_output_filename(manifest, chunk.chunk_id);
            std::string ckpt_file = ChunkManager::chunk_checkpoint_filename(manifest, chunk.chunk_id);
            fs::remove(chunk_file);
            fs::remove(ckpt_file);

            // Also delete sample and showcase files if they exist
            std::string sample_file = manifest.output_dir + "/chunk_" +
                std::to_string(chunk.chunk_id) + "_samples.bin";
            std::string showcase_file = manifest.output_dir + "/chunk_" +
                std::to_string(chunk.chunk_id) + "_showcases.bin";
            fs::remove(sample_file);
            fs::remove(showcase_file);
        }
        std::cout << "Deleted " << manifest.total_chunks << " chunk files\n";
    }

    std::cout << "\nOutput saved to: " << output_file << "\n";
    return 0;
}

// ==============================================================================
// Main Entry Point
// ==============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_main_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "plan") {
        return cmd_plan(argc, argv);
    } else if (command == "run") {
        return cmd_run(argc, argv);
    } else if (command == "status") {
        return cmd_status(argc, argv);
    } else if (command == "merge") {
        return cmd_merge(argc, argv);
    } else if (command == "-h" || command == "--help") {
        print_main_usage(argv[0]);
        return 0;
    } else {
        std::cerr << "Unknown command: " << command << "\n";
        print_main_usage(argv[0]);
        return 1;
    }
}
