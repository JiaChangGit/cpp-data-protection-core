#include "dpc/common/FileUtils.hpp"
#include "dpc/core/BackupEngine.hpp"
#include "dpc/core/ManifestStore.hpp"
#include "dpc/core/ObjectStore.hpp"
#include "dpc/core/RestoreEngine.hpp"
#include "dpc/core/VerifyEngine.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <unistd.h>

namespace {

using Clock = std::chrono::steady_clock;

std::map<std::string, std::string> parseOptions(int argc, char** argv) {
    std::map<std::string, std::string> out;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key.rfind("--", 0) != 0 || i + 1 >= argc) {
            throw dpc::DpcError("invalid argument: " + key);
        }
        out[key] = argv[++i];
    }
    return out;
}

std::string option(const std::map<std::string, std::string>& opts, const std::string& key, const std::string& fallback = "") {
    const auto found = opts.find(key);
    return found == opts.end() ? fallback : found->second;
}

std::uint64_t parseSize(const std::string& value) {
    if (value.empty()) {
        return 0;
    }
    const char suffix = value.back();
    std::uint64_t multiplier = 1;
    std::string number = value;
    if (suffix == 'K' || suffix == 'k') {
        multiplier = 1024ULL;
        number.pop_back();
    } else if (suffix == 'M' || suffix == 'm') {
        multiplier = 1024ULL * 1024ULL;
        number.pop_back();
    } else if (suffix == 'G' || suffix == 'g') {
        multiplier = 1024ULL * 1024ULL * 1024ULL;
        number.pop_back();
    }
    return static_cast<std::uint64_t>(std::stoull(number)) * multiplier;
}

void writePatternFile(const std::filesystem::path& path, std::uint64_t size, std::uint8_t seed) {
    dpc::fileutil::ensureDirectory(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw dpc::DpcError("open benchmark file failed: " + path.string());
    }
    std::array<char, 64 * 1024> buffer {};
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = static_cast<char>((i + seed) % 251);
    }
    std::uint64_t remaining = size;
    while (remaining > 0) {
        const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, buffer.size()));
        out.write(buffer.data(), static_cast<std::streamsize>(n));
        remaining -= n;
    }
}

double seconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double>(end - start).count();
}

std::vector<std::filesystem::path> regularFilesUnder(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file()) {
            files.push_back(std::filesystem::relative(entry.path(), root));
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

void requireDirectoriesEqual(const std::filesystem::path& expected, const std::filesystem::path& actual) {
    const auto expected_files = regularFilesUnder(expected);
    const auto actual_files = regularFilesUnder(actual);
    if (expected_files != actual_files) {
        throw dpc::DpcError("benchmark restore file list mismatch");
    }
    for (const auto& relative : expected_files) {
        const auto expected_data = dpc::fileutil::readFile(expected / relative);
        const auto actual_data = dpc::fileutil::readFile(actual / relative);
        if (expected_data != actual_data) {
            throw dpc::DpcError("benchmark restore content mismatch: " + relative.string());
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto opts = parseOptions(argc, argv);
        const auto workload = option(opts, "--workload", "large-file");
        const auto chunking_text = option(opts, "--chunking", "fixed");
        if (chunking_text != "fixed" && chunking_text != "cdc") {
            throw dpc::DpcError("invalid chunking mode: " + chunking_text);
        }
        const auto chunking_mode = dpc::chunkingModeFromString(chunking_text);
        const auto root = std::filesystem::temp_directory_path() / ("dpc-bench-" + std::to_string(::getpid()));
        const auto source = root / "source";
        const auto repo = root / "repo";
        const auto restore = root / "restore";
        dpc::fileutil::ensureDirectory(source);

        if (workload == "small-files") {
            const auto files = static_cast<int>(std::stoi(option(opts, "--files", "1000")));
            const auto file_size = parseSize(option(opts, "--file-size", "4096"));
            for (int i = 0; i < files; ++i) {
                writePatternFile(source / ("file-" + std::to_string(i) + ".bin"), file_size, static_cast<std::uint8_t>(i));
            }
        } else if (workload == "duplicated") {
            const auto size = parseSize(option(opts, "--size", "64M"));
            writePatternFile(source / "a.bin", size / 2, 7);
            writePatternFile(source / "b.bin", size / 2, 7);
        } else if (workload == "modified") {
            const auto size = parseSize(option(opts, "--base-size", "64M"));
            writePatternFile(source / "base.bin", size, 11);
            writePatternFile(source / "modified.bin", size + std::max<std::uint64_t>(1, size / 100), 12);
        } else {
            const auto size = parseSize(option(opts, "--size", "64M"));
            writePatternFile(source / "large.bin", size, 3);
        }

        const auto backup_start = Clock::now();
        const auto result = dpc::BackupEngine().create(source, repo, chunking_mode);
        const auto backup_end = Clock::now();
        const auto verify_start = Clock::now();
        dpc::VerifyEngine().verify(repo, result.version);
        const auto verify_end = Clock::now();
        const auto restore_start = Clock::now();
        dpc::RestoreEngine().restore(repo, result.version, restore);
        const auto restore_end = Clock::now();
        requireDirectoriesEqual(source, restore);

        const auto object_stats = dpc::ObjectStore(repo).stats();
        const double backup_s = seconds(backup_start, backup_end);
        const double verify_s = seconds(verify_start, verify_end);
        const double restore_s = seconds(restore_start, restore_end);
        const double input_mb = static_cast<double>(result.total_input_bytes) / (1024.0 * 1024.0);

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "workload: " << workload << "\n";
        std::cout << "chunking mode: " << dpc::toString(chunking_mode) << "\n";
        std::cout << "total input size: " << result.total_input_bytes << "\n";
        std::cout << "stored object size: " << object_stats.stored_bytes << "\n";
        std::cout << "dedup ratio: " << (result.unique_chunks == 0 ? 1.0 : static_cast<double>(result.total_chunks) / result.unique_chunks) << "\n";
        std::cout << "compression ratio: " << (result.total_input_bytes == 0 ? 1.0 : static_cast<double>(object_stats.stored_bytes) / result.total_input_bytes) << "\n";
        std::cout << "backup throughput MB/s: " << (backup_s == 0 ? 0.0 : input_mb / backup_s) << "\n";
        std::cout << "restore throughput MB/s: " << (restore_s == 0 ? 0.0 : input_mb / restore_s) << "\n";
        std::cout << "verify throughput MB/s: " << (verify_s == 0 ? 0.0 : input_mb / verify_s) << "\n";
        std::cout << "file count: " << result.file_count << "\n";
        std::cout << "chunk count: " << result.total_chunks << "\n";
        std::cout << "unique chunk count: " << result.unique_chunks << "\n";
        std::cout << "duplicate chunk count: " << result.duplicate_chunks << "\n";
        std::cout << "elapsed time: " << (backup_s + verify_s + restore_s) << "\n";

        std::filesystem::remove_all(root);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
