#include "dpc/metadata/WalLog.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/common/FileUtils.hpp"

#include <array>
#include <fcntl.h>
#include <unistd.h>

namespace dpc {

namespace {
constexpr std::uint32_t kWalMagic = 0x44505741;
constexpr std::uint16_t kWalVersion = 1;
constexpr std::size_t kHeaderSize = 16;
constexpr std::uint32_t kMaxPayloadSize = 8 * 1024 * 1024;

void put16(ByteVector& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void put32(ByteVector& out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xffU));
    }
}

std::uint16_t get16(const ByteVector& data, std::size_t off) {
    return static_cast<std::uint16_t>(data[off] | (data[off + 1] << 8U));
}

std::uint32_t get32(const ByteVector& data, std::size_t off) {
    return static_cast<std::uint32_t>(data[off])
        | (static_cast<std::uint32_t>(data[off + 1]) << 8U)
        | (static_cast<std::uint32_t>(data[off + 2]) << 16U)
        | (static_cast<std::uint32_t>(data[off + 3]) << 24U);
}

bool isValidRecordType(std::uint16_t type) {
    switch (static_cast<WalRecordType>(type)) {
    case WalRecordType::BeginBackup:
    case WalRecordType::PutObject:
    case WalRecordType::WriteManifest:
    case WalRecordType::RenameManifest:
    case WalRecordType::CommitBackup:
    case WalRecordType::Checkpoint:
    case WalRecordType::Compact:
        return true;
    }
    return false;
}
}  // namespace

WalLog::WalLog(std::filesystem::path repo) : repo_(std::move(repo)) {}

std::filesystem::path WalLog::walPath() const {
    return repo_ / "metadata" / "wal.log";
}

void WalLog::append(WalRecordType type, const std::string& payload) {
    append(type, ByteVector(payload.begin(), payload.end()));
}

void WalLog::append(WalRecordType type, const ByteVector& payload) {
    fileutil::ensureDirectory(walPath().parent_path());
    const auto encoded = encodeRecord(type, payload);
    fileutil::UniqueFd fd(::open(walPath().c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644));
    if (fd.get() < 0) {
        throw DpcError(fileutil::errnoMessage("open wal failed"));
    }
    fileutil::writeAll(fd.get(), encoded.data(), encoded.size());
    fileutil::fsyncFd(fd.get(), "wal.log");
}

std::vector<WalRecord> WalLog::readAll() const {
    std::vector<WalRecord> records;
    if (!std::filesystem::exists(walPath())) {
        return records;
    }
    const auto data = fileutil::readFile(walPath());
    std::size_t offset = 0;
    while (offset < data.size()) {
        if (data.size() - offset < kHeaderSize) {
            throw DpcError("wal truncated in record header");
        }
        const auto payload_size = get32(data, offset + 8);
        if (payload_size > kMaxPayloadSize) {
            throw DpcError("wal payload too large");
        }
        const auto record_size = kHeaderSize + static_cast<std::size_t>(payload_size);
        if (data.size() - offset < record_size) {
            throw DpcError("wal truncated in record payload");
        }
        ByteVector encoded(data.begin() + static_cast<std::ptrdiff_t>(offset), data.begin() + static_cast<std::ptrdiff_t>(offset + record_size));
        records.push_back(decodeRecord(encoded));
        offset += record_size;
    }
    return records;
}

void WalLog::replaceWith(const std::vector<WalRecord>& records) const {
    ByteVector data;
    for (const auto& record : records) {
        const auto encoded = encodeRecord(record.type, record.payload);
        data.insert(data.end(), encoded.begin(), encoded.end());
    }
    fileutil::writeFileAtomic(walPath(), data, 0644);
}

std::uint32_t WalLog::crc32(const ByteVector& data) {
    std::uint32_t crc = 0xffffffffU;
    for (const auto byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            const auto mask = static_cast<std::uint32_t>(-(crc & 1U));
            crc = (crc >> 1U) ^ (0xedb88320U & mask);
        }
    }
    return ~crc;
}

ByteVector WalLog::encodeRecord(WalRecordType type, const ByteVector& payload) {
    if (!isValidRecordType(static_cast<std::uint16_t>(type))) {
        throw DpcError("invalid wal record type");
    }
    if (payload.size() > kMaxPayloadSize) {
        throw DpcError("wal payload too large");
    }
    ByteVector out;
    put32(out, kWalMagic);
    put16(out, kWalVersion);
    put16(out, static_cast<std::uint16_t>(type));
    put32(out, static_cast<std::uint32_t>(payload.size()));
    put32(out, crc32(payload));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

WalRecord WalLog::decodeRecord(const ByteVector& encoded) {
    if (encoded.size() < kHeaderSize) {
        throw DpcError("wal record too small");
    }
    const auto magic = get32(encoded, 0);
    const auto version = get16(encoded, 4);
    const auto type = get16(encoded, 6);
    const auto payload_size = get32(encoded, 8);
    const auto expected_crc = get32(encoded, 12);
    if (magic != kWalMagic) {
        throw DpcError("wal magic mismatch");
    }
    if (version != kWalVersion) {
        throw DpcError("wal version mismatch");
    }
    if (!isValidRecordType(type)) {
        throw DpcError("wal record type mismatch");
    }
    if (payload_size > kMaxPayloadSize) {
        throw DpcError("wal payload too large");
    }
    if (encoded.size() != kHeaderSize + payload_size) {
        throw DpcError("wal record size mismatch");
    }
    ByteVector payload(encoded.begin() + static_cast<std::ptrdiff_t>(kHeaderSize), encoded.end());
    if (crc32(payload) != expected_crc) {
        throw DpcError("wal crc mismatch");
    }
    return WalRecord{static_cast<WalRecordType>(type), std::move(payload)};
}

}  // namespace dpc
