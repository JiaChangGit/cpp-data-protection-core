#pragma once

#include <iostream>
#include <string>

namespace dpc {

class Logger {
public:
    static void info(const std::string& message) {
        std::cout << message << '\n';
    }

    static void warn(const std::string& message) {
        std::cerr << "warning: " << message << '\n';
    }

    static void error(const std::string& message) {
        std::cerr << "error: " << message << '\n';
    }
};

}  // namespace dpc
