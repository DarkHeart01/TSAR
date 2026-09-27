#include "jocky/semantic/Types.h"

#include <stdexcept>

namespace jocky {

JockyType stringToType(const std::string& type) {

    if (type == "int")    return JockyType::INT;
    if (type == "long")   return JockyType::LONG;
    if (type == "byte")   return JockyType::BYTE;
    if (type == "bool")   return JockyType::BOOL;
    if (type == "ptr")    return JockyType::PTR;
    if (type == "handle") return JockyType::PTR;
    if (type == "fnptr")  return JockyType::PTR;
    if (type == "void")   return JockyType::VOID;
    if (type == "string") return JockyType::STRING;

    // "byte[N]"
    if (type.rfind("byte[", 0) == 0) return JockyType::BYTE_ARRAY;

    // "int[N]"
    if (type.rfind("int[", 0) == 0) return JockyType::INT_ARRAY;

    // "handle[N]" treated as pointer array (same as ptr array)
    if (type.rfind("handle[", 0) == 0) return JockyType::INT_ARRAY;

    // Unknown — struct name or truly unknown
    // Struct names are passed around as PTR in semantic analysis
    return JockyType::UNKNOWN;
}


std::string typeToString(JockyType type) {
    switch (type) {
        case JockyType::INT:        return "int";
        case JockyType::LONG:       return "long";
        case JockyType::BYTE:       return "byte";
        case JockyType::BOOL:       return "bool";
        case JockyType::PTR:        return "ptr";
        case JockyType::VOID:       return "void";
        case JockyType::STRING:     return "string";
        case JockyType::BYTE_ARRAY: return "byte[]";
        case JockyType::INT_ARRAY:  return "int[]";
        case JockyType::STRUCT:     return "struct";
        case JockyType::UNKNOWN:    return "unknown";
    }
    return "unknown";
}


bool isArrayType(const std::string& typeStr) {
    return typeStr.rfind("byte[", 0) == 0 ||
           typeStr.rfind("int[", 0) == 0 ||
           typeStr.rfind("handle[", 0) == 0;
}


JockyType getArrayElementType(const std::string& typeStr) {
    if (typeStr.rfind("byte[", 0) == 0) return JockyType::BYTE;
    if (typeStr.rfind("int[", 0) == 0)  return JockyType::INT;
    if (typeStr.rfind("handle[", 0) == 0) return JockyType::PTR;
    return JockyType::UNKNOWN;
}


int getArraySize(const std::string& typeStr) {
    auto bracket = typeStr.find('[');
    if (bracket == std::string::npos) return 0;
    auto close = typeStr.find(']', bracket);
    if (close == std::string::npos) return 0;
    try {
        return std::stoi(typeStr.substr(bracket + 1, close - bracket - 1));
    } catch (...) {
        return 0;
    }
}

} // namespace jocky
