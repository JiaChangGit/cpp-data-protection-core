#include "dpc/network/PacketCodec.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/metadata/WalLog.hpp"

#include <gtest/gtest.h>

namespace {

void put32be(dpc::ByteVector& data, std::size_t offset, std::uint32_t value) {
    data[offset] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
    data[offset + 1] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    data[offset + 2] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    data[offset + 3] = static_cast<std::uint8_t>(value & 0xffU);
}

void refreshHeaderCrc(dpc::ByteVector& frame) {
    dpc::ByteVector header(frame.begin(), frame.begin() + static_cast<std::ptrdiff_t>(dpc::PacketCodec::kHeaderSize));
    for (std::size_t i = 28; i < 32; ++i) {
        header[i] = 0;
    }
    const auto crc = dpc::WalLog::crc32(header);
    put32be(frame, 28, crc);
}

}  // namespace

TEST(PacketCodecTest, EncodeDecodeRoundTrip) {
    dpc::Packet packet;
    packet.type = dpc::PacketType::PutChunk;
    packet.session_id_hash = 1234;
    packet.chunk_index = 99;
    packet.chunk_sha256.fill(0xab);
    packet.payload = dpc::ByteVector {'d', 'a', 't', 'a'};
    const auto frame = dpc::PacketCodec::encode(packet);
    const auto decoded = dpc::PacketCodec::decode(frame);
    EXPECT_EQ(decoded.type, packet.type);
    EXPECT_EQ(decoded.session_id_hash, packet.session_id_hash);
    EXPECT_EQ(decoded.chunk_index, packet.chunk_index);
    EXPECT_EQ(decoded.chunk_sha256, packet.chunk_sha256);
    EXPECT_EQ(decoded.payload, packet.payload);
}

TEST(PacketCodecTest, DetectsHeaderCrcError) {
    dpc::Packet packet;
    packet.payload = dpc::ByteVector {'x'};
    auto frame = dpc::PacketCodec::encode(packet);
    frame[10] ^= 0x01;
    EXPECT_THROW(dpc::PacketCodec::decode(frame), dpc::DpcError);
}

TEST(PacketCodecTest, RejectsOversizedPayload) {
    dpc::Packet packet;
    packet.payload.resize(dpc::PacketCodec::kMaxPayloadSize + 1);
    EXPECT_THROW(dpc::PacketCodec::encode(packet), dpc::DpcError);
}

TEST(PacketCodecTest, RejectsInvalidMagic) {
    dpc::Packet packet;
    auto frame = dpc::PacketCodec::encode(packet);
    frame[0] ^= 0xff;
    EXPECT_THROW(dpc::PacketCodec::decode(frame), dpc::DpcError);
}

TEST(PacketCodecTest, RejectsInvalidVersion) {
    dpc::Packet packet;
    auto frame = dpc::PacketCodec::encode(packet);
    frame[5] = 0x02;
    EXPECT_THROW(dpc::PacketCodec::decode(frame), dpc::DpcError);
}

TEST(PacketCodecTest, RejectsInvalidTypeOnEncodeAndDecode) {
    dpc::Packet packet;
    packet.type = static_cast<dpc::PacketType>(999);
    EXPECT_THROW(dpc::PacketCodec::encode(packet), dpc::DpcError);

    packet.type = dpc::PacketType::Hello;
    auto frame = dpc::PacketCodec::encode(packet);
    frame[6] = 0x03;
    frame[7] = 0xe7;
    refreshHeaderCrc(frame);
    EXPECT_THROW(dpc::PacketCodec::decode(frame), dpc::DpcError);
}

TEST(PacketCodecTest, RejectsTruncatedPayload) {
    dpc::Packet packet;
    packet.payload = dpc::ByteVector {'a', 'b', 'c'};
    auto frame = dpc::PacketCodec::encode(packet);
    frame.pop_back();
    EXPECT_THROW(dpc::PacketCodec::decode(frame), dpc::DpcError);
}
