#include "dpc/core/ManifestStore.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/common/FileUtils.hpp"
#include "dpc/common/Hash.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>

namespace dpc {

namespace {

std::string zeroPadVersion(std::uint64_t version) {
    std::ostringstream out;
    out << std::setw(6) << std::setfill('0') << version;
    return out.str();
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

template <typename T>
T parseNumber(const std::string& value, const std::string& field) {
    if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
        })) {
        throw DpcError("invalid manifest number for " + field + ": " + value);
    }
    std::size_t pos = 0;
    unsigned long long parsed = 0;
    try {
        parsed = std::stoull(value, &pos, 10);
    } catch (const std::exception&) {
        throw DpcError("manifest number out of range for " + field + ": " + value);
    }
    if (pos != value.size() || parsed > static_cast<unsigned long long>(std::numeric_limits<T>::max())) {
        throw DpcError("manifest number out of range for " + field + ": " + value);
    }
    return static_cast<T>(parsed);
}

void rejectExtraTokens(std::istringstream& in, const std::string& context) {
    std::string extra;
    if (in >> extra) {
        throw DpcError("unexpected token in " + context + ": " + extra);
    }
}

}  // namespace

ManifestStore::ManifestStore(std::filesystem::path repo) : repo_(std::move(repo)) {}

std::filesystem::path ManifestStore::manifestPath(std::uint64_t version) const {
    return repo_ / "manifests" / versionFileName(version, ".manifest");
}

std::filesystem::path ManifestStore::tmpManifestPath(std::uint64_t version) const {
    return repo_ / "tmp" / versionFileName(version, ".manifest.tmp");
}

std::filesystem::path ManifestStore::commitPath(std::uint64_t version) const {
    return repo_ / "manifests" / versionFileName(version, ".commit");
}

std::string ManifestStore::versionFileName(std::uint64_t version, const std::string& suffix) {
    return "version-" + zeroPadVersion(version) + suffix;
}

std::uint64_t ManifestStore::nextVersion() const {
    auto versions = listCommittedVersions();
    if (versions.empty()) {
        return 1;
    }
    return versions.back() + 1;
}

std::vector<std::uint64_t> ManifestStore::listCommittedVersions() const {
    std::vector<std::uint64_t> versions;
    const auto dir = repo_ / "manifests";
    if (!std::filesystem::exists(dir)) {
        return versions;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto name = entry.path().filename().string();
        if (name.rfind("version-", 0) != 0 || name.size() != std::string("version-000001.commit").size()) {
            continue;
        }
        if (name.substr(name.size() - 7) != ".commit") {
            continue;
        }
        const auto number = name.substr(8, 6);
        versions.push_back(parseNumber<std::uint64_t>(number, "version"));
    }
    std::sort(versions.begin(), versions.end());
    return versions;
}

bool ManifestStore::isCommitted(std::uint64_t version) const {
    return std::filesystem::exists(commitPath(version));
}

Manifest ManifestStore::load(std::uint64_t version) const {
    if (!isCommitted(version)) {
        throw DpcError("version is not committed: " + std::to_string(version));
    }
    return parse(fileutil::readTextFile(manifestPath(version)));
}

void ManifestStore::writeTmp(std::uint64_t version, const Manifest& manifest) const {
    fileutil::writeTextAtomic(tmpManifestPath(version), serialize(manifest), 0644);
}

void ManifestStore::renameTmpToManifest(std::uint64_t version) const {
    const auto tmp = tmpManifestPath(version);
    const auto final = manifestPath(version);
    fileutil::ensureDirectory(final.parent_path());
    if (::rename(tmp.c_str(), final.c_str()) < 0) {
        throw DpcError(fileutil::errnoMessage("rename manifest failed"));
    }
    fileutil::fsyncDirectory(final.parent_path());
}

void ManifestStore::writeAtomic(const Manifest& manifest) const {
    fileutil::writeTextAtomic(manifestPath(manifest.version), serialize(manifest), 0644);
}

std::string ManifestStore::hexEncodeString(const std::string& value) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.resize(value.size() * 2);
    for (std::size_t i = 0; i < value.size(); ++i) {
        const auto c = static_cast<unsigned char>(value[i]);
        out[i * 2] = hex[(c >> 4) & 0x0f];
        out[i * 2 + 1] = hex[c & 0x0f];
    }
    return out;
}

std::string ManifestStore::hexDecodeString(const std::string& value) {
    auto nibble = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') {
            return static_cast<unsigned>(c - '0');
        }
        if (c >= 'a' && c <= 'f') {
            return static_cast<unsigned>(10 + c - 'a');
        }
        if (c >= 'A' && c <= 'F') {
            return static_cast<unsigned>(10 + c - 'A');
        }
        throw DpcError("invalid hex string in manifest");
    };
    if (value.size() % 2 != 0) {
        throw DpcError("invalid hex string length in manifest");
    }
    std::string out;
    out.resize(value.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<char>((nibble(value[i * 2]) << 4U) | nibble(value[i * 2 + 1]));
    }
    return out;
}

