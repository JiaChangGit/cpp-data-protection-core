#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace dpc {

struct CheckpointData {
    std::uint64_t latest_committed_version = 0;
    std::vector<std::uint64_t> committed_versions;
};

class Checkpoint {
public:
    explicit Checkpoint(std::filesystem::path repo);

    void write(const CheckpointData& data) const;
    CheckpointData load() const;

private:
    std::filesystem::path path() const;
    std::filesystem::path repo_;
};

}  // namespace dpc
