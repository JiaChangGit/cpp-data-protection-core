#include "dpc/network/PacketCodec.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/common/FileUtils.hpp"
#include "dpc/common/Hash.hpp"
#include "dpc/metadata/WalLog.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace dpc {

namespace {

void put16(ByteVector& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void put32(ByteVector& out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void put64(ByteVector& out, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

std::uint16_t get16(const ByteVector& data, std::size_t off) {
    return static_cast<std::uint16_t>((data[off] << 8U) | data[off + 1]);
}

std::uint32_t get32(const ByteVector& data, std::size_t off) {
    return (static_cast<std::uint32_t>(data[off]) << 24U)
        | (static_cast<std::uint32_t>(data[off + 1]) << 16U)
        | (static_cast<std::uint32_t>(data[off + 2]) << 8U)
        | static_cast<std::uint32_t>(data[off + 3]);
}

std::uint64_t get64(const ByteVector& data, std::size_t off) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8U) | data[off + static_cast<std::size_t>(i)];
    }
    return value;
}

void readExact(int fd, std::uint8_t* data, std::size_t size) {
    std::size_t offset = 0;
    while (offset < size) {
        ssize_t n = ::read(fd, data + offset, size - offset);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw DpcError(fileutil::errnoMessage("socket read failed"));
        }
        if (n == 0) {
            throw DpcError("socket closed");
        }
        offset += static_cast<std::size_t>(n);
    }
}

void zeroHeaderCrc(ByteVector& header) {
    for (std::size_t i = 28; i < 32; ++i) {
        header[i] = 0;
    }
}

bool isValidPacketType(std::uint16_t type) {
    switch (static_cast<PacketType>(type)) {
    case PacketType::Hello:
    case PacketType::QuerySession:
    case PacketType::SessionStatus:
    case PacketType::PutChunk:
    case PacketType::PutChunkAck:
    case PacketType::CommitSession:
    case PacketType::CommitAck:
    case PacketType::Error:
        return true;
    }
    return false;
}

}  // namespace

ByteVector PacketCodec::encode(const Packet& packet) {
    if (!isValidPacketType(static_cast<std::uint16_t>(packet.type))) {
        throw DpcError("packet type mismatch");
    }
    if (packet.payload.size() > kMaxPayloadSize) {
        throw DpcError("packet payload too large");
    }
    ByteVector header;
    header.reserve(kHeaderSize);
    put32(header, kMagic);
    put16(header, kVersion);
    put16(header, static_cast<std::uint16_t>(packet.type));
    put64(header, packet.session_id_hash);
    put64(header, packet.chunk_index);
    put32(header, static_cast<std::uint32_t>(packet.payload.size()));
    put32(header, 0);
    header.insert(header.end(), packet.chunk_sha256.begin(), packet.chunk_sha256.end());
    const auto crc = WalLog::crc32(header);
    header[28] = static_cast<std::uint8_t>((crc >> 24U) & 0xffU);
    header[29] = static_cast<std::uint8_t>((crc >> 16U) & 0xffU);
    header[30] = static_cast<std::uint8_t>((crc >> 8U) & 0xffU);
    header[31] = static_cast<std::uint8_t>(crc & 0xffU);

    ByteVector frame = header;
    frame.insert(frame.end(), packet.payload.begin(), packet.payload.end());
    return frame;
}

Packet PacketCodec::decode(const ByteVector& frame) {
    if (frame.size() < kHeaderSize) {
        throw DpcError("packet frame too small");
    }
    ByteVector header(frame.begin(), frame.begin() + static_cast<std::ptrdiff_t>(kHeaderSize));
    const auto magic = get32(header, 0);
    const auto version = get16(header, 4);
    const auto type = get16(header, 6);
    const auto session_hash = get64(header, 8);
    const auto chunk_index = get64(header, 16);
    const auto payload_size = get32(header, 24);
    const auto expected_crc = get32(header, 28);
    if (magic != kMagic) {
        throw DpcError("packet magic mismatch");
    }
    if (version != kVersion) {
        throw DpcError("packet version mismatch");
    }
    if (!isValidPacketType(type)) {
        throw DpcError("packet type mismatch");
    }
    if (payload_size > kMaxPayloadSize) {
        throw DpcError("packet payload too large");
    }
    if (frame.size() != kHeaderSize + payload_size) {
        throw DpcError("packet frame size mismatch");
    }
    zeroHeaderCrc(header);
    if (WalLog::crc32(header) != expected_crc) {
        throw DpcError("packet header crc mismatch");
    }
    Packet packet;
    packet.type = static_cast<PacketType>(type);
    packet.session_id_hash = session_hash;
    packet.chunk_index = chunk_index;
    std::copy(frame.begin() + 32, frame.begin() + 64, packet.chunk_sha256.begin());
    packet.payload.insert(packet.payload.end(), frame.begin() + static_cast<std::ptrdiff_t>(kHeaderSize), frame.end());
    return packet;
}

Packet PacketCodec::readPacket(int fd) {
    ByteVector header(kHeaderSize);
    readExact(fd, header.data(), header.size());
    const auto payload_size = get32(header, 24);
    if (payload_size > kMaxPayloadSize) {
        throw DpcError("packet payload too large");
    }
    ByteVector frame = header;
    frame.resize(kHeaderSize + payload_size);
    if (payload_size > 0) {
        readExact(fd, frame.data() + kHeaderSize, payload_size);
    }
    return decode(frame);
}

void PacketCodec::writePacket(int fd, const Packet& packet) {
    const auto frame = encode(packet);
    fileutil::writeAll(fd, frame.data(), frame.size());
}

std::uint64_t PacketCodec::sessionHash(const std::string& session_id) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : session_id) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::array<std::uint8_t, 32> PacketCodec::shaArrayFromHex(const std::string& hex) {
    const auto bytes = Hash::hexToBytes(hex);
    if (bytes.size() != 32) {
        throw DpcError("sha hex must be 32 bytes");
    }
    std::array<std::uint8_t, 32> out {};
    std::copy(bytes.begin(), bytes.end(), out.begin());
    return out;
}

std::string PacketCodec::shaHexFromArray(const std::array<std::uint8_t, 32>& bytes) {
    return Hash::bytesToHex(bytes.data(), bytes.size());
}

}  // namespace dpc
