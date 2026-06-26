#pragma once

#include "dpc/common/Types.hpp"

#include <cstddef>

namespace dpc {

class Compressor {
public:
    static ByteVector compress(const ByteVector& input, int level = 3);
    static ByteVector decompress(const ByteVector& input, std::size_t expected_raw_size = 0);
};

}  // namespace dpc
