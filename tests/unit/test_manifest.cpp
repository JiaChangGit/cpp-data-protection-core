#include "dpc/core/ManifestStore.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/common/FileUtils.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace {

std::string validManifestText() {
    dpc::Manifest manifest;
    manifest.version = 1;
    manifest.created_at = 123;
    manifest.chunking_mode = dpc::ChunkingMode::Fixed;
    manifest.source_root = "/tmp/source";
    manifest.total_input_bytes = 4;
    manifest.total_stored_bytes = 13;

    dpc::FileManifest file;
    file.relative_path = "file.txt";
    file.file_size = 4;
    file.file_mode = 0644;
    file.file_sha256 = std::string(64, 'a');

    dpc::ChunkRef chunk;
    chunk.chunk_index = 0;
    chunk.raw_size = 4;
    chunk.compressed_size = 13;
    chunk.sha256 = std::string(64, 'b');
    chunk.object_path = "objects/bb/" + std::string(64, 'b') + ".zst";
    file.chunks.push_back(chunk);
    manifest.files.push_back(file);
    return dpc::ManifestStore::serialize(manifest);
}

std::string replaceFirst(std::string text, const std::string& from, const std::string& to) {
    const auto pos = text.find(from);
    if (pos == std::string::npos) {
        throw std::runtime_error("test fixture replacement failed");
    }
    text.replace(pos, from.size(), to);
    return text;
}

}  // namespace

TEST(ManifestStoreTest, SerializeParseRoundTrip) {
    dpc::Manifest manifest;
    manifest.version = 7;
    manifest.created_at = 123;
    manifest.chunking_mode = dpc::ChunkingMode::ContentDefined;
    manifest.source_root = "/tmp/source with spaces";
    manifest.total_input_bytes = 10;
    manifest.total_stored_bytes = 5;
    dpc::FileManifest file;
    file.relative_path = "dir/file name.txt";
    file.file_size = 10;
    file.file_mode = 0640;
    file.file_sha256 = std::string(64, 'a');
    dpc::ChunkRef chunk;
    chunk.chunk_index = 0;
    chunk.raw_size = 10;
    chunk.compressed_size = 5;
    chunk.sha256 = std::string(64, 'b');
    chunk.object_path = "objects/bb/" + std::string(64, 'b') + ".zst";
    file.chunks.push_back(chunk);
    manifest.files.push_back(file);

    const auto text = dpc::ManifestStore::serialize(manifest);
    const auto parsed = dpc::ManifestStore::parse(text);
    EXPECT_EQ(parsed.version, manifest.version);
    EXPECT_EQ(parsed.chunking_mode, manifest.chunking_mode);
    EXPECT_EQ(parsed.source_root, manifest.source_root);
    ASSERT_EQ(parsed.files.size(), 1U);
    EXPECT_EQ(parsed.files[0].relative_path, file.relative_path);
    ASSERT_EQ(parsed.files[0].chunks.size(), 1U);
    EXPECT_EQ(parsed.files[0].chunks[0].object_path, chunk.object_path);
}

TEST(ManifestStoreTest, RejectsUnsafeRelativePaths) {
    EXPECT_THROW(dpc::fileutil::safeRelativePath("../evil", "relative_path"), dpc::DpcError);
    EXPECT_THROW(dpc::fileutil::safeRelativePath("/tmp/evil", "relative_path"), dpc::DpcError);
    EXPECT_THROW(dpc::fileutil::safeRelativePath("", "relative_path"), dpc::DpcError);
    EXPECT_NO_THROW(dpc::fileutil::safeRelativePath("nested/file.txt", "relative_path"));
}

TEST(ManifestStoreTest, RejectsMalformedManifests) {
    EXPECT_THROW(dpc::ManifestStore::parse(""), dpc::DpcError);
    EXPECT_THROW(dpc::ManifestStore::parse("DPC_MANIFEST_V1\nversion 1\n"), dpc::DpcError);
    EXPECT_THROW(dpc::ManifestStore::parse(replaceFirst(validManifestText(), "version 1", "version nope")), dpc::DpcError);
    EXPECT_THROW(dpc::ManifestStore::parse(replaceFirst(validManifestText(), "4 420", "-1 420")), dpc::DpcError);
    EXPECT_THROW(dpc::ManifestStore::parse(replaceFirst(validManifestText(), std::string(64, 'a'), "bad-hash")), dpc::DpcError);
    EXPECT_THROW(dpc::ManifestStore::parse(replaceFirst(validManifestText(), " 1\nCHUNK", " 2\nCHUNK")), dpc::DpcError);
    auto truncated = validManifestText();
    truncated.erase(truncated.find("END_FILE"));
    EXPECT_THROW(dpc::ManifestStore::parse(truncated), dpc::DpcError);
}

TEST(ManifestStoreTest, RejectsTraversalPathsInManifest) {
    const auto traversal_hex = dpc::ManifestStore::hexEncodeString("../evil.txt");
    const auto safe_hex = dpc::ManifestStore::hexEncodeString("file.txt");
    EXPECT_THROW(dpc::ManifestStore::parse(replaceFirst(validManifestText(), safe_hex, traversal_hex)), dpc::DpcError);

    const auto object_traversal_hex = dpc::ManifestStore::hexEncodeString("../../evil.zst");
    const auto object_hex = dpc::ManifestStore::hexEncodeString("objects/bb/" + std::string(64, 'b') + ".zst");
    EXPECT_THROW(dpc::ManifestStore::parse(replaceFirst(validManifestText(), object_hex, object_traversal_hex)), dpc::DpcError);
}
