#include "xslog.hpp"
#include <iostream>
#include <cstdlib>
#include <cerrno>
#include <cstring>

namespace xslog {
    void info(const char* message) {
        std::cout << "[INFO]: " << message << std:endl;
    }
    void error(const char* message) {
        int err = errno;
        std::cerr << "[ERROR]: " << message;
        if(err != 0) {
            std::cerr << "(System Error: " << std::strerror(err) << ")";
        }
        std::cerr << std::endl;
    }
}