#pragma once

// Prevent Windows min/max macro conflicts
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

#include "core/types.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace battle {

// ==============================================================================
// Chunk Specification
// ==============================================================================

/**
 * ChunkSpec defines a rectangular region of the matchup matrix.
 *
 * For a matchup matrix of units_a x units_b:
 *   - row_start/row_end define the range of unit_a indices
 *   - col_start/col_end define the range of unit_b indices
 *
 * This allows splitting 5 trillion matchups into manageable chunks that can be:
 *   - Run on different machines
 *   - Run at different times
 *   - Easily resumed if interrupted
 *   - Merged back together after completion
 */
struct ChunkSpec {
    u32 chunk_id;           // Unique identifier for this chunk
    u32 row_start;          // First unit_a index (inclusive)
    u32 row_end;            // Last unit_a index (exclusive)
    u32 col_start;          // First unit_b index (inclusive)
    u32 col_end;            // Last unit_b index (exclusive)

    // Computed properties
    u64 matchup_count() const {
        return static_cast<u64>(row_end - row_start) * (col_end - col_start);
    }

    // For square chunks on diagonal (same units vs same units)
    bool is_diagonal() const {
        return row_start == col_start && row_end == col_end;
    }

    // String representation for logging
    std::string to_string() const {
        std::ostringstream ss;
        ss << "Chunk " << chunk_id << " [rows " << row_start << "-" << row_end
           << ", cols " << col_start << "-" << col_end << "] = " << matchup_count() << " matchups";
        return ss.str();
    }

    // Format for manifest file (tab-separated)
    std::string to_manifest_line() const {
        std::ostringstream ss;
        ss << chunk_id << "\t" << row_start << "\t" << row_end << "\t"
           << col_start << "\t" << col_end << "\t" << matchup_count();
        return ss.str();
    }

    // Parse from manifest line
    static ChunkSpec from_manifest_line(const std::string& line) {
        ChunkSpec spec{};
        std::istringstream ss(line);
        u64 matchups; // ignored, computed
        ss >> spec.chunk_id >> spec.row_start >> spec.row_end
           >> spec.col_start >> spec.col_end >> matchups;
        return spec;
    }
};

// ==============================================================================
// Chunk Status
// ==============================================================================

enum class ChunkStatus : u8 {
    Pending = 0,        // Not yet started
    InProgress = 1,     // Currently being processed
    Completed = 2,      // Successfully finished
    Failed = 3          // Failed (needs retry)
};

struct ChunkProgress {
    u32 chunk_id;
    ChunkStatus status;
    u64 matchups_completed;     // For partial progress within chunk
    u64 matchups_total;
    std::string output_file;    // Result file for this chunk
    std::string worker_id;      // Machine/process that claimed this chunk

    f64 percent_complete() const {
        return matchups_total > 0 ? 100.0 * matchups_completed / matchups_total : 0.0;
    }
};

// ==============================================================================
// Chunk Manifest
// ==============================================================================

/**
 * ChunkManifest describes an entire chunked simulation job.
 *
 * The manifest file format:
 *   Line 1: CHUNK_MANIFEST_V1
 *   Line 2: units_a_count  units_b_count  total_chunks  result_format
 *   Line 3: units_file_path
 *   Line 4+: chunk_id  row_start  row_end  col_start  col_end  matchup_count
 */
struct ChunkManifest {
    u32 units_a_count;
    u32 units_b_count;
    u32 total_chunks;
    u8 result_format;           // 1=Compact, 2=Extended, 3=CompactExtended, 4=Aggregated
    std::string units_file;     // Path to units file
    std::string output_dir;     // Directory for chunk results
    std::vector<ChunkSpec> chunks;

    u64 total_matchups() const {
        return static_cast<u64>(units_a_count) * units_b_count;
    }

    // Save manifest to file
    bool save(const std::string& filepath) const {
        std::ofstream out(filepath);
        if (!out) return false;

        out << "CHUNK_MANIFEST_V1\n";
        out << units_a_count << "\t" << units_b_count << "\t"
            << total_chunks << "\t" << static_cast<int>(result_format) << "\n";
        out << units_file << "\n";
        out << output_dir << "\n";

        for (const auto& chunk : chunks) {
            out << chunk.to_manifest_line() << "\n";
        }

        return out.good();
    }

