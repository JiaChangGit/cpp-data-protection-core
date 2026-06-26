#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dpc {

using ByteVector = std::vector<std::uint8_t>;

enum class ChunkingMode {
    Fixed,
    ContentDefined,
};

inline std::string toString(ChunkingMode mode) {
    return mode == ChunkingMode::Fixed ? "fixed" : "cdc";
}

inline ChunkingMode chunkingModeFromString(const std::string& value) {
    return value == "cdc" ? ChunkingMode::ContentDefined : ChunkingMode::Fixed;
}

struct Chunk {
    std::uint64_t index = 0;
    ByteVector data;
};

struct ChunkRef {
    std::uint64_t chunk_index = 0;
    std::uint64_t raw_size = 0;
    std::uint64_t compressed_size = 0;
    std::string sha256;
    std::string object_path;
};

struct FileManifest {
    std::string relative_path;
    std::uint64_t file_size = 0;
    std::uint32_t file_mode = 0644;
    std::string file_sha256;
    std::vector<ChunkRef> chunks;
};

struct Manifest {
    std::uint64_t version = 0;
    std::uint64_t created_at = 0;
    ChunkingMode chunking_mode = ChunkingMode::Fixed;
    std::string source_root;
    std::uint64_t total_input_bytes = 0;
    std::uint64_t total_stored_bytes = 0;
    std::vector<FileManifest> files;
};

struct BackupResult {
    std::uint64_t version = 0;
    std::uint64_t file_count = 0;
    std::uint64_t total_input_bytes = 0;
    std::uint64_t total_stored_bytes = 0;
    std::uint64_t total_chunks = 0;
    std::uint64_t unique_chunks = 0;
    std::uint64_t duplicate_chunks = 0;
};

}  // namespace dpc
