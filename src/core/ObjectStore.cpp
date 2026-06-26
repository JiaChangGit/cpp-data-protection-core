#include "dpc/core/ObjectStore.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/common/FileUtils.hpp"
#include "dpc/common/Hash.hpp"
#include "dpc/core/Compressor.hpp"

#include <filesystem>

namespace dpc {

ObjectStore::ObjectStore(std::filesystem::path repo) : repo_(std::move(repo)) {}

void ObjectStore::ensureLayout() const {
    fileutil::ensureDirectory(repo_ / "objects");
    fileutil::ensureDirectory(repo_ / "manifests");
    fileutil::ensureDirectory(repo_ / "metadata" / "sessions");
    fileutil::ensureDirectory(repo_ / "tmp");
}

std::string ObjectStore::objectPathForHash(const std::string& sha256) {
    Hash::requireSha256Hex(sha256, "sha256");
    return "objects/" + sha256.substr(0, 2) + "/" + sha256 + ".zst";
}

std::filesystem::path ObjectStore::absoluteObjectPath(const std::string& object_path) const {
    const auto safe = fileutil::safeRelativePath(object_path, "object_path");
    auto it = safe.begin();
    if (it == safe.end() || it->generic_string() != "objects") {
        throw DpcError("object_path must stay under objects/");
    }
    return repo_ / safe;
}

PutObjectResult ObjectStore::putRaw(const ByteVector& raw) {
    const auto sha = Hash::sha256Hex(raw);
    const auto compressed = Compressor::compress(raw);
    return putCompressed(sha, compressed, raw.size());
}

PutObjectResult ObjectStore::putCompressed(const std::string& sha256, const ByteVector& compressed, std::uint64_t raw_size) {
    ensureLayout();
    Hash::requireSha256Hex(sha256, "sha256");
    const auto rel = objectPathForHash(sha256);
    const auto abs = absoluteObjectPath(rel);

    PutObjectResult result;
    result.sha256 = sha256;
    result.object_path = rel;
    result.raw_size = raw_size;
    result.compressed_size = compressed.size();

    if (std::filesystem::exists(abs)) {
        const auto raw = readRaw(sha256, rel);
        if (raw.size() != raw_size) {
            throw DpcError("existing object raw size mismatch: " + sha256);
        }
        result.inserted = false;
        result.compressed_size = fileutil::fileSize(abs);
        return result;
    }

    fileutil::writeFileAtomic(abs, compressed, 0644);
    result.inserted = true;
    return result;
}

ByteVector ObjectStore::readRaw(const std::string& sha256, const std::string& object_path) const {
    const auto expected_path = objectPathForHash(sha256);
    if (object_path != expected_path) {
        throw DpcError("object path does not match chunk hash: " + object_path);
    }
    const auto abs = absoluteObjectPath(object_path);
    if (!std::filesystem::exists(abs)) {
        throw DpcError("object missing: " + object_path);
    }
    const auto compressed = fileutil::readFile(abs);
    const auto raw = Compressor::decompress(compressed);
    const auto actual = Hash::sha256Hex(raw);
    if (actual != sha256) {
        throw DpcError("object checksum mismatch: " + object_path);
    }
    return raw;
}

ObjectStoreStats ObjectStore::stats() const {
    ObjectStoreStats out;
    const auto root = repo_ / "objects";
    if (!std::filesystem::exists(root)) {
        return out;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file()) {
            ++out.object_count;
            out.stored_bytes += fileutil::fileSize(entry.path());
        }
    }
    return out;
}

}  // namespace dpc
