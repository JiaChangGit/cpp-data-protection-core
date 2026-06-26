#pragma once

#include <cstdint>
#include <filesystem>

namespace dpc {

class RestoreEngine {
public:
    void restore(const std::filesystem::path& repo, std::uint64_t version, const std::filesystem::path& target);
};

}  // namespace dpc
