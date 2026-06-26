#pragma once

#include <cstdint>
#include <filesystem>

namespace dpc {

class VerifyEngine {
public:
    void verify(const std::filesystem::path& repo, std::uint64_t version);
};

}  // namespace dpc
