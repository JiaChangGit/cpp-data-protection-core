#include "dpc/core/ContentDefinedChunker.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/common/FileUtils.hpp"

#include <algorithm>

namespace dpc {

ContentDefinedChunker::ContentDefinedChunker(std::size_t min_size, std::size_t avg_size, std::size_t max_size)
    : min_size_(min_size), avg_size_(avg_size), max_size_(max_size), mask_(avg_size - 1) {
    require(min_size_ > 0, "CDC min size must be greater than zero");
    require(min_size_ <= avg_size_, "CDC min size must be <= avg size");
    require(avg_size_ <= max_size_, "CDC avg size must be <= max size");
    require((avg_size_ & (avg_size_ - 1)) == 0, "CDC avg size must be a power of two");
}

std::vector<Chunk> ContentDefinedChunker::chunkBytes(const ByteVector& data) const {
    std::vector<Chunk> chunks;
    if (data.empty()) {
        chunks.push_back(Chunk{0, {}});
        return chunks;
    }

    std::uint64_t rolling = 0x9e3779b97f4a7c15ULL;
    std::size_t start = 0;
    std::uint64_t index = 0;

    for (std::size_t i = 0; i < data.size(); ++i) {
        rolling = (rolling << 1U) ^ (rolling >> 23U) ^ data[i] ^ 0x517cc1b727220a95ULL;
        const auto len = i + 1 - start;
        const bool boundary = len >= min_size_ && ((rolling & mask_) == 0);
        if (len >= max_size_ || boundary) {
            Chunk chunk;
            chunk.index = index++;
            chunk.data.insert(chunk.data.end(), data.begin() + static_cast<std::ptrdiff_t>(start), data.begin() + static_cast<std::ptrdiff_t>(i + 1));
            chunks.push_back(std::move(chunk));
            start = i + 1;
            rolling = 0x9e3779b97f4a7c15ULL;
        }
    }

    if (start < data.size()) {
        Chunk chunk;
        chunk.index = index;
        chunk.data.insert(chunk.data.end(), data.begin() + static_cast<std::ptrdiff_t>(start), data.end());
        chunks.push_back(std::move(chunk));
    }
    return chunks;
}

std::vector<Chunk> ContentDefinedChunker::chunkFile(const std::filesystem::path& path) const {
    return chunkBytes(fileutil::readFile(path));
}

}  // namespace dpc