    // Load manifest from file
    static ChunkManifest load(const std::string& filepath) {
        ChunkManifest manifest{};
        std::ifstream in(filepath);
        if (!in) return manifest;

        std::string header;
        std::getline(in, header);
        if (header != "CHUNK_MANIFEST_V1") return manifest;

        int format;
        in >> manifest.units_a_count >> manifest.units_b_count
           >> manifest.total_chunks >> format;
        manifest.result_format = static_cast<u8>(format);
        in.ignore(); // Skip newline

        std::getline(in, manifest.units_file);
        std::getline(in, manifest.output_dir);

        manifest.chunks.reserve(manifest.total_chunks);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty()) {
                manifest.chunks.push_back(ChunkSpec::from_manifest_line(line));
            }
        }

        return manifest;
    }
};

// ==============================================================================
// Chunk Manager
// ==============================================================================

/**
 * ChunkManager generates and manages chunk specifications for large-scale simulations.
 *
 * Chunking Strategies:
 *
 * 1. ROW_CHUNKS: Split by rows only (recommended for same-units vs same-units)
 *    - Each chunk processes a range of unit_a against ALL unit_b
 *    - Simple to merge, good for aggregated format
 *    - Chunk count = ceil(units_a / rows_per_chunk)
 *
 * 2. GRID_CHUNKS: Split into rectangular grid
 *    - Each chunk processes a range of unit_a against a range of unit_b
 *    - More chunks, but each is smaller
 *    - Better for distributed processing on many machines
 *    - Chunk count = ceil(units_a / rows) * ceil(units_b / cols)
 *
 * 3. SIZED_CHUNKS: Specify target matchups per chunk
 *    - Automatically calculates row/col divisions to achieve target size
 *    - Best for controlling memory and runtime per chunk
 */
enum class ChunkStrategy {
    RowChunks,      // Split by rows only (full column range)
    GridChunks,     // Split into rectangular grid
    SizedChunks     // Target specific matchup count per chunk
};

class ChunkManager {
public:
    /**
     * Generate row-based chunks.
     * Each chunk processes rows_per_chunk rows against all columns.
     *
     * @param units_a_count Number of units in first dimension
     * @param units_b_count Number of units in second dimension
     * @param rows_per_chunk Number of rows per chunk
     * @return Vector of chunk specifications
     */
    static std::vector<ChunkSpec> generate_row_chunks(
        u32 units_a_count,
        u32 units_b_count,
        u32 rows_per_chunk
    ) {
        std::vector<ChunkSpec> chunks;
        u32 chunk_id = 0;

        for (u32 row = 0; row < units_a_count; row += rows_per_chunk) {
            ChunkSpec spec;
            spec.chunk_id = chunk_id++;
            spec.row_start = row;
            spec.row_end = std::min(row + rows_per_chunk, units_a_count);
            spec.col_start = 0;
            spec.col_end = units_b_count;
            chunks.push_back(spec);
        }

        return chunks;
    }

    /**
     * Generate grid-based chunks.
     * Splits the matchup matrix into a grid of rectangular chunks.
     *
     * @param units_a_count Number of units in first dimension
     * @param units_b_count Number of units in second dimension
     * @param rows_per_chunk Number of rows per chunk
     * @param cols_per_chunk Number of columns per chunk
     * @return Vector of chunk specifications
     */
    static std::vector<ChunkSpec> generate_grid_chunks(
        u32 units_a_count,
        u32 units_b_count,
        u32 rows_per_chunk,
        u32 cols_per_chunk
    ) {
        std::vector<ChunkSpec> chunks;
        u32 chunk_id = 0;

        for (u32 row = 0; row < units_a_count; row += rows_per_chunk) {
            for (u32 col = 0; col < units_b_count; col += cols_per_chunk) {
                ChunkSpec spec;
                spec.chunk_id = chunk_id++;
                spec.row_start = row;
                spec.row_end = std::min(row + rows_per_chunk, units_a_count);
                spec.col_start = col;
                spec.col_end = std::min(col + cols_per_chunk, units_b_count);
                chunks.push_back(spec);
            }
        }

        return chunks;
    }

