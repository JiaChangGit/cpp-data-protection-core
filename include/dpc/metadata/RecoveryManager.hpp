#pragma once

#include <filesystem>
#include <string>

namespace dpc {

struct RecoveryReport {
    std::string message;
    std::size_t committed_versions = 0;
    std::size_t removed_tmp_files = 0;
};

class RecoveryManager {
public:
    explicit RecoveryManager(std::filesystem::path repo);

    RecoveryReport recover() const;

private:
    std::filesystem::path repo_;
};

}  // namespace dpc
