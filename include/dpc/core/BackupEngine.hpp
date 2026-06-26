#pragma once

#include "dpc/common/Types.hpp"
#include "dpc/metadata/FaultInjector.hpp"

#include <filesystem>

namespace dpc {

class BackupEngine {
public:
    BackupResult create(
        const std::filesystem::path& source,
        const std::filesystem::path& repo,
        ChunkingMode mode,
        const FaultInjector& fault_injector = FaultInjector());
};

}  // namespace dpc