    /**
     * Generate chunks targeting a specific matchup count per chunk.
     * Automatically calculates grid dimensions to achieve target size.
     *
     * @param units_a_count Number of units in first dimension
     * @param units_b_count Number of units in second dimension
     * @param target_matchups Target matchups per chunk (e.g., 1 billion)
     * @return Vector of chunk specifications
     */
    static std::vector<ChunkSpec> generate_sized_chunks(
        u32 units_a_count,
        u32 units_b_count,
        u64 target_matchups
    ) {
        // Calculate optimal grid dimensions
        // We want rows_per_chunk * cols_per_chunk ≈ target_matchups
        // Prefer more columns than rows (better for cache locality)

        u64 total = static_cast<u64>(units_a_count) * units_b_count;
        u32 num_chunks = std::max(1u, static_cast<u32>((total + target_matchups - 1) / target_matchups));

        // Calculate grid dimensions
        // Prefer fewer row divisions (larger row chunks) for cache efficiency
        u32 row_divisions = static_cast<u32>(std::sqrt(num_chunks * static_cast<f64>(units_a_count) / units_b_count));
        row_divisions = std::max(1u, std::min(row_divisions, units_a_count));

        u32 col_divisions = (num_chunks + row_divisions - 1) / row_divisions;
        col_divisions = std::max(1u, std::min(col_divisions, units_b_count));

        u32 rows_per_chunk = (units_a_count + row_divisions - 1) / row_divisions;
        u32 cols_per_chunk = (units_b_count + col_divisions - 1) / col_divisions;

        return generate_grid_chunks(units_a_count, units_b_count, rows_per_chunk, cols_per_chunk);
    }

    /**
     * Generate chunks by specifying the total number of chunks desired.
     *
     * @param units_a_count Number of units in first dimension
     * @param units_b_count Number of units in second dimension
     * @param num_chunks Desired number of chunks
     * @return Vector of chunk specifications
     */
    static std::vector<ChunkSpec> generate_n_chunks(
        u32 units_a_count,
        u32 units_b_count,
        u32 num_chunks
    ) {
        u64 total = static_cast<u64>(units_a_count) * units_b_count;
        u64 target_per_chunk = (total + num_chunks - 1) / num_chunks;
        return generate_sized_chunks(units_a_count, units_b_count, target_per_chunk);
    }

    /**
     * Create a full manifest for a chunked simulation.
     *
     * @param units_file Path to the units file
     * @param output_dir Directory for chunk output files
     * @param units_a_count Number of units in first dimension
     * @param units_b_count Number of units in second dimension
     * @param chunks Vector of chunk specifications
     * @param result_format Output format (1-4)
     * @return Complete chunk manifest
     */
    static ChunkManifest create_manifest(
        const std::string& units_file,
        const std::string& output_dir,
        u32 units_a_count,
        u32 units_b_count,
        const std::vector<ChunkSpec>& chunks,
        u8 result_format = 1
    ) {
        ChunkManifest manifest;
        manifest.units_file = units_file;
        manifest.output_dir = output_dir;
        manifest.units_a_count = units_a_count;
        manifest.units_b_count = units_b_count;
        manifest.total_chunks = static_cast<u32>(chunks.size());
        manifest.result_format = result_format;
        manifest.chunks = chunks;
        return manifest;
    }

    /**
     * Get the output filename for a specific chunk.
     */
    static std::string chunk_output_filename(const ChunkManifest& manifest, u32 chunk_id) {
        std::ostringstream ss;
        ss << manifest.output_dir << "/chunk_" << std::setfill('0') << std::setw(6) << chunk_id << ".bin";
        return ss.str();
    }

    /**
     * Get the checkpoint filename for a specific chunk.
     */
    static std::string chunk_checkpoint_filename(const ChunkManifest& manifest, u32 chunk_id) {
        std::ostringstream ss;
        ss << manifest.output_dir << "/chunk_" << std::setfill('0') << std::setw(6) << chunk_id << ".ckpt";
        return ss.str();
    }

