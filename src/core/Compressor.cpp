#include "dpc/core/Compressor.hpp"

#include "dpc/common/Error.hpp"

#include <algorithm>
#include <limits>

extern "C" {
size_t ZSTD_compress(void* dst, size_t dstCapacity, const void* src, size_t srcSize, int compressionLevel);
size_t ZSTD_decompress(void* dst, size_t dstCapacity, const void* src, size_t compressedSize);
unsigned long long ZSTD_getFrameContentSize(const void* src, size_t srcSize);
unsigned ZSTD_isError(size_t code);
const char* ZSTD_getErrorName(size_t code);
size_t ZSTD_compressBound(size_t srcSize);
}

namespace dpc {

namespace {
constexpr unsigned long long kZstdContentSizeUnknown = 0ULL - 1ULL;
constexpr unsigned long long kZstdContentSizeError = 0ULL - 2ULL;

void checkZstd(size_t code, const char* operation) {
    if (ZSTD_isError(code)) {
        throw DpcError(std::string(operation) + " failed: " + ZSTD_getErrorName(code));
    }
}
}  // namespace

ByteVector Compressor::compress(const ByteVector& input, int level) {
    const auto bound = ZSTD_compressBound(input.size());
    ByteVector output(bound);
    const auto written = ZSTD_compress(output.data(), output.size(), input.data(), input.size(), level);
    checkZstd(written, "zstd compress");
    output.resize(written);
    return output;
}

ByteVector Compressor::decompress(const ByteVector& input, std::size_t expected_raw_size) {
    std::size_t output_size = expected_raw_size;
    if (output_size == 0) {
        const auto frame_size = ZSTD_getFrameContentSize(input.data(), input.size());
        if (frame_size == kZstdContentSizeError) {
            throw DpcError("zstd frame content size error");
        }
        if (frame_size == kZstdContentSizeUnknown) {
            output_size = std::max<std::size_t>(input.size() * 4, 64 * 1024);
        } else {
            if (frame_size > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
                throw DpcError("zstd frame too large");
            }
            output_size = static_cast<std::size_t>(frame_size);
        }
    }

    for (int attempt = 0; attempt < 8; ++attempt) {
        ByteVector output(output_size);
        const auto written = ZSTD_decompress(output.data(), output.size(), input.data(), input.size());
        if (!ZSTD_isError(written)) {
            output.resize(written);
            if (expected_raw_size != 0 && output.size() != expected_raw_size) {
                throw DpcError("zstd decompressed size mismatch");
            }
            return output;
        }
        if (expected_raw_size != 0) {
            checkZstd(written, "zstd decompress");
        }
        output_size *= 2;
    }
    throw DpcError("zstd decompress failed after growth attempts");
}

}  // namespace dpc
