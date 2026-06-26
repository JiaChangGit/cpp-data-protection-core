#include "dpc/common/Error.hpp"
#include "dpc/common/FileUtils.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

namespace {

std::map<std::string, std::string> parseOptions(int argc, char** argv) {
    std::map<std::string, std::string> out;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "--wal-garbage") {
            out[key] = "1";
            continue;
        }
        if (key.rfind("--", 0) != 0 || i + 1 >= argc) {
            throw dpc::DpcError("invalid argument: " + key);
        }
        out[key] = argv[++i];
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto opts = parseOptions(argc, argv);
        const auto repo = std::filesystem::path(opts.at("--repo"));
        std::ofstream out(repo / "metadata" / "wal.log", std::ios::binary | std::ios::app);
        if (!out) {
            throw dpc::DpcError("open wal failed for corruption");
        }
        out << "garbage";
        std::cout << "corrupted wal\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