    /**
     * Print summary of chunking plan.
     */
    static void print_summary(const ChunkManifest& manifest, std::ostream& out = std::cout) {
        u64 total = manifest.total_matchups();
        out << "=== Chunk Manifest Summary ===\n";
        out << "Units A: " << manifest.units_a_count << "\n";
        out << "Units B: " << manifest.units_b_count << "\n";
        out << "Total matchups: " << total;
        if (total >= 1e12) {
            out << " (" << (total / 1e12) << " trillion)";
        } else if (total >= 1e9) {
            out << " (" << (total / 1e9) << " billion)";
        } else if (total >= 1e6) {
            out << " (" << (total / 1e6) << " million)";
        }
        out << "\n";
        out << "Total chunks: " << manifest.total_chunks << "\n";

        if (!manifest.chunks.empty()) {
            u64 min_size = manifest.chunks[0].matchup_count();
            u64 max_size = min_size;
            for (const auto& c : manifest.chunks) {
                min_size = std::min(min_size, c.matchup_count());
                max_size = std::max(max_size, c.matchup_count());
            }
            out << "Matchups per chunk: ";
            if (min_size == max_size) {
                out << min_size;
            } else {
                out << min_size << " - " << max_size;
            }
            if (max_size >= 1e9) {
                out << " (" << (max_size / 1e9) << "B max)";
            } else if (max_size >= 1e6) {
                out << " (" << (max_size / 1e6) << "M max)";
            }
            out << "\n";
        }

        out << "Output directory: " << manifest.output_dir << "\n";
        out << "Units file: " << manifest.units_file << "\n";

        const char* format_names[] = {"Unknown", "Compact", "Extended", "CompactExtended", "Aggregated"};
        out << "Result format: " << format_names[std::min(manifest.result_format, u8(4))] << "\n";
    }

    /**
     * Estimate storage requirements for a manifest.
     */
    static void print_storage_estimate(const ChunkManifest& manifest, std::ostream& out = std::cout) {
        size_t bytes_per_result[] = {0, 8, 24, 16, 128};
        size_t bpr = bytes_per_result[std::min(manifest.result_format, u8(4))];

        u64 total_bytes;
        if (manifest.result_format == 4) {
            // Aggregated: per-unit storage
            total_bytes = static_cast<u64>(manifest.units_a_count) * bpr;
        } else {
            // Per-matchup storage
            total_bytes = manifest.total_matchups() * bpr;
        }

        out << "=== Storage Estimate ===\n";
        out << "Bytes per result: " << bpr << "\n";
        out << "Total storage: ";
        if (total_bytes >= 1e15) {
            out << (total_bytes / 1e15) << " PB\n";
        } else if (total_bytes >= 1e12) {
            out << (total_bytes / 1e12) << " TB\n";
        } else if (total_bytes >= 1e9) {
            out << (total_bytes / 1e9) << " GB\n";
        } else if (total_bytes >= 1e6) {
            out << (total_bytes / 1e6) << " MB\n";
        } else {
            out << (total_bytes / 1e3) << " KB\n";
        }

        // Per-chunk storage
        if (!manifest.chunks.empty() && manifest.result_format != 4) {
            u64 max_chunk_bytes = manifest.chunks[0].matchup_count() * bpr;
            for (const auto& c : manifest.chunks) {
                max_chunk_bytes = std::max(max_chunk_bytes, c.matchup_count() * bpr);
            }
            out << "Max per chunk: ";
            if (max_chunk_bytes >= 1e9) {
                out << (max_chunk_bytes / 1e9) << " GB\n";
            } else if (max_chunk_bytes >= 1e6) {
                out << (max_chunk_bytes / 1e6) << " MB\n";
            } else {
                out << (max_chunk_bytes / 1e3) << " KB\n";
            }
        }
    }
};

// ==============================================================================
// Chunk Status Tracker
// ==============================================================================

/**
 * ChunkStatusTracker maintains the status of all chunks in a manifest.
 * Uses a simple file-based locking mechanism for distributed coordination.
 */
