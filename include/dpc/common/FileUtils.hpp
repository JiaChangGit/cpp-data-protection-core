#pragma once

#include "dpc/common/Error.hpp"
#include "dpc/common/Types.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

namespace dpc::fileutil {

class UniqueFd {
public:
    UniqueFd() = default;
    explicit UniqueFd(int fd) : fd_(fd) {}
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    ~UniqueFd() {
        reset();
    }

    int get() const {
        return fd_;
    }

    int release() {
        int out = fd_;
        fd_ = -1;
        return out;
    }

    void reset(int fd = -1) {
        if (fd_ >= 0) {
            while (::close(fd_) < 0 && errno == EINTR) {
            }
        }
        fd_ = fd;
    }

private:
    int fd_ = -1;
};

inline std::string errnoMessage(const std::string& prefix) {
    return prefix + ": " + std::strerror(errno);
}

inline void ensureDirectory(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        throw DpcError("create directory failed: " + path.string() + ": " + ec.message());
    }
}

inline void writeAll(int fd, const std::uint8_t* data, std::size_t size) {
    std::size_t offset = 0;
    while (offset < size) {
        ssize_t n = ::write(fd, data + offset, size - offset);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw DpcError(errnoMessage("write failed"));
        }
        if (n == 0) {
            throw DpcError("write returned zero bytes");
        }
        offset += static_cast<std::size_t>(n);
    }
}

inline void fsyncFd(int fd, const std::string& what) {
    while (::fsync(fd) < 0) {
        if (errno == EINTR) {
            continue;
        }
        throw DpcError(errnoMessage("fsync failed for " + what));
    }
}

inline void fsyncDirectory(const std::filesystem::path& path) {
    UniqueFd fd(::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (fd.get() < 0) {
        throw DpcError(errnoMessage("open directory failed: " + path.string()));
    }
    fsyncFd(fd.get(), path.string());
}

inline std::filesystem::path makeTempPath(const std::filesystem::path& final_path) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto name = final_path.filename().string() + ".tmp." + std::to_string(::getpid()) + "." + std::to_string(now);
    return final_path.parent_path() / name;
}

inline void writeFileAtomic(const std::filesystem::path& final_path, const ByteVector& data, mode_t mode = 0644) {
    ensureDirectory(final_path.parent_path());
    const auto tmp_path = makeTempPath(final_path);
    UniqueFd fd(::open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode));
    if (fd.get() < 0) {
        throw DpcError(errnoMessage("open temp file failed: " + tmp_path.string()));
    }
    if (!data.empty()) {
        writeAll(fd.get(), data.data(), data.size());
    }
    fsyncFd(fd.get(), tmp_path.string());
    fd.reset();
    if (::rename(tmp_path.c_str(), final_path.c_str()) < 0) {
        std::error_code ignored;
        std::filesystem::remove(tmp_path, ignored);
        throw DpcError(errnoMessage("rename failed: " + tmp_path.string() + " -> " + final_path.string()));
    }
    fsyncDirectory(final_path.parent_path());
}

inline void writeTextAtomic(const std::filesystem::path& final_path, const std::string& text, mode_t mode = 0644) {
    ByteVector data(text.begin(), text.end());
    writeFileAtomic(final_path, data, mode);
}

inline ByteVector readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw DpcError("open file failed: " + path.string());
    }
    return ByteVector(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

inline std::string readTextFile(const std::filesystem::path& path) {
    const auto data = readFile(path);
    return std::string(data.begin(), data.end());
}

inline std::uint32_t fileMode(const std::filesystem::path& path) {
    struct stat st {};
    if (::stat(path.c_str(), &st) < 0) {
        throw DpcError(errnoMessage("stat failed: " + path.string()));
    }
    return static_cast<std::uint32_t>(st.st_mode & 0777);
}

inline void setFileMode(const std::filesystem::path& path, std::uint32_t mode) {
    if (::chmod(path.c_str(), static_cast<mode_t>(mode)) < 0) {
        throw DpcError(errnoMessage("chmod failed: " + path.string()));
    }
}

inline std::filesystem::path safeRelativePath(const std::string& value, const std::string& field_name) {
    if (value.find('\0') != std::string::npos) {
        throw DpcError(field_name + " must not contain null bytes");
    }
    std::filesystem::path input(value);
    if (value.empty() || input.is_absolute()) {
        throw DpcError(field_name + " must be a non-empty relative path");
    }

    const auto normalized = input.lexically_normal();
    if (normalized.empty() || normalized == ".") {
        throw DpcError(field_name + " must not resolve to current directory");
    }
    for (const auto& part : normalized) {
        if (part == "..") {
            throw DpcError(field_name + " must not contain parent directory traversal");
        }
    }
    return normalized;
}

inline std::filesystem::path safeJoinUnderRoot(
    const std::filesystem::path& root,
    const std::string& relative,
    const std::string& field_name) {
    return root / safeRelativePath(relative, field_name);
}

inline std::uint64_t fileSize(const std::filesystem::path& path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        throw DpcError("file_size failed: " + path.string() + ": " + ec.message());
    }
    return static_cast<std::uint64_t>(size);
}

}  // namespace dpc::fileutil
