#pragma once

#include <string>

namespace jocky {

enum class JockyType {
    INT,
    PTR,
    VOID,
    STRING,
    BOOL,
    UNKNOWN
};

JockyType stringToType(const std::string& type);

std::string typeToString(JockyType type);

} // namespace jocky