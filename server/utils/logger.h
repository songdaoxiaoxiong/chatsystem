#pragma once
#include <string>
#include <iostream>

namespace logger {
    inline void info(const std::string& s) { std::cout << s << std::endl; }
}
