#include "dpc/metadata/WalLog.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/metadata/RecoveryManager.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <unistd.h>

namespace {

void put16le(dpc::ByteVector& data, std::size_t offset, std::uint16_t value) {
    data[offset] = static_cast<std::uint8_t>(value & 0xffU);
    data[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
}

void put32le(dpc::ByteVector& data, std::size_t offset, std::uint32_t value) {
    data[offset] = static_cast<std::uint8_t>(value & 0xffU);
    data[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    data[offset + 2] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    data[offset + 3] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
}

}  // namespace

TEST(WalLogTest, EncodeDecodeRoundTrip) {
    dpc::ByteVector payload {'h', 'e', 'l', 'l', 'o'};
    const auto encoded = dpc::WalLog::encodeRecord(dpc::WalRecordType::PutObject, payload);
    const auto decoded = dpc::WalLog::decodeRecord(encoded);
    EXPECT_EQ(decoded.type, dpc::WalRecordType::PutObject);
    EXPECT_EQ(decoded.payload, payload);
}

TEST(WalLogTest, DetectsCrcError) {
    dpc::ByteVector payload {'h', 'e', 'l', 'l', 'o'};
    auto encoded = dpc::WalLog::encodeRecord(dpc::WalRecordType::PutObject, payload);
    encoded.back() ^= 0xff;
    EXPECT_THROW(dpc::WalLog::decodeRecord(encoded), dpc::DpcError);
}

TEST(WalLogTest, RejectsInvalidRecordType) {
    dpc::ByteVector payload {'x'};
    auto encoded = dpc::WalLog::encodeRecord(dpc::WalRecordType::PutObject, payload);
    put16le(encoded, 6, 999);
    EXPECT_THROW(dpc::WalLog::decodeRecord(encoded), dpc::DpcError);
    EXPECT_THROW(dpc::WalLog::encodeRecord(static_cast<dpc::WalRecordType>(999), payload), dpc::DpcError);
}

TEST(WalLogTest, RejectsTruncatedHeaderAndPayload) {
    dpc::ByteVector payload {'x', 'y'};
    auto encoded = dpc::WalLog::encodeRecord(dpc::WalRecordType::PutObject, payload);
    dpc::ByteVector short_header(encoded.begin(), encoded.begin() + 8);
    EXPECT_THROW(dpc::WalLog::decodeRecord(short_header), dpc::DpcError);
    encoded.pop_back();
    EXPECT_THROW(dpc::WalLog::decodeRecord(encoded), dpc::DpcError);
}

TEST(WalLogTest, RejectsUnreasonablePayloadSize) {
    auto encoded = dpc::WalLog::encodeRecord(dpc::WalRecordType::PutObject, {});
    put32le(encoded, 8, 0xffffffffU);
    EXPECT_THROW(dpc::WalLog::decodeRecord(encoded), dpc::DpcError);
}

TEST(WalLogTest, RecoveryDoesNotCommitWalOnlyRecords) {
    const auto repo = std::filesystem::temp_directory_path() / ("dpc-wal-test-" + std::to_string(::getpid()));
    std::filesystem::remove_all(repo);

    dpc::WalLog(repo).append(dpc::WalRecordType::BeginBackup, "version=1\n");
    auto report = dpc::RecoveryManager(repo).recover();
    EXPECT_EQ(report.committed_versions, 0U);

    dpc::WalLog(repo).append(dpc::WalRecordType::CommitBackup, "version=1\n");
    report = dpc::RecoveryManager(repo).recover();
    EXPECT_EQ(report.committed_versions, 0U);

    std::filesystem::remove_all(repo);
}
