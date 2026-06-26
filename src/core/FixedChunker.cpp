#include "dpc/core/FixedChunker.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/common/FileUtils.hpp"

#include <algorithm>

namespace dpc {

FixedChunker::FixedChunker(std::size_t chunk_size) : chunk_size_(chunk_size) {
    require(chunk_size_ > 0, "fixed chunk size must be greater than zero");
}

std::vector<Chunk> FixedChunker::chunkBytes(const ByteVector& data) const {
    std::vector<Chunk> chunks;
    std::uint64_t index = 0;
    for (std::size_t offset = 0; offset < data.size(); offset += chunk_size_) {
        const auto end = std::min(offset + chunk_size_, data.size());
        Chunk chunk;
        chunk.index = index++;
        chunk.data.insert(chunk.data.end(), data.begin() + static_cast<std::ptrdiff_t>(offset), data.begin() + static_cast<std::ptrdiff_t>(end));
        chunks.push_back(std::move(chunk));
    }
    if (data.empty()) {
        chunks.push_back(Chunk{0, {}});
    }
    return chunks;
}

std::vector<Chunk> FixedChunker::chunkFile(const std::filesystem::path& path) const {
    return chunkBytes(fileutil::readFile(path));
}

}  // namespace dpc
