#include "dpc/core/VerifyEngine.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/common/Hash.hpp"
#include "dpc/core/ManifestStore.hpp"
#include "dpc/core/ObjectStore.hpp"

namespace dpc {

void VerifyEngine::verify(const std::filesystem::path& repo, std::uint64_t version) {
    ManifestStore manifests(repo);
    const auto manifest = manifests.load(version);
    ObjectStore objects(repo);

    for (const auto& file : manifest.files) {
        Hash::Sha256 file_hash;
        std::uint64_t actual_size = 0;
        for (const auto& chunk : file.chunks) {
            const auto raw = objects.readRaw(chunk.sha256, chunk.object_path);
            if (raw.size() != chunk.raw_size) {
                throw DpcError("chunk raw size mismatch: " + file.relative_path);
            }
            const auto chunk_sha = Hash::sha256Hex(raw);
            if (chunk_sha != chunk.sha256) {
                throw DpcError("chunk checksum mismatch: " + file.relative_path);
            }
            file_hash.update(raw);
            actual_size += raw.size();
        }
        if (actual_size != file.file_size) {
            throw DpcError("file size mismatch: " + file.relative_path);
        }
        const auto actual_file_hash = file_hash.finalHex();
        if (actual_file_hash != file.file_sha256) {
            throw DpcError("file checksum mismatch: " + file.relative_path);
        }
    }
}

}  // namespace dpc
