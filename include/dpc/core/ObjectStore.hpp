#pragma once

#include "dpc/common/Types.hpp"

#include <filesystem>

namespace dpc {

struct PutObjectResult {
    std::string sha256;
    std::string object_path;
    std::uint64_t raw_size = 0;
    std::uint64_t compressed_size = 0;
    bool inserted = false;
};

struct ObjectStoreStats {
    std::uint64_t object_count = 0;
    std::uint64_t stored_bytes = 0;
};

class ObjectStore {
public:
    explicit ObjectStore(std::filesystem::path repo);

    void ensureLayout() const;
    PutObjectResult putRaw(const ByteVector& raw);
    PutObjectResult putCompressed(const std::string& sha256, const ByteVector& compressed, std::uint64_t raw_size);
    ByteVector readRaw(const std::string& sha256, const std::string& object_path) const;
    ObjectStoreStats stats() const;

    static std::string objectPathForHash(const std::string& sha256);

private:
    std::filesystem::path repo_;
    std::filesystem::path absoluteObjectPath(const std::string& object_path) const;
};

}  // namespace dpc
