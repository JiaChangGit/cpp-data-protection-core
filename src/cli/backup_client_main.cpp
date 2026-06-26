#include "dpc/common/Error.hpp"
#include "dpc/common/Logger.hpp"
#include "dpc/network/BackupClient.hpp"

#include <filesystem>
#include <iostream>
#include <map>
#include <string>

namespace {

void printUsage() {
    std::cout << "usage: backup-client upload --source <path> --server <host:port> --session <id> [--exit-after-chunks <n>]\n";
}

std::map<std::string, std::string> parseOptions(int argc, char** argv, int start) {
    std::map<std::string, std::string> out;
    for (int i = start; i < argc; ++i) {
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

std::pair<std::string, int> parseServer(const std::string& value) {
    const auto pos = value.rfind(':');
    if (pos == std::string::npos) {
        throw dpc::DpcError("server must be host:port");
    }
    return {value.substr(0, pos), std::stoi(value.substr(pos + 1))};
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            printUsage();
            return 0;
        }
        if (argc < 3 || std::string(argv[1]) != "upload") {
            printUsage();
            return 2;
        }
        const auto opts = parseOptions(argc, argv, 2);
        const auto [host, port] = parseServer(option(opts, "--server"));
        const auto exit_after = static_cast<std::size_t>(std::stoull(option(opts, "--exit-after-chunks", "0")));
        return dpc::BackupClient().upload(option(opts, "--source"), host, port, option(opts, "--session"), exit_after);
    } catch (const std::exception& e) {
        dpc::Logger::error(e.what());
        return 1;
    }
}
