#include "jocky/semantic/Types.h"

namespace jocky {

JockyType stringToType(
    const std::string& type
) {
    if (type == "int") {
        return JockyType::INT;
    }

    if (type == "ptr") {
        return JockyType::PTR;
    }

    if (type == "void") {
        return JockyType::VOID;
    }

    if (type == "string") {
        return JockyType::STRING;
    }

    if (type == "bool") {
        return JockyType::BOOL;
    }

    return JockyType::UNKNOWN;
}


std::string typeToString(
    JockyType type
) {
    switch (type) {
        case JockyType::INT:
            return "int";

        case JockyType::PTR:
            return "ptr";

        case JockyType::VOID:
            return "void";

        case JockyType::STRING:
            return "string";

        case JockyType::BOOL:
            return "bool";

        case JockyType::UNKNOWN:
            return "unknown";
    }

    return "unknown";
}

}