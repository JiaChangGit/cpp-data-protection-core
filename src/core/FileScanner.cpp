#include "dpc/core/FileScanner.hpp"

#include "dpc/common/Error.hpp"

#include <algorithm>

namespace dpc {

std::vector<ScannedFile> FileScanner::scanRegularFiles(const std::filesystem::path& root) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        throw DpcError("source path is not a directory: " + root.string());
    }

    std::vector<ScannedFile> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file()) {
            files.push_back(ScannedFile{
                std::filesystem::absolute(entry.path()),
                std::filesystem::relative(entry.path(), root)
            });
        }
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        return a.relative_path.generic_string() < b.relative_path.generic_string();
    });
    return files;
}

}  // namespace dpc