class ChunkStatusTracker {
public:
    explicit ChunkStatusTracker(const std::string& status_file)
        : status_file_(status_file) {}

    // Initialize status file for a manifest
    bool initialize(const ChunkManifest& manifest) {
        std::ofstream out(status_file_);
        if (!out) return false;

        out << "CHUNK_STATUS_V1\n";
        out << manifest.total_chunks << "\n";

        for (u32 i = 0; i < manifest.total_chunks; ++i) {
            out << i << "\t" << static_cast<int>(ChunkStatus::Pending)
                << "\t0\t" << manifest.chunks[i].matchup_count() << "\t\t\n";
        }

        return out.good();
    }

    // Load current status
    std::vector<ChunkProgress> load_status() {
        std::vector<ChunkProgress> status;
        std::ifstream in(status_file_);
        if (!in) return status;

        std::string header;
        std::getline(in, header);
        if (header != "CHUNK_STATUS_V1") return status;

        u32 count;
        in >> count;
        in.ignore();

        status.reserve(count);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;

            ChunkProgress prog;
            std::istringstream ss(line);
            int status_int;
            ss >> prog.chunk_id >> status_int >> prog.matchups_completed >> prog.matchups_total;
            prog.status = static_cast<ChunkStatus>(status_int);

            // Read remaining tab-separated fields
            std::string field;
            if (std::getline(ss, field, '\t')) {} // skip leading tab
            if (std::getline(ss, prog.output_file, '\t')) {}
            if (std::getline(ss, prog.worker_id, '\t')) {}

            status.push_back(prog);
        }

        return status;
    }

    // Update status for a chunk (atomic file replace)
    bool update_chunk(const ChunkProgress& progress) {
        auto all_status = load_status();
        if (progress.chunk_id >= all_status.size()) return false;

        all_status[progress.chunk_id] = progress;
        return save_status(all_status);
    }

    // Find next pending chunk (returns -1 if none available)
    int claim_next_pending(const std::string& worker_id) {
        auto all_status = load_status();

        for (auto& prog : all_status) {
            if (prog.status == ChunkStatus::Pending) {
                prog.status = ChunkStatus::InProgress;
                prog.worker_id = worker_id;
                if (save_status(all_status)) {
                    return static_cast<int>(prog.chunk_id);
                }
            }
        }

        return -1;
    }

    // Get summary statistics
    struct Summary {
        u32 pending;
        u32 in_progress;
        u32 completed;
        u32 failed;
        u64 matchups_completed;
        u64 matchups_total;

        f64 percent_complete() const {
            return matchups_total > 0 ? 100.0 * matchups_completed / matchups_total : 0.0;
        }
    };

    Summary get_summary() {
        Summary s{};
        auto all_status = load_status();

        for (const auto& prog : all_status) {
            switch (prog.status) {
                case ChunkStatus::Pending: ++s.pending; break;
                case ChunkStatus::InProgress: ++s.in_progress; break;
                case ChunkStatus::Completed:
                    ++s.completed;
                    s.matchups_completed += prog.matchups_total;
                    break;
                case ChunkStatus::Failed: ++s.failed; break;
            }
            s.matchups_total += prog.matchups_total;
        }

        // Add partial progress from in-progress chunks
        for (const auto& prog : all_status) {
            if (prog.status == ChunkStatus::InProgress) {
                s.matchups_completed += prog.matchups_completed;
            }
        }

        return s;
    }

private:
    std::string status_file_;

    bool save_status(const std::vector<ChunkProgress>& status) {
        std::string temp_file = status_file_ + ".tmp";
        std::ofstream out(temp_file);
        if (!out) return false;

        out << "CHUNK_STATUS_V1\n";
        out << status.size() << "\n";

        for (const auto& prog : status) {
            out << prog.chunk_id << "\t" << static_cast<int>(prog.status)
                << "\t" << prog.matchups_completed << "\t" << prog.matchups_total
                << "\t" << prog.output_file << "\t" << prog.worker_id << "\n";
        }

        out.close();

        // Atomic replace
        std::filesystem::rename(temp_file, status_file_);
        return true;
    }
};

} // namespace battle
