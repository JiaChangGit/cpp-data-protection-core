#pragma once

#include "dpc/common/Types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace dpc {

struct PreparedChunk {
    std::uint64_t global_index = 0;
    std::string relative_path;
    std::uint64_t file_size = 0;
    std::uint32_t file_mode = 0644;
    std::string file_sha256;
    std::uint64_t chunk_index = 0;
    std::string chunk_sha256;
    ByteVector raw;
    ByteVector compressed;
};

class TransferSession {
public:
    static std::vector<PreparedChunk> prepareChunks(const std::filesystem::path& source);
};

}  // namespace dpc
