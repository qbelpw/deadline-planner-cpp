#pragma once

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace deadline {

class PlannerError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class IdGenerator final {
public:
    static std::string create() {
        return boost::uuids::to_string(
            boost::uuids::random_generator{}()
        );
    }

    static bool isValid(const std::string& value) noexcept {
        try {
            static_cast<void>(boost::uuids::string_generator{}(value));
            return true;
        } catch (...) {
            return false;
        }
    }

    IdGenerator() = delete;
};

inline std::string trim(std::string value) {
    const auto notSpace = [](unsigned char character) {
        return !std::isspace(character);
    };
    value.erase(
        value.begin(),
        std::find_if(value.begin(), value.end(), notSpace)
    );
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(),
        value.end()
    );
    return value;
}

inline void require(
    const bool condition,
    const std::string& message
) {
    if (!condition) {
        throw PlannerError(message);
    }
}

}  // namespace deadline
