#include "dpc/network/TransferSession.hpp"

#include "dpc/common/FileUtils.hpp"
#include "dpc/common/Hash.hpp"
#include "dpc/core/Compressor.hpp"
#include "dpc/core/FileScanner.hpp"
#include "dpc/core/FixedChunker.hpp"

namespace dpc {

std::vector<PreparedChunk> TransferSession::prepareChunks(const std::filesystem::path& source) {
    std::vector<PreparedChunk> out;
    FixedChunker chunker;
    std::uint64_t global_index = 0;
    for (const auto& file : FileScanner::scanRegularFiles(source)) {
        const auto file_sha = Hash::sha256File(file.absolute_path);
        const auto file_size = fileutil::fileSize(file.absolute_path);
        const auto file_mode = fileutil::fileMode(file.absolute_path);
        const auto chunks = chunker.chunkFile(file.absolute_path);
        for (const auto& chunk : chunks) {
            PreparedChunk prepared;
            prepared.global_index = global_index++;
            prepared.relative_path = file.relative_path.generic_string();
            prepared.file_size = file_size;
            prepared.file_mode = file_mode;
            prepared.file_sha256 = file_sha;
            prepared.chunk_index = chunk.index;
            prepared.raw = chunk.data;
            prepared.chunk_sha256 = Hash::sha256Hex(chunk.data);
            prepared.compressed = Compressor::compress(chunk.data);
            out.push_back(std::move(prepared));
        }
    }
    return out;
}

}  // namespace dpc
