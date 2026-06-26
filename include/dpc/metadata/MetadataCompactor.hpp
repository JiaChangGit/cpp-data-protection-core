#pragma once

#include <filesystem>

namespace dpc {

class MetadataCompactor {
public:
    explicit MetadataCompactor(std::filesystem::path repo);

    void compact() const;

private:
    std::filesystem::path repo_;
};

}  // namespace dpc
