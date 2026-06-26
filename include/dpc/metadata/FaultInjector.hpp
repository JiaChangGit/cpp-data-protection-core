#pragma once

#include "dpc/common/Error.hpp"

#include <string>

namespace dpc {

enum class FaultStage {
    None,
    AfterBegin,
    AfterObjectWrite,
    AfterManifestWrite,
    AfterManifestRename,
    AfterCommitMarker,
};

class FaultInjectedCrash : public DpcError {
public:
    explicit FaultInjectedCrash(const std::string& stage);
};

class FaultInjector {
public:
    FaultInjector() = default;
    explicit FaultInjector(FaultStage stage);

    void trigger(FaultStage stage) const;
    FaultStage stage() const;

    static FaultStage parse(const std::string& value);
    static std::string toString(FaultStage stage);

private:
    FaultStage stage_ = FaultStage::None;
    mutable bool fired_ = false;
};

}  // namespace dpc
