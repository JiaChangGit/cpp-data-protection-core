#include "dpc/core/BackupEngine.hpp"

#include "dpc/common/FileUtils.hpp"
#include "dpc/common/Hash.hpp"
#include "dpc/core/ContentDefinedChunker.hpp"
#include "dpc/core/FileScanner.hpp"
#include "dpc/core/FixedChunker.hpp"
#include "dpc/core/ManifestStore.hpp"
#include "dpc/core/ObjectStore.hpp"
#include "dpc/metadata/Checkpoint.hpp"
#include "dpc/metadata/CommitMarker.hpp"
#include "dpc/metadata/WalLog.hpp"

#include <chrono>
#include <set>

namespace dpc {

namespace {

std::uint64_t nowSeconds() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

std::vector<Chunk> chunkFile(const std::filesystem::path& path, ChunkingMode mode) {
    if (mode == ChunkingMode::ContentDefined) {
        return ContentDefinedChunker().chunkFile(path);
    }
    return FixedChunker().chunkFile(path);
}

}  // namespace

BackupResult BackupEngine::create(
    const std::filesystem::path& source,
    const std::filesystem::path& repo,
    ChunkingMode mode,
    const FaultInjector& fault_injector) {

    ObjectStore objects(repo);
    objects.ensureLayout();
    ManifestStore manifests(repo);
    WalLog wal(repo);

    const auto version = manifests.nextVersion();
    wal.append(WalRecordType::BeginBackup, "version=" + std::to_string(version) + "\n");
    fault_injector.trigger(FaultStage::AfterBegin);

    Manifest manifest;
    manifest.version = version;
    manifest.created_at = nowSeconds();
    manifest.chunking_mode = mode;
    manifest.source_root = std::filesystem::absolute(source).string();

    BackupResult result;
    result.version = version;

    const auto files = FileScanner::scanRegularFiles(source);
    std::set<std::string> unique_seen;

    for (const auto& scanned : files) {
        FileManifest file;
        file.relative_path = scanned.relative_path.generic_string();
        file.file_size = fileutil::fileSize(scanned.absolute_path);
        file.file_mode = fileutil::fileMode(scanned.absolute_path);

        auto chunks = chunkFile(scanned.absolute_path, mode);
        Hash::Sha256 file_hash;
        std::uint64_t chunk_index = 0;
        for (const auto& chunk : chunks) {
            file_hash.update(chunk.data);
            auto put = objects.putRaw(chunk.data);
            wal.append(WalRecordType::PutObject, put.sha256 + " " + put.object_path + "\n");
            fault_injector.trigger(FaultStage::AfterObjectWrite);

            ChunkRef ref;
            ref.chunk_index = chunk_index++;
            ref.raw_size = put.raw_size;
            ref.compressed_size = put.compressed_size;
            ref.sha256 = put.sha256;
            ref.object_path = put.object_path;
            file.chunks.push_back(std::move(ref));

            ++result.total_chunks;
            if (put.inserted) {
                ++result.unique_chunks;
                manifest.total_stored_bytes += put.compressed_size;
            } else {
                ++result.duplicate_chunks;
            }
            unique_seen.insert(put.sha256);
        }
        file.file_sha256 = file_hash.finalHex();
        manifest.total_input_bytes += file.file_size;
        manifest.files.push_back(std::move(file));
    }

    result.file_count = manifest.files.size();
    result.total_input_bytes = manifest.total_input_bytes;
    result.total_stored_bytes = manifest.total_stored_bytes;

    manifests.writeTmp(version, manifest);
    wal.append(WalRecordType::WriteManifest, "version=" + std::to_string(version) + "\n");
    fault_injector.trigger(FaultStage::AfterManifestWrite);

    manifests.renameTmpToManifest(version);
    wal.append(WalRecordType::RenameManifest, "version=" + std::to_string(version) + "\n");
    fault_injector.trigger(FaultStage::AfterManifestRename);

    CommitMarker(repo).create(version);
    fault_injector.trigger(FaultStage::AfterCommitMarker);
    wal.append(WalRecordType::CommitBackup, "version=" + std::to_string(version) + "\n");

    auto versions = manifests.listCommittedVersions();
    CheckpointData checkpoint;
    checkpoint.committed_versions = versions;
    checkpoint.latest_committed_version = versions.empty() ? 0 : versions.back();
    Checkpoint(repo).write(checkpoint);

    return result;
}

}  // namespace dpc
