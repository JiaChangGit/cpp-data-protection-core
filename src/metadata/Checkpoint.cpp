#include "dpc/metadata/Checkpoint.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/common/FileUtils.hpp"

#include <sstream>

namespace dpc {

Checkpoint::Checkpoint(std::filesystem::path repo) : repo_(std::move(repo)) {}

std::filesystem::path Checkpoint::path() const {
    return repo_ / "metadata" / "checkpoint.dat";
}

void Checkpoint::write(const CheckpointData& data) const {
    std::ostringstream out;
    out << "DPC_CHECKPOINT_V1\n";
    out << "latest_committed_version " << data.latest_committed_version << "\n";
    out << "committed_versions";
    for (const auto version : data.committed_versions) {
        out << " " << version;
    }
    out << "\n";
    fileutil::writeTextAtomic(path(), out.str(), 0644);
}

CheckpointData Checkpoint::load() const {
    CheckpointData data;
    if (!std::filesystem::exists(path())) {
        return data;
    }
    std::istringstream in(fileutil::readTextFile(path()));
    std::string header;
    in >> header;
    if (header != "DPC_CHECKPOINT_V1") {
        throw DpcError("invalid checkpoint header");
    }
    std::string key;
    in >> key >> data.latest_committed_version;
    if (key != "latest_committed_version") {
        throw DpcError("invalid checkpoint latest key");
    }
    in >> key;
    if (key != "committed_versions") {
        throw DpcError("invalid checkpoint versions key");
    }
    std::uint64_t version = 0;
    while (in >> version) {
        data.committed_versions.push_back(version);
    }
    return data;
}

}  // namespace dpc
