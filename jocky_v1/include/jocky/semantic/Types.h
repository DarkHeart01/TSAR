#pragma once

#include <string>

namespace jocky {

enum class JockyType {
    INT,
    LONG,
    BYTE,
    BOOL,
    PTR,
    VOID,
    STRING,
    BYTE_ARRAY,
    INT_ARRAY,
    STRUCT,
    UNKNOWN
};

JockyType stringToType(const std::string& type);

std::string typeToString(JockyType type);

// Returns true for "byte[N]" or "int[N]" type strings
bool isArrayType(const std::string& typeStr);

// Returns the element JockyType for an array type string
JockyType getArrayElementType(const std::string& typeStr);

// Returns the array size N for "byte[N]" or "int[N]", or 0 on error
int getArraySize(const std::string& typeStr);

} // namespace jocky
