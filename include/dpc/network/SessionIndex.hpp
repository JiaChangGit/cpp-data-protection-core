#pragma once

#include "dpc/common/Types.hpp"

#include <filesystem>
#include <map>
#include <set>
#include <string>

namespace dpc {

struct ReceivedChunkRecord {
    std::uint64_t global_index = 0;
    std::string sha256;
    std::string relative_path;
    std::uint64_t file_size = 0;
    std::uint32_t file_mode = 0644;
    std::string file_sha256;
    std::uint64_t chunk_index = 0;
    std::uint64_t raw_size = 0;
    std::uint64_t compressed_size = 0;
    std::string object_path;
};

class SessionIndex {
public:
    SessionIndex(std::filesystem::path repo, std::string session_id);

    std::map<std::uint64_t, std::string> received() const;
    void appendChunk(const ReceivedChunkRecord& record) const;
    Manifest buildManifest(std::uint64_t version, std::uint64_t total_chunks) const;
    void markCommitted(std::uint64_t version) const;

private:
    std::vector<ReceivedChunkRecord> records() const;
    std::filesystem::path path() const;

    std::filesystem::path repo_;
    std::string session_id_;
};

}  // namespace dpc
