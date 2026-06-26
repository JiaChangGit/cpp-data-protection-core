#pragma once

#include "dpc/common/Types.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace dpc {

enum class PacketType : std::uint16_t {
    Hello = 1,
    QuerySession = 2,
    SessionStatus = 3,
    PutChunk = 4,
    PutChunkAck = 5,
    CommitSession = 6,
    CommitAck = 7,
    Error = 8,
};

struct Packet {
    PacketType type = PacketType::Hello;
    std::uint64_t session_id_hash = 0;
    std::uint64_t chunk_index = 0;
    std::array<std::uint8_t, 32> chunk_sha256 {};
    ByteVector payload;
};

class PacketCodec {
public:
    static constexpr std::uint32_t kMagic = 0x44504350;
    static constexpr std::uint16_t kVersion = 1;
    static constexpr std::size_t kHeaderSize = 64;
    static constexpr std::uint32_t kMaxPayloadSize = 8 * 1024 * 1024;

    static ByteVector encode(const Packet& packet);
    static Packet decode(const ByteVector& frame);
    static Packet readPacket(int fd);
    static void writePacket(int fd, const Packet& packet);
    static std::uint64_t sessionHash(const std::string& session_id);
    static std::array<std::uint8_t, 32> shaArrayFromHex(const std::string& hex);
    static std::string shaHexFromArray(const std::array<std::uint8_t, 32>& bytes);
};

}  // namespace dpc
