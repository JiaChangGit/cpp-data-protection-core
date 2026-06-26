#include "dpc/core/Compressor.hpp"

#include <gtest/gtest.h>

TEST(CompressorTest, RoundTrip) {
    dpc::ByteVector data;
    for (int i = 0; i < 200000; ++i) {
        data.push_back(static_cast<std::uint8_t>(i % 251));
    }
    const auto compressed = dpc::Compressor::compress(data);
    const auto decompressed = dpc::Compressor::decompress(compressed, data.size());
    EXPECT_EQ(decompressed, data);
}

TEST(CompressorTest, EmptyRoundTrip) {
    dpc::ByteVector data;
    const auto compressed = dpc::Compressor::compress(data);
    const auto decompressed = dpc::Compressor::decompress(compressed, data.size());
    EXPECT_EQ(decompressed, data);
}
