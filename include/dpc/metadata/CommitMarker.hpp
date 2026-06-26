#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace dpc {

class CommitMarker {
public:
    explicit CommitMarker(std::filesystem::path repo);

    void create(std::uint64_t version) const;
    bool exists(std::uint64_t version) const;
    std::vector<std::uint64_t> listCommittedVersions() const;

private:
    std::filesystem::path commitPath(std::uint64_t version) const;
    std::filesystem::path repo_;
};

}  // namespace dpc
