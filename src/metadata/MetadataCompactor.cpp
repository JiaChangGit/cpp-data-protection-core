#include "dpc/metadata/MetadataCompactor.hpp"

#include "dpc/core/ManifestStore.hpp"
#include "dpc/metadata/Checkpoint.hpp"
#include "dpc/metadata/WalLog.hpp"

#include <sstream>

namespace dpc {

MetadataCompactor::MetadataCompactor(std::filesystem::path repo) : repo_(std::move(repo)) {}

void MetadataCompactor::compact() const {
    auto versions = ManifestStore(repo_).listCommittedVersions();
    CheckpointData checkpoint;
    checkpoint.committed_versions = versions;
    checkpoint.latest_committed_version = versions.empty() ? 0 : versions.back();
    Checkpoint(repo_).write(checkpoint);

    std::ostringstream payload;
    payload << "latest=" << checkpoint.latest_committed_version << "\n";
    payload << "versions";
    for (auto version : versions) {
        payload << " " << version;
    }
    payload << "\n";

    WalLog wal(repo_);
    std::vector<WalRecord> records;
    const auto checkpoint_payload = payload.str();
    records.push_back(WalRecord{WalRecordType::Checkpoint, ByteVector(checkpoint_payload.begin(), checkpoint_payload.end())});
    records.push_back(WalRecord{WalRecordType::Compact, ByteVector{'c', 'o', 'm', 'p', 'a', 'c', 't'}});
    wal.replaceWith(records);
}

}  // namespace dpc
