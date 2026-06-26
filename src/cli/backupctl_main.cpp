#include "dpc/common/Error.hpp"
#include "dpc/common/Logger.hpp"
#include "dpc/core/BackupEngine.hpp"
#include "dpc/core/ManifestStore.hpp"
#include "dpc/core/ObjectStore.hpp"
#include "dpc/core/RestoreEngine.hpp"
#include "dpc/core/VerifyEngine.hpp"
#include "dpc/metadata/Checkpoint.hpp"
#include "dpc/metadata/FaultInjector.hpp"
#include "dpc/metadata/MetadataCompactor.hpp"
#include "dpc/metadata/RecoveryManager.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>

namespace {

using dpc::DpcError;

std::map<std::string, std::string> parseOptions(int argc, char** argv, int start) {
    std::map<std::string, std::string> out;
    for (int i = start; i < argc; ++i) {
        std::string key = argv[i];
        if (key.rfind("--", 0) != 0) {
            throw DpcError("unexpected argument: " + key);
        }
        if (i + 1 >= argc) {
            throw DpcError("missing value for argument: " + key);
        }
        out[key] = argv[++i];
    }
    return out;
}

std::string option(const std::map<std::string, std::string>& opts, const std::string& key, const std::string& fallback = "") {
    const auto found = opts.find(key);
    if (found == opts.end()) {
        if (!fallback.empty()) {
            return fallback;
        }
        throw DpcError("missing required argument: " + key);
    }
    return found->second;
}

std::uint64_t parseVersion(const std::map<std::string, std::string>& opts) {
    return static_cast<std::uint64_t>(std::stoull(option(opts, "--version")));
}

void printUsage() {
    std::cerr
        << "usage:\n"
        << "  backupctl create --source <path> --repo <path> [--chunking fixed|cdc] [--fault-stage <stage>]\n"
        << "  backupctl list --repo <path>\n"
        << "  backupctl restore --repo <path> --version <n> --target <path>\n"
        << "  backupctl verify --repo <path> --version <n>\n"
        << "  backupctl stats --repo <path>\n"
        << "  backupctl recover --repo <path>\n"
        << "  backupctl checkpoint --repo <path>\n"
        << "  backupctl compact --repo <path>\n";
}

void printStats(const std::filesystem::path& repo) {
    dpc::ManifestStore manifests(repo);
    const auto versions = manifests.listCommittedVersions();
    const auto object_stats = dpc::ObjectStore(repo).stats();
    std::uint64_t total_chunks = 0;
    std::uint64_t total_input = 0;
    std::set<std::string> unique_hashes;
    for (const auto version : versions) {
        const auto manifest = manifests.load(version);
        total_input += manifest.total_input_bytes;
        for (const auto& file : manifest.files) {
            for (const auto& chunk : file.chunks) {
                ++total_chunks;
                unique_hashes.insert(chunk.sha256);
            }
        }
    }
    const auto unique_chunks = static_cast<std::uint64_t>(unique_hashes.size());
    const auto duplicate_chunks = total_chunks > unique_chunks ? total_chunks - unique_chunks : 0;
    const double dedup_ratio = total_chunks == 0 ? 1.0 : static_cast<double>(total_chunks) / static_cast<double>(unique_chunks == 0 ? 1 : unique_chunks);
    const double compression_ratio = total_input == 0 ? 1.0 : static_cast<double>(object_stats.stored_bytes) / static_cast<double>(total_input);

    std::cout << "versions: " << versions.size() << "\n";
    std::cout << "objects: " << object_stats.object_count << "\n";
    std::cout << "object_bytes: " << object_stats.stored_bytes << "\n";
    std::cout << "total_input_bytes: " << total_input << "\n";
    std::cout << "total_chunks: " << total_chunks << "\n";
    std::cout << "unique_chunks: " << unique_chunks << "\n";
    std::cout << "duplicate_chunks: " << duplicate_chunks << "\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "dedup_ratio: " << dedup_ratio << "\n";
    std::cout << "compression_ratio: " << compression_ratio << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            printUsage();
            return argc < 2 ? 2 : 0;
        }
        const std::string command = argv[1];
        const auto opts = parseOptions(argc, argv, 2);

        if (command == "create") {
            const auto mode = dpc::chunkingModeFromString(option(opts, "--chunking", "fixed"));
            const auto fault = dpc::FaultInjector(dpc::FaultInjector::parse(option(opts, "--fault-stage", "none")));
            const auto result = dpc::BackupEngine().create(option(opts, "--source"), option(opts, "--repo"), mode, fault);
            std::cout << "created_version: " << result.version << "\n";
            std::cout << "files: " << result.file_count << "\n";
            std::cout << "total_input_bytes: " << result.total_input_bytes << "\n";
            std::cout << "stored_bytes: " << result.total_stored_bytes << "\n";
            std::cout << "total_chunks: " << result.total_chunks << "\n";
            std::cout << "unique_chunks: " << result.unique_chunks << "\n";
            std::cout << "duplicate_chunks: " << result.duplicate_chunks << "\n";
            return 0;
        }

        const auto repo = std::filesystem::path(option(opts, "--repo"));
        if (command == "list") {
            dpc::ManifestStore manifests(repo);
            const auto versions = manifests.listCommittedVersions();
            for (const auto version : versions) {
                const auto manifest = manifests.load(version);
                std::cout << "version: " << version
                          << " files: " << manifest.files.size()
                          << " input_bytes: " << manifest.total_input_bytes
                          << " stored_bytes: " << manifest.total_stored_bytes
                          << " chunking: " << dpc::toString(manifest.chunking_mode) << "\n";
            }
            if (versions.empty()) {
                std::cout << "no committed versions\n";
            }
            return 0;
        }
        if (command == "restore") {
            dpc::RestoreEngine().restore(repo, parseVersion(opts), option(opts, "--target"));
            std::cout << "restore_ok\n";
            return 0;
        }
        if (command == "verify") {
            dpc::VerifyEngine().verify(repo, parseVersion(opts));
            std::cout << "verify_ok\n";
            return 0;
        }
        if (command == "stats") {
            printStats(repo);
            return 0;
        }
        if (command == "recover") {
            const auto report = dpc::RecoveryManager(repo).recover();
            std::cout << report.message << "\n";
            return 0;
        }
        if (command == "checkpoint") {
            dpc::ManifestStore manifests(repo);
            dpc::CheckpointData checkpoint;
            checkpoint.committed_versions = manifests.listCommittedVersions();
            checkpoint.latest_committed_version = checkpoint.committed_versions.empty() ? 0 : checkpoint.committed_versions.back();
            dpc::Checkpoint(repo).write(checkpoint);
            std::cout << "checkpoint_ok latest_committed_version=" << checkpoint.latest_committed_version << "\n";
            return 0;
        }
        if (command == "compact") {
            dpc::MetadataCompactor(repo).compact();
            std::cout << "compact_ok\n";
            return 0;
        }

        printUsage();
        return 2;
    } catch (const dpc::FaultInjectedCrash& e) {
        dpc::Logger::error(e.what());
        return 86;
    } catch (const std::exception& e) {
        dpc::Logger::error(e.what());
        return 1;
    }
}
