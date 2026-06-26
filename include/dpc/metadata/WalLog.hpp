#pragma once

#include "dpc/common/Types.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dpc {

enum class WalRecordType : std::uint16_t {
    BeginBackup = 1,
    PutObject = 2,
    WriteManifest = 3,
    RenameManifest = 4,
    CommitBackup = 5,
    Checkpoint = 6,
    Compact = 7,
};

struct WalRecord {
    WalRecordType type = WalRecordType::BeginBackup;
    ByteVector payload;
};

class WalLog {
public:
    explicit WalLog(std::filesystem::path repo);

    void append(WalRecordType type, const std::string& payload);
    void append(WalRecordType type, const ByteVector& payload);
    std::vector<WalRecord> readAll() const;
    void replaceWith(const std::vector<WalRecord>& records) const;

    static std::uint32_t crc32(const ByteVector& data);
    static ByteVector encodeRecord(WalRecordType type, const ByteVector& payload);
    static WalRecord decodeRecord(const ByteVector& encoded);

private:
    std::filesystem::path walPath() const;
    std::filesystem::path repo_;
};

}  // namespace dpc
