#include "dpc/common/Error.hpp"
#include "dpc/common/Logger.hpp"
#include "dpc/network/BackupServer.hpp"

#include <csignal>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>

namespace {

void printUsage() {
    std::cout << "usage: backup-server --repo <path> --port <port> [--threads <n>]\n";
}

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
    if (found == opts.end()) {
        if (!fallback.empty()) {
            return fallback;
        }
        throw dpc::DpcError("missing required argument: " + key);
    }
    return found->second;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            printUsage();
            return 0;
        }
        const auto opts = parseOptions(argc, argv);
        dpc::BackupServer server(
            option(opts, "--repo"),
            std::stoi(option(opts, "--port")),
            static_cast<std::size_t>(std::stoull(option(opts, "--threads", "4"))));
        server.run();
        return 0;
    } catch (const std::exception& e) {
        dpc::Logger::error(e.what());
        return 1;
    }
}