std::string ManifestStore::serialize(const Manifest& manifest) {
    std::ostringstream out;
    out << "DPC_MANIFEST_V1\n";
    out << "version " << manifest.version << "\n";
    out << "created_at " << manifest.created_at << "\n";
    out << "chunking_mode " << toString(manifest.chunking_mode) << "\n";
    out << "source_root_hex " << hexEncodeString(manifest.source_root) << "\n";
    out << "file_count " << manifest.files.size() << "\n";
    out << "total_input_bytes " << manifest.total_input_bytes << "\n";
    out << "total_stored_bytes " << manifest.total_stored_bytes << "\n";
    for (const auto& file : manifest.files) {
        out << "FILE " << hexEncodeString(file.relative_path) << " "
            << file.file_size << " " << file.file_mode << " " << file.file_sha256 << " " << file.chunks.size() << "\n";
        for (const auto& chunk : file.chunks) {
            out << "CHUNK " << chunk.chunk_index << " "
                << chunk.raw_size << " "
                << chunk.compressed_size << " "
                << chunk.sha256 << " "
                << hexEncodeString(chunk.object_path) << "\n";
        }
        out << "END_FILE\n";
    }
    out << "END\n";
    return out.str();
}

Manifest ManifestStore::parse(const std::string& text) {
    const auto lines = splitLines(text);
    if (lines.empty() || lines.front() != "DPC_MANIFEST_V1") {
        throw DpcError("invalid manifest header");
    }
    Manifest manifest;
    std::size_t i = 1;
    auto consume = [&](const std::string& key) {
        if (i >= lines.size()) {
            throw DpcError("manifest missing key: " + key);
        }
        std::istringstream in(lines[i++]);
        std::string actual;
        std::string value;
        in >> actual >> value;
        if (actual != key || value.empty()) {
            throw DpcError("manifest expected key: " + key);
        }
        rejectExtraTokens(in, "manifest key " + key);
        return value;
    };

    manifest.version = parseNumber<std::uint64_t>(consume("version"), "version");
    manifest.created_at = parseNumber<std::uint64_t>(consume("created_at"), "created_at");
    const auto chunking_mode = consume("chunking_mode");
    if (chunking_mode != "fixed" && chunking_mode != "cdc") {
        throw DpcError("invalid manifest chunking_mode: " + chunking_mode);
    }
    manifest.chunking_mode = chunkingModeFromString(chunking_mode);
    manifest.source_root = hexDecodeString(consume("source_root_hex"));
    const auto expected_files = parseNumber<std::size_t>(consume("file_count"), "file_count");
    manifest.total_input_bytes = parseNumber<std::uint64_t>(consume("total_input_bytes"), "total_input_bytes");
    manifest.total_stored_bytes = parseNumber<std::uint64_t>(consume("total_stored_bytes"), "total_stored_bytes");

    while (i < lines.size() && lines[i] != "END") {
        std::istringstream file_line(lines[i++]);
        std::string tag;
        std::string rel_hex;
        std::string file_size_text;
        std::string file_mode_text;
        std::string chunk_count_text;
        std::size_t chunk_count = 0;
        FileManifest file;
        file_line >> tag >> rel_hex >> file_size_text >> file_mode_text >> file.file_sha256 >> chunk_count_text;
        if (tag != "FILE" || !file_line) {
            throw DpcError("invalid FILE line in manifest");
        }
        file.file_size = parseNumber<std::uint64_t>(file_size_text, "file_size");
        file.file_mode = parseNumber<std::uint32_t>(file_mode_text, "file_mode");
        chunk_count = parseNumber<std::size_t>(chunk_count_text, "chunk_count");
        file.relative_path = hexDecodeString(rel_hex);
        fileutil::safeRelativePath(file.relative_path, "manifest relative_path");
        Hash::requireSha256Hex(file.file_sha256, "manifest file_sha256");
        rejectExtraTokens(file_line, "FILE line");
        for (std::size_t c = 0; c < chunk_count; ++c) {
            if (i >= lines.size()) {
                throw DpcError("manifest ended inside chunks");
            }
            std::istringstream chunk_line(lines[i++]);
            ChunkRef chunk;
            std::string object_hex;
            std::string chunk_index_text;
            std::string raw_size_text;
            std::string compressed_size_text;
            chunk_line >> tag >> chunk_index_text >> raw_size_text >> compressed_size_text >> chunk.sha256 >> object_hex;
            if (tag != "CHUNK" || !chunk_line) {
                throw DpcError("invalid CHUNK line in manifest");
            }
            chunk.chunk_index = parseNumber<std::uint64_t>(chunk_index_text, "chunk_index");
            chunk.raw_size = parseNumber<std::uint64_t>(raw_size_text, "raw_size");
            chunk.compressed_size = parseNumber<std::uint64_t>(compressed_size_text, "compressed_size");
            chunk.object_path = hexDecodeString(object_hex);
            Hash::requireSha256Hex(chunk.sha256, "manifest chunk sha256");
            fileutil::safeRelativePath(chunk.object_path, "manifest object_path");
            rejectExtraTokens(chunk_line, "CHUNK line");
            file.chunks.push_back(std::move(chunk));
        }
        if (i >= lines.size() || lines[i++] != "END_FILE") {
            throw DpcError("manifest missing END_FILE");
        }
        manifest.files.push_back(std::move(file));
    }
    if (i >= lines.size() || lines[i] != "END") {
        throw DpcError("manifest missing END");
    }
    if (manifest.files.size() != expected_files) {
        throw DpcError("manifest file_count mismatch");
    }
    if (i + 1 != lines.size()) {
        throw DpcError("unexpected data after manifest END");
    }
    return manifest;
}

}  // namespace dpc
