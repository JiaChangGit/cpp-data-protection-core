#include "dpc/common/Hash.hpp"

#include <gtest/gtest.h>

TEST(HashTest, Sha256KnownVector) {
    const std::string input = "abc";
    dpc::ByteVector data(input.begin(), input.end());
    EXPECT_EQ(
        dpc::Hash::sha256Hex(data),
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(HashTest, HexRoundTrip) {
    dpc::ByteVector data {0x00, 0x01, 0xab, 0xff};
    const auto hex = dpc::Hash::bytesToHex(data.data(), data.size());
    EXPECT_EQ(hex, "0001abff");
    EXPECT_EQ(dpc::Hash::hexToBytes(hex), data);
}
