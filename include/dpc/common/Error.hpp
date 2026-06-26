#pragma once

#include <stdexcept>
#include <string>

namespace dpc {

class DpcError : public std::runtime_error {
public:
    explicit DpcError(const std::string& message) : std::runtime_error(message) {}
};

inline void require(bool condition, const std::string& message) {
    if (!condition) {
        throw DpcError(message);
    }
}

}  // namespace dpc
