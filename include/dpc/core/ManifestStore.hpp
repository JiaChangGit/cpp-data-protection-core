#pragma once

#include "dpc/common/Types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace dpc {

class ManifestStore {
public:
    explicit ManifestStore(std::filesystem::path repo);

    std::uint64_t nextVersion() const;
    std::vector<std::uint64_t> listCommittedVersions() const;
    bool isCommitted(std::uint64_t version) const;
    Manifest load(std::uint64_t version) const;
    void writeTmp(std::uint64_t version, const Manifest& manifest) const;
    void renameTmpToManifest(std::uint64_t version) const;
    void writeAtomic(const Manifest& manifest) const;

    std::filesystem::path manifestPath(std::uint64_t version) const;
    std::filesystem::path tmpManifestPath(std::uint64_t version) const;
    std::filesystem::path commitPath(std::uint64_t version) const;

    static std::string serialize(const Manifest& manifest);
    static Manifest parse(const std::string& text);
    static std::string hexEncodeString(const std::string& value);
    static std::string hexDecodeString(const std::string& value);
    static std::string versionFileName(std::uint64_t version, const std::string& suffix);

private:
    std::filesystem::path repo_;
};

}  // namespace dpc
