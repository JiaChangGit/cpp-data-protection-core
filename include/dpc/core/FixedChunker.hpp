#pragma once

#include "dpc/common/Types.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace dpc {

class FixedChunker {
public:
    static constexpr std::size_t kDefaultChunkSize = 64 * 1024;

    explicit FixedChunker(std::size_t chunk_size = kDefaultChunkSize);
    std::vector<Chunk> chunkBytes(const ByteVector& data) const;
    std::vector<Chunk> chunkFile(const std::filesystem::path& path) const;

private:
    std::size_t chunk_size_;
};

}  // namespace dpc
