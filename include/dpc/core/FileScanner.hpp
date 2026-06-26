#pragma once

#include <filesystem>
#include <vector>

namespace dpc {

struct ScannedFile {
    std::filesystem::path absolute_path;
    std::filesystem::path relative_path;
};

class FileScanner {
public:
    static std::vector<ScannedFile> scanRegularFiles(const std::filesystem::path& root);
};

}  // namespace dpc
