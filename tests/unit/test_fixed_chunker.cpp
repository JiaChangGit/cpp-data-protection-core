#include "dpc/core/FixedChunker.hpp"

#include <gtest/gtest.h>

TEST(FixedChunkerTest, SplitsAtConfiguredSize) {
    dpc::ByteVector data(10, 42);
    dpc::FixedChunker chunker(4);
    const auto chunks = chunker.chunkBytes(data);
    ASSERT_EQ(chunks.size(), 3U);
    EXPECT_EQ(chunks[0].data.size(), 4U);
    EXPECT_EQ(chunks[1].data.size(), 4U);
    EXPECT_EQ(chunks[2].data.size(), 2U);
    EXPECT_EQ(chunks[2].index, 2U);
}

TEST(FixedChunkerTest, EmptyFileStillProducesOneChunk) {
    dpc::FixedChunker chunker(4);
    const auto chunks = chunker.chunkBytes({});
    ASSERT_EQ(chunks.size(), 1U);
    EXPECT_TRUE(chunks[0].data.empty());
}
