#include "dpc/core/ContentDefinedChunker.hpp"

#include <gtest/gtest.h>

TEST(ContentDefinedChunkerTest, ReconstructsInput) {
    dpc::ByteVector data(512 * 1024);
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<std::uint8_t>((i * 17) % 251);
    }
    dpc::ContentDefinedChunker chunker;
    const auto chunks = chunker.chunkBytes(data);
    dpc::ByteVector reconstructed;
    for (const auto& chunk : chunks) {
        EXPECT_LE(chunk.data.size(), 256U * 1024U);
        reconstructed.insert(reconstructed.end(), chunk.data.begin(), chunk.data.end());
    }
    EXPECT_EQ(reconstructed, data);
}
