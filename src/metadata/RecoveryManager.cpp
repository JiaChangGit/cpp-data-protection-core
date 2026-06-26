#include "dpc/metadata/RecoveryManager.hpp"

#include "dpc/common/FileUtils.hpp"
#include "dpc/core/ManifestStore.hpp"
#include "dpc/metadata/Checkpoint.hpp"
#include "dpc/metadata/WalLog.hpp"

#include <algorithm>

namespace dpc {

RecoveryManager::RecoveryManager(std::filesystem::path repo) : repo_(std::move(repo)) {}

RecoveryReport RecoveryManager::recover() const {
    fileutil::ensureDirectory(repo_ / "objects");
    fileutil::ensureDirectory(repo_ / "manifests");
    fileutil::ensureDirectory(repo_ / "metadata" / "sessions");
    fileutil::ensureDirectory(repo_ / "tmp");

    WalLog(repo_).readAll();

    std::size_t removed_tmp = 0;
    const auto tmp = repo_ / "tmp";
    if (std::filesystem::exists(tmp)) {
        for (const auto& entry : std::filesystem::directory_iterator(tmp)) {
            if (entry.is_regular_file()) {
                std::filesystem::remove(entry.path());
                ++removed_tmp;
            }
        }
        fileutil::fsyncDirectory(tmp);
    }

    ManifestStore manifests(repo_);
    auto versions = manifests.listCommittedVersions();
    CheckpointData checkpoint;
    checkpoint.committed_versions = versions;
    checkpoint.latest_committed_version = versions.empty() ? 0 : versions.back();
    Checkpoint(repo_).write(checkpoint);

    RecoveryReport report;
    report.committed_versions = versions.size();
    report.removed_tmp_files = removed_tmp;
    report.message = "recovery complete: committed_versions=" + std::to_string(report.committed_versions)
        + " removed_tmp_files=" + std::to_string(report.removed_tmp_files);
    return report;
}

}  // namespace dpc
