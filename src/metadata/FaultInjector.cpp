#include "dpc/metadata/FaultInjector.hpp"

#include <cstdlib>

namespace dpc {

FaultInjectedCrash::FaultInjectedCrash(const std::string& stage)
    : DpcError("injected crash at " + stage) {}

FaultInjector::FaultInjector(FaultStage stage) : stage_(stage) {}

void FaultInjector::trigger(FaultStage stage) const {
    if (stage_ == FaultStage::None || stage_ != stage || fired_) {
        return;
    }
    fired_ = true;
    const auto name = toString(stage);
    const char* abort_mode = std::getenv("DPC_FAULT_ABORT");
    if (abort_mode && std::string(abort_mode) == "1") {
        std::abort();
    }
    throw FaultInjectedCrash(name);
}

FaultStage FaultInjector::stage() const {
    return stage_;
}

FaultStage FaultInjector::parse(const std::string& value) {
    if (value == "none" || value.empty()) {
        return FaultStage::None;
    }
    if (value == "after-begin") {
        return FaultStage::AfterBegin;
    }
    if (value == "after-object-write") {
        return FaultStage::AfterObjectWrite;
    }
    if (value == "after-manifest-write") {
        return FaultStage::AfterManifestWrite;
    }
    if (value == "after-manifest-rename") {
        return FaultStage::AfterManifestRename;
    }
    if (value == "after-commit-marker") {
        return FaultStage::AfterCommitMarker;
    }
    throw DpcError("unknown fault stage: " + value);
}

std::string FaultInjector::toString(FaultStage stage) {
    switch (stage) {
        case FaultStage::None:
            return "none";
        case FaultStage::AfterBegin:
            return "after-begin";
        case FaultStage::AfterObjectWrite:
            return "after-object-write";
        case FaultStage::AfterManifestWrite:
            return "after-manifest-write";
        case FaultStage::AfterManifestRename:
            return "after-manifest-rename";
        case FaultStage::AfterCommitMarker:
            return "after-commit-marker";
    }
    return "unknown";
}

}  // namespace dpc
