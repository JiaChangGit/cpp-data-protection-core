#include "dpc/network/SessionIndex.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/common/FileUtils.hpp"
#include "dpc/core/ManifestStore.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace dpc {

SessionIndex::SessionIndex(std::filesystem::path repo, std::string session_id)
    : repo_(std::move(repo)), session_id_(std::move(session_id)) {}

std::filesystem::path SessionIndex::path() const {
    return repo_ / "metadata" / "sessions" / (ManifestStore::hexEncodeString(session_id_) + ".session");
}

std::vector<ReceivedChunkRecord> SessionIndex::records() const {
    std::vector<ReceivedChunkRecord> out;
    if (!std::filesystem::exists(path())) {
        return out;
    }
    std::istringstream in(fileutil::readTextFile(path()));
    std::string tag;
    while (in >> tag) {
        if (tag == "CHUNK") {
            ReceivedChunkRecord r;
            std::string rel_hex;
            std::string obj_hex;
            in >> r.global_index >> r.sha256 >> rel_hex >> r.file_size >> r.file_mode
               >> r.file_sha256 >> r.chunk_index >> r.raw_size >> r.compressed_size >> obj_hex;
            if (!in) {
                throw DpcError("invalid session CHUNK record");
            }
            r.relative_path = ManifestStore::hexDecodeString(rel_hex);
            r.object_path = ManifestStore::hexDecodeString(obj_hex);
            out.push_back(std::move(r));
        } else if (tag == "COMMITTED") {
            std::string rest;
            std::getline(in, rest);
        } else {
            throw DpcError("invalid session index tag: " + tag);
        }
    }
    return out;
}

std::map<std::uint64_t, std::string> SessionIndex::received() const {
    std::map<std::uint64_t, std::string> out;
    for (const auto& record : records()) {
        out[record.global_index] = record.sha256;
    }
    return out;
}

void SessionIndex::appendChunk(const ReceivedChunkRecord& record) const {
    auto existing = received();
    const auto found = existing.find(record.global_index);
    if (found != existing.end()) {
        if (found->second != record.sha256) {
            throw DpcError("session index checksum mismatch for duplicate chunk index");
        }
        return;
    }

    fileutil::ensureDirectory(path().parent_path());
    std::ofstream out(path(), std::ios::app);
    if (!out) {
        throw DpcError("open session index failed: " + path().string());
    }
    out << "CHUNK "
        << record.global_index << " "
        << record.sha256 << " "
        << ManifestStore::hexEncodeString(record.relative_path) << " "
        << record.file_size << " "
        << record.file_mode << " "
        << record.file_sha256 << " "
        << record.chunk_index << " "
        << record.raw_size << " "
        << record.compressed_size << " "
        << ManifestStore::hexEncodeString(record.object_path) << "\n";
    out.flush();
    if (!out) {
        throw DpcError("write session index failed: " + path().string());
    }
}

Manifest SessionIndex::buildManifest(std::uint64_t version, std::uint64_t total_chunks) const {
    auto recs = records();
    std::map<std::uint64_t, ReceivedChunkRecord> by_global;
    for (const auto& record : recs) {
        by_global[record.global_index] = record;
    }
    for (std::uint64_t i = 0; i < total_chunks; ++i) {
        if (by_global.find(i) == by_global.end()) {
            throw DpcError("session is missing chunk index " + std::to_string(i));
        }
    }

    Manifest manifest;
    manifest.version = version;
    manifest.created_at = static_cast<std::uint64_t>(std::time(nullptr));
    manifest.chunking_mode = ChunkingMode::Fixed;
    manifest.source_root = "network-session:" + session_id_;

    std::map<std::string, FileManifest> files;
    std::set<std::string> unique_hashes;
    for (const auto& [_, record] : by_global) {
        auto& file = files[record.relative_path];
        if (file.relative_path.empty()) {
            file.relative_path = record.relative_path;
            file.file_size = record.file_size;
            file.file_mode = record.file_mode;
            file.file_sha256 = record.file_sha256;
            manifest.total_input_bytes += record.file_size;
        }
        ChunkRef ref;
        ref.chunk_index = record.chunk_index;
        ref.raw_size = record.raw_size;
        ref.compressed_size = record.compressed_size;
        ref.sha256 = record.sha256;
        ref.object_path = record.object_path;
        file.chunks.push_back(std::move(ref));
        if (unique_hashes.insert(record.sha256).second) {
            manifest.total_stored_bytes += record.compressed_size;
        }
    }

    for (auto& [_, file] : files) {
        std::sort(file.chunks.begin(), file.chunks.end(), [](const auto& a, const auto& b) {
            return a.chunk_index < b.chunk_index;
        });
        manifest.files.push_back(std::move(file));
    }
    return manifest;
}

void SessionIndex::markCommitted(std::uint64_t version) const {
    fileutil::ensureDirectory(path().parent_path());
    std::ofstream out(path(), std::ios::app);
    if (!out) {
        throw DpcError("open session index failed: " + path().string());
    }
    out << "COMMITTED " << version << "\n";
}

}  // namespace dpc
