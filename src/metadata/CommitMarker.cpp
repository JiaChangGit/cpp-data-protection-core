#include "dpc/metadata/CommitMarker.hpp"

#include "dpc/common/FileUtils.hpp"
#include "dpc/core/ManifestStore.hpp"

#include <algorithm>

namespace dpc {

CommitMarker::CommitMarker(std::filesystem::path repo) : repo_(std::move(repo)) {}

std::filesystem::path CommitMarker::commitPath(std::uint64_t version) const {
    return repo_ / "manifests" / ManifestStore::versionFileName(version, ".commit");
}

void CommitMarker::create(std::uint64_t version) const {
    fileutil::writeTextAtomic(commitPath(version), "committed\n", 0644);
}

bool CommitMarker::exists(std::uint64_t version) const {
    return std::filesystem::exists(commitPath(version));
}

std::vector<std::uint64_t> CommitMarker::listCommittedVersions() const {
    return ManifestStore(repo_).listCommittedVersions();
}

}  // namespace dpc
