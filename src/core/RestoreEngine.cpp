#include "dpc/core/RestoreEngine.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/common/FileUtils.hpp"
#include "dpc/common/Hash.hpp"
#include "dpc/core/ManifestStore.hpp"
#include "dpc/core/ObjectStore.hpp"

namespace dpc {

void RestoreEngine::restore(const std::filesystem::path& repo, std::uint64_t version, const std::filesystem::path& target) {
    ManifestStore manifests(repo);
    const auto manifest = manifests.load(version);
    ObjectStore objects(repo);

    fileutil::ensureDirectory(target);
    for (const auto& file : manifest.files) {
        ByteVector assembled;
        assembled.reserve(static_cast<std::size_t>(file.file_size));
        for (const auto& chunk : file.chunks) {
            const auto raw = objects.readRaw(chunk.sha256, chunk.object_path);
            if (raw.size() != chunk.raw_size) {
                throw DpcError("chunk raw size mismatch during restore: " + file.relative_path);
            }
            assembled.insert(assembled.end(), raw.begin(), raw.end());
        }
        if (assembled.size() != file.file_size) {
            throw DpcError("file size mismatch during restore: " + file.relative_path);
        }
        const auto actual_file_hash = Hash::sha256Hex(assembled);
        if (actual_file_hash != file.file_sha256) {
            throw DpcError("file checksum mismatch during restore: " + file.relative_path);
        }

        const auto safe_relative_path = fileutil::safeRelativePath(file.relative_path, "manifest relative_path");
        const auto out_path = target / safe_relative_path;
        fileutil::writeFileAtomic(out_path, assembled, static_cast<mode_t>(file.file_mode));
        fileutil::setFileMode(out_path, file.file_mode);
    }
}

}  // namespace dpc
