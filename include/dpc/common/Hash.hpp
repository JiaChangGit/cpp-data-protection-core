#pragma once

#include "dpc/common/Error.hpp"
#include "dpc/common/FileUtils.hpp"
#include "dpc/common/Types.hpp"

#include <array>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <openssl/evp.h>
#include <sstream>

namespace dpc {

class Hash {
public:
    class Sha256 {
    public:
        Sha256() : ctx_(EVP_MD_CTX_new()) {
            if (!ctx_ || EVP_DigestInit_ex(ctx_, EVP_sha256(), nullptr) != 1) {
                throw DpcError("OpenSSL SHA-256 init failed");
            }
        }

        Sha256(const Sha256&) = delete;
        Sha256& operator=(const Sha256&) = delete;

        Sha256(Sha256&& other) noexcept : ctx_(other.ctx_), finalized_(other.finalized_) {
            other.ctx_ = nullptr;
        }

        ~Sha256() {
            if (ctx_) {
                EVP_MD_CTX_free(ctx_);
            }
        }

        void update(const void* data, std::size_t size) {
            if (finalized_) {
                throw DpcError("SHA-256 context already finalized");
            }
            if (size > 0 && EVP_DigestUpdate(ctx_, data, size) != 1) {
                throw DpcError("OpenSSL SHA-256 update failed");
            }
        }

        void update(const ByteVector& data) {
            update(data.data(), data.size());
        }

        std::array<std::uint8_t, 32> finalBytes() {
            if (finalized_) {
                throw DpcError("SHA-256 context already finalized");
            }
            std::array<std::uint8_t, 32> digest {};
            unsigned int len = 0;
            if (EVP_DigestFinal_ex(ctx_, digest.data(), &len) != 1 || len != digest.size()) {
                throw DpcError("OpenSSL SHA-256 final failed");
            }
            finalized_ = true;
            return digest;
        }

        std::string finalHex() {
            return bytesToHex(finalBytes().data(), 32);
        }

    private:
        EVP_MD_CTX* ctx_ = nullptr;
        bool finalized_ = false;
    };

    static std::string bytesToHex(const std::uint8_t* data, std::size_t size) {
        static constexpr char hex[] = "0123456789abcdef";
        std::string out;
        out.resize(size * 2);
        for (std::size_t i = 0; i < size; ++i) {
            out[i * 2] = hex[(data[i] >> 4) & 0x0f];
            out[i * 2 + 1] = hex[data[i] & 0x0f];
        }
        return out;
    }

    static ByteVector hexToBytes(const std::string& hex) {
        if (hex.size() % 2 != 0) {
            throw DpcError("hex string has odd length");
        }
        ByteVector out(hex.size() / 2);
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = static_cast<std::uint8_t>((fromHex(hex[i * 2]) << 4) | fromHex(hex[i * 2 + 1]));
        }
        return out;
    }

    static bool isSha256Hex(const std::string& value) {
        return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isxdigit(c) != 0;
        });
    }

    static void requireSha256Hex(const std::string& value, const std::string& field) {
        if (!isSha256Hex(value)) {
            throw DpcError(field + " must be a 64-character SHA-256 hex string");
        }
    }

    static std::string sha256Hex(const ByteVector& data) {
        Sha256 ctx;
        ctx.update(data);
        return ctx.finalHex();
    }

    static std::string sha256Hex(const std::uint8_t* data, std::size_t size) {
        Sha256 ctx;
        ctx.update(data, size);
        return ctx.finalHex();
    }

    static std::string sha256File(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            throw DpcError("open file failed for SHA-256: " + path.string());
        }
        Sha256 ctx;
        std::array<char, 64 * 1024> buffer {};
        while (in) {
            in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto n = in.gcount();
            if (n > 0) {
                ctx.update(buffer.data(), static_cast<std::size_t>(n));
            }
        }
        if (!in.eof()) {
            throw DpcError("read file failed for SHA-256: " + path.string());
        }
        return ctx.finalHex();
    }

private:
    static std::uint8_t fromHex(char c) {
        if (c >= '0' && c <= '9') {
            return static_cast<std::uint8_t>(c - '0');
        }
        if (c >= 'a' && c <= 'f') {
            return static_cast<std::uint8_t>(10 + c - 'a');
        }
        if (c >= 'A' && c <= 'F') {
            return static_cast<std::uint8_t>(10 + c - 'A');
        }
        throw DpcError("invalid hex character");
    }
};

}  // namespace dpc
