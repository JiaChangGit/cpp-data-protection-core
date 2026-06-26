#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace dpc {

class BackupClient {
public:
    int upload(
        const std::filesystem::path& source,
        const std::string& host,
        int port,
        const std::string& session_id,
        std::size_t exit_after_chunks = 0);
};

}  // namespace dpc
