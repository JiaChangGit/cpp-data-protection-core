#pragma once

#include "dpc/common/Types.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace dpc {

class ContentDefinedChunker {
public:
    ContentDefinedChunker(
        std::size_t min_size = 16 * 1024,
        std::size_t avg_size = 64 * 1024,
        std::size_t max_size = 256 * 1024);

    std::vector<Chunk> chunkBytes(const ByteVector& data) const;
    std::vector<Chunk> chunkFile(const std::filesystem::path& path) const;

private:
    std::size_t min_size_;
    std::size_t avg_size_;
    std::size_t max_size_;
    std::uint64_t mask_;
};

}  // namespace dpc
