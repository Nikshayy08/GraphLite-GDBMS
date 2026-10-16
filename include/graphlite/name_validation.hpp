
#pragma once

#include <string>

namespace graphlite {

inline bool isValidName(const std::string& name) {
    if (name.empty()) {
        return false;
    }

    const auto isLetter = [](unsigned char ch) {
        return (ch >= 'A' && ch <= 'Z') ||
               (ch >= 'a' && ch <= 'z');
    };

    const auto isDigit = [](unsigned char ch) {
        return ch >= '0' && ch <= '9';
    };

    const unsigned char first =
        static_cast<unsigned char>(name.front());

    if (!isLetter(first) && first != '_') {
        return false;
    }

    for (unsigned char ch : name) {
        if (!isLetter(ch) && !isDigit(ch) && ch != '_') {
            return false;
        }
    }

    return true;
}

} // namespace graphlite
